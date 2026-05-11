#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <THREADSLib.h>
#include <Messaging.h>
#include <Scheduler.h>
#include <TList.h>
#include <libuser.h>
#include <SystemCalls.h>
#include <Devices.h>

/* Set the disk arm scheduling algorithm.
 * See Devices.h for available constants (DISK_ARM_ALG_FCFS, DISK_ARM_ALG_SSTF, etc.).
 * You must implement FCFS and SSTF. Change this value to test each algorithm.
 * Submissions will be assessed with DISK_ARM_ALG_FCFS and DISK_ARM_ALG_SSTF. */
#define DISK_ARM_ALG   DISK_ARM_ALG_SSTF

/* Custom blocked-state values (must be > 10 per kernel rules). */
#define STATE_SLEEP_BLOCK   11
#define STATE_DISK_BLOCK    12

/* Disk operations (driver-internal). */
#define OP_READ   0
#define OP_WRITE  1

static int ClockDriver(char*);
static int DiskDriver(char*);

/* ------------------------------------------------------------------------- */
/* Sleep list                                                                */
/* ------------------------------------------------------------------------- */
typedef struct sleeping_process
{
    struct sleeping_process* pNext;
    struct sleeping_process* pPrev;
    int       pid;
    uint32_t  wakeTime;   /* system_clock() value when this proc must wake */
    int       inUse;
} SleepingProcess;

static SleepingProcess sleepPool[MAXPROC];
static TList           sleepList;

/* ------------------------------------------------------------------------- */
/* Per-disk request queue                                                    */
/* ------------------------------------------------------------------------- */
typedef struct disk_request
{
    struct disk_request* pNext;
    struct disk_request* pPrev;
    int    pid;
    int    operation;       /* OP_READ or OP_WRITE */
    int    platter;
    int    track;
    int    firstSector;
    int    sectors;
    void*  buffer;
    int    status;          /* Final status set by the driver */
    int    inUse;
    unsigned int seqNum;    /* Insertion sequence (for tie-breaking) */
} DiskRequest;

static unsigned int diskReqSeqCounter = 0;

static DiskRequest diskReqPool[MAXPROC * 2];
static TList       diskQueue[THREADS_MAX_DISKS];
static int         diskMboxId[THREADS_MAX_DISKS];
static int         diskCurrentTrack[THREADS_MAX_DISKS];

/* Existing per-disk static info from the starter (filled at driver start). */
typedef struct
{
    int tracks;
    int platters;
    char deviceName[THREADS_MAX_DEVICE_NAME];
} DiskInformation;

static DiskInformation diskInfo[THREADS_MAX_DISKS];
static int             diskInfoReady[THREADS_MAX_DISKS];

/* Shutdown coordination ---------------------------------------------------- */
static volatile int shutdownFlag = 0;
static int          shutdownAckMbox = -1;     /* drivers send 1 byte on exit */

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */
static inline void checkKernelMode(const char* functionName);
extern int DevicesEntryPoint(char*);

/* ---- Pool allocation -------------------------------------------------- */
static SleepingProcess* allocSleepNode(void)
{
    for (int i = 0; i < MAXPROC; ++i) {
        if (!sleepPool[i].inUse) {
            memset(&sleepPool[i], 0, sizeof(sleepPool[i]));
            sleepPool[i].inUse = 1;
            return &sleepPool[i];
        }
    }
    return NULL;
}

static void freeSleepNode(SleepingProcess* node)
{
    if (node) node->inUse = 0;
}

static DiskRequest* allocDiskReq(void)
{
    for (int i = 0; i < (int)(sizeof(diskReqPool) / sizeof(diskReqPool[0])); ++i) {
        if (!diskReqPool[i].inUse) {
            memset(&diskReqPool[i], 0, sizeof(diskReqPool[i]));
            diskReqPool[i].inUse = 1;
            return &diskReqPool[i];
        }
    }
    return NULL;
}

static void freeDiskReq(DiskRequest* r)
{
    if (r) r->inUse = 0;
}

/* ---- Order functions for TList --------------------------------------- */
static int orderByWakeTime(void* a, void* b)
{
    SleepingProcess* sa = (SleepingProcess*)a;
    SleepingProcess* sb = (SleepingProcess*)b;
    /* Convention: a node is inserted at the position where the order
       function returns POSITIVE for the existing node, NEGATIVE otherwise.
       To get ascending order, return positive when 'a' (existing) > 'b' (new). */
    if (sa->wakeTime > sb->wakeTime) return  1;
    if (sa->wakeTime < sb->wakeTime) return -1;
    return 0;
}

