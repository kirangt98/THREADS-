#define _CRT_SECURE_NO_WARNINGS
#define SYSTEM_CALLS_PROJECT

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <THREADSLib.h>
#include <Messaging.h>
#include <Scheduler.h>
#include <TList.h>
#include <SystemCalls.h>
#include "libuser.h"

/*********************************************************************************
*
* SystemCalls.c
*
*********************************************************************************/

/* -------------------- Typedefs and Structs -------------------------------- */

struct psr_bits {
    unsigned int cur_int_enable : 1;
    unsigned int cur_mode : 1;
    unsigned int prev_int_enable : 1;
    unsigned int prev_mode : 1;
    unsigned int unused : 28;
};

union psr_values {
    struct psr_bits  bits;
    unsigned int     integer_part;
};

static inline void checkKernelMode(const char* functionName)
{
    union psr_values psrValue;
    psrValue.integer_part = get_psr();
    if (psrValue.bits.cur_mode == 0)
    {
        console_output(FALSE, "Kernel mode expected, but function called in user mode.\n");
        stop(1);
    }
}

/*
 * SemData - Kernel-side state for a single semaphore.
 */
typedef struct sem_data
{
    int status;     /* 0 = free, 1 = in use */
    int sem_id;     /* unique identifier (index) */
    int count;      /* current semaphore value */
    int waitCount;  /* number of processes blocked on this sem */
    int waitPids[MAXPROC]; /* PIDs of blocked processes (FIFO queue) */
    int waitPriorities[MAXPROC]; /* priorities of blocked processes */
} SemData;


/*
 * UserProcess - Kernel-side state for a single user-level process.
 */
typedef struct user_proc
{
    struct user_proc* pNextSibling;
    struct user_proc* pPrevSibling;
    struct user_proc* pNextSem;
    struct user_proc* pPrevSem;
    TList             children;
    int               mboxStartup;
    int              (*startFunc)(char*);

    int               pid;
    int               inUse;
    int               parentSlot;
} UserProcess;


/* -------------------- Constants ------------------------------------------ */

#define MAXSEMS     200
#define USERMODE    set_psr(get_psr() & ~PSR_KERNEL_MODE)
#define SEM_BLOCKED 11


/* -------------------- Globals -------------------------------------------- */

static UserProcess  userProcTable[MAXPROC];
static SemData      semTable[MAXSEMS];


/* -------------------- Prototypes ----------------------------------------- */

void    sys_exit(int resultCode);
int     sys_wait(int* pStatus);
int     sys_spawn(char* name, int (*startFunc)(char*), char* arg,
    int stackSize, int priority);

int     k_semcreate(int initial_value);
int     k_semp(int sem_id);
int     k_semv(int sem_id);
int     k_semfree(int sem_id);

static int  launchUserProcess(char* pArg);
static void system_call_handler(system_call_arguments_t* args);
static void sysNull(system_call_arguments_t* args);

int MessagingEntryPoint(char*);
extern int SystemCallsEntryPoint(char*);

static int childrenOrder(void* pNode1, void* pNode2);


/*********************************************************************************
* childrenOrder - order function for TList (FIFO, no reordering)
*********************************************************************************/
static int childrenOrder(void* pNode1, void* pNode2)
{
    return 1; /* always add at end */
}


