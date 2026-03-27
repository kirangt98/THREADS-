#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SpawnTwoDifferentPriorities(char* strArgs);
int SpawnJoiner(char* strArgs);
extern int pidToJoin;

/*********************************************************************************
*
* SchedulerTest26
*
* Tests: Join with a process that has already exited.
*
* Description:
*   Spawns Child1 (SimpleDelayExit, priority 4) whose PID is stored in 
*   pidToJoin. Because Child1 is high priority, it runs and exits before 
*   Child2 is even spawned. Then spawns Child2 (SpawnJoiner, priority 2), 
*   which attempts to k_join on pidToJoin — a process that has already quit.
*   Tests that k_join correctly handles joining a process in STATUS_QUIT.
*
* Functions exercised:
*   k_spawn, k_join (on already-exited process), k_wait, k_exit
*
* Process tree:
*   SchedulerTest26 (priority 5)
*     ├── SchedulerTest26-Child1 (priority 4, SimpleDelayExit) [exits first]
*     └── SchedulerTest26-Child2 (priority 2, SpawnJoiner → joins Child1)
*
* Expected behavior:
*   - Child1 runs and exits at priority 4 before Child2 starts
*   - Child2 calls k_join on the already-quit Child1
*   - k_join should return immediately with the exit code
*   - Parent waits for both children
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid;
    char nameBuffer[512];
    char* testName = "SchedulerTest26";

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pidToJoin = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pidToJoin);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child2", testName);
    kidpid = k_spawn(nameBuffer, SpawnJoiner, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}
