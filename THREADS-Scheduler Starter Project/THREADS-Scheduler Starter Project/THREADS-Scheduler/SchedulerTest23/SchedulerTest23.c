#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SpawnTwoPriorities(char* strArgs);
int UnblockTwoPriorities(char* strArgs);
extern int pids[];

/*********************************************************************************
*
* SchedulerTest23
*
* Tests: Block and unblock with two different priority groups.
*
* Description:
*   Spawns a child (SpawnTwoPriorities) at priority 2, which spawns two 
*   groups of blocked children: BLOCK_UNBLOCK_COUNT (4) at priority 3 and 
*   4 more at priority 4, all using SimpleBockExit (block status 14). Then 
*   spawns a high-priority unblocking process (UnblockTwoPriorities, 
*   priority 5) that unblocks all 8 children. Tests that unblocking 
*   correctly handles processes at different priorities and that the 
*   dispatcher schedules them in priority order.
*
* Functions exercised:
*   k_spawn, block, unblock, k_wait, k_exit, display_process_table
*
* Process tree:
*   SchedulerTest23 (priority 5)
*     └── SchedulerTest23-Child1 (priority 2, SpawnTwoPriorities)
*           ├── ...-Child1 through Child4 (priority 3, SimpleBockExit)
*           ├── ...-Child5 through Child8 (priority 4, SimpleBockExit)
*           └── ...-Child8 (priority 5, UnblockTwoPriorities)
*
* Expected behavior:
*   - Process table shows 8 blocked processes at two priority levels
*   - Priority 5 unblocker runs, unblocks all 8
*   - Higher priority (4) children run first after unblocking
*   - All children exit with -3, parent waits for all
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char nameBuffer[512];
    char* testName = "SchedulerTest23";

    console_output(FALSE, "\n%s: started\n", testName);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SpawnTwoPriorities, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: joining child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}


int SpawnTwoPriorities(char* strArgs)
{
    int kidpid, status = 0;
    char nameBuffer[512];

    console_output(FALSE, "%s: started\n", strArgs);

    for (int i = 0; i < BLOCK_UNBLOCK_COUNT; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", strArgs);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", strArgs, i + 1);
        pids[i] = k_spawn(nameBuffer, SimpleBockExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", strArgs, pids[i]);
    }

    for (int i = BLOCK_UNBLOCK_COUNT; i < BLOCK_UNBLOCK_COUNT*2; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", strArgs);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", strArgs, i + 1);
        pids[i] = k_spawn(nameBuffer, SimpleBockExit, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", strArgs, pids[i]);
    }

    display_process_table();

    console_output(FALSE, "%s: performing spawn of child\n", strArgs);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", strArgs, BLOCK_UNBLOCK_COUNT * 2);
    kidpid = k_spawn(nameBuffer, UnblockTwoPriorities, nameBuffer, THREADS_MIN_STACK_SIZE, 5);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", strArgs, kidpid);

    console_output(FALSE, "%s: waiting for child process\n", strArgs);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", strArgs, kidpid, status);

    for (int i = 0; i < BLOCK_UNBLOCK_COUNT * 2; ++i)
    {
        console_output(FALSE, "%s: waiting for child process\n", strArgs);
        kidpid = k_wait(&status);
        console_output(FALSE, "%s: exit status for child %d is %d\n", strArgs, kidpid, status);
    }

    k_exit(-2);

    return 0;
}

