# OS Vocabulary

A living glossary of operating systems terms. Add to it as you go.
Search with `Ctrl+F`.

---

## A

### Absolute vs. Relative Path
An **absolute path** (`/home/oski/cs162`) is resolved from the
filesystem root and ignores the current working directory. A
**relative path** (`index.html`, `./index.html`) is resolved against
the process's CWD.

### Address Space
The set of memory addresses a program can access, plus the state
associated with them. May be virtual (translated by hardware) or
physical. A 32-bit process has a $2^{32}$-address space (~4 GB);
a 64-bit process has $2^{64}$ (~18 EB).

### Amdahl's Law
The speedup from parallelizing a program is limited by the fraction
that must stay sequential. If fraction $P$ can be parallelized across
$N$ cores: $\text{Speedup} = \frac{1}{(1-P) + P/N}$. Even with infinite
cores, the serial portion $(1-P)$ caps the gains.

### ASID (Address Space Identifier)
A tag attached to TLB entries identifying which address space they
belong to. Lets the TLB survive a context switch without being
flushed, by ignoring entries that don't match the current ASID.

---

## B

### Base and Bound
A simple hardware protection mechanism. Each thread has two
registers: `Base` (start of its memory region) and `Bound` (one past
the end). Every memory access is checked: `Base ≤ addr < Bound`.
Predecessor to modern paging.

### BIOS (Basic Input/Output System)
Firmware that provides standardized low-level services on top of
hardware at boot time. Largely replaced by UEFI in modern systems,
but the concept is the same: a bridge between raw hardware and the
OS boot process.

### Buffering (I/O)
Accumulating I/O data in memory rather than transferring it
immediately. Happens at **two independent layers**: the **user level**
(libc, inside the `FILE*`) and the **kernel level** (transparent to
the user). Exists for performance (a syscall costs ~25× a function
call) and functionality (e.g. read-until-newline, which the kernel
can't do). See also: *Stream*, *fflush*.

---

## C

### Concurrency
Handling multiple things at once — a property of program *structure*.
Distinct from **parallelism** (doing multiple things at the same
instant). Concurrency is possible on a single core via interleaving;
parallelism requires multiple cores.

### Context Switch
The process of saving one thread's state (PC, registers, flags, stack
pointer) into its TCB, then loading another thread's state from its
TCB and resuming. Takes on the order of microseconds. If done too
often → **thrashing**.

### Cooperating Threads
Threads that share state with each other. Require synchronization to
stay correct. Opposite of **independent threads**.

### Copy-on-Write (COW)
An optimization for `fork`: instead of physically copying the parent's
memory, the child shares the parent's physical pages (marked
read-only). A page is only actually duplicated when one process
writes to it. Makes `fork` cheap.

### CPU Scheduling
The OS's decision about which thread or process gets the processor at
any given time. Covered in lectures 11–13.

### Critical Section
Code that exactly one thread may execute at a time. Achieved through
mutual exclusion (typically a lock).

### CWD (Current Working Directory)
The directory a process treats as its starting point for resolving
relative paths. Changed with the `chdir` syscall.

---

## D

### Device Driver
Software that mediates between the OS and a specific hardware
device. Usually runs in kernel mode. The most unreliable part of
most operating systems because it's often written by hardware
vendors against complex, quirky hardware.

### Directory
A folder containing files and other directories. Provides
hierarchical naming — a file is reached by a path through the
directory graph.

### Dual-Mode Operation
Hardware-enforced separation between **user mode** (restricted) and
**kernel mode** (unrestricted). Prevents user programs from
directly manipulating hardware, page tables, or kernel memory.

### dup / dup2
Syscalls that duplicate a file descriptor. `dup(fd)` returns a new
fd (lowest available) pointing at the same open file description.
`dup2(fd, target)` makes a *specific* fd number point there (closing
it first if open). The mechanism behind shell I/O redirection.

---

## E

### Exception
A synchronous interrupt caused by the currently-executing
instruction. Examples: divide by zero, invalid instruction, page
fault. Causes a transition from user mode to kernel mode so the OS
can handle it.

