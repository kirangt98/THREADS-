#define _CRT_SECURE_NO_WARNINGS 1

/* ------------------------------------------------------------------------
   Messaging.c
   College of Applied Science and Technology
   The University of Arizona
   CYBV 489

   Student Names:  <add your group members here>

   ------------------------------------------------------------------------ */
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <THREADSLib.h>
#include <Scheduler.h>
#include <Messaging.h>
#include <stdint.h>
#include "message.h"

   /* ------------------------- Prototypes ----------------------------------- */
static void nullsys(system_call_arguments_t* args);
static void InitializeHandlers();
static int check_io_messaging(void);
extern int MessagingEntryPoint(void*);
static void checkKernelMode(const char* functionName);

/* Clock handler and I/O handler prototypes */
static void clock_handler(char deviceId[32], uint8_t command, uint32_t status, void* pArgs);
static void io_handler(char deviceId[32], uint8_t command, uint32_t status, void* pArgs);
static void syscall_handler(char deviceId[32], uint8_t command, uint32_t status, void* pArgs);

/* Scheduler functions � already declared in Scheduler.h, but add forward if needed */
extern int unblock(int pid);   // unblock a process (exported by scheduler)
/* k_kill, k_spawn, k_wait, k_exit, etc. are in THREADSLib.h / Scheduler.h */

struct psr_bits {
    unsigned int cur_int_enable : 1;
    unsigned int cur_mode : 1;
    unsigned int prev_int_enable : 1;
    unsigned int prev_mode : 1;
    unsigned int unused : 28;
};

union psr_values {
    struct psr_bits bits;
    unsigned int integer_part;
};

/* -------------------------- Globals ------------------------------------- */

/* Obtained from THREADS*/
interrupt_handler_t* handlers;

/* system call array of function pointers */
void (*systemCallVector[THREADS_MAX_SYSCALLS])(system_call_arguments_t* args);

/* the mail boxes */
MailBox mailboxes[MAXMBOX];
MailSlot mailSlots[MAXSLOTS];

typedef struct
{
    void* deviceHandle;
    int deviceMbox;
    int deviceType;
    char deviceName[16];
} DeviceManagementData;

static DeviceManagementData devices[THREADS_MAX_DEVICES];
static int waitingOnDevice = 0;
static int tickCount = 0;

/* For tracking waiting processes */
static WaitingProcess* sendWaitQueues[MAXMBOX];
static WaitingProcess* receiveWaitQueues[MAXMBOX];

/* For zero-slot mailbox message passing */
static char zeroSlotMsg[MAXMBOX][MAX_MESSAGE];
static int  zeroSlotMsgSize[MAXMBOX];

/* ------------------------------------------------------------------------
   Helper functions
----------------------------------------------------------------------- */
static MailSlot* findFreeSlot()
{
    for (int i = 0; i < MAXSLOTS; i++) {
        if (mailSlots[i].mbox_id == -1) {
            return &mailSlots[i];
        }
    }
    return NULL;
}

static int countMessages(int mboxId)
{
    int count = 0;
    MailSlot* current = mailboxes[mboxId].pSlotListHead;
    while (current != NULL) {
        count++;
        current = current->pNextSlot;
    }
    return count;
}

static void addToWaitQueue(WaitingProcess** queue, int pid)
{
    WaitingProcess* newProcess = (WaitingProcess*)malloc(sizeof(WaitingProcess));
    if (newProcess == NULL) return;
    newProcess->pid = pid;
    newProcess->pNextProcess = NULL;
    newProcess->pPrevProcess = NULL;

    if (*queue == NULL) {
        *queue = newProcess;
    }
    else {
        WaitingProcess* current = *queue;
        while (current->pNextProcess != NULL) {
            current = current->pNextProcess;
        }
        current->pNextProcess = newProcess;
        newProcess->pPrevProcess = current;
    }
}

static int removeFromWaitQueue(WaitingProcess** queue)
{
    if (*queue == NULL) {
        return -1;
    }

    int pid = (*queue)->pid;
    WaitingProcess* temp = *queue;
    *queue = (*queue)->pNextProcess;
    if (*queue != NULL) {
        (*queue)->pPrevProcess = NULL;
    }
    free(temp);
    return pid;
}

static void signalAllWaiting(WaitingProcess** queue)
{
    int pid;
    while ((pid = removeFromWaitQueue(queue)) != -1) {
        k_kill(pid, SIG_TERM);
        unblock(pid);
    }
}

