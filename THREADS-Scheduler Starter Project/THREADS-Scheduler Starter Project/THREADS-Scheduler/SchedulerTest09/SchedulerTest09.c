#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest09
*
* Tests: Parent exits without waiting for child (orphan process handling).
*
* Description:
*   Spawns a single child (SimpleDelayExit) at priority 3 and then 
*   immediately calls k_exit() without calling k_wait(). The child 
*   becomes an orphan. Tests that the scheduler correctly handles 
*   a parent exiting while an active child still exists.
*
* Functions exercised:
*   k_spawn, k_exit (without k_wait)
*
* Process tree:
*   SchedulerTest09 (priority 5)
*     └── SchedulerTest09-Child1 (priority 3, SimpleDelayExit) [becomes orphan]
*
* Expected behavior:
*   - k_exit should halt with an error because the process has an active 
*     child (STATUS_READY) and is attempting to quit
*   - This tests the guard in k_exit that prevents quitting with active children
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char nameBuffer[512];
    char* testName = "SchedulerTest09";

    /* spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    pid1 = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    k_exit(0);

    return 0;
}
