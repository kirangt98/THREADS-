#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"


int RunUntilSignaled(char* strArgs);

/*********************************************************************************
*
* SchedulerTest29
*
* Tests: Time slicing with a long-running background process.
*
* Description:
*   Spawns a long-running child (RunUntilSignaled, priority 3) that 
*   busy-waits until signaled. Then, in three iterations, spawns four 
*   additional children (DelayAndDump, priority 3) that busy-wait for 
*   ~10 seconds each. Since all five processes share the same priority 
*   and exceed the 80ms time slice, the scheduler must round-robin between 
*   them. Children whose name is divisible by 4 dump the process table.
*   After all three iterations, the long-running process is signaled and 
*   reaped.
*
* Functions exercised:
*   k_spawn, k_wait, k_kill, k_exit, time_slice, dispatcher (round-robin),
*   signaled(), display_process_table
*
* Process tree:
*   SchedulerTest29 (priority 5)
*     ├── SchedulerTest29-Child1 (priority 3, RunUntilSignaled) [persistent]
*     ├── SchedulerTest29-Child2..5  (priority 3, DelayAndDump) [iteration 1]
*     ├── SchedulerTest29-Child6..9  (priority 3, DelayAndDump) [iteration 2]
*     └── SchedulerTest29-Child10..13 (priority 3, DelayAndDump) [iteration 3]
*
* Expected behavior:
*   - Process table dumps show non-zero CPU time for all concurrent children,
*     confirming time slicing works with a persistent background process
*   - Long-running process continues across all iterations
*   - After iterations complete, signaling terminates the long-running process
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid = -1, longRunningPid;
    int i, j;
    char nameBuffer[1028];
    char* testName = "SchedulerTest29";
    int count = 1;

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, count++);
    longRunningPid = k_spawn(nameBuffer, RunUntilSignaled, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, longRunningPid);

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

    /* terminate and wait for the first process.*/
    k_kill(longRunningPid, SIG_TERM);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n",
        testName, kidpid, status);

    k_exit(0);

    return 0;
}


int RunUntilSignaled(char* strArgs)
{

    if (strArgs != NULL)
    {
        console_output(FALSE, "%s: started\n", strArgs);
        console_output(FALSE, "%s: performing spawn of first child\n", strArgs);

        while (!signaled());
    }
    k_exit(-3);

    return 0;
}