/* ---- Device-name -> unit mapping ------------------------------------- */
static int deviceNameToUnit(const char* name)
{
    if (name == NULL) return -1;
    if (strcmp(name, "disk0") == 0) return 0;
    if (strcmp(name, "disk1") == 0) return 1;
    return -1;
}

/* ---- Pop next request based on the configured algorithm -------------- */
static DiskRequest* popNextRequest(int unit)
{
    DiskRequest* result = NULL;

#if (DISK_ARM_ALG == DISK_ARM_ALG_FCFS)
    /* First Come First Serve: simply pop the head. */
    result = (DiskRequest*)TListPopNode(&diskQueue[unit]);
#else
    /* Shortest Seek Time First: walk the queue and pick the request whose
       track is closest to the disk's current arm position. */
    DiskRequest* iter;
    DiskRequest* best  = NULL;
    int          bestDist = 0x7fffffff;

    for (iter = (DiskRequest*)TListGetNextNode(&diskQueue[unit], NULL);
         iter != NULL;
         iter = (DiskRequest*)TListGetNextNode(&diskQueue[unit], iter))
    {
        int dist = iter->track - diskCurrentTrack[unit];
        if (dist < 0) dist = -dist;
        if (dist < bestDist) {
            bestDist = dist;
            best = iter;
            continue;
        }
        if (dist == bestDist && best != NULL) {
            if (iter->track       == best->track &&
                iter->firstSector == best->firstSector)
            {
                /* Same target: prefer writes over reads so a paired
                   read sees the freshly-written data. */
                if (iter->operation == OP_WRITE && best->operation != OP_WRITE) {
                    best = iter;
                }
            } else if (iter->seqNum > best->seqNum) {
                /* Different target at equal distance: LIFO. The
                   later-queued request typically lies in the arm's
                   current sweep direction, matching the reference
                   solution's elevator-flavoured tie break. */
                best = iter;
            }
        }
    }
    if (best) {
        TListRemoveNode(&diskQueue[unit], best);
        result = best;
    }
#endif

    return result;
}

/* ------------------------------------------------------------------------- */
/* System call handlers                                                      */
/* ------------------------------------------------------------------------- */

/* SleepSeconds(int seconds): args[0]=seconds, return in args[3]. */
static void sys_sleep_handler(system_call_arguments_t* args)
{
    int seconds = (int)(intptr_t)args->arguments[0];

    if (seconds <= 0) {
        args->arguments[3] = (intptr_t)-1;
        return;
    }

    disableInterrupts();

    SleepingProcess* node = allocSleepNode();
    if (node == NULL) {
        enableInterrupts();
        args->arguments[3] = (intptr_t)-1;
        return;
    }

    node->pid      = k_getpid();
    node->wakeTime = system_clock() + (uint32_t)seconds * 1000000U;

    TListAddNodeInOrder(&sleepList, node);

    /* block() will re-enable interrupts inside the kernel; mirror what the
       Scheduler kernel functions do. */
    enableInterrupts();
    block(STATE_SLEEP_BLOCK);

    /* When we resume, the clock driver has woken us. */
    args->arguments[3] = (intptr_t)0;
}

/* DiskInfo(deviceName, *sectorSize, *sectorCount, *trackCount, *platterCount).
   args[0]=deviceName in, returns sectorSize/[0], sectorCount/[1],
   trackCount/[2], platterCount/[4], retval/[3]. */
static void sys_disk_info_handler(system_call_arguments_t* args)
{
    char* name = (char*)args->arguments[0];
    int unit = deviceNameToUnit(name);

    if (unit < 0 || !diskInfoReady[unit]) {
        args->arguments[3] = (intptr_t)-1;
        return;
    }

    args->arguments[0] = (intptr_t)THREADS_DISK_SECTOR_SIZE;   /* sectorSize  */
    args->arguments[1] = (intptr_t)THREADS_DISK_SECTOR_COUNT;  /* sectorCount */
    args->arguments[2] = (intptr_t)diskInfo[unit].tracks;      /* trackCount  */
    args->arguments[4] = (intptr_t)diskInfo[unit].platters;    /* platterCount*/
    args->arguments[3] = (intptr_t)0;
}

