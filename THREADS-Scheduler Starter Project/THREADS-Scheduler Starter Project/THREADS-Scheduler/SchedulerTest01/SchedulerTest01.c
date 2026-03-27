
#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest01
*
* Tests: Multi-level process hierarchy with spawn, wait, and exit.
*
* Description:
*   Creates a three-level process tree. The entry point spawns a child at 
*   priority 3 (SpawnTwoPriorityTwo), which in turn spawns two grandchildren
*   at priority 2 (SimpleDelayExit). Each level waits for its children to 
*   complete before exiting. The top-level process does NOT call k_exit(), 
*   relying instead on the return-to-launch path to trigger exit.
*
* Functions exercised:
*   k_spawn, k_wait, k_exit (via launch return path)
*
* Process tree:
*   SchedulerTest01 (priority 5, does not call k_exit)
*     └── SchedulerTest01-Child1 (priority 3, SpawnTwoPriorityTwo)
*           ├── ...-Child1-Child1 (priority 2, SimpleDelayExit)
*           └── ...-Child1-Child2 (priority 2, SimpleDelayExit)
*
* Expected behavior:
*   - Grandchildren run at priority 2, exit with -3
*   - Middle child waits for both grandchildren, exits with 1
*   - Parent waits for middle child, receives exit status
*   - Tests that launch() correctly calls k_exit when process function returns
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char* testName = "SchedulerTest01";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    /* Use the -Child naming convention for the child process name. */
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    pid1 = k_spawn(nameBuffer, SpawnTwoPriorityTwo, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);

    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    return 0;
}
