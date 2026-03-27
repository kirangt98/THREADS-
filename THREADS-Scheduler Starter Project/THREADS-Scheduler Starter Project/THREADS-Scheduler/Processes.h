#pragma once

typedef struct _process
{
    struct _process* nextReadyProcess;
    struct _process* nextSiblingProcess;

    struct _process* pParent;
    struct _process* pChildren;
    struct _process* joinQueue;     /* Queue of processes waiting to join this process */
    struct _process* nextJoiner;    /* Next in join queue */

    char           name[MAXNAME];     /* Process name */
    char           startArgs[MAXARG]; /* Process arguments */
    void* context;           /* Process's current context */
    short          pid;               /* Process id (pid) */
    int            priority;
    int (*entryPoint) (void*);        /* The entry point that is called from launch */
    char* stack;             /* Stack pointer */
    unsigned int   stacksize;
    int            status;            /* READY, RUNNING, WAIT_BLOCK, JOIN_BLOCK, QUIT, BLOCKED */
    int            exitCode;          /* Exit code when process quits */
    int            cpuTime;           /* CPU time used by this process */
} Process;