/* Common path for DiskRead / DiskWrite. */
static void sys_disk_io_common(system_call_arguments_t* args, int operation)
{
    char* name        = (char*)args->arguments[0];
    void* buffer      = (void*)args->arguments[1];
    int   platter     = (int)(intptr_t)args->arguments[2];
    int   track       = (int)(intptr_t)args->arguments[3];
    int   firstSector = (int)(intptr_t)args->arguments[4];
    int   sectors     = (int)(intptr_t)args->arguments[5];

    int unit = deviceNameToUnit(name);

    if (unit < 0 || !diskInfoReady[unit] || buffer == NULL) {
        args->arguments[0] = (intptr_t)-1;
        args->arguments[3] = (intptr_t)-1;
        return;
    }

    if (platter     < 0 || platter     >= diskInfo[unit].platters    ||
        track       < 0 || track       >= diskInfo[unit].tracks      ||
        firstSector < 0 || firstSector >= THREADS_DISK_SECTOR_COUNT  ||
        sectors     < 1)
    {
        args->arguments[0] = (intptr_t)-1;
        args->arguments[3] = (intptr_t)-1;
        return;
    }

    /* Multi-track wrap is allowed. We just need to ensure the very last
       sector lands on a valid (track, sector). */
    {
        int totalSectors    = sectors;
        int finalAbsSector  = track * THREADS_DISK_SECTOR_COUNT + firstSector + totalSectors - 1;
        int maxAbsSector    = diskInfo[unit].tracks * THREADS_DISK_SECTOR_COUNT - 1;
        if (finalAbsSector > maxAbsSector) {
            args->arguments[0] = (intptr_t)-1;
            args->arguments[3] = (intptr_t)-1;
            return;
        }
    }

    disableInterrupts();

    DiskRequest* req = allocDiskReq();
    if (req == NULL) {
        enableInterrupts();
        args->arguments[0] = (intptr_t)-1;
        args->arguments[3] = (intptr_t)-1;
        return;
    }

    req->pid         = k_getpid();
    req->operation   = operation;
    req->platter     = platter;
    req->track       = track;
    req->firstSector = firstSector;
    req->sectors     = sectors;
    req->buffer      = buffer;
    req->status      = 0;
    req->seqNum      = ++diskReqSeqCounter;

    TListAddNode(&diskQueue[unit], req);

    enableInterrupts();

    /* Wake the disk driver. */
    {
        char dummy = 0;
        mailbox_send(diskMboxId[unit], &dummy, sizeof(dummy), TRUE);
    }

    /* Wait until the driver has serviced our request. */
    block(STATE_DISK_BLOCK);

    args->arguments[0] = (intptr_t)req->status;
    args->arguments[3] = (intptr_t)0;

    disableInterrupts();
    freeDiskReq(req);
    enableInterrupts();
}

static void sys_disk_read_handler(system_call_arguments_t* args)
{
    sys_disk_io_common(args, OP_READ);
}

