# Lecture 04 — Files and I/O

> [!NOTE]
> **Big idea:** Unix's defining design choice is *"everything is a
> file"* — disks, devices, networks, and pipes all share one interface
> (`open`/`read`/`write`/`close`). This lecture covers that interface
> at two levels (high-level C streams vs. low-level syscalls), how
> buffering works at both the user and kernel level, and what happens
> to file descriptors across `fork`.

---

## Recap: synchronization between threads

| Concept | Meaning |
|---|---|
| Mutual exclusion | Only one thread does a thing at a time |
| Critical section | Code only one thread runs at once |
| Lock | Object only one thread holds at a time |
| Semaphore | Generalized lock (see below) |

---

## Semaphores

A **semaphore** is a generalized lock. It holds a **non-negative
value** and supports two atomic operations:

| Operation | Aliases | Effect |
|---|---|---|
| `P()` | `down()`, `wait()` | Wait until value > 0, then decrement by 1 |
| `V()` | `up()`, `signal()` | Increment by 1; wake a waiting `P()` if any |

### Two classic semaphore patterns

**Pattern 1 — Mutual exclusion** (initial value = 1)

```c
semaphore = 1;

semaphore.down();
    // critical section
semaphore.up();
```

**Pattern 2 — Signaling / ThreadJoin** (initial value = 0)

```c
semaphore = 0;

ThreadJoin() {            ThreadFinish() {
    semaphore.down();         semaphore.up();
}                         }
```

