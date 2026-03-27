#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int parentPid = -1;

int JoinParent(char* args);

/*********************************************************************************
*
* SchedulerTest10
*
* Tests: Child attempts to join its parent (should halt).
*
* Description:
*   The test process records its own PID, then spawns a child (JoinParent)
*   at priority 3. The child attempts to call k_join() on its parent's PID.
*   This is an illegal operation and should cause the system to halt, since
*   k_join validates that a process is not joining its parent.
*
* Functions exercised:
*   k_spawn, k_getpid, k_join (parent-join guard), k_wait, k_exit
*
* Process tree:
*   SchedulerTest10 (priority 5)
*     └── SchedulerTest10-Child1 (priority 3, JoinParent)
*
* Expected behavior:
*   - Child calls k_join with parent's PID
*   - k_join detects the illegal join-parent operation
*   - System halts with error message "join: process attempted to join parent"
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status=-1, pid1, kidpid=-1;
    char nameBuffer[512];
    char* testName = "SchedulerTest10";

    /* spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);

    parentPid = k_getpid();

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    pid1 = k_spawn(nameBuffer, JoinParent, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}

int JoinParent(char* strArgs)
{
    int exitCode;

    console_output(FALSE, "%s: started\n", strArgs);
    console_output(FALSE, "%s: joining parent process\n", strArgs);
    k_join(parentPid, &exitCode);
    console_output(FALSE, "%s: finished joining parent process\n", strArgs);

    k_exit(-3);

    return 0;

}