# Lecture 03 — Abstractions 1: Threads and Processes

> [!NOTE]
> **Big idea:** A **thread** is the OS's unit of concurrency — a single
> execution context. Threads let one program handle many things at
> once. **Processes** wrap threads in a protected address space. This
> lecture covers what threads are, why we need them, and the two ways
> programs create new execution contexts: `pthread_create` and `fork`.

---

## Recap: protection via page table translation

Every program runs in a **virtual address space**. The hardware
translates virtual addresses → physical addresses through the
**page table**. Done correctly, no program can touch another's memory.

```
Program's view          Page Table          Physical reality
(virtual addresses)   (in the OS)          (physical memory)
        │                  │                       │
        └──── translate ───┴───────────────────────┘
```

Recall the four parts of a program's address space: **code, data,
heap, stack**.

> [!IMPORTANT]
> The page table lives **in the OS**. If a program could alter its own
> page table, all protection breaks. **Dual-mode operation** is what
> prevents user code from touching it.

---

## What is a thread?

- A single, unique **execution context**
- The abstraction of one execution sequence
- A mechanism for **concurrency** — and can also run in **parallel**

> [!TIP]
> **Protection is orthogonal to threading.** A protection domain (a
> process) can contain one thread or many. The *process* is the
> environment; the *thread* is the execution context inside it.

---

## Why threads? Handling Multiple Things At Once (MTAO)

| Who needs MTAO | Why |
|---|---|
| Operating systems | Processes, interrupts, background maintenance, keystrokes, mouse |
| Networked servers | Many simultaneous client connections |
| Parallel programs | Use multiple cores for performance |

Threads are the **unit of concurrency** the OS provides. Each thread
can represent one task or one "thing happening."

---

## Multiprocessing vs. multiprogramming vs. multithreading

| Term | Meaning |
|---|---|
| **Multiprocessing** | Multiple physical CPUs / cores |
| **Multiprogramming** | Multiple jobs / processes |
| **Multithreading** | Multiple threads per process |

---

## Concurrency ≠ Parallelism

> [!IMPORTANT]
> - **Concurrency** is about *handling* multiple things at once
>   (structure / dealing with many tasks).
> - **Parallelism** is about *doing* multiple things simultaneously
>   (execution / literally at the same instant).
>
> You can have concurrency on a single core (interleaving). You need
> multiple cores for true parallelism.

### What does "run two threads concurrently" mean?

- The scheduler is free to run threads in **any order**, with **any
  interleaving**
- A thread may run to completion, or in big time slices, or tiny ones
- **The moment you have more than one thread, you must reason about
  correctness under *all* possible interleavings**

<details>
<summary><b>Amdahl's Law (to fill in fully later)</b></summary>

The speedup from parallelizing a program is limited by the fraction
that *must* remain sequential. If a fraction $P$ of the program can
be parallelized, the maximum speedup on $N$ cores is:

$$\text{Speedup} = \frac{1}{(1 - P) + \frac{P}{N}}$$

Even with infinite cores, the serial portion $(1-P)$ caps your gains.

</details>

---

## Threads in action: a motivating example

**Without threads** — runs sequentially, second function never starts
until the first finishes:

```c
main() {
    ComputePI("pi.txt");          // runs forever computing pi
    PrintClassList("classlist.txt");  // never reached
}
```

**With threads** — both run concurrently:

```c
main() {
    create_thread(ComputePI, "pi.txt");
    create_thread(PrintClassList, "classlist.txt");
}
```

Now the system behaves as if there are two virtual CPUs. Output of
`pi.txt` is interleaved (in time) with the class list printing.

---

## Numbers Everyone Should Know (Jeff Dean)

| Operation | Time (ns) | ≈ µs | ≈ ms |
|---|---:|---:|---:|
| L1 cache reference | 0.5 | — | — |
| Branch mispredict | 5 | — | — |
| L2 cache reference | 7 | — | — |
| Mutex lock/unlock | 25 | — | — |
| Main memory reference | 100 | 0.1 | — |
| Compress 1 KB with Zippy | 3,000 | 3 | — |
| Send 2 KB over 1 Gbps network | 20,000 | 20 | — |
| Read 1 MB sequentially from memory | 250,000 | 250 | 0.25 |
| Round trip within same datacenter | 500,000 | 500 | 0.5 |
| Read 1 MB sequentially from disk | 20,000,000 | 20,000 | 20 |
| Send packet CA → Netherlands → CA | 150,000,000 | 150,000 | 150 |

