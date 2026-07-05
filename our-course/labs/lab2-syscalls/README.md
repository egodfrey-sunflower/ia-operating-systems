# Lab 2 — xv6 system calls

**Weeks:** 4 · **Budget:** 6–8 hours · **Track:** kernel · **Weight:** 5%

**Syllabus:** IA L2 (protection, dual-mode
operation, system calls) · **Reading:** OSTEP ch. 6 (Limited Direct
Execution); the xv6 book, ch. 2 (operating system organization) and ch. 4
(traps and system calls). See `../../reading-list.md`.

This is your first lab *below* the system-call line. In Labs 0–1 you used
system calls from userspace; now you implement them. You will trace one
existing call across the user/kernel boundary, then add three of your own to
xv6: `trace`, `sysinfo`, and `getppid`.

---

## Background

A system call is a controlled doorway from unprivileged user code into the
privileged kernel. On RISC-V the doorway is the `ecall` instruction, which
raises an exception that hardware vectors through a trap handler. xv6's path
looks like this (real symbols in this tree, verify them yourself in Task 0):

```
user program   write(fd, buf, n)
  |
  v
ecall          enters supervisor mode; hardware vectors through the trap handler
  |
  v
  ...          the kernel saves the user's registers, identifies the call,
  |            dispatches it, runs the work, then unwinds back out ...
  v
sret           returns to the user program with the result in a0
```

Task 0 is where you fill that middle in — naming each file, function, and
key line for `write()` by reading the real source in your tree.

Arguments travel in registers `a0`–`a5`; the call number is in `a7`; the
return value comes back in `a0`. The kernel never trusts a user pointer: it
copies data in and out with `copyin`/`copyout`, which validate the address
against the process's page table.

> **Naming note.** Some names differ from the xv6 book: the kernel print
> routine is `printk` (not `printf`), some kernel
> entry points are renamed (`kfork`, `kexit`, `kwait`, `kexec`, `kkill`),
> the return-to-user helper is `prepare_return()` (not `usertrapret()`), and
> `sbrk()` supports a lazy mode. Task 0 asks you to read the real code, so
> these names are what you will actually see.

---

## Setup

