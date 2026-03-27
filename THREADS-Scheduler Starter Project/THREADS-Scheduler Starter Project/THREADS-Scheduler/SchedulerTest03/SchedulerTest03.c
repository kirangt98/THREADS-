
#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest03
*
* Tests: Signal (k_kill) and wait interaction.
*
* Description:
*   Spawns a single child at priority 1 (SimpleDelayExit), then immediately
*   sends it a SIG_TERM signal via k_kill() before calling k_wait(). Tests
*   that a signaled process is correctly terminated and that the parent 
*   can reap it with k_wait().
*
* Functions exercised:
*   k_spawn, k_kill (SIG_TERM), k_wait, k_exit
*
* Process tree:
*   SchedulerTest03 (priority 5)
*     └── SchedulerTest03-Child1 (priority 1, SimpleDelayExit)
*
* Expected behavior:
*   - Child is spawned at lower priority, does not run immediately
*   - Parent signals child with SIG_TERM
*   - Parent waits; child detects signal and exits with -5
*   - Parent receives the signaled exit status
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid;
    char* testName = "SchedulerTest03";
    char nameBuffer[512];

    /* spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);
    
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    kidpid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 1);

    console_output(FALSE, "%s: signaling first child\n", testName);
    k_kill(kidpid, SIG_TERM);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;

}