> [!TIP]
> **The takeaway:** I/O (disk, network) is *orders of magnitude*
> slower than CPU/memory. An SSD has no seek time but still has a
> nonzero access latency. The fix: do I/O in a separate thread so the
> rest of the program keeps working.

---

## Threads mask I/O latency

A thread is always in one of three states:

```
        ┌─────────────────────────────────┐
        │                                 │
        ▼                                 │
   ┌─────────┐   needs I/O   ┌─────────┐  │ scheduled
   │ RUNNING │ ────────────► │ BLOCKED │  │
   └─────────┘               └─────────┘  │
        ▲                         │       │
        │                         │ I/O done
        │                         ▼       │
        │                    ┌─────────┐  │
        └──────────────────  │  READY  │ ─┘
           scheduler picks   └─────────┘
```

- Waiting for I/O → OS marks the thread **BLOCKED**
- I/O finishes → OS marks it **READY**

> [!QUESTION]
> **Can a thread go directly from BLOCKED to RUNNING?**
> No. It goes BLOCKED → READY queue → (scheduler picks it) → RUNNING.

### The common, useful pattern

```c
main() {
    create_thread(ReadLargeFile, "pi.txt");
    create_thread(RenderUserInterface);
}
```

UI stays responsive while the file reads in the background. **Separate
threads for I/O and for compute/UI is a fundamental, everyday pattern.**

---

## Creating multithreaded processes

> [!NOTE]
> A freshly-started C program has **one thread** in its own address
> space. To become multithreaded, the process issues **system calls**
> to create new threads. Those new threads are part of the same
> process and **share its address space**.

---

## System calls ("syscalls")

The layered picture:

```
┌──────────────────────────────────────────────┐
│  USER MODE                                   │
│  word processors, compilers, browsers,       │
│  email, databases, web servers               │
├──────────────────────────────────────────────┤
│  Portable OS library  (system call interface)│  ← libc, language runtimes
├──────────────────────────────────────────────┤
│  KERNEL MODE                                 │
│  portable OS kernel, platform support,       │
│  device drivers                              │
├──────────────────────────────────────────────┤
│  HARDWARE  (x86, PowerPC, ARM, PCI)          │
└──────────────────────────────────────────────┘
```

- OS libraries issue system calls; language runtimes use the OS library
- **POSIX** is the attempt to standardize the syscall interface across
  operating systems
- `libc` wraps syscalls so you can make an ordinary *procedure call*
  that internally performs the *system call* — and the syscall is what
  transitions to kernel mode

---

## pthreads — the POSIX threads API

The **`p`** in pthreads = POSIX.

### `pthread_create`

```c
int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg);
```

- Creates a thread that begins executing `start_routine` with `arg` as
  its only argument
- When `start_routine` returns, there's an implicit call to
  `pthread_exit`

> [!TIP]
> **Decoding `void *(*start_routine)(void *)`** — read it inside-out:
> `start_routine` is a **pointer to a function** that **takes a
> `void *`** and **returns a `void *`**.

### `pthread_exit`

```c
void pthread_exit(void *value_ptr);
```

Terminates the calling thread and makes `value_ptr` available to any
successful `pthread_join`.

### `pthread_join`

```c
int pthread_join(pthread_t thread, void **value_ptr);
```

Suspends the calling thread until the target thread terminates. If
`value_ptr` is non-null, the value passed to `pthread_exit` by the
terminating thread is stored there.

---

## What actually happens inside `pthread_create`

The library function is a **wrapper around a system call**:

