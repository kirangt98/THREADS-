#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest05
*
* Tests: Time slicing with multiple same-priority CPU-bound processes.
*
* Description:
*   Spawns four children at priority 3 in a loop repeated three times (12 total
*   batches of children). Each child runs DelayAndDump, which busy-waits for
*   ~10 seconds. Children whose name ends in a number divisible by 4 periodically
*   dump the process table during their execution. Since the busy-wait exceeds
*   the 80ms time slice, the scheduler must time-slice between children.
*
* Functions exercised:
*   k_spawn, k_wait, k_exit, time_slice, dispatcher (round-robin),
*   display_process_table
*
* Process tree (per iteration):
*   SchedulerTest05 (priority 5)
*     ├── SchedulerTest05-ChildN   (priority 3, DelayAndDump)
*     ├── SchedulerTest05-ChildN+1 (priority 3, DelayAndDump)
*     ├── SchedulerTest05-ChildN+2 (priority 3, DelayAndDump)
*     └── SchedulerTest05-ChildN+3 (priority 3, DelayAndDump)
*
* Expected behavior:
*   - Process table dumps show non-zero CPU time for all children,
*     confirming that time slicing is distributing CPU among same-priority
*     processes
*   - Each child exits with its negated PID
*   - Three full iterations complete successfully
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid = -1;
    int i, j;
    char nameBuffer[1028];
    char* testName = "SchedulerTest05";
    int count = 1;

    console_output(FALSE, "\n%s: started\n", testName);
    for (j = 0; j < 3; j++)
    {
        for (i = 2; i < 6; i++)
        {
            snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, count++);
            kidpid = k_spawn(nameBuffer, DelayAndDump, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
            console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);
        }

        for (i = 2; i < 6; i++)
        {
            kidpid = k_wait(&status);
            console_output(FALSE, "%s: exit status for child %d is %d\n",
                testName, kidpid, status);
        }
    }
    k_exit(0);

    return 0;
}