/* ------------------------------------------------------------------------
     Name - SchedulerEntryPoint
----------------------------------------------------------------------- */
int SchedulerEntryPoint(void* arg)
{
    int status;
    int pid;
    char* deviceNames[] = { "disk0", "disk1", "term0", "term1", "term2", "term3" };

    checkKernelMode("SchedulerEntryPoint");

    disableInterrupts();

    check_io = check_io_messaging;

    /* Initialize mailboxes array */
    for (int i = 0; i < MAXMBOX; i++) {
        mailboxes[i].status = MBSTATUS_EMPTY;
        mailboxes[i].pSlotListHead = NULL;
        mailboxes[i].mbox_id = -1;
        mailboxes[i].slotCount = 0;
        mailboxes[i].slotSize = 0;
        mailboxes[i].type = MB_ZEROSLOT;
        sendWaitQueues[i] = NULL;
        receiveWaitQueues[i] = NULL;
        zeroSlotMsgSize[i] = 0;
    }

    /* Initialize mail slots array */
    for (int i = 0; i < MAXSLOTS; i++) {
        mailSlots[i].pNextSlot = NULL;
        mailSlots[i].pPrevSlot = NULL;
        mailSlots[i].mbox_id = -1;
        mailSlots[i].messageSize = 0;
        memset(mailSlots[i].message, 0, MAX_MESSAGE);
    }

    /* Initialize devices array */
    for (int i = 0; i < THREADS_MAX_DEVICES; i++) {
        devices[i].deviceMbox = -1;
        devices[i].deviceHandle = NULL;
        devices[i].deviceType = -1;
        memset(devices[i].deviceName, 0, sizeof(devices[i].deviceName));
    }

    /* Create mailbox for clock device (ID 0) */
    devices[0].deviceMbox = mailbox_create(0, sizeof(int));
    if (devices[0].deviceMbox >= 0) {
        memcpy(devices[0].deviceName, "clock", 6);
        devices[0].deviceType = DEVICE_CLOCK;
        devices[0].deviceHandle = NULL;
    }

    /* Create mailboxes for all device slots 1..7 */
    for (int deviceId = 1; deviceId < THREADS_MAX_DEVICES; deviceId++) {
        // I/O devices need slotted mailboxes
        devices[deviceId].deviceMbox = mailbox_create(5, sizeof(int));
        if (deviceId <= 6) { // six physical devices (disk0, disk1, term0-3)
            int idx = deviceId - 1;
            uint32_t handle = device_initialize(deviceNames[idx]);
            devices[deviceId].deviceHandle = (void*)(uintptr_t)handle;
            memcpy(devices[deviceId].deviceName, deviceNames[idx], strlen(deviceNames[idx]) + 1);
            devices[deviceId].deviceType = (idx < 2) ? DEVICE_DISK : DEVICE_TERMINAL;
        }
        else {
            // unused device slot (ID 7)
            memcpy(devices[deviceId].deviceName, "unused", 7);
            devices[deviceId].deviceType = DEVICE_CLOCK; // dummy
        }
    }

    InitializeHandlers();

    enableInterrupts();

    /* Spawn the Messaging test process */
    pid = k_spawn("MessagingEntryPoint", MessagingEntryPoint, NULL,
        4 * THREADS_MIN_STACK_SIZE, 3);

    if (pid > 0) {
        k_wait(&status);
    }

    k_exit(0);
    return 0;
}

/* ------------------------------------------------------------------------
   Name - mailbox_create
----------------------------------------------------------------------- */
int mailbox_create(int slots, int slot_size)
{
    int newId = -1;

    checkKernelMode("mailbox_create");

    if (slots < 0 || slots > 2500) return -1;
    if (slot_size < 0 || slot_size > MAX_MESSAGE) return -1;

    for (int i = 0; i < MAXMBOX; i++) {
        if (mailboxes[i].status == MBSTATUS_EMPTY) {
            newId = i;
            break;
        }
    }

    if (newId != -1) {
        mailboxes[newId].mbox_id = newId;
        mailboxes[newId].status = MBSTATUS_INUSE;
        mailboxes[newId].pSlotListHead = NULL;
        mailboxes[newId].slotCount = slots;
        mailboxes[newId].slotSize = slot_size;

        if (slots == 0) {
            mailboxes[newId].type = MB_ZEROSLOT;
        }
        else if (slots == 1) {
            mailboxes[newId].type = MB_SINGLESLOT;
        }
        else {
            mailboxes[newId].type = MB_MULTISLOT;
        }
    }

    return newId;
}

