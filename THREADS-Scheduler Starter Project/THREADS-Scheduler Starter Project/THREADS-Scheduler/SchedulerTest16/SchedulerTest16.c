#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

#define PID_TEST_COUNT 5
int childPids[PID_TEST_COUNT];
int VerifyGetPid(char* strArgs);
/*********************************************************************************
*
* SchedulerTest16
*
* Tests: k_getpid() returns correct PID for each process.
*
* Description:
*   Spawns PID_TEST_COUNT (5) children at priority 3. Each child (VerifyGetPid)
*   calls k_getpid() and compares the returned value against the PID that was
*   returned by k_spawn() in the parent (stored in childPids[]). Reports 
*   PASSED or FAILED for each child.
*
* Functions exercised:
*   k_spawn, k_getpid, k_wait, k_exit
*
* Process tree:
*   SchedulerTest16 (priority 5)
*     ├── SchedulerTest16-Child1 (priority 3, VerifyGetPid)
*     ├── SchedulerTest16-Child2 (priority 3, VerifyGetPid)
*     ├── SchedulerTest16-Child3 (priority 3, VerifyGetPid)
*     ├── SchedulerTest16-Child4 (priority 3, VerifyGetPid)
*     └── SchedulerTest16-Child5 (priority 3, VerifyGetPid)
*
* Expected behavior:
*   - Each child prints PASSED confirming k_getpid matches spawn return value
*   - All children exit with -2
*   - Parent waits for all 5 children
*
*********************************************************************************/
int SchedulerEntryPoint(void* args)
{
    int status = -1, kidpid = -1;
    char* testName = "SchedulerTest16";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    for (int i = 0; i < PID_TEST_COUNT; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", testName);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, i+1);
        childPids[i] = k_spawn(nameBuffer, VerifyGetPid, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, childPids[i]);
    }

    for (int i = 0; i < PID_TEST_COUNT; ++i)
    {
        console_output(FALSE, "%s: waiting for child process\n", testName);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    }

    k_exit(0);

    return 0;
}

int VerifyGetPid(char* strArgs)
{
    static int i = 0;
    int pid;
    if ((pid = k_getpid()) != childPids[i])
    {
        console_output(FALSE, "%s: k_getPid test FAILED by returning %d\n", strArgs, pid);
    }
    else
    {
        console_output(FALSE, "%s: k_getPid test PASSED by returning %d\n", strArgs, pid);

    }
    ++i;
    k_exit(-2);

    return 0;
}