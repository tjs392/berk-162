# Synchronization 1: Concurrency and Mutual Exclusion

> [!NOTE]
> This lecture is about **how the OS creates the illusion that many threads run at once on one CPU**, and the trouble that illusion causes. The mechanism is the *context switch*: save a thread's state into its control block, load another's, jump. Once threads start sharing state, non-deterministic scheduling turns innocent code like `balance += amount` into a race. The fix introduced here is the **lock** — semaphores come next time.

**Recall (from the IPC lecture):** inter-process communication, POSIX/UNIX pipes, the socket endpoint as a communication abstraction, and connection setup over TCP/IP. Those let *separate* processes talk; today we go *inside* one process and let its threads share memory directly — which is faster but far more dangerous.

---

## 🧵 Part 1 — How the OS actually runs a thread

### The Process Control Block (PCB)

The kernel represents every process as a **PCB**, and the scheduler keeps a data structure full of them. The PCB is the process's "save file" — everything needed to pause it and later resume it as if nothing happened.

```
┌──────────────────────────────┐
│     Process Control Block     │
├──────────────────────────────┤
│ Process state                │  ← ready / running / waiting …
│ Process number (PID)         │
│ Program counter              │  ← where to resume execution
│ Registers                    │  ← saved CPU register state
│ Memory limits                │
│ List of open files           │
└──────────────────────────────┘
```

Besides the CPU itself, the kernel also hands out **non-CPU resources** to processes — memory, I/O devices, and other shared resources (e.g., open-file handles).

### The context switch

A context switch is the act of multiplexing the single CPU across many processes. An interrupt or syscall forces control into the kernel, which **saves** the current state and **reloads** another's.

```
 Process P0          OS                       Process P1
 ──────────          ──                       ──────────
 executing │
           │  interrupt / syscall
           ▼
       ┌────────────────────────────┐
       │ save state    →  PCB_0      │
       │   · · · (overhead) · · ·    │
       │ reload state  ←  PCB_1      │
       └────────────────────────────┘
                                              ▼
                                              │ executing
           interrupt / syscall                ▼
       ┌────────────────────────────┐
       │ save state    →  PCB_1      │
       │   · · · (overhead) · · ·    │
       │ reload state  ←  PCB_0      │
       └────────────────────────────┘
 ▼
 │ executing
```

> [!WARNING]
> The shaded "overhead" in the middle is pure waste — no useful work happens while the OS shuffles state. Switch too often and you get **thrashing**: the machine spends its time switching instead of computing.

### Lifecycle of a process or thread

Five states, and the *transitions between them* are the whole story:

```
                          ┌──────────── dispatch ───────────┐
   ┌─────┐  admitted   ┌──┴────┐                       ┌─────────┐   exit    ┌────────────┐
   │ new │ ──────────▶ │ ready │                       │ running │ ────────▶ │ terminated │
   └─────┘             └───────┘ ◀── interrupt ──────── └─────────┘           └────────────┘
                          ▲          (preempted)            │
            I/O or event  │                                 │ wait for
            completion    │          ┌─────────┐            │ I/O or event
                          └───────── │ waiting │ ◀──────────┘
                                     └─────────┘
```

| State | Meaning |
|---|---|
| **new** | process is being created |
| **ready** | waiting for its turn on the CPU |
| **running** | instructions are executing right now |
| **waiting** | blocked on some event (e.g., a disk read / I/O) |
| **terminated** | finished execution |

### Shared vs. per-thread state

This is *the* picture to burn in, because it's the root of every bug in the second half of the lecture: threads in one process **share memory**.