/*********************************************************************************
* MessagingEntryPoint
*********************************************************************************/
int MessagingEntryPoint(char* arg)
{
    int status = -1;
    int pid;

    checkKernelMode(__func__);

    /* Initialize the semaphore table */
    memset(semTable, 0, sizeof(semTable));
    for (int i = 0; i < MAXSEMS; i++)
    {
        semTable[i].status = 0;
        semTable[i].sem_id = i;
        semTable[i].count = 0;
        semTable[i].waitCount = 0;
    }

    /* Initialize the system call vector */
    for (int i = 0; i < THREADS_MAX_SYSCALLS; i++)
    {
        systemCallVector[i] = system_call_handler;
    }

    /* Initialize the user process table */
    for (int i = 0; i < MAXPROC; ++i)
    {
        userProcTable[i].mboxStartup = mailbox_create(1, sizeof(int));
        userProcTable[i].pid = -1;
        userProcTable[i].inUse = 0;
        userProcTable[i].parentSlot = -1;
        userProcTable[i].startFunc = NULL;
        TListInitialize(&userProcTable[i].children,
            (int)((size_t)&((UserProcess*)0)->pNextSibling),
            NULL);
    }

    pid = sys_spawn("SystemCalls", SystemCallsEntryPoint, NULL,
        THREADS_MIN_STACK_SIZE * 4, 3);

    sys_wait(&status);

    return 0;

} /* MessagingEntryPoint */


/*********************************************************************************
* launchUserProcess
*********************************************************************************/
static int launchUserProcess(char* pArg)
{
    UserProcess* pProc;
    int resultCode;

    /* Wait for sys_spawn to finish initializing our process table entry */
    mailbox_receive(userProcTable[k_getpid() % MAXPROC].mboxStartup, NULL, 0, TRUE);

    if (signaled())
    {
        console_output(FALSE, "%s - Process signaled in launch.\n", "launchUserProcess");
        sys_exit(0);
    }

    USERMODE;

    pProc = &userProcTable[k_getpid() % MAXPROC];
    resultCode = pProc->startFunc(pArg);

    /* If the entry point returns rather than calling Exit, switch back to kernel mode */
    set_psr(get_psr() | PSR_KERNEL_MODE);
    sys_exit(resultCode);

    return 0;

} /* launchUserProcess */


/*********************************************************************************
* sys_spawn
*********************************************************************************/
int sys_spawn(char* name, int (*startFunc)(char*), char* arg,
    int stackSize, int priority)
{
    int pid = -1;
    UserProcess* pProcess;
    int parentSlot;

    pid = k_spawn(name, launchUserProcess, arg, stackSize, priority);
    if (pid >= 0)
    {
        int slot = pid % MAXPROC;
        pProcess = &userProcTable[slot];
        pProcess->startFunc = startFunc;
        pProcess->pid = pid;
        pProcess->inUse = 1;

        /* Track parent-child relationship */
        parentSlot = k_getpid() % MAXPROC;
        pProcess->parentSlot = parentSlot;
        TListAddNode(&userProcTable[parentSlot].children, pProcess);

        /* Signal the new process to begin */
        mailbox_send(pProcess->mboxStartup, NULL, 0, FALSE);
    }

    return pid;

} /* sys_spawn */


/*********************************************************************************
* sys_wait
*********************************************************************************/
int sys_wait(int* pStatus)
{
    int pid;
    int slot;

    checkKernelMode(__func__);

    pid = k_wait(pStatus);

    if (pid > 0)
    {
        /* Clean up the child's user proc table entry */
        slot = pid % MAXPROC;
        if (userProcTable[slot].inUse && userProcTable[slot].pid == pid)
        {
            /* Remove from parent's children list */
            int parentSlot = userProcTable[slot].parentSlot;
            if (parentSlot >= 0 && parentSlot < MAXPROC)
            {
                TListRemoveNode(&userProcTable[parentSlot].children, &userProcTable[slot]);
            }
            userProcTable[slot].inUse = 0;
            userProcTable[slot].pid = -1;
            userProcTable[slot].parentSlot = -1;
            userProcTable[slot].startFunc = NULL;
        }
    }

    return pid;

} /* sys_wait */


