#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"


int SpawnDumpProcess(char* strArgs);
int DumpProcess(char* strArgs);
int SignalJoinGlobalPid(char* arg);

/*********************************************************************************
*
* SchedulerTest13
*
* Tests: Join and signal interaction with blocked processes.
*
* Description:
*   Spawns two children: Child1 (SpawnDumpProcess, priority 4) spawns a 
*   grandchild (DumpProcess) that dumps the process table and blocks in 
*   k_wait. Child2 (SignalJoinGlobalPid, priority 3) signals the grandchild 
*   via gPid and then joins it. This tests that signaling a process that is 
*   being waited on by its parent correctly handles the signal propagation 
*   and join/wait interaction.
*
* Functions exercised:
*   k_spawn, k_kill, k_join, k_wait, k_exit, display_process_table,
*   signaled()
*
* Process tree:
*   SchedulerTest13 (priority 5)
*     ├── SchedulerTest13-Child1 (priority 4, SpawnDumpProcess)
*     │     └── ...-Child1-Child1 (priority 2, DumpProcess) [gPid target]
*     └── SchedulerTest13-Child2 (priority 3, SignalJoinGlobalPid)
*
* Expected behavior:
*   - Process table dump shows WAIT BLOCK status for Child1
*   - Child2 signals the grandchild, then joins it
*   - Signaled grandchild exits with -5
*   - All processes are reaped correctly
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status = -1, pid1, pid2, kidpid = -1;
    char* testName = "SchedulerTest13";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SpawnDumpProcess, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

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
