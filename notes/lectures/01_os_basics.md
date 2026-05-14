# Lecture 01 — What is an Operating System?

> [!NOTE]
> **Big idea:** An OS is the layer between hardware and applications.
> It plays three roles: **illusionist** (abstracts hardware),
> **referee** (mediates shared resources), and **glue** (provides
> common services).

---

## What an OS does

| Role | What it provides |
|---|---|
| Memory management | Virtual address spaces, allocation |
| I/O management | Drivers, buffering, abstraction |
| CPU scheduling | Time-sharing across processes |
| Multitasking | Illusion of concurrent execution |
| Protection | Isolation between processes |
| Security | Authentication and authorization |

There's no universally accepted definition. One useful framing: *the
one program running at all times on the computer is the kernel.*
Everything else is a system program or application.

### A working definition

A special layer of software that gives applications access to hardware
resources, by providing:

- Convenient abstractions over complex hardware
- Protected access to shared resources
- Security and authentication
- Communication primitives

---

## What makes something a "system"?

Multiple interrelated parts, each potentially interacting with the
others. If every part can touch every other part, complexity grows
as $O(N^2)$. Managing that interaction blowup is a recurring theme
of the course.

> [!IMPORTANT]
> Robustness requires an engineering mindset:
> meticulous error handling, defending against careless or malicious
> users, and treating the computer as a concrete machine with real
> limitations and failure modes.

---

## The hardware/software stack

The OS sits on top of:

- Processors and caches
- Memory (DRAM, OS memory)
- Page tables and TLB
- Storage
- Input devices and displays
- Buses and controllers

> [!TIP]
> A well-designed memory hierarchy makes the system *feel* as fast as
> the smallest, fastest layer (registers) and as large as the largest
> (disk). Caches make this work.

---

## 🎭 The OS as illusionist

The OS hides hardware ugliness behind clean abstractions.

| Hardware reality | OS abstraction |
|---|---|
| Registers, CPU cores | **Threads** |
| DRAM | **Address spaces** |
| Disk blocks | **Files** |
| Ethernet cards | **Sockets**, routing |

The application thinks it has:
- Infinite memory
- A dedicated machine
- High-level objects (files, users, messages) instead of bits and blocks

---

## The process abstraction

A **process** is the execution environment with restricted rights that
the OS provides. It's the "virtual machine" a program runs inside.

```
┌─────────────────────────────────┐
│       Application program       │
├─────────────────────────────────┤
│   System libraries (libc, SSL)  │
├─────────────────────────────────┤
│      Process abstraction        │   ← provided by OS
├─────────────────────────────────┤
│        Operating System         │
├─────────────────────────────────┤
│           Hardware              │
└─────────────────────────────────┘
```

> [!NOTE]
> **The application's machine IS the process abstraction provided by
> the OS.** Programs don't run on hardware — they run on the OS's
> model of it.

### What a process consists of

- An **address space** (chunk of memory)
- One or more **threads of control** executing in that address space
- **Additional system state**:
  - Open files
  - Open sockets
  - Other OS-managed resources

Each process is a totally independent environment. Task Manager on
Windows or `ps` on Unix shows currently running processes.

### When does a program become a process?

When its binary is **loaded into memory**, given a **process structure**,
and **scheduled** to run.

> [!TIP]
> A program is the executable on disk. A process is that program
> *while it's executing*. Two processes can run the same program in
> different states — e.g., two users editing in `vim` simultaneously.

<details>
<summary><b>Q: Is the Docker daemon part of the process running on the VM?</b></summary>

Yes. Docker wraps multiple environments and runs them inside process
abstractions. Containers share the host kernel but get isolated
process, filesystem, and network views — lighter than full VMs but
also less isolated.

</details>

---

## ⚖️ The OS as referee

The OS manages execution, memory, and resource allocation. It mediates
communication and prevents processes from interfering with each other.

### Multitasking on a single core

With one core, P1 and P2 only *appear* to run simultaneously.

```
        P1 RUNNING                      P2 WAITING
            │                               │
            │                               │
            ▼                               │
   ┌──────────────────┐                     │
   │  CONTEXT SWITCH  │                     │
   │  • save P1 regs  │ ──→ P1 descriptor   │
   │  • load P2 regs  │ ←── P2 descriptor   │
   └──────────────────┘                     │
            │                               │
            │                               ▼
        P1 WAITING                      P2 RUNNING
```

**Steps in a context switch:**

1. P1 is running with registers loaded
2. OS triggers a switch (timer interrupt, syscall, etc.)
3. P1's registers are saved to P1's process descriptor block
4. P2's registers are loaded from P2's descriptor block
5. P2 now runs until the next switch

The switching is fast enough that both processes feel continuously alive.

> [!QUESTION]
> **"P1 has 90% of the CPU" — what does this mean?**
> Over some time window, P1 ran 90% of the time, P2 ran 10%.
> Not literally simultaneous — it's a fine-grained time-share.

### Protection

If P1 tries to access P2's memory:

```
P1 ──→ [forbidden address] ──→ MMU detects violation
                                     │
                                     ▼
                              Page fault → OS handler
                                     │
                                     ▼
                              Segmentation fault
                                     │
                                     ▼
                              P1 terminated
```

