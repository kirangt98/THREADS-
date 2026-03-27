#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

#define JOIN_TEST_COUNT 10

extern int JoinProcess(char* strArgs); // from test 15


/*********************************************************************************
*
* SchedulerTest17
*
* Tests: Multiple joiners on a single process.
*
* Description:
*   Spawns one target child (SimpleDelayExit, priority 2) whose PID is stored
*   in gPid. Then spawns JOIN_TEST_COUNT (10) joiner processes (JoinProcess, 
*   priority 3), each of which calls k_join on the same target process.
*   Tests that multiple processes can successfully join on a single target 
*   and that all joiners are awakened when the target exits.
*
* Functions exercised:
*   k_spawn, k_join (multiple joiners), k_wait, k_exit
*
* Process tree:
*   SchedulerTest17 (priority 5)
*     ├── SchedulerTest17-Child1  (priority 2, SimpleDelayExit) [gPid target]
*     ├── SchedulerTest17-Child2  (priority 3, JoinProcess → joins gPid)
*     ├── SchedulerTest17-Child3  (priority 3, JoinProcess → joins gPid)
*     ├── ...
*     └── SchedulerTest17-Child11 (priority 3, JoinProcess → joins gPid)
*
* Expected behavior:
*   - All 10 joiners block in k_join waiting for the target
*   - Target exits, all joiners are unblocked and receive exit code
*   - Parent waits for all 11 children (1 target + 10 joiners)
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid = -1;
    char* testName = "SchedulerTest17";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    console_output(FALSE, "%s: performing spawn of child\n", testName);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    gPid = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 2);

    for (int i = 0; i < JOIN_TEST_COUNT; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", testName);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, i + 2);
        kidpid = k_spawn(nameBuffer, JoinProcess, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);
    }

    for (int i = 0; i < JOIN_TEST_COUNT+1; ++i)
    {
        console_output(FALSE, "%s: waiting for child process\n", testName);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    }

    k_exit(0);

    return 0;

}