```
int pthread_create(...) {
    // 1. do some normal C work

    // 2. assembly: put syscall number into %eax
    //              put args into %ebx, %ecx, ...
    //              execute special trap instruction
    //    ─────────────────────────────────────────────
    //    KERNEL:  get args from registers
    //             dispatch to the right system function
    //             do the work to spawn the new thread
    //             store return value in %eax
    //    ─────────────────────────────────────────────
    // 3. get return value from registers
    // 4. do some normal C work
}
```

> [!IMPORTANT]
> A system call can cost **~1000 cycles**. Creating a thread creates a
> **schedulable entity** — from that point on, multiple things can be
> running.

> [!QUESTION]
> **When we enter the syscall, are we context switching?**
> No. It's still your process — C compiles to assembly, and this is
> just *special* assembly (the trap instruction) that a C compiler
> wouldn't normally emit. Entering the kernel ≠ switching to another
> process.

---

## The Fork-Join pattern

```
        ┌─────────────┐
        │ Main thread │
        └──────┬──────┘
               │ fork
      ┌────────┼────────┐
      ▼        ▼        ▼
  ┌──────┐ ┌──────┐ ┌──────┐
  │ Sub  │ │ Sub  │ │ Sub  │   ← each gets its own args / work
  └──┬───┘ └──┬───┘ └──┬───┘
     │        │        │
     └────────┼────────┘
              │ join
        ┌─────▼───────┐
        │ Main thread │   ← collects results
        └─────────────┘
```

One parent thread forks a collection of sub-threads, hands them work,
then joins with each to collect results.

---

## Thread state: what's per-thread vs. shared

| Per-thread (private) | Shared (across threads in a process) |
|---|---|
| Thread Control Block (TCB) | Heap |
| Stack | Global variables |
| Stack pointer / saved registers | Code |
| Thread metadata | |

### Memory layout with two threads

Two threads means two stacks in one address space — they can **crash
into each other**.

> [!WARNING]
> Design questions this raises:
> - How do you position the stacks relative to each other?
> - What maximum size do you pick for each stack?
> - What happens if a thread overflows its stack?
> - How might the OS *catch* such a violation? *(guard pages)*

---

## Correctness with concurrent threads

> [!IMPORTANT]
> **Non-determinism:** the scheduler can run threads in any order,
> switching at any time. This makes testing hard — a bug might appear
> only under one specific interleaving.

| Thread type | Property |
|---|---|
| **Independent threads** | No shared state. Deterministic, reproducible. |
| **Cooperating threads** | Share state. Need synchronization. |

### Key vocabulary

| Term | Definition |
|---|---|
| **Synchronization** | Coordination among threads, usually over shared data |
| **Mutual exclusion** | Ensuring only one thread does a particular thing at a time |
| **Critical section** | Code that exactly one thread may execute at once |
| **Lock** | An object only one thread can hold at a time; provides mutual exclusion |

### What a lock provides — two atomic operations

| Operation | Meaning |
|---|---|
| `lock.acquire()` | Wait until the lock is free, then mark it busy |
| `lock.release()` | Mark the lock free — *only* callable by the thread holding it |

### pthreads locks

```c
int pthread_mutex_init(pthread_mutex_t *mutex,
                       const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
```

---

## Bootstrapping: where does the first process come from?

> [!QUESTION]
> Processes are created by other processes. So how does the *first*
> process start?

The **kernel** starts the first process — often configured as an
argument to the kernel before it boots. This is the **`init`**
process. Every other process descends from it.

---

## Creating processes: `fork`

| Syscall | Effect |
|---|---|
| `exit` | Terminate a process |
| `fork` | Copy the current process |

```c
pid_t fork();
```

- Copies the entire current process into a **new address space**
- New process gets a **different PID**
- New process contains a **single thread**

### `fork`'s return value — the key to telling parent from child

| Return value | Meaning |
|---|---|
| **`> 0`** | You're the **parent**; value is the **child's PID** |
| **`== 0`** | You're the **child** |
| **`< 0`** | Error — must be handled |

> [!IMPORTANT]
> After `fork`, the parent's state is **duplicated in both**: address
> space, file descriptors, stack — *everything*, down to the program
> counter. The **only** difference is the return value of `fork`.
> That return value is how each process knows who it is.

### Example: identifying parent vs. child

