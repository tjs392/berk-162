# Lecture 02 — Four Fundamental OS Concepts

> [!NOTE]
> **Big idea:** Every OS is built on four core abstractions —
> **threads**, **address spaces**, **processes**, and **dual-mode
> operation**. Everything else in the course extends these.

---

## Recap: what is an OS?

| Role | What it does |
|---|---|
| 🎭 **Illusionist** | Gives the program a clean view of resources |
| ⚖️ **Referee** | Protection, isolation, sharing |
| 🧩 **Glue** | Common services (windowing, file systems, …) |

Concretely, P2 must not be able to:
- Read or modify P1's state
- Modify the OS in unauthorized ways
- Modify storage outside its permissions

---

## The complexity problem

Hardware is messy. The OS exists to tame that complexity so applications
can be written sanely. But complexity leaks into the OS itself if it's
not carefully designed.

> [!WARNING]
> **Where complexity bites:**
> - **Third-party device drivers** — the most unreliable part of most
>   OSes. Written by non-stakeholders, against the most complex hardware,
>   running in supervisor mode.
> - **Security holes** — Meltdown (2017), Heartbleed (SSL)
> - **Version skew** between OS components
> - **Data breaches, DDoS, timing channels**

<details>
<summary><b>Why are device drivers so vulnerable?</b></summary>

They touch the most complicated hardware and try to put a clean
interface over it. They run in supervisor mode, so a bug can crash
or compromise the whole system.

</details>

---

## How the OS tames complexity: abstraction layering

```
┌──────────────────────────────────┐
│         Application              │
├──────────────────────────────────┤  ← Abstract machine interface
│         Operating System         │
├──────────────────────────────────┤  ← Physical machine interface
│           Hardware               │
└──────────────────────────────────┘
```

The OS sits between the physical machine and a much nicer abstract
machine it presents upward.

| Physical | Abstraction |
|---|---|
| Processor | **Thread** |
| Memory | **Address space** |
| Disks, SSDs | **Files** |
| Networks | **Sockets** |
| Machines | **Processes** |

> [!TIP]
> **For each OS area, ask two questions:**
> 1. What hardware interface do I have to handle? *(physical reality)*
> 2. What software interface do I want to provide? *(nice abstraction)*

<details>
<summary><b>What is the BIOS?</b></summary>

A set of standardized services on top of hardware. Part of it is a
legacy from the IBM PC era. Modern systems mostly use UEFI but the
idea is the same: bridge from raw hardware to OS bootup.

</details>

---

# The Four Concepts

| # | Concept | One-line description |
|---|---|---|
| 1 | **Thread** | Execution context — what's needed to run code |
| 2 | **Address space** | The set of memory addresses a program can touch |
| 3 | **Process** | Protected address space + one or more threads |
| 4 | **Dual-mode operation** | Hardware-enforced split between kernel and user privileges |

---

## 1️⃣ Thread — execution context

A **thread** fully describes a single program's execution state:

- **Program counter (PC)** — what instruction is next
- **Registers** — current working values
- **Execution flags** — comparison results, status bits
- **Stack** — local variables, call frames
- **(Plus the memory it accesses)**

Think of it as a *virtualized simple processor*.

> [!NOTE]
> A thread is **resident** when its state is loaded into a real
> processor — PC pointing at its next instruction, registers holding
> its values, stack pointer pointing into its stack.
>
> A thread is **suspended** when its state is saved to memory and the
> processor is doing something else.

### What an execution flag is

A small set of 1-bit registers that record the result of the most
recent operation: was the result zero, negative, did it overflow,
etc. Branch instructions (`jz`, `jnz`, `jl`) make decisions on these
flags. They're saved/restored during context switches.

### Execution sequence

```
┌─────────────────┐
│  Fetch @ PC     │
└────────┬────────┘
         ▼
┌─────────────────┐
│     Decode      │
└────────┬────────┘
         ▼
┌─────────────────┐
│ Execute on regs │
└────────┬────────┘
         ▼
┌─────────────────┐
│ Write results   │
└────────┬────────┘
         ▼
┌─────────────────┐
│ PC = next PC    │
└────────┬────────┘
         └─── repeat
```

