#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest18
*
* Tests: k_spawn rejects stack size below THREADS_MIN_STACK_SIZE.
*
* Description:
*   Attempts to spawn a child with a stack size of (THREADS_MIN_STACK_SIZE - 10),
*   which is below the minimum allowed. k_spawn should reject this and return 
*   -2. The test verifies the return code and prints PASSED or FAILED.
*
* Functions exercised:
*   k_spawn (stack size validation), k_exit
*
* Process tree:
*   SchedulerTest18 (priority 5)
*     └── (spawn attempt fails, no child created)
*
* Expected behavior:
*   - k_spawn returns -2 (stack too small)
*   - Test prints "TEST PASSED"
*   - If spawn unexpectedly succeeds, test prints "TEST FAILED" and waits
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status=-1, pid1, kidpid=-1;
    char nameBuffer[512];
    char* testName = "SchedulerTest18";

    console_output(FALSE, "\n%s: started\n", testName);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE -10 , 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);
    if (pid1 == -2)
    {
        console_output(FALSE, "%s: TEST PASSED\n", testName);
    }
    else
    {
        console_output(FALSE, "%s: TEST FAILED\n", testName);

        /* Wait for the child and print the results. */
        console_output(FALSE, "%s: joining child process\n", testName);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    }
    k_exit(0);

    return 0;
}
