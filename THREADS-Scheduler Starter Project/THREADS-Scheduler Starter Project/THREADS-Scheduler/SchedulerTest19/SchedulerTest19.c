#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest19
*
* Tests: Process table overflow returns error code.
*
* Description:
*   Attempts to spawn (MAXPROC + 2) children in a loop. Since the process 
*   table has MAXPROC slots and two are already occupied (watchdog and test 
*   process), spawns should succeed for (MAXPROC - 2) children and then fail
*   with a negative return code for the remaining attempts. After verifying
*   the overflow behavior, waits for all successfully spawned children.
*
* Functions exercised:
*   k_spawn (table full detection), k_wait, k_exit
*
* Process tree:
*   SchedulerTest19 (priority 5)
*     ├── SchedulerTest19-Child1 through ChildN (priority 3, SimpleDelayExit)
*     └── (additional spawn attempts fail with negative return)
*
* Expected behavior:
*   - First (MAXPROC - 2) spawns succeed
*   - Subsequent spawns return -4 (table full)
*   - Test logs each failure
*   - All successful children are reaped via k_wait
*
*********************************************************************************/
int SchedulerEntryPoint(void* args)
{
    int status = -1, kidpid = -1;
    char* testName = "SchedulerTest19";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    for (int i = 0; i < MAXPROC+2; ++i)
    {
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, i + 1);
        kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        if (kidpid < 0)
        {
            console_output(FALSE, "%s: k_spawn returned %d at attempt %d\n", testName, kidpid, i + 1);
        }
    }
    for (int i = 0; i < MAXPROC; ++i)
    {
        console_output(FALSE, "%s: waiting for child process\n", testName);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    }

    k_exit(0);

    return 0;
}