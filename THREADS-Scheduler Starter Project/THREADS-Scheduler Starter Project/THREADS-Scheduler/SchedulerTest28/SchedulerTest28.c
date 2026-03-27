#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SpawnTwoDifferentPriorities(char* strArgs);
extern int SpawnJoiner(char* strArgs);  // from test27
extern int pidToJoin;

/*********************************************************************************
*
* SchedulerTest28
*
* Tests: Join with mixed priorities and preemption.
*
* Description:
*   Spawns three children at different priorities:
*     - Child1 (SimpleDelayExit, priority 4): high priority, runs first
*     - Child2 (SpawnJoiner, priority 3): joins pidToJoin (Child3)
*     - Child3 (SpawnOnePriorityOne, priority 2): spawns a priority 1 
*       grandchild, set as the join target via pidToJoin
*   Tests that join works correctly when the joiner is higher priority 
*   than the join target, and that preemption interacts correctly with 
*   the join mechanism.
*
* Functions exercised:
*   k_spawn, k_join, k_wait, k_exit
*
* Process tree:
*   SchedulerTest28 (priority 5)
*     ├── SchedulerTest28-Child1 (priority 4, SimpleDelayExit)
*     ├── SchedulerTest28-Child2 (priority 3, SpawnJoiner → joins Child3)
*     └── SchedulerTest28-Child3 (priority 2, SpawnOnePriorityOne)
*           └── ...-Child1 (priority 1, SimpleDelayExit)
*
* Expected behavior:
*   - Child1 (priority 4) preempts and runs first
*   - Child2 blocks in k_join on Child3
*   - Child3 runs, spawns grandchild, waits for it
*   - After Child3 exits, Child2 unblocks
*   - Parent waits for all three
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid;
    char nameBuffer[512];
    char* testName = "SchedulerTest28";

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child2", testName);
    kidpid = k_spawn(nameBuffer, SpawnJoiner, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child3", testName);
    pidToJoin = k_spawn(nameBuffer, SpawnOnePriorityOne, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pidToJoin);


    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}