/*********************************************************************************
* sys_exit
*********************************************************************************/
void sys_exit(int resultCode)
{
    int mySlot;
    UserProcess* child;

    checkKernelMode(__func__);

    mySlot = k_getpid() % MAXPROC;

    /* Kill all children */
    child = (UserProcess*)TListGetNextNode(&userProcTable[mySlot].children, NULL);
    while (child != NULL)
    {
        UserProcess* next = (UserProcess*)TListGetNextNode(&userProcTable[mySlot].children, child);
        if (child->pid > 0)
        {
            k_kill(child->pid, SIG_TERM);
        }
        child = next;
    }

    /* Wait for all children to terminate */
    while (userProcTable[mySlot].children.count > 0)
    {
        int status;
        int pid = k_wait(&status);
        if (pid > 0)
        {
            int childSlot = pid % MAXPROC;
            if (userProcTable[childSlot].inUse && userProcTable[childSlot].pid == pid)
            {
                TListRemoveNode(&userProcTable[mySlot].children, &userProcTable[childSlot]);
                userProcTable[childSlot].inUse = 0;
                userProcTable[childSlot].pid = -1;
                userProcTable[childSlot].parentSlot = -1;
                userProcTable[childSlot].startFunc = NULL;
            }
        }
        else
        {
            break;
        }
    }

    /* Clean up own entry */
    userProcTable[mySlot].inUse = 0;
    userProcTable[mySlot].pid = -1;
    userProcTable[mySlot].startFunc = NULL;

    k_exit(resultCode);

} /* sys_exit */


/*********************************************************************************
* k_semcreate
*********************************************************************************/
int k_semcreate(int initial_value)
{
    int sem_id = -1;

    checkKernelMode(__func__);

    if (initial_value < 0)
        return -1;

    /* Find a free slot */
    for (int i = 0; i < MAXSEMS; i++)
    {
        if (semTable[i].status == 0)
        {
            semTable[i].status = 1;
            semTable[i].count = initial_value;
            semTable[i].waitCount = 0;
            sem_id = i;
            break;
        }
    }

    return sem_id;

} /* k_semcreate */


/*********************************************************************************
* k_semp - P operation (wait/decrement)
*********************************************************************************/
int k_semp(int sem_id)
{
    int result = -1;

    checkKernelMode(__func__);

    /* Validate */
    if (sem_id < 0 || sem_id >= MAXSEMS)
        return -1;
    if (semTable[sem_id].status != 1)
        return -1;

    if (semTable[sem_id].count > 0)
    {
        /* Resource available */
        semTable[sem_id].count--;
        result = 0;
    }
    else
    {
        /* Must block - add to wait queue */
        int pid = k_getpid();
        int idx = semTable[sem_id].waitCount;
        semTable[sem_id].waitPids[idx] = pid;
        semTable[sem_id].waitPriorities[idx] = 0; /* will be used for priority ordering */
        semTable[sem_id].waitCount++;

        /* Block this process */
        block(SEM_BLOCKED + sem_id);

        /* When we return from block, check if semaphore was freed */
        if (semTable[sem_id].status != 1)
        {
            /* Semaphore was freed while we were blocked - exit with code 1 */
            sys_exit(1);
            return -1; /* unreachable */
        }

        result = 0;
    }

    return result;

} /* k_semp */


/*********************************************************************************
* k_semv - V operation (signal/increment)
*********************************************************************************/
int k_semv(int sem_id)
{
    int result = -1;

    checkKernelMode(__func__);

    /* Validate */
    if (sem_id < 0 || sem_id >= MAXSEMS)
        return -1;
    if (semTable[sem_id].status != 1)
        return -1;

    if (semTable[sem_id].waitCount > 0)
    {
        /* Unblock the first waiter (FIFO order) */
        int pidToUnblock = semTable[sem_id].waitPids[0];

        /* Shift the queue */
        for (int i = 0; i < semTable[sem_id].waitCount - 1; i++)
        {
            semTable[sem_id].waitPids[i] = semTable[sem_id].waitPids[i + 1];
            semTable[sem_id].waitPriorities[i] = semTable[sem_id].waitPriorities[i + 1];
        }
        semTable[sem_id].waitCount--;

        unblock(pidToUnblock);
        result = 0;
    }
    else
    {
        /* No waiters, just increment */
        semTable[sem_id].count++;
        result = 0;
    }

    return result;

} /* k_semv */