```
            ADDRESS SPACE  (one process, two threads)
  ┌─────────────────────────────────────────────────────┐
  │  Code            ┐                                    │
  │  Global vars     ├──  SHARED by every thread          │
  │  Heap            ┘    (this is where races live)      │
  ├─────────────────────────────────────────────────────┤
  │  Thread 1: stack  ┐                                   │
  │  Thread 1: TCB    ├── PER-THREAD / private            │
  │  Thread 1: regs   ┘   (stack, saved registers, TCB,   │
  │  ─────────────────    thread metadata)                │
  │  Thread 2: stack  ┐                                   │
  │  Thread 2: TCB    ├── PER-THREAD / private            │
  │  Thread 2: regs   ┘                                   │
  └─────────────────────────────────────────────────────┘
```

| Shared state | Per-thread state |
|---|---|
| Heap | Thread Control Block (TCB) |
| Global variables | Stack (+ stack info) |
| Code | Saved registers |
| | Thread metadata |

### The dispatch loop — the core of concurrency

Conceptually, the OS scheduler is one infinite loop:

```c
Loop {
    RunThread();
    ChooseNextThread();
    SaveStateOfCPU(curTCB);
    LoadStateOfCPU(newTCB);
}
```

> [!QUESTION]
> **Should we ever exit this loop?**
> Essentially never. The loop ends only on shutdown, a power outage, or when the machine as a whole panics or halts. You could argue this loop *is* the operating system.

**Running a thread** means: load its saved state (registers, PC, stack pointer) into the CPU, load its environment (virtual memory space, etc.), then jump to the PC.

**Getting control back** happens two ways — and the rest of the lecture is basically these two columns:

| | Internal events | External events |
|---|---|---|
| **Who decides** | the thread itself | the hardware |
| **How** | thread yields / blocks voluntarily | thread is forced off |
| **Examples** | blocking on I/O, waiting on a signal, `yield()` | device interrupts, timer interrupt |

### Internal events — the thread gives up the CPU