```c
int main(int argc, char *argv[]) {
    pid_t cpid, mypid;
    pid_t pid = getpid();
    printf("parent pid: %d\n", pid);

    cpid = fork();
    if (cpid > 0) {                       // parent branch
        mypid = getpid();
        printf("[%d] parent of [%d]\n", mypid, cpid);
    } else if (cpid == 0) {               // child branch
        mypid = getpid();
        printf("[%d] child\n", mypid);
    } else {                              // error branch
        perror("Fork failed");
    }
}
```

Both processes run identical code; only the `fork` return value
differs. The kernel tracks parent/child relationships in a **process
tree** — if the child itself calls `fork`, it becomes a parent of
*its* children.

### Isn't copying everything wildly expensive?

> [!TIP]
> You don't actually copy everything. You **copy the page tables**,
> and let the two processes share physical pages until one writes to
> a page — that's **copy-on-write** (covered later).
>
> Linux also offers `spawn` (and `posix_spawn`) which avoids the full
> copy semantics when you just want to launch a new program.

### `fork_race.c` — reasoning about interleaving

```c
int i;
pid_t cpid = fork();
if (cpid > 0) {                  // parent
    for (i = 0; i < 10; i++) {
        printf("Parent: %d\n", i);
        // sleep(1);
    }
} else if (cpid == 0) {          // child
    for (i = 0; i > -10; i--) {
        printf("Child: %d\n", i);
        // sleep(1);
    }
}
```

> [!QUESTION]
> **What does this print? Does adding/removing `sleep()` matter?**
>
> My reasoning: without `sleep`, the parent likely gets a run of its
> prints out while the OS is still setting up the child's address
> space — output is bursty, mostly-parent-then-mostly-child but not
> guaranteed. With `sleep(1)` in both loops, the two processes yield
> the CPU constantly, so their output **interleaves** much more
> evenly.
>
> Either way: the two processes have **completely independent** copies
> of `i` (parent counts up, child counts down), and the exact
> interleaving depends on scheduling — it's **non-deterministic**.

---

## Conclusion

> [!NOTE]
> **Threads** are the OS's unit of concurrency — the abstraction of a
> virtual CPU core.
> - Manage them with `pthread_create`, `pthread_join`, etc.
> - They share data → you need **synchronization**
>
> **Processes** consist of one or more threads inside an address space
> — the abstraction of the machine: an execution environment for a
> program.
> - Manage them with `fork`, `exec`, etc.
>
> We also saw the role of the **OS library**:
> - Provides an API to programs
> - Interfaces with the OS to request services

---

## Key takeaways

1. A **thread** is a single execution context; the OS's unit of
   concurrency
2. **Concurrency ≠ parallelism** — handling many things vs. doing many
   things at once
3. Threads **mask I/O latency** by moving between RUNNING / READY /
   BLOCKED
4. `pthread_create` makes new threads **within** a process (shared
   address space); it's a **wrapper around a syscall**
5. `fork` makes a new **process** — a full copy, distinguished only by
   the return value; cheap in practice thanks to copy-on-write
6. The moment you have cooperating threads, you must reason about
   **every possible interleaving** — hence locks, critical sections,
   mutual exclusion

---

## Confusions to revisit

- [ ] Amdahl's Law — work through concrete speedup calculations
- [ ] Guard pages — how the OS actually catches stack overflow
- [ ] Copy-on-write mechanics — when exactly pages get duplicated
- [ ] The exact trap instruction and how registers carry syscall args
      *(→ lecture 6, system call entry/exit)*
- [ ] How `pthread_join` interacts with the scheduler internally

---

## Vocab to add to `os-vocab.md`

- Thread states: running / ready / blocked
- Concurrency vs. parallelism
- Multiprocessing / multiprogramming / multithreading
- Amdahl's Law
- POSIX
- pthreads
- Fork-join pattern
- Synchronization
- Mutual exclusion
- Critical section
- Lock / mutex
- `fork`, `exit`, `getpid`
- `init` process
- Process tree
- Copy-on-write
- Guard page

---

🔗 **Links:** [[02-four-concepts]] · [[04-process-management]] · [[os-vocab]]