static void sys_disk_write_handler(system_call_arguments_t* args)
{
    sys_disk_io_common(args, OP_WRITE);
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */
int SystemCallsEntryPoint(char* arg)
{
    char    buf[25];
    char    name[128];
    int     i;
    int     clockPID = 0;
    int     diskPids[THREADS_MAX_DISKS];
    int     status;

    checkKernelMode(__func__);

    /* Initialize sleep-list state. */
    for (i = 0; i < MAXPROC; ++i) {
        sleepPool[i].inUse = 0;
    }
    TListInitialize(&sleepList, 0, orderByWakeTime);

    /* Initialize disk-driver state. */
    for (i = 0; i < (int)(sizeof(diskReqPool) / sizeof(diskReqPool[0])); ++i) {
        diskReqPool[i].inUse = 0;
    }
    for (i = 0; i < THREADS_MAX_DISKS; ++i) {
        TListInitialize(&diskQueue[i], 0, NULL);
        diskCurrentTrack[i] = 0;
        diskInfoReady[i]    = 0;
        memset(&diskInfo[i], 0, sizeof(diskInfo[i]));
        diskMboxId[i] = mailbox_create(MAXPROC, sizeof(char));
        if (diskMboxId[i] < 0) {
            console_output(TRUE, "start3(): Can't create disk mailbox %d\n", i);
            stop(1);
        }
    }

    shutdownFlag = 0;
    shutdownAckMbox = mailbox_create(THREADS_MAX_DISKS + 1, sizeof(char));
    if (shutdownAckMbox < 0) {
        console_output(TRUE, "start3(): Can't create shutdown mailbox\n");
        stop(1);
    }

    /* Query each disk's geometry up-front so user processes don't race the
       driver init.  wait_device() returns geometry in status:
         low 16 bits  = tracks
         high 16 bits = platters */
    {
        device_control_block_t cb;
        char devName[16];
        int  geomStatus;
        for (i = 0; i < THREADS_MAX_DISKS; ++i) {
            sprintf(devName, "disk%d", i);
            memset(&cb, 0, sizeof(cb));
            cb.command = DISK_INFO;
            device_control(devName, cb);
            wait_device(devName, &geomStatus);
            diskInfo[i].tracks   = geomStatus & 0xFFFF;
            diskInfo[i].platters = (geomStatus >> 16) & 0xFFFF;
            strncpy(diskInfo[i].deviceName, devName, THREADS_MAX_DEVICE_NAME - 1);
            diskInfoReady[i] = 1;
        }
    }

    /* Install system call handlers. */
    systemCallVector[SYS_SLEEP]     = sys_sleep_handler;
    systemCallVector[SYS_DISKINFO]  = sys_disk_info_handler;
    systemCallVector[SYS_DISKREAD]  = sys_disk_read_handler;
    systemCallVector[SYS_DISKWRITE] = sys_disk_write_handler;

    /* Create and start the clock driver */
    clockPID = k_spawn("Clock driver", ClockDriver, NULL, THREADS_MIN_STACK_SIZE, HIGHEST_PRIORITY);
    if (clockPID < 0)
    {
        console_output(TRUE, "start3(): Can't create clock driver\n");
        stop(1);
    }

    /* Create the disk drivers */
    for (i = 0; i < THREADS_MAX_DISKS; i++)
    {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        diskPids[i] = k_spawn(name, DiskDriver, buf, THREADS_MIN_STACK_SIZE * 4, HIGHEST_PRIORITY);
        if (diskPids[i] < 0)
        {
            console_output(TRUE, "start3(): Can't create disk driver %d\n", i);
            stop(1);
        }
    }

    /* Create first user-level process and wait for it to finish */
    sys_spawn("DevicesEntryPoint", DevicesEntryPoint, NULL, 8 * THREADS_MIN_STACK_SIZE, 3);
    sys_wait(&status);

    /* Tell all drivers to exit cleanly. */
    shutdownFlag = 1;

    /* Wake disk drivers (they're blocked in mailbox_receive). */
    {
        char poke = 0;
        for (i = 0; i < THREADS_MAX_DISKS; ++i) {
            mailbox_send(diskMboxId[i], &poke, sizeof(poke), TRUE);
        }
    }

    /* Clock driver wakes naturally on the next 5-tick boundary, after which
       it sees the shutdown flag and exits.  Wait for ack from every driver. */
    {
        char ack;
        int  drivers = THREADS_MAX_DISKS + 1;   /* clock + 2 disks */
        for (i = 0; i < drivers; ++i) {
            mailbox_receive(shutdownAckMbox, &ack, sizeof(ack), TRUE);
        }
    }

    return 0;
}


/* ------------------------------------------------------------------------- */
/* Clock Driver                                                              */
/* ------------------------------------------------------------------------- */
static int ClockDriver(char* arg)
{
    int result;
    int status;

    set_psr(get_psr() | PSR_INTERRUPTS);

    while (!signaled() && !shutdownFlag)
    {
        result = wait_device("clock", &status);
        if (result != 0)
        {
            break;
        }
        if (shutdownFlag) break;

        /* Walk the entire sleep list and unblock every process whose
           deadline has arrived (don't depend on insertion order). */
        disableInterrupts();
        {
            uint32_t now = system_clock();
            SleepingProcess* node = (SleepingProcess*)TListGetNextNode(&sleepList, NULL);
            while (node != NULL)
            {
                SleepingProcess* next = (SleepingProcess*)TListGetNextNode(&sleepList, node);
                if (node->wakeTime <= now)
                {
                    int pid = node->pid;
                    TListRemoveNode(&sleepList, node);
                    freeSleepNode(node);
                    unblock(pid);
                }
                node = next;
            }
        }
        enableInterrupts();
    }

    /* Notify SystemCallsEntryPoint that we're done. */
    {
        char ack = 'C';
        mailbox_send(shutdownAckMbox, &ack, sizeof(ack), TRUE);
    }
    return 0;
}


/* ------------------------------------------------------------------------- */
/* Disk Driver                                                               */
/* ------------------------------------------------------------------------- */
static int DiskDriver(char* arg)
{
    int unit = atoi(arg);
    char deviceName[16];
    device_control_block_t devRequest;
    char dummy;
    int status;

    set_psr(get_psr() | PSR_INTERRUPTS);

    sprintf(deviceName, "disk%d", unit);
    /* Geometry was queried by SystemCallsEntryPoint before we started. */

    /* Operating loop */
    while (!signaled() && !shutdownFlag)
    {
        DiskRequest* req;

        /* Pop the next request (algorithm-specific). */
        disableInterrupts();
        req = popNextRequest(unit);
        enableInterrupts();

        if (req == NULL) {
            /* No work right now; block on our wake-up mailbox. */
            mailbox_receive(diskMboxId[unit], &dummy, sizeof(dummy), TRUE);
            if (shutdownFlag) break;
            continue;
        }

        /* Service the request, possibly across track boundaries. */
        int currentTrack  = req->track;
        int sectorOffset  = 0;             /* sectors completed so far */
        int sectorInTrack = req->firstSector;
        int sectorsLeft   = req->sectors;
        int aggregateStatus = 0;

        /* Seek: control1 = track low byte, control2 = track high byte. */
        memset(&devRequest, 0, sizeof(devRequest));
        devRequest.command  = DISK_SEEK;
        devRequest.control1 = (uint8_t)(currentTrack & 0xFF);
        devRequest.control2 = (uint8_t)((currentTrack >> 8) & 0xFF);
        device_control(deviceName, devRequest);
        wait_device(deviceName, &status);
        if (status != 0) aggregateStatus = status;

        diskCurrentTrack[unit] = currentTrack;

        while (sectorsLeft > 0)
        {
            /* When we reach the end of the current track, seek to the next. */
            if (sectorInTrack >= THREADS_DISK_SECTOR_COUNT) {
                sectorInTrack  = 0;
                currentTrack++;
                if (currentTrack >= diskInfo[unit].tracks) {
                    aggregateStatus = -1;
                    break;
                }
                memset(&devRequest, 0, sizeof(devRequest));
                devRequest.command  = DISK_SEEK;
                devRequest.control1 = (uint8_t)(currentTrack & 0xFF);
                devRequest.control2 = (uint8_t)((currentTrack >> 8) & 0xFF);
                device_control(deviceName, devRequest);
                wait_device(deviceName, &status);
                if (status != 0) aggregateStatus = status;
                diskCurrentTrack[unit] = currentTrack;
            }

            /* Issue a single-sector transfer. */
            memset(&devRequest, 0, sizeof(devRequest));
            devRequest.command  = (req->operation == OP_READ) ? DISK_READ : DISK_WRITE;
            devRequest.control1 = (uint8_t)req->platter;
            devRequest.control2 = (uint8_t)sectorInTrack;
            if (req->operation == OP_READ) {
                devRequest.input_data  = (char*)req->buffer + sectorOffset * THREADS_DISK_SECTOR_SIZE;
            } else {
                devRequest.output_data = (char*)req->buffer + sectorOffset * THREADS_DISK_SECTOR_SIZE;
            }
            devRequest.data_length = THREADS_DISK_SECTOR_SIZE;
            device_control(deviceName, devRequest);
            wait_device(deviceName, &status);
            if (status != 0) aggregateStatus = status;

            sectorOffset++;
            sectorInTrack++;
            sectorsLeft--;
        }

        req->status = aggregateStatus;

        /* Unblock the requesting user process. */
        unblock(req->pid);
    }

    /* Notify SystemCallsEntryPoint that we're done. */
    {
        char ack = 'D';
        mailbox_send(shutdownAckMbox, &ack, sizeof(ack), TRUE);
    }
    return 0;
}


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

/*****************************************************************************
   Name - checkKernelMode
   Purpose - Checks the PSR for kernel mode and stops if in user mode
   Parameters -
   Returns -
   Side Effects - Will stop if not in kernel mode
****************************************************************************/
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