- **Blocking on I/O** — requesting I/O *implicitly* yields the CPU (you can't do anything until the data arrives).
- **Waiting on a signal from another thread** — the thread asks to wait, which yields.
- **Calling `yield()`** — the thread volunteers to give up the CPU.

```c
ComputePI() {
    while (TRUE) {
        ComputeNextDigit();
        yield();              // voluntarily give up the CPU each round
    }
}
```

> [!NOTE]
> **Recall — POSIX API for threads: `pthreads`.** This is the standard interface (`pthread_create`, `pthread_join`, …) for spinning up and managing the kernel-visible threads we're describing here.

**The stack of a yielding thread.** `yield()` doesn't return the way a normal call does — it traps into the kernel, picks a new thread, and `switch()` swaps the CPU's registers out from under it:

```
   Stack of the yielding thread          (grows downward ▼)

   ┌──────────────────────────────┐  high addresses
   │ ComputePI()                  │   user code, was running
   ├──────────────────────────────┤
   │ yield()                      │   thread calls yield
   ├──────────────────────────────┤
   │ → trap to OS                 │   yield traps into the kernel
   ├──────────────────────────────┤
   │ kernel_yield()               │   kernel-side handler
   ├──────────────────────────────┤
   │ run_new_thread()             │   choose the next thread
   ├──────────────────────────────┤
   │ switch()                     │   save THESE regs, load the
   └──────────────────────────────┘   other thread's regs
              ▼                       low addresses
```

### `switch()` — the most dangerous function in the kernel

The TCB + stack together hold the **complete, restartable state** of a thread. `switch()` is what copies the live CPU registers into the old TCB and pulls the new TCB's registers into the CPU.

> [!WARNING]
> What if you implement `switch()` wrong — say you forget to save register 32? You don't crash. You get **intermittent failures**: the system silently produces wrong results, only when the schedule happens to expose the missing register. No warning, no clean stack trace. This class of bug is brutal to find precisely because it's correct *most* of the time.

**Is a thread switch still a context switch?** Yes — but it's far cheaper than a *process* switch, because you keep the same address space:

| Operation | Cost / interval |
|---|---|
| Context-switch interval (time slice) | 10–100 ms |
| Switching between **processes** | 3–4 µs |
| Switching between **threads** | ~100 ns |
| Switching threads via `yield()` (M:1, no kernel entry) | cheaper still |

**Threading models** explain that last row:

| Model | Mapping | Yield path | Speed | Example |
|---|---|---|---|---|
| **One-to-one** | 1 user thread : 1 kernel thread | through the kernel | normal | most modern OSes |
| **Many-to-one** | many user threads : 1 kernel thread | stays in user space | **super fast!** | green threads |

> [!TIP]
> In a many-to-one model, when a user thread calls `yield()` the *kernel thread* saves and restores registers entirely in user space — it never enters the kernel at all. That's why green threads are so cheap. (The catch — which the lecture leaves for later — is that one blocking syscall blocks *all* of them.)

### Processes vs. threads, side by side

| Dimension | Same process | Different process |
|---|---|---|
| **Switch overhead** | low | high |
| **Protection** | low (threads can stomp each other) | high (isolated address spaces) |
| **Sharing overhead** | low | medium (simultaneous core) → high (offloaded core) |
| **Parallelism** | yes | yes |

### Simultaneous Multithreading (SMT / hyperthreading)

A *hardware* scheduling trick, not an OS one. A core can already execute independent instructions in parallel; hyperthreading **duplicates the register state** to create a second hardware thread that fills the otherwise-idle execution slots.

```
   ONE physical core, presented as TWO logical CPUs
  ┌──────────────────────────────────────────────────────┐
  │  Duplicated:  [ reg state HT0 ]   [ reg state HT1 ]    │
  │  Shared:      execution units · ALUs · caches         │
  └──────────────────────────────────────────────────────┘
   The OS schedules each hardware thread as if it were a
   separate CPU.
```

---

## ⚡ Part 2 — External events: interrupts and the timer

> [!IMPORTANT]
> Internal events only work if threads *cooperate*. **What if a thread never does I/O, never waits, never yields** — just spins forever? Then the OS can never get control back through internal events. The answer is **external events**: the hardware forcibly yanks the CPU away.

- **Interrupts** — signals that stop the running code and jump into the kernel.
- **Timer** — an alarm clock that fires every few milliseconds, guaranteeing the kernel runs no matter what the thread does.

### The interrupt controller

```
   Devices            Interrupt Controller                 CPU
   ───────            ────────────────────                 ───
   Net   ─IRQ─▶   ┌────────────────────────────┐
   Disk  ─IRQ─▶   │ mask     enable/disable IRQs │
   Timer ─IRQ─▶   │ priority encoder: pick the   │ ─Int─▶ ┌───────────────────┐
   ...   ─IRQ─▶   │   highest enabled request    │ ─ID──▶ │ internal flag can │
                  │ software interrupts          │        │ disable all IRQs  │
                  └────────────────────────────┘         │ NMI: cannot be    │
                                                          │ masked            │
                                                          └───────────────────┘
```

- The **mask** enables/disables individual interrupts; a **priority encoder** picks the highest-priority enabled request; the interrupt's **identity** comes in on the ID line; **software interrupts** can be set/cleared by software.
- The CPU can disable *all* interrupts with an internal flag — *except* the **non-maskable interrupt (NMI)** line, which can never be disabled (it's for things like unrecoverable hardware errors).

<details>
<summary>Example: a network interrupt (filled in — flag this if your prof did it differently)</summary>

A packet arrives at the network card. The NIC raises its IRQ line; the interrupt controller forwards it; the CPU stops the current thread and jumps to the network interrupt handler. The handler copies the packet out of the device into a kernel buffer, possibly wakes whatever thread was blocked waiting on that socket (moving it `waiting → ready`), then returns so the interrupted thread can resume.

</details>

### Using the timer interrupt to return control

This is the clean solution to the "spinning thread" dispatcher problem — even a thread doing no I/O gets forced off:

```
   Stack of a thread that never yields       (grows downward ▼)

   ┌──────────────────────────────┐
   │ someRoutine()                │   busy thread: no I/O, no yield
   ├──────────────────────────────┤
   │ → timer interrupt            │   timer fires → forced into kernel
   ├──────────────────────────────┤
   │ run_new_thread()             │
   ├──────────────────────────────┤
   │ switch()                     │
   └──────────────────────────────┘
              ▼
```

**Compare** what happens when a thread *blocks on I/O* — same bottom of the stack (`run_new_thread → switch`), but it got there voluntarily through `read()`:

```
   Stack of a thread blocking on I/O          (grows downward ▼)

   ┌──────────────────────────────┐
   │ CopyFile()                   │   user code
   ├──────────────────────────────┤
   │ read()                       │   request a block of data
   ├──────────────────────────────┤
   │ → trap to OS                 │
   ├──────────────────────────────┤
   │ kernel_read()                │   start disk read, block this thread
   ├──────────────────────────────┤
   │ run_new_thread()             │
   ├──────────────────────────────┤
   │ switch()                     │
   └──────────────────────────────┘
              ▼
```

> [!TIP]
> Notice the symmetry: `yield()`, `read()`, and the timer interrupt all funnel into the same `run_new_thread → switch` machinery. The only difference is *what put you on the path* — a voluntary call vs. a forced interrupt. Once you see that, the whole scheduler clicks into place.

---

## 🏗️ Part 3 — Building a thread from scratch

How do you set up a brand-new TCB and stack so the very first `switch()` into it "just works"?

**Initialize the register fields of the TCB:**

```
   ┌──────────────┬───────────────────────────────────────┐
   │ SP           │ → top of the new stack                 │
   │ PC (return)  │ → ThreadRoot()   (an asm OS routine)    │
   │ a0           │ → fcnPtr     (the thread's function)    │
   │ a1           │ → fcnArgPtr  (its argument)             │
   └──────────────┴───────────────────────────────────────┘
   Stack DATA: not initialized — it'll fill in as the thread runs.
```

The **ThreadRoot stub** sets up the stack pointer, the ThreadRoot function pointer, and the function-argument pointers, so the first time the dispatcher loads this TCB, execution begins inside `ThreadRoot()`:

```c
ThreadRoot(fcnPtr, fcnArgPtr) {
    DoStartupHousekeeping();
    UserModeSwitch();           // drop from kernel into user mode
    call fcnPtr(fcnArgPtr);     // run the thread's actual function
    ThreadFinish();             // clean up after the function returns
}
```

> [!NOTE]
> `ThreadRoot()` is the common "wrapper" every thread starts and ends in. Startup housekeeping runs first; the real work is the `call fcnPtr(...)` in the middle; and when that returns, `ThreadFinish()` tears the thread down. The thread's function never has to know it's being wrapped.

---

## ⚔️ Part 4 — Correctness with concurrent threads

> [!IMPORTANT]
> Concurrency introduces **non-determinism**: the scheduler can run threads in *any order* and switch between them at *any time*. Your program must be correct for **every** possible interleaving — not just the one you tested.

| Kind of thread | Shared state? | Behavior |
|---|---|---|
| **Independent** | none | deterministic, reproducible |
| **Cooperating** | shared | non-deterministic — correctness must be *designed in* |

**Goal: correctness by design** — not by getting lucky with the scheduler.

### Motivating example: the ATM bank server

Suppose we build a server process to handle requests from an ATM network:

```c
BankServer() {
    while (TRUE) {
        ReceiveRequest(&op, &acctId, &amount);
        ProcessRequest(op, acctId, amount);
    }
}

ProcessRequest(op, acctId, amount) {
    if      (op == deposit)  Deposit(acctId, amount);
    else if (op == withdraw) Withdraw(acctId, amount);
    // ... other operations
}

Deposit(acctId, amount) {
    acct = GetAccount(acctId);    // may involve disk I/O
    acct->balance += amount;
    StoreAccount(acct);           // may involve disk I/O
}
```

**How could we speed this up?** Process more than one request at once:

- **event-driven** — overlap computation with I/O, or
- **multiple threads** — true parallelism on a multiprocessor, or overlap of computation and I/O.

…but the moment two threads touch the same account, we have a problem.

### Atomic operations

To reason about a concurrent program at all, you must know which underlying operations are **indivisible**.

> [!NOTE]
> An **atomic operation** always runs to completion or not at all. It can't be stopped in the middle, and no one else can observe or modify its state mid-flight. Atomic operations are the *fundamental building block* of synchronization — without at least one of them, threads would have no way to coordinate.

- On most machines, **loads and stores of words are atomic**.
- **Many instructions are not** — and that's the trap.

### The race condition, drawn out

That innocent-looking `acct->balance += amount` is **not** one atomic step. The compiler turns it into three:

```
       (1) load  r1 <- balance
       (2) add   r1 <- r1 + amount
       (3) store balance <- r1
```

Now let two deposits interleave on a starting balance of 100:

```
   Thread A (deposit 100)           Thread B (deposit 50)        balance
   ───────────────────────────────────────────────────────────────────
   load  r1 <- 100                                                 100
   add   r1 <- 200                                                 100
                                    load  r1 <- 100                100
                                    add   r1 <- 150                100
   store balance <- 200                                            200
                                    store balance <- 150           150  ✗
   ───────────────────────────────────────────────────────────────────
   Expected 250.  Got 150.  Thread A's entire deposit vanished.
```

> [!WARNING]
> This is a **lost update**, the canonical race condition. Nothing here is "buggy" in the single-threaded sense — each line is fine. The defect is that the read-modify-write spans three steps and the scheduler is free to wedge another thread into the gap. Test it once and it'll probably pass; ship it and it'll corrupt one balance in ten million.

### The fix: locks

> [!IMPORTANT]
> Wrap the **critical section** — reading the account balance and incrementing it — in a lock. A lock enforces **mutual exclusion**: at most one thread is inside the critical section at a time, which makes the read-modify-write *effectively* atomic even though the hardware instructions aren't.

```c
Deposit(acctId, amount) {
    acquire(lock);                  // ── enter critical section ──
    acct = GetAccount(acctId);
    acct->balance += amount;
    StoreAccount(acct);
    release(lock);                  // ── exit critical section ──
}
```

With the lock held, Thread B simply can't start its load until Thread A has released — so the two deposits serialize and the balance lands at the correct 250. *How* `acquire`/`release` are themselves built (atomic test-and-set, etc.) is the next problem to solve — and it's where **semaphores** enter the picture.

---

## Key takeaways

1. The kernel represents each process as a **PCB** and multiplexes the CPU by **context switching** — save state into one control block, reload from another, jump.
2. A process/thread moves through **new → ready → running → waiting → terminated**, and the transitions (dispatch, preempt, block, wake) are what matter.
3. Threads in one process **share code, globals, and heap** but keep **private stacks, registers, and TCBs** — the sharing is the source of every race.
4. The OS regains the CPU two ways: **internal events** (`yield`, blocking I/O, waiting) where the thread cooperates, and **external events** (device + timer interrupts) where the hardware forces the issue.
5. **Thread switches are ~100 ns vs. ~3–4 µs for process switches** because the address space stays put; many-to-one "green thread" yields avoid the kernel entirely.
6. The **timer interrupt** is what guarantees the dispatcher runs even against a thread that never yields.
7. `yield()`, `read()`, and the timer all converge on the same `run_new_thread → switch` path.
8. Concurrency is **non-deterministic**: correctness means being right for *every* interleaving, by design.
9. **Atomic** = runs fully or not at all. Word loads/stores are atomic; `balance += amount` (load-add-store) is **not** — hence the **lost-update** race.
10. A **lock** makes a critical section mutually exclusive, restoring effective atomicity to the read-modify-write.

---

## Things to revisit (with quick refreshers)

**Why is a thread switch ~30× cheaper than a process switch?**
Because the expensive part of a process switch isn't the registers — it's the *address space*. Switching processes means swapping page tables and flushing (or at least invalidating) TLB entries, so the next process starts cold and re-faults its translations back in. Threads in the same process share one address space, so `switch()` only has to swap the register set and stack pointer; the TLB and page tables stay valid. That's the whole reason threads exist as a cheaper unit than processes.

**What actually makes something "atomic," and why isn't `x += 1` atomic?**
Atomic means indivisible: it completes entirely or appears never to have started, with no observable in-between state. Hardware guarantees this for a single word load or store, but `x += 1` is *three* operations — load, add, store — and the scheduler can interrupt between any of them. So two threads can both load the old value, both add to it, and both store, with the second store clobbering the first. The lesson is that "one line of C" tells you nothing about atomicity; you have to think in terms of the underlying instructions.

**If a thread spins forever doing no I/O, how does the OS ever get control back?**
Through the **timer interrupt**, and only through it. Internal events (`yield`, blocking I/O, waiting) all depend on the thread *choosing* to relinquish the CPU, which a buggy or hostile infinite loop never will. The timer is a hardware alarm wired to fire every few milliseconds regardless of what the thread is doing; when it fires, the CPU jumps into the kernel, which can then `run_new_thread`. This is exactly why preemptive multitasking needs a hardware timer — without it, one runaway thread freezes the machine.

**Why does a bug in `switch()` cause *intermittent* failures instead of an obvious crash?**
Because `switch()` is only wrong on the *state it forgot to save*. If you fail to save register 32, the thread is corrupted only when (a) it had something live in register 32, and (b) the other thread overwrote it before control came back. Most schedules don't line those up, so the program runs correctly the vast majority of the time and then occasionally returns a silently wrong answer. There's no exception, no segfault — just bad data — which is why these are some of the hardest kernel bugs to track down.

**What's the real difference between blocking on I/O and calling `yield()`?**
Mechanically, almost none — both end up in `run_new_thread → switch`. The difference is intent and what happens to the thread's state afterward. `yield()` says "I'm still runnable, just let someone else go," so the thread goes back on the ready queue. Blocking on I/O says "I literally can't proceed until the disk/network responds," so the thread moves to the **waiting** state and isn't rescheduled until the I/O completion event wakes it. Same plumbing, different state transition in the lifecycle diagram.

**Why are green threads (many-to-one) so fast, and what's the catch?**
They're fast because a `yield()` between green threads never enters the kernel — the user-level runtime saves and restores registers itself, so there's no mode switch or trap. The catch is the flip side of the same coin: because the kernel only sees *one* thread, if any green thread makes a blocking system call, the kernel blocks the whole group, and none of the other green threads can run. That's why pure M:1 models pair badly with blocking I/O and why real systems often use hybrid (M:N) schemes.

---

## Other confusions worth revisiting later

- [ ] **Semaphores** — promised in the title but not reached; the general counting primitive that generalizes locks.
- [ ] **How are `acquire`/`release` actually implemented?** Test-and-set, compare-and-swap, and other atomic read-modify-write hardware instructions.
- [ ] **Condition variables and monitors** — coordinating *when* a thread should proceed, not just mutual exclusion.
- [ ] **Deadlock** — what happens when locks are acquired in the wrong order; detection vs. avoidance vs. prevention.
- [ ] **Memory consistency / instruction reordering** — modern CPUs reorder loads and stores, so "word loads/stores are atomic" isn't the whole story for correctness.
- [ ] **Interrupt context vs. thread context** — what you're allowed to do inside an interrupt handler (and why you can't block there).

---

🔗 **Links:** [[ipc-pipes-and-sockets]] · [[threads-and-pthreads]] · [[locks-and-semaphores]] · [[cpu-scheduling]]