#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest08
*
* Tests: Parent spawns child that spawns two lower-priority grandchildren.
*
* Description:
*   Spawns a child (SpawnTwoPriorityTwo) at priority 3, which spawns two 
*   grandchildren at priority 2 (SimpleDelayExit). The grandchildren are 
*   lower priority than their parent, so the parent continues to run after 
*   each spawn. The parent then waits for both grandchildren before exiting.
*   Tests downward priority spawning (no preemption at spawn).
*
* Functions exercised:
*   k_spawn (no preemption), k_wait, k_exit
*
* Process tree:
*   SchedulerTest08 (priority 5)
*     └── SchedulerTest08-Child1 (priority 3, SpawnTwoPriorityTwo)
*           ├── ...-Child1-Child1 (priority 2, SimpleDelayExit)
*           └── ...-Child1-Child2 (priority 2, SimpleDelayExit)
*
* Expected behavior:
*   - Parent runs through both spawns before grandchildren execute
*   - Grandchildren exit with -3
*   - Middle child waits for both, exits with 1
*   - Test process waits for middle child
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char* testName = "SchedulerTest08";
    char nameBuffer[512];

    /* spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SpawnTwoPriorityTwo, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}
