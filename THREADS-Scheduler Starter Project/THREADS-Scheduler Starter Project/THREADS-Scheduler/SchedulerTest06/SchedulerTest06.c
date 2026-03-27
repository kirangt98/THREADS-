#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SimpleDelayExit(void* pArgs);
void SystemDelay(int millisTime);
int SpawnTwoPriorityFour(char* pArgs);
int DelayAndDump(char* arg);
int SignalAndJoinTwoLower(char* strArgs);


/*********************************************************************************
*
* SchedulerTest06
*
* Tests: Signal and join with multi-level process hierarchy.
*
* Description:
*   Spawns a child (SignalAndJoinTwoLower) at priority 3, which in turn
*   spawns two grandchildren at priority 2 (SpawnOnePriorityOne). Each 
*   grandchild spawns a great-grandchild at priority 1. The middle child
*   then signals and joins each grandchild in sequence, causing a cascade
*   of signal handling down the tree. The process table is dumped at key
*   points to show blocked/signaled states.
*
* Functions exercised:
*   k_spawn, k_kill (SIG_TERM), k_join, k_wait, k_exit,
*   display_process_table
*
* Process tree:
*   SchedulerTest06 (priority 5)
*     └── SchedulerTest06-Child1 (priority 3, SignalAndJoinTwoLower)
*           ├── ...-Child1-Child1 (priority 2, SpawnOnePriorityOne)
*           │     └── ...-Child1 (priority 1, SimpleDelayExit)
*           └── ...-Child1-Child2 (priority 2, SpawnOnePriorityOne)
*                 └── ...-Child1 (priority 1, SimpleDelayExit)
*
* Expected behavior:
*   - Process table dumps show WAIT BLOCK and JOIN BLOCK states
*   - Signaled processes exit with code -5
*   - All processes are correctly reaped
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char* testName = "SchedulerTest06";
    char nameBuffer[512];

    /* spawn one simple child process at a lower priority. */
    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SignalAndJoinTwoLower, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);
 
    display_process_table();

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}