/* ------------------------------------------------------------------------
   Name - mailbox_send
----------------------------------------------------------------------- */
int mailbox_send(int mboxId, void* pMsg, int msg_size, int wait)
{
    int result = -1;

    checkKernelMode("mailbox_send");

    /* Validate mailbox */
    if (mboxId < 0 || mboxId >= MAXMBOX) return -1;
    if (mailboxes[mboxId].status != MBSTATUS_INUSE) return -1;
    if (pMsg == NULL && msg_size > 0) return -1;
    if (msg_size > mailboxes[mboxId].slotSize) return -1;

    /* Zero-slot mailbox */
    if (mailboxes[mboxId].type == MB_ZEROSLOT) {
        int receiverPid = removeFromWaitQueue(&receiveWaitQueues[mboxId]);
        if (receiverPid != -1) {
            /* Direct handoff: copy message to a per-mailbox buffer */
            if (pMsg != NULL && msg_size > 0) {
                memcpy(zeroSlotMsg[mboxId], pMsg, msg_size);
                zeroSlotMsgSize[mboxId] = msg_size;
            }
            else {
                zeroSlotMsgSize[mboxId] = 0;
            }
            unblock(receiverPid);
            return 0;
        }
        else if (wait) {
            addToWaitQueue(&sendWaitQueues[mboxId], k_getpid());
            block(BLOCKED_SEND);
            if (signaled()) return -5;
            if (mailboxes[mboxId].status != MBSTATUS_INUSE) return -5;
            /* After being woken, the message was already transferred by receiver */
            return 0;
        }
        else {
            return -2;
        }
    }

    /* Slotted mailbox */
    while (1) {
        int current = countMessages(mboxId);
        if (current < mailboxes[mboxId].slotCount) {
            /* There is space */
            MailSlot* newSlot = findFreeSlot();
            if (newSlot == NULL) {
                console_output(FALSE, "No mail slots available.\n");
                stop(1);
            }
            newSlot->mbox_id = mboxId;
            newSlot->messageSize = msg_size;
            if (pMsg != NULL && msg_size > 0) {
                memcpy(newSlot->message, pMsg, msg_size);
            }
            newSlot->pNextSlot = NULL;
            newSlot->pPrevSlot = NULL;

            if (mailboxes[mboxId].pSlotListHead == NULL) {
                mailboxes[mboxId].pSlotListHead = newSlot;
            }
            else {
                MailSlot* last = mailboxes[mboxId].pSlotListHead;
                while (last->pNextSlot != NULL) last = last->pNextSlot;
                last->pNextSlot = newSlot;
                newSlot->pPrevSlot = last;
            }

            int waitingReceiver = removeFromWaitQueue(&receiveWaitQueues[mboxId]);
            if (waitingReceiver != -1) unblock(waitingReceiver);

            result = 0;
            break;
        }
        else if (wait) {
            addToWaitQueue(&sendWaitQueues[mboxId], k_getpid());
            block(BLOCKED_SEND);
            if (signaled()) return -5;
            if (mailboxes[mboxId].status != MBSTATUS_INUSE) return -5;
            /* Loop to try again */
        }
        else {
            return -2;
        }
    }

    return result;
}

