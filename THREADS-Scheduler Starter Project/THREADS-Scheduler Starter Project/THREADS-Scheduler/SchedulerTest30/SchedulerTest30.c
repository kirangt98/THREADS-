#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int DelayLongAndExit(char* strArgs);

/*********************************************************************************
*
* SchedulerTest30
*
* Tests: read_time() returns correct CPU execution time.
*
* Description:
*   Spawns three children (DelayLongAndExit, priority 3) that each 
*   busy-wait for an increasing duration based on their child number 
*   (child 1 = 1000ms, child 2 = 2000ms, child 3 = 3000ms). Each child 
*   calls read_time() before exiting and reports the result. The reported 
*   CPU time should approximate the actual busy-wait duration, confirming 
*   that the cputime accounting and read_time() function work correctly.
*
* Functions exercised:
*   k_spawn, k_wait, k_exit, read_time, SystemDelay
*
* Process tree:
*   SchedulerTest30 (priority 5)
*     ├── SchedulerTest30-Child1 (priority 3, DelayLongAndExit, ~1000ms)
*     ├── SchedulerTest30-Child2 (priority 3, DelayLongAndExit, ~2000ms)
*     └── SchedulerTest30-Child3 (priority 3, DelayLongAndExit, ~3000ms)
*
* Expected behavior:
*   - Each child reports a read_time value proportional to its delay
*   - With time slicing, CPU time may be less than wall-clock delay
*   - All children exit with -3
*   - Parent waits for all three
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid = -1;
    char* testName = "SchedulerTest30";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    for (int i = 0; i < 3; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", testName);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", testName, i + 1);
        kidpid = k_spawn(nameBuffer, DelayLongAndExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, kidpid);
    }

    for (int i = 0; i < 3; ++i)
    {
        console_output(FALSE, "%s: waiting for child process\n", testName);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    }
    k_exit(0);

    return 0;
}


/*********************************************************************************
*
* DelayLongAndExit
*
* Delays based on the child number.
*
*********************************************************************************/
int DelayLongAndExit(char* strArgs)
{
    int testNumber;

    if (strArgs != NULL)
    {
        int delayTime, readTimeResult;

        // Get the test number
        testNumber = GetChildNumber(strArgs);

        // calculate the delay
        delayTime = testNumber * 1000;

        console_output(FALSE, "%s: started\n", strArgs);

        SystemDelay(delayTime);

        if (strlen(strArgs) <= 0)
        {
            console_output(FALSE, "NO STRING: %d, %d\n", k_getpid(), testNumber);
            display_process_table();
            ExitProcess(0);
        }
        console_output(FALSE, "%s: calling readTime\n", strArgs);
        readTimeResult = read_time();
        console_output(FALSE, "%s: readTime returned %d\n", strArgs, readTimeResult);

        console_output(FALSE, "%s: quitting\n", strArgs);

    }

    k_exit(-3);

    return 0;
}