> [!IMPORTANT]
> Take this loop for granted as the hardware's job — the OS is what
> sits **on top** and manages many of these loops at once.

This course uses **x86** (complex, memory-memory, segmented) rather
than RISC-V.

---

## The illusion of multiple processors

Multiple threads share one physical core via **multiplexing**: the OS
rapidly swaps which thread is resident.

```
   vCPU1   vCPU2   vCPU3
     │       │       │
     └───────┼───────┘
             ▼
      ┌─────────────┐
      │ Real core 0 │   ← only one runs at a time
      └─────────────┘
             │
             ▼
      ┌─────────────┐
      │   Memory    │   ← all threads share it
      └─────────────┘
```

### The Thread Control Block (TCB)

A thread lives in one of two places:

| Location | When |
|---|---|
| **On the real core** | While executing |
| **In a TCB in memory** | While suspended |

The TCB stores the thread's PC, SP, registers, and execution flags
while it's not running.

### A context switch, step by step

```
P1 RUNNING                                   P2 WAITING
    │                                            │
    │    1. OS runs (timer, syscall, etc.)       │
    │                                            │
    │    2. Save P1's PC, SP, regs → TCB1        │
    │                                            │
    │    3. Load P2's PC, SP, regs ← TCB2        │
    │                                            │
    │    4. Jump to P2's PC                      │
    │                                            ▼
P1 WAITING                                   P2 RUNNING
```

> [!QUESTION]
> **How long does a context switch take?**
> A few microseconds. If switches happen too often → "thrashing,"
> where you spend most of your time switching instead of working.

<details>
<summary><b>Does each thread get its own cache?</b></summary>

Generally no — there's one cache per core, shared across whichever
thread happens to be running. Switching too fast destroys cache
locality (nobody gets to warm the cache before being kicked off).

Primitive processors flush the TLB on every switch; modern ones tag
TLB entries with an address-space ID so they survive. Caches are
typically physically addressed, so they don't need flushing — you
just switch the page table pointer.

</details>

<details>
<summary><b>Why not run each process to completion before switching?</b></summary>

Efficient (no switch overhead) but terrible for responsiveness. A
long-running task would freeze everything else. Time-sharing is the
tradeoff: a little overhead for a lot of perceived fluidity.

</details>

### What triggers a context switch?

- **Timer interrupt** (preemption)
- **Voluntary yield** (`yield()`)
- **I/O blocking** (thread waits for disk/network)
- Various exceptions and syscalls

---

## Multiprogramming: many threads of control

Each thread has its own:
- Stack
- Registers (in its TCB when not running)
- PC

Threads share:
- Code segment
- Static data
- Heap

> [!TIP]
> **For Pintos:** TCBs are stored in the kernel. Read `thread.h` and
> `thread.c` in the Pintos source — that's where Project 1 lives.

---

## 2️⃣ Address space

The set of accessible memory addresses, plus the state associated with
them.

| Architecture | Address space size |
|---|---|
| 32-bit | $2^{32}$ ≈ 4 billion addresses |
| 64-bit | $2^{64}$ ≈ 18 quintillion addresses |

### What can happen when you read or write an address?

- Acts like normal memory (regular load/store)
- Ignored (write to read-only)
- Triggers an I/O operation (memory-mapped I/O, `mmap`)
- Causes an exception (segfault, page fault)
- Communicates with another program (shared memory)

### Anatomy of a typical address space

```
High addresses
   ┌──────────────────┐
   │      Stack       │  ← grows down; local vars, call frames
   ├──────────────────┤
   │        ↓         │
   │     (unused)     │
   │        ↑         │
   ├──────────────────┤
   │      Heap        │  ← grows up; malloc'd memory
   ├──────────────────┤
   │   Static data    │  ← globals, constants
   ├──────────────────┤
   │      Code        │  ← the program's instructions
   └──────────────────┘
Low addresses
```

