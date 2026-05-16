## Files and IO

Recall: Synchronization between Threads
Mutual Exclusion
Critical Section
Locks
Semaphores

What is a Seamphore?
Semaphores are a kind of generalized lock
A semaphore has a non negative value and supports the two ops
- P() or down(), atomic op that waits for semaphore to become positive then decrements it by 1
- V() or up() increments and wakes up if waiting P

Two Semaphore Patterns:

Mutual Exclusion
initial value of semaphore = 1;
semaphore.down();
    // Critical section goes here
semaphore.up();

Signaling other threads, ThreadJoin

Initial value of semaphore = 0;
Threadjoin {
    semaphore.down();
}

ThreadFinish {
    semaphore.up();
}

Starting new Program (in shell)

the typical pattern is fork, then the parent waits, then the child runs the program. it actually forks a separate thing (make a diagram for this)
so like typing a command into some shell, it breaks off a child process to run the command.

int status;
pid_t tcpid;

cpid = fork();
if (cpid > 0) {
    mypid = getpid();
    printf("[%d] parent of [%d]\n", mypid, cpid):
    tcpid = wait(&status);
    printf("bye")
} else if (cpid == 0) {
    mypid = getpid();
    printf("[%d] child\n", mypid);
    exit(42);
}

inf_loop.c

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void signal_callback_handler(int signum) {
    printf("Caught signal!\n");
    exit(1);
}
int main() {
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = signal_callback_handler;
    sigaction(SIGINT, &sa, NULL);
    while (1) {}
}

this code looks like it goes into infinite loop, however if you send a sigint, then it goes into the callback handler and exits it.

Q: what would happen if the process receives a sigint signal but does not register a signal handler?
A: Defaults it to killing the process

You can make your own signal handlers.

Common POSIX Signals
SIGINT - ctrl c
SIGTERM default for kill shell
SIGSTP - ctrl-z
SIGKIll, SIGSTOP- terminate / stop process

POSIX = Portable Operating System Interface for uniX



   