From this directory, create a fresh working tree (a copy of the vendored
xv6 with the lab's starter files overlaid):

```
./starter/setup.sh ~/lab2
cd ~/lab2
make qemu          # boots xv6; quit with Ctrl-a x
```

`setup.sh` refuses to overwrite an existing directory. The starter tree
**builds and boots as-is.** The three new system calls are already wired up
on the *user* side (declarations in `user/user.h`, ecall stubs from
`user/usys.pl`, numbers in `kernel/syscall.h`, and the test programs
`trace`, `sysinfotest`, `ppidtest` are in `UPROGS`). What is missing is the
*kernel* side. Until you add it, invoking one of the new calls prints

```
<pid> <name>: unknown sys call 23
```

and returns −1 — so `trace`, `sysinfotest`, and `ppidtest` all fail. Making
them pass is the lab.

Run the autograder at any time:

```
tests/run.sh ~/lab2          # full run (incl. usertests -q)
QUICK=1 tests/run.sh ~/lab2  # skip the long usertests run
```

---

## Tasks

### Task 0 — Trace one system call end to end (20%, written)

Pick the `write()` system call and follow it from the user-space call to the
kernel and back, reading the *real source in your tree*. Produce an
annotated call path (a page is plenty) naming each file, function, and the
key lines. Your write-up must answer these five questions precisely, with
file:line references:

1. **Number.** When `ecall` executes for `write()`, which register holds the
   system-call number, and which line of `user/usys.pl` (equivalently, the
   generated `user/usys.S`) puts it there?
2. **Dispatch and result.** In `kernel/syscall.c`, where does `syscall()`
   read the call number from, and into which field does it store the return
   value that the user will see in `a0`?
3. **The `+ 4`.** Find `p->trapframe->epc += 4`. Which function is it in
   (careful — in this xv6 it is **not** in `syscall()`), what does the `4`
   compensate for, and what would go wrong if you deleted this line?
4. **The way back.** Which function re-points `stvec` at `uservec` for the
   *next* trap, and where does it set the `sepc` that the final `sret` will
   return to?
5. **Kernel context.** `uservec` in `trampoline.S` runs with the *user* page
   table still installed. How does it obtain the kernel stack pointer, the
   kernel page table, and the address of `usertrap()`? Name the trapframe
   fields and say who filled them in.

Deliverable: `answers.md` (or a PDF) with the call path and the five
answers. This task is graded by a human, not `run.sh`.

### Task 1 — `trace(int mask)` (30%)

Add a system call `int trace(int mask)` that turns on per-process
system-call tracing. `mask` is a bitmask: bit *i* (i.e. `1 << i`) selects the
system call whose number is *i*. When a traced process returns from a traced
system call, the kernel prints one line, on return, in exactly this format:

```
<pid>: syscall <name> -> <retval>
```

e.g. `4: syscall read -> 1023`. Semantics (follow these exactly — the
autograder checks them):

- The mask is **per process**.
- It is **inherited by children** across `fork`.
- It is **preserved across `exec`** (not cleared).

The wrapper program `user/trace.c` is provided: `trace <mask> <cmd> [args]`
sets the mask then `exec`s the command, so the command and everything it
forks run traced. Example:

```
$ trace 32 grep hello README      # 32 == 1<<5 == read
3: syscall read -> 1023
3: syscall read -> 965
3: syscall read -> 0
```

### Task 2 — `sysinfo(struct sysinfo *info)` (30%)

Add `int sysinfo(struct sysinfo *info)` that fills a caller-supplied struct
(defined in `kernel/sysinfo.h`, already in your tree) with two numbers:

```c
struct sysinfo {
  uint64 freemem; // free physical memory, in BYTES
  uint64 nproc;   // number of processes with state != UNUSED
};
```

`freemem` requires walking the free-page list in `kernel/kalloc.c` and
multiplying by the page size. `nproc` requires scanning the process table in
`kernel/proc.c`. Then `copyout` the filled struct to the user pointer.
Return 0 on success, −1 if the copyout fails. The provided
`user/sysinfotest.c` checks both fields and prints `sysinfotest: OK`.

### Task 3 — `getppid()` (20%)

Add `int getppid(void)` returning the pid of the calling process's parent.
Small, but it forces you to think about the process table and locking:

- `p->parent` is guarded by `wait_lock` (read the comment on the field in
  `kernel/proc.h` and how `kfork`/`reparent`/`kwait` use it). Read it under
  that lock.
- Handle the **parent-exited** case. When a parent exits, `kexit()` calls
  `reparent()`, which hands orphans to `initproc`. So a process whose
  original parent is gone should see `getppid()` return init's pid (**1**).
  The provided `user/ppidtest.c` checks both the normal case and this
  adoption case.

Think about *why* the answer is 1 and not, say, −1 or a stale pid: what
invariant does `reparent()` maintain, and why is init (pid 1) never allowed
to exit?

---

## Hints

**Which files to touch (all four new calls follow the same recipe):**

| Step | File |
|------|------|
| Call number is already defined | `kernel/syscall.h` (given) |
| User prototype is already declared | `user/user.h` (given) |
| ecall stub is already generated | `user/usys.pl` (given) |
| Add `extern` prototype + table entry | `kernel/syscall.c` |
| Write the handler `sys_<name>()` | `kernel/sysproc.c` |
| Per-process state (Task 1's mask field) | `kernel/proc.h` |
| Helpers that touch kernel structures | `kernel/proc.c`, `kernel/kalloc.c` |
| Export those helpers | `kernel/defs.h` |

Because the numbers/stubs/UPROGS are already in place, you only ever edit the
six kernel files in the lower half of that table.

**Reading arguments.** In a `sys_*` handler, use `argint(n, &i)` for an
integer argument and `argaddr(n, &p)` for a pointer argument (the *n*-th
argument, 0-based). These read the saved `a0..a5` from the trapframe. See the
existing `sys_kill`, `sys_wait` for examples.

**Task 1.** The trace state is a new per-process field — see how `kfork`
copies fields between `struct proc`s (that is the inheritance path) and where
`allocproc` initialises them. For the trace print, study how `syscall()`
dispatches via the `syscalls[]` table and where it puts the handler's return
value; the same table shows how a parallel name array would be indexed. Think
about which of the three semantics (per-process, inherited across `fork`,
preserved across `exec`) each need code and which comes for free.

**Task 2.** `freemem` is a walk of the free list in `kalloc.c`; `nproc` is a
scan of `proc[]` in `proc.c` — look at how each structure is locked wherever it
is already traversed. To return the struct, fill a local one and `copyout` it
the way other `sys_*` handlers hand data back to user space (`#include
"sysinfo.h"` where you build it).

**Task 3.** `p->parent` is guarded by `wait_lock`, a global defined in
`proc.c` — see how `kfork`/`reparent`/`kwait` take it, and put your helper
alongside them in `proc.c`. Decide what init (which has no parent) should
return.

**A note on locking.** The theory of locks arrives in week 6 (with Lab 5's
reading); it is not assumed here. For this lab the expectation is
*pattern-copying*: wherever a structure you touch is already traversed, copy
the locking that the neighbouring code does.

**Rebuild and re-test** after each task: `make qemu` to try by hand, or the
autograder to check. `make clean` if the disk image seems stale.

---

## Deliverables

1. Your modified xv6 tree (the six edited kernel files), or a patch of them.
2. `answers.md` — the Task 0 call-path write-up with the five answers.
3. `tests/run.sh` passes on your tree (all functional tests **and**
   `usertests -q`).

---

## Rubric (100%)

| Task | Weight | What is graded |
|------|-------:|----------------|
| 0 — call-path write-up | 20% | Correct files/symbols; all five questions answered precisely with references. Human-graded. |
| 1 — `trace` | 30% | Exact output format; per-process mask; inherited across fork; preserved across exec. (`trace: *` tests) |
| 2 — `sysinfo` | 30% | `freemem` in bytes tracks allocation/free; `nproc` counts non-UNUSED procs; correct `copyout`. (`sysinfo` test) |
| 3 — `getppid` | 20% | Correct parent pid; safe under `wait_lock`; adoption case returns 1. (`getppid` test) |
| Regression | gate | `usertests -q` must still pass — a change that breaks the kernel forfeits the coding marks. |

The autograder prints a PASS/FAIL table and exits non-zero on any failure.
`QUICK=1` skips `usertests -q` while you iterate; the final check runs it.
