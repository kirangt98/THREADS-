
#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest00
*
* Tests: Basic spawn, wait, and exit.
*
* Description:
*   The simplest scheduler test. Spawns a single child process 
*   (SimpleDelayExit) at priority 3, waits for it to complete using k_wait(),
*   verifies the exit status, and then calls k_exit().
*
* Functions exercised:
*   k_spawn, k_wait, k_exit, console_output
*
* Process tree:
*   SchedulerTest00 (priority 5)
*     └── SchedulerTest00-Child1 (priority 3, SimpleDelayExit)
*
* Expected behavior:
*   - Child spawns and runs after parent blocks in k_wait()
*   - Child delays briefly, then exits with code -3
*   - Parent receives child's pid and exit status from k_wait()
*   - Parent exits with code 0
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    char* testName = "SchedulerTest00";
    int status=-1, pid1, kidpid=-1;
    char nameBuffer[512];

    /* Spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);

    /* Use the -Child naming convention for the child process name. */
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}

