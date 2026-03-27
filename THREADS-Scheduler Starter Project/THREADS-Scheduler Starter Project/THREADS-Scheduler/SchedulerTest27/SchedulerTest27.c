#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SpawnTwoDifferentPriorities(char* strArgs);
int SpawnJoiner(char* strArgs);
int pidToJoin;

/*********************************************************************************
*
* SchedulerTest27
*
* Tests: Join across a complex multi-priority process hierarchy.
*
* Description:
*   Spawns three children at priority 2:
*     - Child1 (SpawnOnePriorityFour): spawns one priority 4 grandchild
*     - Child2 (SpawnJoiner): joins pidToJoin (set to Child3's PID)
*     - Child3 (SpawnTwoDifferentPriorities): spawns two grandchildren at 
*       priorities 3 and 4
*   pidToJoin is set to Child3's PID, so Child2 blocks in k_join waiting 
*   for Child3 to exit. Tests cross-process join with mixed priorities.
*
* Functions exercised:
*   k_spawn, k_join (cross-process), k_wait, k_exit
*
* Process tree:
*   SchedulerTest27 (priority 5)
*     ├── SchedulerTest27-Child1 (priority 2, SpawnOnePriorityFour)
*     │     └── ...-Child1 (priority 4, SimpleDelayExit)
*     ├── SchedulerTest27-Child2 (priority 2, SpawnJoiner → joins Child3)
*     └── SchedulerTest27-Child3 (priority 2, SpawnTwoDifferentPriorities)
*           ├── ...-Child1 (priority 3, SimpleDelayExit)
*           └── ...-Child2 (priority 4, SimpleDelayExit)
*
* Expected behavior:
*   - Child2 blocks in k_join waiting for Child3
*   - Child3 completes after its two grandchildren exit
*   - Child2 unblocks and exits
*   - Parent waits for all three children
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid;
    char nameBuffer[512];
    char* testName = "SchedulerTest27";

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    kidpid = k_spawn(nameBuffer, SpawnOnePriorityFour, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child2", testName);
    kidpid = k_spawn(nameBuffer, SpawnJoiner, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child3", testName);
    pidToJoin = k_spawn(nameBuffer, SpawnTwoDifferentPriorities, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
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

int SpawnTwoDifferentPriorities(char* strArgs)
{
    int kidpid;
    int status = 0xff;
    char nameBuffer[1024];

    if (strArgs != NULL)
    {
        console_output(FALSE, "%s: started\n", strArgs);
        console_output(FALSE, "%s: performing spawn of first child\n", strArgs);

        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", strArgs);
        kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        console_output(FALSE, "%s: spawn of first child returned pid = %d\n", strArgs, kidpid);

        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child2", strArgs);
        console_output(FALSE, "%s: performing spawn of second child\n", strArgs);
        kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
        console_output(FALSE, "%s: spawn of second child returned pid = %d\n", strArgs, kidpid);

        console_output(FALSE, "%s: performing first wait\n", strArgs);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", strArgs,
            kidpid, status);

        console_output(FALSE, "%s: performing second wait\n", strArgs);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", strArgs,
            kidpid, status);
    }
    k_exit(-3);

    return 0;
}
