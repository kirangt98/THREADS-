
#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest04
*
* Tests: Process table fill, reap, and reuse across multiple iterations.
*
* Description:
*   Fills the process table to capacity by spawning (MAX_PROCESSES - 2)
*   children (slots 0 and 1 are occupied by watchdog and the test process),
*   dumps the process table, then waits for all children to exit. This 
*   cycle is repeated twice to verify that process table slots are correctly
*   freed and can be reused.
*
* Functions exercised:
*   k_spawn (to table capacity), k_wait, k_exit, display_process_table
*
* Process tree:
*   SchedulerTest04 (priority 5)
*     ├── SchedulerTest04-Child1  (priority 3, SimpleDelayExit)
*     ├── SchedulerTest04-Child2  (priority 3, SimpleDelayExit)
*     ├── ...
*     └── SchedulerTest04-ChildN  (priority 3, SimpleDelayExit)
*   (repeated twice)
*
* Expected behavior:
*   - First iteration: all slots fill, process table dump shows all entries
*   - All children exit with -3, parent reaps each one
*   - Second iteration: same slots are reused successfully
*   - PID values increase across iterations (PIDs are not reused)
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid = -1;
    int i, j;
    char nameBuffer[512];
    char* testName = "SchedulerTest04";
    int count = 1;

    console_output(FALSE, "\n%s: started\n", testName);
    for (j = 0; j < 2; j++)
    {
        for (i = 2; i < MAX_PROCESSES; i++)
        {
            snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, count++);
            kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
            console_output(FALSE, "%s: after spawn of child %d\n", testName, kidpid);
        }

        display_process_table();

        for (i = 2; i < MAX_PROCESSES; i++)
        {
            kidpid = k_wait(&status);
            console_output(FALSE, "%s: exit status for child %d is %d\n",
                testName, kidpid, status);
        }
    }

    fflush(stdout);

    k_exit(0);

    return 0;
}


