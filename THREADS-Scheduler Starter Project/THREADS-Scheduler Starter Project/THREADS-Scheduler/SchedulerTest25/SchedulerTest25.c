#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest25
*
* Tests: Basic spawn, wait, and exit (duplicate of Test00 structure).
*
* Description:
*   Spawns a single child (SimpleDelayExit) at priority 3, waits for it,
*   and prints the exit status. Functionally identical to SchedulerTest00.
*   May serve as a baseline regression test or placeholder for additional
*   functionality.
*
* Functions exercised:
*   k_spawn, k_wait, k_exit
*
* Process tree:
*   SchedulerTest25 (priority 5)
*     └── P1-Child1 (priority 3, SimpleDelayExit)
*
* Expected behavior:
*   - Child spawns, delays, exits with -3
*   - Parent waits and receives exit status
*   - Parent exits with code 0
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status=-1, pid1, kidpid=-1;
    char* testName = "SchedulerTest25";

    /* spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);
    pid1 = k_spawn("P1-Child1", SimpleDelayExit, "P1-Child1", THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: joining child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}
