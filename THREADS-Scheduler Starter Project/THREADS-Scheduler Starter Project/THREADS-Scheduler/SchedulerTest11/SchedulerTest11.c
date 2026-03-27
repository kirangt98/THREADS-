#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest11
*
* Tests: Kernel mode validation on k_spawn.
*
* Description:
*   Switches the processor to user mode by clearing the PSR_KERNEL_MODE bit,
*   then attempts to call k_spawn(). The checkKernelMode() guard inside 
*   k_spawn should detect that the caller is in user mode and halt the system.
*
* Functions exercised:
*   set_psr, get_psr, k_spawn (kernel mode check), checkKernelMode
*
* Process tree:
*   SchedulerTest11 (priority 5) — halts before child is created
*
* Expected behavior:
*   - PSR is set to user mode
*   - k_spawn is called, triggering checkKernelMode
*   - System halts with "Kernel mode expected, but function called in user mode"
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char nameBuffer[512];
    char* testName = "SchedulerTest11";

    console_output(FALSE, "\n%s: started\n", testName);

    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);

    /* set the mode to user mode. */
    set_psr(get_psr() & ~PSR_KERNEL_MODE);
    pid1 = k_spawn(nameBuffer, SimpleDelayExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);
    k_exit(0);

    return 0; 
}
