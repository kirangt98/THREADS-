#include <stdio.h>
#include "THREADSLib.h"
#include "SchedulerTesting.h"
#include "Scheduler.h"

int SpawnTwoPrioritiesReversed(char* strArgs);
int UnblockTwoPriorities(char* strArgs);
#define BLOCK_UNBLOCK_COUNT 4
extern int pids[BLOCK_UNBLOCK_COUNT];

/*********************************************************************************
*
* SchedulerTest24
*
* Tests: Block and unblock with reversed priority ordering.
*
* Description:
*   Same concept as SchedulerTest23 but with reversed priority assignment.
*   Spawns a child (SpawnTwoPrioritiesReversed) at priority 2, which creates
*   BLOCK_UNBLOCK_COUNT (4) children at priority 4 and 4 more at priority 3 
*   (reversed from Test23). All children use SimpleBockExit (block status 14).
*   A high-priority unblocker (priority 5) then unblocks all 8. Tests that 
*   priority ordering is respected regardless of spawn order.
*
* Functions exercised:
*   k_spawn, block, unblock, k_wait, k_exit, display_process_table
*
* Process tree:
*   SchedulerTest24 (priority 5)
*     └── SchedulerTest24-Child1 (priority 2, SpawnTwoPrioritiesReversed)
*           ├── ...-Child1 through Child4 (priority 4, SimpleBockExit)
*           ├── ...-Child5 through Child8 (priority 3, SimpleBockExit)
*           └── ...-Child8 (priority 5, UnblockTwoPriorities)
*
* Expected behavior:
*   - Priority 4 children (spawned first) should run before priority 3
*     children after all are unblocked
*   - Verifies dispatcher respects priority regardless of spawn/unblock order
*   - All children exit with -3
*
*********************************************************************************/
int SchedulerEntryPoint(void* pArgs)
{
    int status = -1, pid1, kidpid = -1;
    char nameBuffer[512];
    char* testName = "SchedulerTest24";

    console_output(FALSE, "\n%s: started\n", testName);
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child1", testName);
    pid1 = k_spawn(nameBuffer, SpawnTwoPrioritiesReversed, nameBuffer, THREADS_MIN_STACK_SIZE, 2);
    console_output(FALSE, "%s: after spawn of child with pid %d\n", testName, pid1);

    /* Wait for the child and print the results. */
    console_output(FALSE, "%s: joining child process\n", testName);
    kidpid = k_wait(&status);
    console_output(FALSE, "%s: exit status for child %d is %d\n", testName, kidpid, status);

    k_exit(0);

    return 0;
}


int SpawnTwoPrioritiesReversed(char* strArgs)
{
    int kidpid, status = 0;
    char nameBuffer[512];

    console_output(FALSE, "%s: started\n", strArgs);

    for (int i = 0; i < BLOCK_UNBLOCK_COUNT; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", strArgs);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", strArgs, i + 1);
        pids[i] = k_spawn(nameBuffer, SimpleBockExit, nameBuffer, THREADS_MIN_STACK_SIZE, 4);
        console_output(FALSE, "%s: after spawn of child with pid %d\n", strArgs, pids[i]);
    }

    for (int i = BLOCK_UNBLOCK_COUNT; i < BLOCK_UNBLOCK_COUNT * 2; ++i)
    {
        console_output(FALSE, "%s: performing spawn of child\n", strArgs);
        snprintf(nameBuffer, sizeof(nameBuffer), "%s-Child%d", strArgs, i + 1);
        pids[i] = k_spawn(nameBuffer, SimpleBockExit, nameBuffer, THREADS_MIN_STACK_SIZE, 3);
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