> [!WARNING]
> Protection is enforced by **hardware** (MMU, privilege levels)
> cooperating with the OS. Software alone can't do it — a buggy or
> malicious program could lie.

---

## 🧩 The OS as glue

The OS provides common services applications would otherwise have to
reimplement:

- File system / storage abstractions
- Windowing and input handling (mouse, keyboard)
- Networking (TCP, sockets)
- Authentication and sharing
- Display services
- Power management

These services are typically exposed through libraries linked into
applications.

<details>
<summary><b>Q: Is the windowing system part of the OS?</b></summary>

Depends on the OS. Windows originally kept it in userspace, then moved
it into the kernel for performance. Linux keeps it in userspace
(X11, Wayland). Tradeoff: in-kernel is faster, out-of-kernel is safer
and more modular.

</details>

<details>
<summary><b>Smallest OS?</b></summary>

TinyOS is among the smallest — used for embedded sensor networks.

</details>

---

## Why take CS 162?

| Audience | Why it matters |
|---|---|
| Future OS designers | You'll build the thing |
| Systems builders | DBs and distributed systems use OS primitives |
| Application developers | You build on top — understand what you're standing on |

> [!NOTE]
> **My reason:** databases and distributed systems both depend on OS
> primitives — page cache, concurrency, file I/O, networking.

---

## Syllabus overview

| Unit | Topics |
|---|---|
| **OS concepts** | Processes, I/O, networks, virtual machines |
| **Concurrency** | Threads, scheduling, locks, deadlock, scalability, fairness |
| **Address spaces** | Virtual memory, address translation, protection, sharing |
| **File systems** | I/O devices, files, storage, naming, caching, paging, transactions |
| **Distributed systems** | Protocols, N-tier, RPC, NFS, DHTs, consistency, scalability |
| **Reliability & security** | End-to-end |

---

## Coursework structure

### Homeworks

| HW | Topic |
|---|---|
| 0 | Tools, environment, autograding, C refresher |
| 1 | Lists in C (Pintos list abstraction) |
| 2 | Basic threads |
| 3 | Build your own shell |
| 4 | Sockets + threads (HTTP server) |
| 5 | Memory mapping and management (malloc) |
| 6 | `sbrk` and the heap |

### Group projects

| Project | Topic |
|---|---|
| 0 | Pregame — getting started |
| 1 | User programs |
| 2 | Threads and scheduling |
| 3 | File systems |

### Prerequisites

Projects assume comfort with:
- C programming and debugging
- Pointers
- Memory management (`malloc`, `free`, stack vs. heap)
- GDB

---

## Trends shaping operating systems

### Moore's Law

For decades: 2× transistors per chip every ~1.5 years.

> [!WARNING]
> **Officially ended around February 2016.** Vendors moved to 3D
> stacked chips and more layers in older process nodes.

### Joy's Law slowdown

Until ~2000, more transistors → faster single-threaded performance.
After that: power density and heat killed clock scaling. The industry
shifted to **multi-core** to keep delivering gains. This is *the*
reason concurrency matters so much in modern OS design.

### Other trends

- Storage capacity still growing
- Network capacity still growing
- IoT explosion: fridges, watches, cars, thermostats all connected
- More devices + more storage + more people → more OS demand

---

## Complexity is the real challenge

Software can't be tested in all possible environments and
configurations. The question isn't whether bugs exist — it's how
serious they are.

### Software size over time

| System | Lines of code |
|---|---|
| Linux 2.2.0 | ~1M |
| Space Shuttle | ~2M |
| Firefox | ~10M |
| Windows 7 | ~40M |
| Facebook codebase | ~60M |
| Mac OS X Tiger | ~80M |
| Modern car | ~100M |
| *(Mouse DNA, for scale)* | *~120M base pairs* |

---

## Case study: Mars Pathfinder

**Hardware:** 20 MHz CPU, 128 MB DRAM, VxWorks OS, camera + scientific
instruments.

**Constraints:**
- Can't easily hit reset
- Must reboot itself if needed
- Must receive commands from Earth

**Requirements:**
- Programs must not interfere with each other
  - If the science task crashes, antenna positioning **must** survive
- All software may crash occasionally → **automatic restart is critical**

> [!IMPORTANT]
> This is a clean illustration of why isolation and protection matter.
> In a system where you can't physically intervene, the OS has to
> keep working even when individual programs fail.

---

## Key takeaways

1. An OS is **illusionist, referee, and glue**
2. Core abstractions: **process, thread, address space, file, socket**
3. Multitasking on one core works via **context switches** that
   save/restore state from process descriptors
4. **Protection** is enforced by hardware (MMU) + OS cooperation
5. The hard problem is **complexity** at system scale

---

## Confusions to revisit

- [ ] Exact mechanics of a context switch — what's saved, what's not
- [ ] Why windowing belongs in-kernel vs userspace (and the tradeoff)
- [ ] How the MMU and page tables actually enforce protection
      *(→ Memory lectures will cover this)*

---

## Vocab to add to `os-vocab.md`

- Kernel
- Process
- Thread
- Address space
- Context switch
- System call
- Page fault / segmentation fault
- ISA (Instruction Set Architecture)
- MMU (Memory Management Unit)

---

🔗 **Links:** [[02-four-concepts]] · [[os-vocab]]