/* ------------------------------------------------------------------------
   Name - mailbox_receive
----------------------------------------------------------------------- */
int mailbox_receive(int mboxId, void* pMsg, int msg_size, int wait)
{
    int result = -1;

    checkKernelMode("mailbox_receive");

    disableInterrupts();

    /* Validate mailbox */
    if (mboxId < 0 || mboxId >= MAXMBOX) {
        enableInterrupts();
        return -1;
    }
    if (mailboxes[mboxId].status != MBSTATUS_INUSE) {
        enableInterrupts();
        return -1;
    }
    if (pMsg == NULL && msg_size > 0) {
        enableInterrupts();
        return -1;
    }

    /* Zero-slot mailbox */
    if (mailboxes[mboxId].type == MB_ZEROSLOT) {
        int senderPid = removeFromWaitQueue(&sendWaitQueues[mboxId]);
        if (senderPid != -1) {
            /* Sender waiting � direct handoff */
            if (pMsg != NULL && zeroSlotMsgSize[mboxId] > 0 && zeroSlotMsgSize[mboxId] <= msg_size) {
                memcpy(pMsg, zeroSlotMsg[mboxId], zeroSlotMsgSize[mboxId]);
                result = zeroSlotMsgSize[mboxId];
            }
            else {
                result = 0;
            }
            zeroSlotMsgSize[mboxId] = 0;
            unblock(senderPid);
            enableInterrupts();
            return result;
        }
        else if (wait) {
            addToWaitQueue(&receiveWaitQueues[mboxId], k_getpid());
            block(BLOCKED_RECEIVE);
            enableInterrupts();
            if (signaled()) return -5;
            if (mailboxes[mboxId].status != MBSTATUS_INUSE) return -5;
            /* After being woken, message was already placed by sender */
            if (pMsg != NULL && zeroSlotMsgSize[mboxId] > 0 && zeroSlotMsgSize[mboxId] <= msg_size) {
                memcpy(pMsg, zeroSlotMsg[mboxId], zeroSlotMsgSize[mboxId]);
                result = zeroSlotMsgSize[mboxId];
            }
            else {
                result = 0;
            }
            zeroSlotMsgSize[mboxId] = 0;
            return result;
        }
        else {
            enableInterrupts();
            return -2;
        }
    }

    /* Slotted mailbox */
    while (1) {
        MailSlot* first = mailboxes[mboxId].pSlotListHead;
        if (first != NULL) {
            result = first->messageSize;
            if (pMsg != NULL && result > 0 && result <= msg_size) {
                memcpy(pMsg, first->message, result);
            }

            /* Remove slot from list */
            mailboxes[mboxId].pSlotListHead = first->pNextSlot;
            if (first->pNextSlot != NULL) {
                first->pNextSlot->pPrevSlot = NULL;
            }

            first->mbox_id = -1;
            first->messageSize = 0;
            first->pNextSlot = NULL;
            first->pPrevSlot = NULL;

            int waitingSender = removeFromWaitQueue(&sendWaitQueues[mboxId]);
            if (waitingSender != -1) unblock(waitingSender);

            enableInterrupts();
            return result;
        }
        else if (wait) {
            addToWaitQueue(&receiveWaitQueues[mboxId], k_getpid());
            block(BLOCKED_RECEIVE);
            if (signaled()) {
                enableInterrupts();
                return -5;
            }
            if (mailboxes[mboxId].status != MBSTATUS_INUSE) {
                enableInterrupts();
                return -5;
            }
            /* Loop to try again */
        }
        else {
            enableInterrupts();
            return -2;
        }
    }
}

/* ------------------------------------------------------------------------
   Name - mailbox_free
----------------------------------------------------------------------- */
int mailbox_free(int mboxId)
{
    int result = -1;

    checkKernelMode("mailbox_free");

    if (mboxId < 0 || mboxId >= MAXMBOX) return -1;
    if (mailboxes[mboxId].status != MBSTATUS_INUSE) return -1;

    /* Free all message slots */
    MailSlot* current = mailboxes[mboxId].pSlotListHead;
    while (current != NULL) {
        MailSlot* next = current->pNextSlot;
        current->mbox_id = -1;
        current->messageSize = 0;
        current = next;
    }
    mailboxes[mboxId].pSlotListHead = NULL;

    /* Mark as released BEFORE unblocking so waiters detect it */
    mailboxes[mboxId].status = MBSTATUS_RELEASED;

    /* Signal all waiting processes */
    signalAllWaiting(&sendWaitQueues[mboxId]);
    signalAllWaiting(&receiveWaitQueues[mboxId]);

    mailboxes[mboxId].status = MBSTATUS_EMPTY;
    mailboxes[mboxId].mbox_id = -1;

    result = 0;
    if (signaled()) result = -5;

    return result;
}

/* ------------------------------------------------------------------------
   Name - wait_device
----------------------------------------------------------------------- */
int wait_device(char* deviceName, int* status)
{
    int result = 0;
    int deviceIdx = -1;
    checkKernelMode("wait_device");

    enableInterrupts();

    /* Find our device by name */
    for (int i = 0; i < THREADS_MAX_DEVICES; i++) {
        if (strlen(devices[i].deviceName) > 0 && strcmp(devices[i].deviceName, deviceName) == 0) {
            deviceIdx = i;
            break;
        }
    }

    if (deviceIdx >= 0) {
        if (devices[deviceIdx].deviceMbox < 0) {
            if (deviceIdx == 0) {
                devices[deviceIdx].deviceMbox = mailbox_create(0, sizeof(int));
            }
            else {
                devices[deviceIdx].deviceMbox = mailbox_create(5, sizeof(int));
            }
        }
        waitingOnDevice++;
        mailbox_receive(devices[deviceIdx].deviceMbox, status, sizeof(int), TRUE);
        disableInterrupts();
        waitingOnDevice--;
    }
    else {
        console_output(FALSE, "Unknown device type.");
        stop(-1);
    }

    if (signaled()) result = -5;
    return result;
}