### exec
A family of syscalls that replaces the current process's memory image
with a new program. Often used right after `fork`: the child `exec`s
the program you actually want to run.

### exit
A syscall that terminates the calling process.

### Execution Flags
A small set of 1-bit registers that record the result of the most
recent operation (was the result zero, negative, did it overflow,
etc). Branch instructions (`jz`, `jnz`, `jl`) read these flags.
Saved/restored during context switches.

### "Everything is a File"
The defining Unix design principle: disks, devices, terminals,
networking, and IPC all expose the *same* interface
(`open`/`read`/`write`/`close`). Lets a program work with any of them
without code changes.

---

## F

### fflush
Forces libc to flush a stream's user-level buffer to the kernel
immediately, instead of waiting for the buffer to fill, a newline, or
program exit. Use it to make I/O happen exactly when you need it.

### File
The OS abstraction over disk blocks, networked storage, or other
persistent media. Presents a uniform read/write interface
regardless of the underlying device. Has **data** (a byte sequence)
and **metadata** (size, owner, timestamps, permissions).

### File Descriptor
An opaque non-negative integer the kernel hands a process to refer to
an open file. Indexes into the process's file descriptor table. The
user gets an integer rather than a kernel pointer for security — a
number can't be used to reach into kernel memory. Standard ones:
`0` = stdin, `1` = stdout, `2` = stderr.

### fork
A syscall that creates a new process by copying the current one. The
child gets a different PID and a single thread, but otherwise an
identical copy of the parent's address space, file descriptors, and
state. Distinguished from the parent only by `fork`'s return value
(`>0` in parent = child's PID, `0` in child, `<0` = error). Cheap in
practice thanks to **copy-on-write**.

### Fork-Join Pattern
A concurrency structure: a main thread *forks* a collection of
sub-threads, hands each some work, then *joins* with them to collect
results.

### Frame
A fixed-size chunk of physical memory that holds one page of a
virtual address space. Same size as a page (typically 4 KB).

---

## G

### Guard Page
An unmapped page placed at the boundary of a thread's stack. If the
stack overflows into it, the access triggers a page fault, letting
the OS catch the overflow instead of silently corrupting adjacent
memory.

---

## H

### Heap
The region of an address space where dynamically-allocated memory
lives (`malloc`/`free` in C, `new`/`delete` in C++). Grows upward
on demand via `sbrk` or `mmap`.

---

## I

### Illusionist (role of the OS)
The OS hides hardware complexity behind clean abstractions: threads
instead of processors, address spaces instead of raw memory, files
instead of disk blocks, sockets instead of network cards.

### Independent Threads
Threads that share no state with other threads. Their behavior is
deterministic and reproducible. Opposite of **cooperating threads**.

### init Process
The first user process, started directly by the kernel at boot. Every
other process descends from it via `fork`. Solves the bootstrapping
problem (processes are created by other processes — but something has
to be first).

### Inode
The on-disk data structure describing where a file's data lives and
its metadata. An open file description references the inode of the
file it has open. (Internals covered in the File Systems lectures.)

### Interrupt
An asynchronous event from hardware that diverts execution to a
handler in kernel mode. Examples: timer ticks, disk I/O completion,
keyboard input.

### Interrupt Vector
A table indexed by interrupt number. Each entry holds the address
of that interrupt's handler. The hardware uses it to jump to the
right handler when an interrupt fires.

### ioctl
"I/O control" — a catch-all syscall for device-specific operations
that don't fit the uniform `read`/`write` model (e.g. setting a
terminal's mode or a device's baud rate).

### ISA (Instruction Set Architecture)
The interface between hardware and software at the instruction
level — the set of instructions a processor understands, plus
register layout and addressing modes. Examples: x86, ARM, RISC-V.

---

## K

### Kernel
The core of the OS — the code that runs in kernel mode with full
hardware access. Manages processes, memory, file systems, and
hardware. "The one program running at all times on the computer."

### Kernel Mode
The privileged hardware mode in which the kernel runs. Can execute
any instruction, access any memory, control any device.

