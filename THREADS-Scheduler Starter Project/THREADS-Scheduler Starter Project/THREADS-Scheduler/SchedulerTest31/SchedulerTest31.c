
#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

/*********************************************************************************
*
* SchedulerTest31
*
* Tests: block() rejects reserved status values.
*
* Description:
*   Calls block(6) with a status value of 6, which falls within the 
*   reserved range (values <= 10 are reserved for internal scheduler 
*   statuses). The block() function should detect this and halt the 
*   system with an error.
*
* Functions exercised:
*   block (reserved status validation)
*
* Process tree:
*   SchedulerTest31 (priority 5) — halts during block call
*
* Expected behavior:
*   - block(6) detects the reserved status value
*   - System halts with "block: function called with a reserved status value"
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    char* testName = "SchedulerTest31";

    console_output(FALSE, "\n%s: started\n", testName);

    /* Use the -Child naming convention for the child process name. */
    console_output(FALSE, "%s: blocking with a value of 6\n", testName);
    block(6);

    k_exit(0);

    return 0;
}