| Segment | Contents | How allocated |
|---|---|---|
| **Code** | Compiled instructions | Loaded from executable |
| **Static** | Globals, constants | Loaded from executable |
| **Heap** | `malloc`/`free` memory | Grows on demand via `sbrk`/`mmap` |
| **Stack** | Local vars, call frames | Grows on function calls |

---

## The protection problem

Earlier multiprogramming (very early computing, embedded systems,
MacOS 1–9, Windows 95–ME) had all threads sharing one address space.

> [!WARNING]
> **Why this is unsafe:**
> - Any thread can read/write any memory, including other threads'
> - Any thread can corrupt the OS
> - A bug in one program crashes everything

**The OS must protect:**
- Itself from user programs (reliability, security, privacy, fairness)
- User programs from each other

---

## Hardware mechanisms for protection

### Base and bound (simplest)

Two registers per thread say "you can only touch addresses in this
range."

```
Memory layout              Hardware check
                           
┌──────────┐  ← Bound      every access:
│  Other   │               
│ programs │               if (addr >= Base
├──────────┤  ← Base           && addr < Bound)
│   This   │                  → allow
│  thread  │               else
└──────────┘                  → fault
```

> [!IMPORTANT]
> The check is **`Base ≤ addr < Bound`** (bound is exclusive).

**Tradeoffs:**
- ✅ Simple, no page tables
- ❌ Allocation size hard to change (everything has to move)
- ❌ Requires a relocating loader at program load time

### Variation: hardware adder

Instead of compile-time relocation, the hardware adds `Base` to every
address on the fly. The program acts like it starts at address 0;
hardware translates transparently.

- The program counter operates as if memory starts at 0
- Hardware adds `Base` to produce the real physical address
- Can the program touch the OS? **No** — the bound check prevents it

### x86 segments

x86 has dedicated registers (code segment, stack segment, data
segment) that encode base + length. The instruction pointer is an
offset into the code segment. Hardware checks every access.

---

## Address space translation (the modern approach)

The program operates in an address space that's **distinct from**
physical memory. The hardware translates between them.

### Paged virtual memory

Break the virtual address space into equal-size **pages**. Each page
maps to some **frame** in physical memory via a **page table**.

```
Virtual                    Page Table             Physical
address space              (per process)          memory
                           
┌──────┐ Page 0  ──────►  [ Frame 7 ]  ──────►  ┌──────┐
├──────┤ Page 1  ──────►  [ Frame 2 ]  ──────►  │ ...  │
├──────┤ Page 2  ──────►  [ Frame 9 ]  ──────►  │      │
├──────┤ Page 3  ──────►  [ on disk  ]          │      │
└──────┘                                        └──────┘
```

> [!TIP]
> Any virtual page can live in any physical frame. The translation is
> transparent to the program. This is what makes modern memory
> management — sharing, paging, copy-on-write — possible.

---

## 3️⃣ Process

A **process** = protected address space + one or more threads + the
OS state to support them.

### What a process owns

- **Address space** (its private memory)
- **Threads** (one or more, sharing the address space)
- **File descriptors**
- **File system context** (working directory, permissions)
- **Sockets and other resources**

### Why processes exist

| Goal | Why it matters |
|---|---|
| **Reliability** | One process crashing doesn't kill others |
| **Security** | Processes can't read/write each other's memory |
| **Privacy** | One user's data isolated from another's |
| **Fairness** | OS can schedule resources across processes |

### The fundamental tradeoff

> [!IMPORTANT]
> **Protection vs. efficiency:**
> - Communication is **easier within** a process (threads share memory)
> - Communication is **harder between** processes (need IPC, syscalls)
>
> Multi-threading buys you concurrency cheaply; multi-process buys you
> isolation expensively.

### Single vs. multi-threaded processes

