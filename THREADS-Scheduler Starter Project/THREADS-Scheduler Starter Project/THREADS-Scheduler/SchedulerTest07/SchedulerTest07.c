#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest07
*
* Tests: Lower-priority parent spawns a higher-priority child (preemption).
*
* Description:
*   Spawns a child (SpawnOnePriorityFour) at priority 2, which in turn spawns
*   a grandchild (SimpleDelayExit) at priority 4. Since the grandchild has a
*   higher priority than its parent, it should preempt the parent when spawned.
*   Tests that priority preemption works correctly when the spawning process
*   is not the highest priority in the system.
*
* Functions exercised:
*   k_spawn (with upward preemption), k_wait, k_exit
*
* Process tree:
*   SchedulerTest07 (priority 5)
*     └── SchedulerTest07-Child1 (priority 2, SpawnOnePriorityFour)
*           └── ...-Child1-Child1 (priority 4, SimpleDelayExit)
*
* Expected behavior:
*   - Priority 4 grandchild preempts priority 2 parent
*   - Grandchild runs to completion before parent resumes
*   - Clean exit chain via k_wait
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, kidpid;
    char nameBuffer[512];
    char* testName = "SchedulerTest07";


    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    kidpid = k_spawn(nameBuffer, SpawnOnePriorityFour, nameBuffer, THREADS_MIN_STACK_SIZE, 2);

    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;

} 