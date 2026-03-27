#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

extern int SpawnDumpProcess(char* strArgs);
extern int SignalJoinGlobalPid(char* arg);
extern int gPid;


/*********************************************************************************
*
* SchedulerTest14
*
* Tests: Signal and join with global PID targeting (same structure as Test13).
*
* Description:
*   Same structure as SchedulerTest13 but uses the gPid global variable
*   set by SpawnDumpProcess. Child1 (SpawnDumpProcess, priority 4) spawns 
*   a grandchild that dumps the process table. Child2 (SignalJoinGlobalPid, 
*   priority 3) signals and joins the grandchild via gPid. Tests the 
*   interaction between signal, join, and wait when a non-parent process 
*   joins a signaled target.
*
* Functions exercised:
*   k_spawn, k_kill, k_join, k_wait, k_exit, display_process_table
*
* Process tree:
*   SchedulerTest14 (priority 5)
*     ├── SchedulerTest14-Child1 (priority 4, SpawnDumpProcess)
*     │     └── ...-Child1-Child1 (priority 2, DumpProcess) [gPid target]
*     └── SchedulerTest14-Child2 (priority 3, SignalJoinGlobalPid)
*
* Expected behavior:
*   - gPid is set to the grandchild's PID by SpawnDumpProcess
*   - Child2 signals then joins the grandchild
*   - Signaled process exits with -5
*   - Parent waits for both children
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid2, kidpid = -1;
    char* testName = "SchedulerTest14";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    gPid = k_spawn(nameBuffer, SpawnDumpProcess, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, gPid);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child2", testName);
    pid2 = k_spawn(nameBuffer, SignalJoinGlobalPid, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid2);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);

    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    return 0;
}