| Shared across threads | Not shared |
|---|---|
| Code | Registers |
| Data (globals, heap) | Stack |
| Files, sockets | PC |

**Why multiple threads per process?**
- **Parallelism** (multiple cores running threads simultaneously)
- **Concurrency** (handling I/O while other work continues)

---

## 4️⃣ Dual-mode operation

Address translation alone isn't enough — what stops a process from
just changing its own page table pointer to escape its sandbox?

> [!IMPORTANT]
> **Hardware must enforce privilege levels.**

### Two modes

| Mode | Who runs here | Capabilities |
|---|---|---|
| **Kernel mode** | The OS | Everything — change page tables, talk to hardware, disable interrupts |
| **User mode** | Applications | Restricted — no direct hardware access, no kernel memory |

### Operations forbidden in user mode

- Changing the page table pointer
- Disabling interrupts
- Direct hardware access
- Writing to kernel memory

### Controlled transitions

The only way from user mode to kernel mode is through
**carefully-controlled gates**:

```
USER MODE                          KERNEL MODE
─────────                          ───────────

App calls read()                   
       │                           
       ▼                           
  syscall instruction  ──────────►  syscall handler runs
                                          │
                                          ▼
                                    do the work
                                          │
                                          ▼
  resume in user mode  ◄──────────  return to user
       │                           
       ▼                           
  app continues                    
```

### Three types of user → kernel transitions

| Trigger | Cause | Example |
|---|---|---|
| **Syscall** | Program asks the kernel to do something | `read()`, `write()`, `fork()` |
| **Interrupt** | Hardware demands attention | Timer tick, disk I/O complete, keyboard press |
| **Trap / Exception** | Program did something illegal | Page fault, divide by zero, invalid instruction |

### Interrupt vector

A table indexed by interrupt number. Each entry holds the address and
properties of that interrupt's handler. When an interrupt fires, the
hardware jumps to the corresponding handler in kernel mode.

---

## Running many programs

Mechanisms cover *how* — now we need policy for *which* and *when*.

### The Process Control Block (PCB)

The kernel represents each process as a **PCB** containing:
- Address space info (page table pointer)
- Open file table
- TCBs for each of its threads
- Scheduling state (priority, runtime stats)
- Parent/child relationships

### The scheduler

> [!NOTE]
> **The scheduler arguably IS the operating system.**
>
> On every timer tick (or other scheduling event), the scheduler picks
> which process/thread runs next: unload current, reload selected,
> repeat.

Scheduling policy is a huge topic — covered in lectures 11–13.

---

## Key takeaways

1. **Hardware is complex; the OS provides clean abstractions** mapped
   to physical resources
2. **Four core concepts:** thread, address space, process, dual-mode
3. **Threads** are execution contexts; multiplexed via TCBs and
   context switches
4. **Address spaces** isolate processes; modern systems use paged
   virtual memory
5. **Processes** combine address space + threads + OS resources, with
   protection enforced by hardware
6. **Dual-mode operation** uses hardware privilege levels; user code
   can only reach the kernel via syscalls, interrupts, or traps

---

## Confusions to revisit

- [ ] Exactly how the page table pointer switch works during a
      context switch
- [ ] How TLB invalidation interacts with context switches (ASIDs)
- [ ] Mechanics of `sbrk` / heap growth — covered in HW 6
- [ ] How the interrupt vector is set up at boot
- [ ] How syscalls actually transition modes (covered in lecture 6)

---

## Vocab to add to `os-vocab.md`

- TCB (Thread Control Block)
- PCB (Process Control Block)
- Multiplexing
- Resident vs. suspended thread
- Execution flags
- Base and bound
- Segment (x86)
- Page, frame, page table
- Virtual address vs. physical address
- Kernel mode / user mode
- Syscall, interrupt, trap, exception
- Interrupt vector
- Scheduler
- Thrashing
- ASID (Address Space Identifier)

---

🔗 **Links:** [[01-what-is-an-os]] · [[03-threads-and-processes]] · [[os-vocab]]