---

## L

### Lock (Mutex)
An object only one thread can hold at a time, providing mutual
exclusion. Offers two atomic operations: `acquire()` (wait until free,
then mark busy) and `release()` (mark free — only valid for the
holding thread). `mutex` = "mutual exclusion."

---

## M

### Metadata (file)
Information *about* a file, as opposed to its contents: size,
modification time, owner, permissions, and other access-control
information.

### MMU (Memory Management Unit)
Hardware that translates virtual addresses to physical addresses
using the page table. Sits between the CPU and memory.

### Multiplexing
Sharing a single physical resource among multiple virtual users by
rapidly switching between them. Threads multiplex a CPU core;
address spaces multiplex physical memory.

### Multiprocessing
Having multiple physical CPUs or cores.

### Multiprogramming
Running multiple jobs / processes.

### Multitasking
The OS's ability to run multiple processes seemingly at the same
time, achieved through rapid context switching between threads.

### Multithreading
Running multiple threads within a single process.

### Mutual Exclusion
Ensuring that only one thread does a particular thing (e.g., enters a
critical section) at a time. Provided by locks.

---

## N

### Non-determinism
The property that a concurrent program's behavior depends on the
scheduler's choices — thread order and switch timing can vary between
runs. Makes concurrency bugs hard to reproduce and test for.

---

## O

### Open File Description
A kernel object representing one instance of an open file. Holds the
**inode reference** (where the file is) and the **current offset**
(position within the file). File descriptors point *to* these. Across
`fork`, parent and child fds are **aliased** to the *same* open file
description — so they share the offset — and it's **reference-counted**,
freed only when all referencing fds close.

---

## P

### Page
A fixed-size chunk of a virtual address space (typically 4 KB).
The unit at which memory is mapped and protected.

### Page Fault
An exception triggered when a program accesses a virtual address
whose page isn't currently mapped to a physical frame. The OS's
page fault handler decides what to do: load from disk, allocate a
new frame, or kill the process.

### Page Table
A per-process data structure mapping virtual page numbers to
physical frame numbers. Used by the MMU to translate addresses.

### Parallelism
Doing multiple things at the same instant — requires multiple cores.
Distinct from **concurrency**, which is about program structure and
is possible even on one core.

### PCB (Process Control Block)
The kernel's record of everything it needs to know about a process:
its address space, open files, TCBs for its threads, scheduling
state, and parent/child relationships.

### Pipe
A one-way in-kernel communication channel. `pipe(int pipefd[2])`
creates it and returns two fds: `pipefd[0]` (read end) and
`pipefd[1]` (write end). Combined with `fork`, it lets a parent and
child communicate — the basis of shell pipelines (`cmd1 | cmd2`).

### POSIX
A standard that defines a common system call interface across
operating systems, so programs can be portable. The `p` in
`pthreads` stands for POSIX. Expands to "Portable Operating System
Interface for uniX."

### Process
An instance of a running program. Consists of a protected address
space + one or more threads + OS state (file descriptors, sockets,
working directory, etc.). The unit of protection in a modern OS.

### Process Tree
The kernel's hierarchy of processes formed by `fork`: every process
records its parent, and a process that forks becomes the parent of
its children. Rooted at the `init` process.

### Program Counter (PC)
The register holding the address of the next instruction to be
executed. Part of every thread's state.

### Protection
Mechanisms that prevent processes from interfering with each other
or with the OS. Enforced by hardware (MMU, privilege levels) in
cooperation with the OS.

### pthreads
The POSIX threads API — the standard library interface for creating
and managing threads. Key calls: `pthread_create`, `pthread_join`,
`pthread_exit`, `pthread_mutex_lock` / `pthread_mutex_unlock`.

---

## R

### Referee (role of the OS)
The OS mediates access to shared resources (CPU, memory, devices)
and enforces protection between processes.

### Reference Count
A count of how many things currently refer to a shared resource. The
resource is freed only when the count drops to zero. Used for open
file descriptions: `close` decrements the count; the description
survives until every referencing fd is closed.

