
#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest02
*
* Tests: Higher-priority child preemption at spawn.
*
* Description:
*   Spawns a child at priority 3 (SpawnTwoPriorityFour), which in turn spawns 
*   two grandchildren at priority 4. Since the grandchildren are higher priority 
*   than their parent (4 > 3), they should preempt the parent immediately upon 
*   being spawned and run before the parent continues.
*
* Functions exercised:
*   k_spawn (with preemption), k_wait, k_exit
*
* Process tree:
*   SchedulerTest02 (priority 5)
*     └── SchedulerTest02-Child1 (priority 3, SpawnTwoPriorityFour)
*           ├── ...-Child1-Child1 (priority 4, SimpleDelayExit)
*           └── ...-Child1-Child2 (priority 4, SimpleDelayExit)
*
* Expected behavior:
*   - Priority 4 grandchildren preempt the priority 3 parent at spawn
*   - Console output should show grandchildren starting before parent's 
*     spawn calls return
*   - All processes exit cleanly via k_wait chain
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char* testName = "SchedulerTest02";
    char nameBuffer[512];

    console_output(FALSE, "\n%s: started\n", testName);

    /* Use the -Child naming convention for the child process name. */
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    pid1 = k_spawn(nameBuffer, SpawnTwoPriorityFour, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);

    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    return 0;
}

