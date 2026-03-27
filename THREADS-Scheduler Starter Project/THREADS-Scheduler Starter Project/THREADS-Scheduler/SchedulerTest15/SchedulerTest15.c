#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

extern int DumpProcess(char* strArgs);
int SpawnAndJoinProcess(char* arg);
int JoinProcess(char* arg);

extern int gPid;

/*********************************************************************************
*
* SchedulerTest15
*
* Tests: Join blocked and wait blocked statuses; join then wait on same child.
*
* Description:
*   Spawns Child1 (SpawnAndJoinProcess, priority 4) which spawns a grandchild 
*   (DumpProcess, priority 2) and both joins AND waits for it. This tests 
*   using k_join followed by k_wait on the same child process. Child2 
*   (JoinProcess, priority 3) also joins the grandchild via gPid, testing 
*   multiple joiners on a single process. The process table dump from the 
*   grandchild shows JOIN BLOCK and WAIT BLOCK statuses.
*
* Functions exercised:
*   k_spawn, k_join, k_wait, k_exit, display_process_table
*
* Process tree:
*   SchedulerTest15 (priority 5)
*     ├── SchedulerTest15-Child1 (priority 4, SpawnAndJoinProcess)
*     │     └── ...-Child1-Child1 (priority 2, DumpProcess) [gPid target]
*     └── SchedulerTest15-Child2 (priority 3, JoinProcess, joins gPid)
*
* Expected behavior:
*   - Process table shows JOIN BLOCK for Child2 and joiner in Child1
*   - Grandchild dumps process table showing blocked states
*   - k_join and k_wait both succeed for the same child
*   - All processes exit cleanly
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid2, kidpid = -1;
    char* testName = "SchedulerTest15";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    gPid = k_spawn(nameBuffer, SpawnAndJoinProcess, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, gPid);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child2", testName);
    pid2 = k_spawn(nameBuffer, JoinProcess, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid2);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);

    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    return 0;
}

int SpawnAndJoinProcess(char* strArgs)
{
    int kidpid;
    int exitCode = 0xff;
    char nameBuffer[1024];

    if (strArgs != NULL)
    {
        console_output(FALSE, "%s: started\n", strArgs);
        console_output(FALSE, "%s: performing spawn of child\n", strArgs);

        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", strArgs);
        gPid = k_spawn(nameBuffer, DumpProcess, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
        console_output(FALSE, "%s: spawn of child returned pid = %d\n", strArgs, gPid);

        console_output(FALSE, "%s: joining %d\n", strArgs, gPid);
        kidpid = k_join(gPid, &exitCode);
        console_output(FALSE, "%s: after joining child, pid %d, status = %d\n", strArgs, gPid, exitCode);

        console_output(FALSE, "%s: waiting for %d\n", strArgs, gPid);
        kidpid = k_wait(&exitCode);
        console_output(FALSE, "%s: exit status for child %d is %d\n", strArgs, kidpid, exitCode);

    }
    k_exit(-3);

    return 0;
}