### Resident (thread)
A thread is *resident* when its state is loaded into a physical
processor and it's currently executing. Otherwise it's
*suspended*, with state in its TCB.

---

## S

### Scheduler
The component of the OS that decides which thread/process runs next.
Invoked on every timer tick and other scheduling events. Arguably
*is* the operating system.

### Segment
Historically (x86): a contiguous region of memory with a base and a
length, accessed via segment registers (CS, DS, SS, etc.). Modern
OSes mostly ignore segmentation in favor of paging.

In the context of address space layout: code segment, data segment,
heap segment, stack segment.

### Segmentation Fault
The user-visible name for a memory access violation. The MMU
detected an illegal access, the OS killed the process.

### Semaphore
A generalized lock holding a non-negative value, with two atomic
operations: `P()` / `down()` (wait until value > 0, then decrement)
and `V()` / `up()` (increment, waking a waiter). Initial value 1 →
mutual exclusion; initial value 0 → signaling between threads.

### Signal
An asynchronous notification delivered to a process (e.g. `SIGINT`
from Ctrl-C, `SIGTERM`, `SIGKILL`). A process can install a handler
via `sigaction`; otherwise a default action runs (often termination).
`SIGKILL` and `SIGSTOP` cannot be caught or ignored.

### Socket
The OS abstraction over networking. Presents a read/write interface
over network connections, hiding TCP/IP and hardware details.

### Stack
The region of an address space holding function call frames, local
variables, and return addresses. Grows downward as functions are
called.

### Stream
The high-level C I/O abstraction — a `FILE*` returned by `fopen`.
Wraps a file descriptor with a user-level buffer, a lock, and
bookkeeping. Operated on by `fread`, `fwrite`, `fprintf`, `fseek`,
etc. Contrast with raw file descriptors.

### Supervisor Mode
Synonym for **kernel mode**.

### Synchronization
Coordination among threads, usually concerning shared data. Locks,
semaphores, monitors, and condition variables are synchronization
primitives.

### Syscall (System Call)
A controlled transition from user mode to kernel mode. The mechanism
by which user programs request OS services (file I/O, networking,
process creation, etc.).

---

## T

### TCB (Thread Control Block)
The kernel's record of a suspended thread's state: PC, stack
pointer, registers, execution flags. Lets the OS resume the thread
later.

### Thread
A single execution context within a process: PC, registers, flags,
and stack. Multiple threads in one process share the address space,
code, and most resources but have their own execution state. The
unit of scheduling.

### Thread States (Running / Ready / Blocked)
The three states a thread can be in:
- **Running** — currently executing on a core
- **Ready** — runnable, waiting for the scheduler to pick it
- **Blocked** — waiting for an event (e.g., I/O); not runnable

Transitions: a blocked thread whose event completes goes to *Ready*
(not directly to *Running*) — it must wait for the scheduler.

### Thrashing
The pathological state where the system spends most of its time on
overhead (context switches, page faults) and little on actual work.
Happens when resources are oversubscribed.

### TLB (Translation Lookaside Buffer)
A small fast cache inside the MMU that holds recent
virtual-to-physical translations. Avoids the cost of walking the
page table for every memory access.

### Trap
A synchronous transition to kernel mode triggered by the running
instruction (either an exception or an explicit syscall). Compare:
**interrupt** is asynchronous, from hardware.

---

## U

### User Mode
The restricted hardware mode in which application code runs.
Cannot directly access hardware, modify page tables, or touch
kernel memory.

---

## V

### Virtual Address
An address as seen by a process. Translated by the MMU into a
physical address before reaching memory.

### Virtual Memory
The abstraction in which each process has its own address space,
distinct from physical memory, with the OS and MMU translating
between them. Enables protection, sharing, paging, and more.

---

## W

### wait / waitpid
Syscalls that let a parent process block until a child terminates,
retrieving the child's exit status. Used by shells: fork a child to
run a command, then `wait` for it to finish.

---

🔗 **Linked from:** [[01-what-is-an-os]] · [[02-four-concepts]] · [[03-threads-and-processes]] · [[04-files-and-io]]