> [!TIP]
> The initial value tells you the pattern: **1 = mutual exclusion**
> (one permit to hand out), **0 = signaling** (wait for an event that
> hasn't happened yet).

---

## Starting a new program (in a shell)

The typical pattern: **`fork`, parent waits, child runs the program.**

```
        Shell process
              │
              │ fork()
       ┌──────┴───────┐
       │              │
   ┌───────┐      ┌────────┐
   │ Parent│      │ Child  │
   │ wait()│      │ runs   │
   │       │      │ command│
   │  ...  │◄─────┤ exit() │
   │ resume│      └────────┘
   └───────┘
```

When you type a command into a shell, it forks a child process to run
that command, and the shell waits for it to finish.

```c
int status;
pid_t cpid, tcpid;

cpid = fork();
if (cpid > 0) {                          // parent
    mypid = getpid();
    printf("[%d] parent of [%d]\n", mypid, cpid);
    tcpid = wait(&status);               // block until child exits
    printf("bye\n");
} else if (cpid == 0) {                  // child
    mypid = getpid();
    printf("[%d] child\n", mypid);
    exit(42);
}
```

`wait(&status)` blocks the parent until a child terminates, and writes
the child's exit info into `status`.

---

## Signals

### Example: `inf_loop.c`

```c
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
    while (1) {}                          // looks infinite...
}
```

This *looks* like an infinite loop, but sending it a **SIGINT**
(Ctrl-C) diverts execution into `signal_callback_handler`, which exits.

> [!QUESTION]
> **What if a process receives SIGINT but has registered no handler?**
> The default action runs — for SIGINT, that's **killing the process**.

You can install your own handlers for most signals via `sigaction`.

### Common POSIX signals

| Signal | Triggered by | Default action |
|---|---|---|
| `SIGINT` | Ctrl-C | Terminate |
| `SIGTERM` | `kill` (shell default) | Terminate |
| `SIGTSTP` | Ctrl-Z | Stop (suspend) |
| `SIGKILL` | `kill -9` | Terminate — **cannot be caught** |
| `SIGSTOP` | — | Stop — **cannot be caught** |

> [!NOTE]
> **POSIX** = Portable Operating System Interface for uniX.

---

## The Unix idea: "Everything is a File"

One identical interface for:

- Regular files on disk
- Devices (terminals, printers, …)
- Networking (sockets)
- Local interprocess communication (pipes, sockets)

Built on four system calls: **`open()`, `read()`, `write()`,
`close()`** — plus **`ioctl()`** ("I/O control") for device-specific
configuration that doesn't fit the uniform model.

> [!IMPORTANT]
> "Everything is a file" was a **radical idea** when proposed. It
> means a program that reads from a file can, unchanged, read from a
> network socket or a pipe.

---

## The file system abstraction

### File

- A named collection of data in a file system
- **POSIX file data:** a sequence of bytes
- **File metadata:** information *about* the file — size,
  modification time, owner, security info, access control

### Directory

- A folder containing files and other directories
- **Hierarchical naming** — a path through the directory graph
- Supports links and volumes

---

## Processes, file systems, and users

Every process has a **current working directory (CWD)**.

| Path type | Example | Resolved against |
|---|---|---|
| **Absolute** | `/home/oski/cs162` | Root — ignores CWD |
| **Relative** | `index.html`, `./index.html` | The CWD |

Change the CWD with the syscall:

```c
int chdir(const char *path);
```

---

## High-level file API: C streams

The C library operates on **streams** — unformatted sequences of bytes
(text or binary).

```c
FILE *fopen(const char *filename, const char *mode);
int   fclose(FILE *fp);
```

`fopen` returns a pointer to a `FILE` struct; that struct is the
handle you use for all subsequent operations.

### Standard streams

`#include <stdio.h>` provides everything for I/O. Three streams are
opened implicitly when a program starts:

| Stream | Purpose |
|---|---|
| `FILE *stdin` | Standard input |
| `FILE *stdout` | Standard output |
| `FILE *stderr` | Standard error |

> [!QUESTION]
> **What if you open a file but never close it before the process
> ends?** It typically gets flushed and cleaned up automatically when
> the process exits — but relying on this is bad practice.

### Piping between processes via stdin/stdout

If you connect one process's `stdout` to another's `stdin`, the two
can communicate. That's what shell pipes do:

```
cat hello.txt | grep "World!"

  cat's stdout ──────────► grep's stdin
```

### Character-by-character copy

```c
int main(void) {
    FILE *input  = fopen("input.txt", "r");
    FILE *output = fopen("output.txt", "w");
    int c;

    c = fgetc(input);
    while (c != EOF) {
        fputc(c, output);
        c = fgetc(input);
    }
    fclose(input);
    fclose(output);
}
```

> [!TIP]
> **Why is `c` an `int` and not a `char`?** ASCII characters are 8
> bits, but `EOF` is **not** an 8-bit value — it's a distinct sentinel
> (typically `-1`). If `c` were a `char`, you couldn't distinguish
> `EOF` from a legitimate byte.

### Block-by-block copy

```c
int main(void) {
    FILE *input  = fopen("input.txt", "r");
    FILE *output = fopen("output.txt", "w");
    char buffer[BUFFER_SIZE];
    size_t length;

    length = fread(buffer, sizeof(char), BUFFER_SIZE, input);
    while (length > 0) {
        fwrite(buffer, sizeof(char), length, output);
        length = fread(buffer, sizeof(char), BUFFER_SIZE, input);
    }
}
```

> [!WARNING]
> **System programming discipline: always check return values.**
> ```c
> FILE *input = fopen("input.txt", "r");
> if (input == NULL) {
>     perror("Failed to open input file");
> }
> ```
> Files fail to open constantly (missing, no permission, …). Code that
> ignores this crashes mysteriously later.

### Positioning the stream pointer

```c
int fseek(FILE *stream, long int offset, int whence);
```

The `whence` argument (constants in `stdio.h`) interprets the offset:

| `whence` | Offset measured from |
|---|---|
| `SEEK_SET` | The start of the file |
| `SEEK_CUR` | The current position |
| `SEEK_END` | The end of the file |

---

## Low-level I/O: the raw syscall interface

### Key Unix I/O design concepts

| Concept | What it means |
|---|---|
| **Uniformity** | Files, devices, IPC all use `open`/`read`/`write`/`close` |
| **Open before use** | Creates a chance for access control; sets up machinery |
| **Byte-oriented** | Addressing is in bytes, even if blocks are transferred |
| **Kernel-buffered reads** | Stream and block devices look the same; reads can yield the CPU |
| **Kernel-buffered writes** | Outgoing transfer completion is decoupled from the app |

> [!NOTE]
> Buffering inside reads/writes exists for **performance** and to
> **match block-structured devices to the byte-oriented user view**.

### The raw syscalls

```c
int open(const char *filename,
         int flags          /* access modes, open flags, modes */
         [, mode_t mode]);   /* permission bits: User|Group|Other × R|W|X */

int creat(const char *filename, mode_t mode);
int close(int fd);
```

`open` returns an **integer file descriptor**. It's a wrapper around a
system call. The call creates an **open file description** entry in a
system-wide table of open files in the kernel.

> [!IMPORTANT]
> **Why give the user an integer, not a pointer to the kernel's file
> description?**
> Security and isolation. A bare integer can't be used to reach into
> kernel memory. The user can only name files via these opaque
> numbers — there's no way to forge access to something you shouldn't
> have. It closes off information-leakage avenues.

### Standard descriptors (low-level)

```c
#include <unistd.h>
```

| Macro | Value |
|---|---|
| `STDIN_FILENO` | 0 |
| `STDOUT_FILENO` | 1 |
| `STDERR_FILENO` | 2 |

Bridges between the two API levels:

```c
int   fileno(FILE *stream);    // FILE* → fd
FILE *fdopen(int fd, ...);     // fd → FILE*
```

### Low-level read/write

```c
ssize_t read(int fd, void *buffer, size_t maxsize);
ssize_t write(int fd, const void *buffer, size_t size);
```

### Example: `lowio.c`

```c
int main() {
    char buf[1000];
    int fd = open("lowio.c", O_RDONLY, S_IRUSR | S_IWUSR);
    ssize_t rd = read(fd, buf, sizeof(buf));
    int err = close(fd);
    ssize_t wr = write(STDOUT_FILENO, buf, rd);
}
```

### Operations specific to terminals, devices, networking

- `ioctl` — device-specific control (e.g., set terminal baud rate,
  put a terminal in raw mode)
- Duplicating descriptors (`dup`, `dup2` — see below)
- **Pipes** — a one-way channel: `int pipe(int pipefd[2])`
- File locks
- Memory-mapping files (`mmap`)
- Asynchronous I/O

<details>
<summary><b>How do pipes work?</b></summary>

`pipe(int pipefd[2])` creates a one-way channel and fills the array
with **two file descriptors**: `pipefd[0]` is the **read end**,
`pipefd[1]` is the **write end**. Bytes written to the write end come
out the read end.

The trick: if you call `pipe` and then `fork`, *both* processes
inherit both ends. The parent closes the end it doesn't need, the
child closes the other — and now you have a communication channel
between parent and child. This is how shells implement `cmd1 | cmd2`.

</details>

<details>
<summary><b>Memory-mapped files (mmap)</b></summary>

`mmap` maps a file's contents directly into a process's address
space. Instead of `read`/`write` syscalls, you access the file as if
it were ordinary memory — loads and stores. The OS pages file content
in and out behind the scenes. Covered in depth in the Memory lectures.

</details>

---

# Buffering — the central theme

There are **two independent layers** of buffering. Understanding both
is the key insight of this lecture.

```
   Your program
        │
   printf / fread / fwrite
        │
   ┌────────────────────┐
   │  USER-LEVEL BUFFER │  ← managed by libc, inside FILE*
   └─────────┬──────────┘
        │ flush
   read / write syscall
        │
   ┌────────────────────┐
   │ KERNEL-LEVEL BUFFER│  ← managed by the OS, transparent to user
   └─────────┬──────────┘
        │
     Disk / device
```

> [!QUESTION]
> **Is there kernel buffering even when there's user-level buffering?**
> Yes — two separate buffers. The kernel buffer is **completely
> transparent** to the user; there's no way to even observe it.

---

## High-level vs. low-level: what really happens

| | `fread` / `fwrite` / `printf` (high-level) | `read` / `write` (low-level) |
|---|---|---|
| Layer | C library (`libc`) | Raw syscall |
| Buffering | User-level buffer in the `FILE*` | None at user level |
| What a call does | Check user buffer first; only syscall if needed | Always a syscall |
| `read` is... | The sophisticated wrapper | Literally just the syscall |

> [!TIP]
> When you `fread`, libc asks the kernel for a big chunk (e.g. 4 KB),
> stashes it in the `FILE*`'s buffer, and serves subsequent reads from
> that buffer — no syscall per read. `read` is just the wrapper around
> the syscall; `fread` is the smart one.

---

## User-level buffering: visible behavior

**Streams (`printf`) are buffered in user memory:**

```c
printf("Beginning of line");
sleep(10);
printf("and end of line\n");
```

→ Often prints **everything at once** after the sleep — both strings
sat in the user buffer.

**File descriptors (`write`) are visible immediately:**

```c
write(STDOUT_FILENO, "Beginning of line", 18);
sleep(10);
write(STDOUT_FILENO, "and end of line\n", 16);
```

→ "Beginning of line" appears **10 seconds before** the rest — `write`
*is* the syscall, no user buffer.

---

## What's inside a `FILE*`?

| Field | Role |
|---|---|
| File descriptor | The underlying low-level `fd` |
| Buffer | The user-level buffer |
| Lock | For thread-safe access |
| (other bookkeeping) | position, flags, error state, … |

### FILE buffering and flushing

When you `fwrite`, data goes into the `FILE`'s buffer until it's
**flushed**. The C standard library decides *when* to flush — unless
you force it with `fflush`.

> [!WARNING]
> **Write code that makes the weakest possible assumptions about when
> buffers flush.** Add your own `fflush` calls so data is written
> exactly when you need it.

### The classic buffering hazard

```c
char x = 'c';
FILE *f1 = fopen("file.txt", "w");
fwrite("b", sizeof(char), 1, f1);

FILE *f2 = fopen("file.txt", "r");
fread(&x, sizeof(char), 1, f2);
```

The `fread` **might** see the `'b'`, or **might** miss it and see
`EOF` — because `fwrite` only wrote to a *buffer*, not necessarily to
the file.

> [!TIP]
> Add `fflush(f1)` after the `fwrite` and the `fread` will reliably
> see `'b'`.

---

## Why buffer in user space?

### Reason 1 — Performance

> [!IMPORTANT]
> A syscall is **~25× more expensive** than a plain function call.

| Approach | Throughput |
|---|---|
| `read`/`write` byte-by-byte (syscall per byte) | ~10 MB/s |
| `fgetc` (buffered) | Keeps up with your SSD |

`fgetc` reads a whole chunk from the kernel once, then hands you one
byte at a time from the user buffer — no syscall per character.

### Reason 2 — Functionality

Syscall operations are deliberately **less capable**, which keeps the
OS simple.

> [!NOTE]
> **Example:** there's no syscall to "read until newline" — the kernel
> doesn't know what a newline *means*. The buffered library calls
> solve this: they pull a chunk out of the kernel and walk through it
> in user space looking for the newline, then hand you the line.

---

## I/O and storage layers: file descriptors in the kernel

On a successful `open()`:
1. A **file descriptor** (integer) is returned to the user
2. An **open file description** is created in the kernel

For each process, the kernel maintains a **file descriptor table**
mapping integers → open file descriptions.

### What's in an open file description?

| Field | Role |
|---|---|
| Inode reference | Where to find the file on disk |
| Offset | The current position within the file |

```
Process                          Kernel
┌──────────────┐         ┌─────────────────────────┐
│ fd table     │         │ Open file descriptions  │
│              │         │                         │
│  0 ──────────┼────────►│  { inode, offset }      │
│  1 ──────────┼────────►│  { inode, offset }      │
│  3 ──────────┼────────►│  { inode: file.txt,     │
│              │         │    offset: 0 }          │
└──────────────┘         └─────────────────────────┘
```

### A sequence of operations

| Call | Effect |
|---|---|
| `open("file.txt")` → returns `3` | fd 3 → open file description, offset = 0 |
| `read(3, buf, 100)` | Reads 100 bytes into `buf`; offset advances to 100 |
| `close(3)` | fd 3's table entry and its description are cleared |

---

## File descriptors across `fork`

When you `fork`, the file descriptor *table* is duplicated — but...

> [!IMPORTANT]
> The child's file descriptors point to the **same open file
> descriptions** as the parent's. They are **aliased**, not copied.

```
        Parent fd table          Child fd table
              │                        │
         fd 3 │                        │ fd 3
              └──────────┐  ┌──────────┘
                         ▼  ▼
              ┌──────────────────────┐
              │ ONE open file        │
              │ description          │
              │ { inode, offset }    │   ← shared offset!
              └──────────────────────┘
```

**Consequences:**

- The **offset is shared.** If the parent reads 100 bytes (offset →
  100), a subsequent read by the child starts at position 100.
- `close` in one process **doesn't** destroy the description — the
  kernel keeps a **reference count**. The description is freed only
  when *all* referencing fds are closed.

### Why is aliasing the open file description a good idea?

> [!NOTE]
> It enables **shared resources between processes.** Recall: in POSIX,
> everything is a file — so this one mechanism gives you shared
> terminals, shared network connections, and pipes, all for free.

**Shared terminal:** when a shell forks a child, the child inherits
copies of fds 0/1/2, all pointing at the shell's terminal. That's why
a forked command's output appears in the **same terminal** as the
shell, interleaved with it.

Other examples:
- Shared network connections after `fork`
- Shared access through pipes

---

## `dup` and `dup2`

These duplicate file descriptors — crucial for shell I/O redirection
(you'll use them in HW 2).

| Call | Effect |
|---|---|
| `dup(3)` | Returns a new fd (lowest available, e.g. `4`) pointing at the **same** open file description as fd 3 |
| `dup2(3, 162)` | Makes fd `162` point at the same description as fd 3 (closing 162 first if it was open) |

```
dup(3):                      dup2(3, 162):

 fd 3 ─┐                      fd 3   ─┐
       ├──► open file desc           ├──► open file desc
 fd 4 ─┘                      fd 162 ─┘
 (new, lowest available)      (specific number you chose)
```

> [!TIP]
> `dup2` is how a shell implements redirection: to make a command
> write to a file instead of the terminal, the shell `open`s the file,
> then `dup2(file_fd, STDOUT_FILENO)` — now fd 1 points at the file,
> and the command (which just writes to fd 1) lands in the file
> without knowing anything changed.

---

## Key takeaways

1. **"Everything is a file"** — one interface (`open`/`read`/`write`/
   `close`) for disk, devices, networking, and IPC
2. **Two API levels:** high-level C streams (`FILE*`, `fopen`,
   `fread`) vs. low-level syscalls (`fd`, `open`, `read`)
3. **Two buffering layers:** user-level (libc, in the `FILE*`) and
   kernel-level (transparent). User buffering exists for performance
   (~25× syscall savings) and functionality (e.g. read-until-newline)
4. Streams are buffered → output may be delayed; raw `write` is
   immediate. **Use `fflush` to control flushing explicitly**
5. A **file descriptor** is an opaque integer indexing a kernel table;
   it points to an **open file description** holding the inode and offset
6. Across `fork`, fds are **aliased** to the same open file
   descriptions — shared offset, reference-counted close. This is what
   makes shared terminals, pipes, and network connections work
7. **`dup`/`dup2`** duplicate fds — the mechanism behind shell I/O
   redirection

---

## Confusions to revisit

- [ ] Exactly how `pipe` + `fork` + `close` wires up a shell pipeline
      *(→ HW 2: build your own shell)*
- [ ] `mmap` mechanics — how file pages map into the address space
      *(→ Memory lectures, HW 5)*
- [ ] How the kernel's buffer cache decides when to write to disk
      *(→ File Systems lectures)*
- [ ] `dup2` for redirection — practice the exact call sequence
      *(→ HW 2)*
- [ ] Signal handling subtleties — what's safe to do in a handler

---

## Vocab to add to `os-vocab.md`

- Semaphore (P/V, down/up)
- Signal (SIGINT, SIGTERM, SIGKILL, …)
- `wait` / `waitpid`
- File descriptor
- Open file description
- File / directory / inode
- Metadata
- CWD (current working directory)
- Absolute vs. relative path
- Stream (`FILE*`)
- `fopen` / `fread` / `fwrite` / `fseek` / `fflush`
- `open` / `read` / `write` / `close`
- `ioctl`
- Pipe
- `dup` / `dup2`
- User-level vs. kernel-level buffering
- Reference count (open file descriptions)

---

🔗 **Links:** [[03-threads-and-processes]] · [[05-sockets-and-ipc]] · [[os-vocab]]