/* ------------------------------------------------------------------------
   check_io_messaging
----------------------------------------------------------------------- */
int check_io_messaging(void)
{
    return waitingOnDevice;
}

/* ------------------------------------------------------------------------
   Clock Handler
----------------------------------------------------------------------- */
static void clock_handler(char deviceId[32], uint8_t command, uint32_t status, void* pArgs)
{
    tickCount++;
    if (tickCount >= 5) {
        tickCount = 0;
        int zero = 0;
        int result = mailbox_send(devices[THREADS_CLOCK_DEVICE_ID].deviceMbox, &zero, sizeof(int), FALSE);
        if (result == -1 || result == -5) {
            console_output(FALSE, "Clock handler: mailbox_send failed with error %d\n", result);
            stop(1);
        }
    }
    time_slice();
}

/* ------------------------------------------------------------------------
   I/O Handler
----------------------------------------------------------------------- */
static void io_handler(char deviceId[32], uint8_t command, uint32_t status, void* pArgs)
{
    int deviceIdx = -1;
    /* Find our device by name */
    for (int i = 0; i < THREADS_MAX_DEVICES; i++) {
        if (strlen(devices[i].deviceName) > 0 && strcmp(devices[i].deviceName, deviceId) == 0) {
            deviceIdx = i;
            break;
        }
    }
    if (deviceIdx >= 0 && devices[deviceIdx].deviceMbox >= 0) {
        int result = mailbox_send(devices[deviceIdx].deviceMbox, &status, sizeof(status), FALSE);
        if (result == -1 || result == -5) {
            /* Ignore send failures in interrupt context */
        }
    }
}

/* ------------------------------------------------------------------------
   System Call Handler
----------------------------------------------------------------------- */
static void syscall_handler(char deviceId[32], uint8_t command, uint32_t status, void* pArgs)
{
    system_call_arguments_t* sysArgs = (system_call_arguments_t*)pArgs;

    if (sysArgs == NULL) {
        console_output(FALSE, "syscall_handler: NULL arguments\n");
        stop(1);
    }
    if (sysArgs->call_id >= THREADS_MAX_SYSCALLS) {
        console_output(FALSE, "syscall_handler: Invalid call_id %d\n", sysArgs->call_id);
        stop(1);
    }

    if (systemCallVector[sysArgs->call_id] != NULL) {
        systemCallVector[sysArgs->call_id](sysArgs);
    }
    else {
        nullsys(sysArgs);
    }
}

/* ------------------------------------------------------------------------
   InitializeHandlers
----------------------------------------------------------------------- */
static void InitializeHandlers()
{
    handlers = get_interrupt_handlers();
    checkKernelMode("InitializeHandlers");

    handlers[THREADS_TIMER_INTERRUPT] = clock_handler;
    handlers[THREADS_IO_INTERRUPT] = io_handler;
    handlers[THREADS_SYS_CALL_INTERRUPT] = syscall_handler;

    for (int i = 0; i < THREADS_MAX_SYSCALLS; i++) {
        systemCallVector[i] = nullsys;
    }
}

/* ------------------------------------------------------------------------
   nullsys - Error handler for invalid system calls
----------------------------------------------------------------------- */
static void nullsys(system_call_arguments_t* args)
{
    checkKernelMode("nullsys");
    console_output(FALSE, "nullsys(): Invalid syscall %d. Halting...\n", args->call_id);
    stop(1);
}

/* ------------------------------------------------------------------------
   checkKernelMode
----------------------------------------------------------------------- */
static inline void checkKernelMode(const char* functionName)
{
    union psr_values psrValue;
    psrValue.integer_part = get_psr();
    if (psrValue.bits.cur_mode == 0) {
        console_output(FALSE, "Kernel mode expected, but function %s called in user mode.\n", functionName);
        stop(1);
    }
}