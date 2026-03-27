#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SpawnAndUnblock(char* strArgs);

/*********************************************************************************
*
* SchedulerTest22
*
* Tests: Basic block and unblock functionality.
*
* Description:
*   Spawns a child (SpawnAndUnblock) at priority 2, which spawns three 
*   grandchildren (SimpleBockExit, priority 3). Each grandchild calls 
*   block(14) to enter a custom blocked state. The parent then unblocks 
*   each grandchild in order. After unblocking, each grandchild resumes 
*   and exits. The process table is dumped while children are blocked to
*   show the custom blocked status.
*
* Functions exercised:
*   k_spawn, block (custom status 14), unblock, k_wait, k_exit,
*   display_process_table
*
* Process tree:
*   SchedulerTest22 (priority 5)
*     └── SchedulerTest22-Child1 (priority 2, SpawnAndUnblock)
*           ├── ...-Child1-Child1 (priority 3, SimpleBockExit)
*           ├── ...-Child1-Child2 (priority 3, SimpleBockExit)
*           └── ...-Child1-Child3 (priority 3, SimpleBockExit)
*
* Expected behavior:
*   - Process table shows status 14 for blocked children
*   - unblock moves each child back to ready
*   - Children resume and exit with -3
*   - Parent waits for all three children
*
*********************************************************************************/
int SchedulerEntryPoint(void *pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char nameBuffer[512];
    char* testName = "SchedulerTest22";

    console_output(FALSE, "\n%s: started\n", testName);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SpawnAndUnblock, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: waiting for child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}

int SpawnAndUnblock(char* strArgs)
{
    int pids[BLOCK_UNBLOCK_COUNT], status = 0;
    char nameBuffer[512];

    console_output(FALSE, "%s: started\n", strArgs);

    for (int i = 0; i < 3; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", strArgs);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", strArgs, i + 1);
        pids[i] = k_spawn(nameBuffer, SimpleBockExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", strArgs, pids[i]);
    }

    display_process_table();

    for (int i = 0; i < 3; ++i)
    {
        console_output(FALSE, "%s: Unblocking process %d\n", strArgs, pids[i]);
        unblock(pids[i]);
    }

    for (int i = 0; i < 3; ++i)
    {
        int pid;
        console_output(FALSE, "%s: waiting for child process\n", strArgs);
        pid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", strArgs, pid, status);
    }

    k_exit(-2);

    return 0;
}