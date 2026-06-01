# Synchronization 1: Concurrency and Mutual Exclusion

- How does an OS provide concurrency through threads?
- Introduce needs for synchronization
- Discussion of Locks and Semaphores

Recall:
- Inter-Process Communication
- POSIX/UNIX Pipe
- Socket endpoint for Communication
- Conncetion setup over TCP/IP

Multiplexing Processes: The Process Control Block
    - Process State
    - Process Number
    - Program Counter
    - Registers
    - Memory Limits
    - List of open files

- Kernel represents each process as a PCB
- Kernel Scheduler maintains a data structure containing the PCBs
- Give out non-CPU resources
    - Memoy/IO
    - Another

Context Switch
- Switching from one process to another? 
    - Thread running process P_1
    - Interrupt or Syscall
    - Save State in PCB_0
    - Reload state from PCB_1
    - Start executing process P_1
- Beware thrashing

Lifecycle of a Process of Thread
- new: process is being created
- ready: process is waiting to run
- running: instructions are being executed
- waiting: process waiting for some event to occur (example: disk read/IO)
- terminated: process has finished execution

Recall: Shared vs. Per Thread States
- Shared State
    - Heap
    - Global Variables
    - Code
- Per Thread States
    - TCB
    - Stack Information
    - Saved Registers
    - Thread Metadata
    - Stack

The Core of Concurrency: The Dispatch Loop
- Conceptually, the scheduling loop of the operating systems looks like:
Loop {
    RunThread();
    ChooseNextThread();
    SaveStateofCPU(currTCB);
    LoadStateofCPU(newTCB);
}
- Infinite loop
    - One could argue this is all the OS des

Q: Should we ever exit this loop?
A: Shutting down the machine, power outages, really when the machine as a whole panics or exits

Running a thread
- How do I run a thread?
    - Load its state (regs, PC, stack pointer) into CPU
    - Load environment (virtual mem space, etc.)
    - Jump to the PC
- How does the dispatcher get control back?
    - Internal events: thread returns control
    - External events: Tthread get preempted