/*********************************************************************************
* k_semfree
*********************************************************************************/
int k_semfree(int sem_id)
{
    int result = -1;

    checkKernelMode(__func__);

    /* Validate */
    if (sem_id < 0 || sem_id >= MAXSEMS)
        return -1;
    if (semTable[sem_id].status != 1)
        return -1;

    if (semTable[sem_id].waitCount > 0)
    {
        /* Unblock all waiters - they will detect freed sem and exit with 1 */
        int count = semTable[sem_id].waitCount;
        semTable[sem_id].status = 0; /* Mark as freed BEFORE unblocking */

        for (int i = 0; i < count; i++)
        {
            unblock(semTable[sem_id].waitPids[i]);
        }
        semTable[sem_id].waitCount = 0;

        result = 1; /* had waiters */
    }
    else
    {
        semTable[sem_id].status = 0;
        semTable[sem_id].count = 0;
        result = 0; /* no waiters */
    }

    return result;

} /* k_semfree */


/*********************************************************************************
* sysNull
*********************************************************************************/
static void sysNull(system_call_arguments_t* args)
{
    console_output(FALSE, "nullsys(): Invalid syscall %d. Halting...\n", args->call_id);
    sys_exit(1);

} /* sysNull */


/*********************************************************************************
* system_call_handler
*********************************************************************************/
static void system_call_handler(system_call_arguments_t* args)
{
    if (args == NULL)
    {
        console_output(FALSE, "system_call_handler(): NULL args, terminating.\n");
        sys_exit(1);
    }

    switch (args->call_id)
    {
    case SYS_SPAWN:
        args->arguments[0] = (intptr_t)sys_spawn(
            (char*)args->arguments[4],
            (int (*)(char*))args->arguments[0],
            (char*)args->arguments[1],
            (int)args->arguments[2],
            (int)args->arguments[3]);
        args->arguments[3] = (intptr_t)((intptr_t)args->arguments[0] >= 0 ? 0 : -1);
        break;

    case SYS_WAIT:
    {
        int status = -1;
        int pid = sys_wait(&status);
        args->arguments[0] = (intptr_t)pid;
        args->arguments[1] = (intptr_t)status;
        args->arguments[3] = (intptr_t)(pid > 0 ? 0 : -1);
        break;
    }

    case SYS_EXIT:
        sys_exit((int)args->arguments[0]);
        /* Does not return */
        break;

    case SYS_SEMCREATE:
    {
        int sem_id = k_semcreate((int)args->arguments[0]);
        args->arguments[0] = (intptr_t)sem_id;
        args->arguments[3] = (intptr_t)(sem_id >= 0 ? 0 : -1);
        break;
    }

    case SYS_SEMP:
    {
        int result = k_semp((int)args->arguments[0]);
        args->arguments[3] = (intptr_t)result;
        break;
    }

    case SYS_SEMV:
    {
        int result = k_semv((int)args->arguments[0]);
        args->arguments[3] = (intptr_t)result;
        break;
    }

    case SYS_SEMFREE:
    {
        int result = k_semfree((int)args->arguments[0]);
        args->arguments[3] = (intptr_t)result;
        break;
    }

    case SYS_GETTIMEOFDAY:
        args->arguments[0] = (intptr_t)system_clock();
        break;

    case SYS_CPUTIME:
        args->arguments[0] = (intptr_t)system_clock();
        break;

    case SYS_GETPID:
        args->arguments[0] = (intptr_t)k_getpid();
        break;

    default:
        sysNull(args);
        break;
    }

    if (signaled())
    {
        console_output(FALSE, "Process signaled while in system call.\n");
        sys_exit(0);
    }

    USERMODE;

} /* system_call_handler */
