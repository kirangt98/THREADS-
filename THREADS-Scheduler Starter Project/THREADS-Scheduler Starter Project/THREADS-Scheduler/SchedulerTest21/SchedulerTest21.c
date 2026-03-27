#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest21
*
* Tests: k_spawn rejects out-of-range priority values.
*
* Description:
*   Attempts to spawn two children with invalid priorities: one with 
*   priority -10 (below LOWEST_PRIORITY) and one with priority 7 (above 
*   HIGHEST_PRIORITY). Both should be rejected by k_spawn. Then waits
*   for children (k_wait should return -1 since no valid children exist).
*
* Functions exercised:
*   k_spawn (priority range validation), k_wait, k_exit
*
* Process tree:
*   SchedulerTest21 (priority 5)
*     └── (both spawn attempts fail, no children created)
*
* Expected behavior:
*   - k_spawn with priority -10 returns -3 (priority out of range)
*   - k_spawn with priority 7 returns -3 (priority out of range)
*   - k_wait returns -1 (no children)
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status = -1, kidpid = 0;
    char nameBuffer[512];
    char* testName = "SchedulerTest21";

    console_output(FALSE, "\n%s: started\n", testName);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, -10);
    if (kidpid == -1)
    {
        console_output(FALSE, "%s: Could not spawn process, invalid priority\n", testName);
    }

    kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 7);
    if (kidpid == -1)
    {
        console_output(FALSE, "%s: Could not spawn process, invalid priority\n", testName);
    }

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    return 0;
}
