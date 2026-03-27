#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest12
*
* Tests: Kernel mode validation on k_exit.
*
* Description:
*   Spawns a child, waits for it to complete normally, then switches the 
*   processor to user mode by clearing the PSR_KERNEL_MODE bit before 
*   calling k_exit(). The checkKernelMode() guard inside k_exit should 
*   detect user mode and halt the system.
*
* Functions exercised:
*   k_spawn, k_wait, set_psr, get_psr, k_exit (kernel mode check)
*
* Process tree:
*   SchedulerTest12 (priority 5)
*     └── SchedulerTest12-Child1 (priority 3, SimpleDelayExit)
*
* Expected behavior:
*   - Child spawns, runs, and exits normally
*   - Parent successfully waits for child
*   - Parent switches to user mode, then calls k_exit
*   - System halts with "Kernel mode expected, but function called in user mode"
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char nameBuffer[512];
    char* testName = "SchedulerTest12";

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    pid1 = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    /* set the mode to user mode. */
    set_psr(get_psr() & ~PSR_KERNEL_MODE);

    k_exit(0);
    return 0; /* The compiler wants to see this... */
}
