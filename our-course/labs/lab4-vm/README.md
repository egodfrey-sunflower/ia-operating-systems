# Lab 4 — xv6 virtual memory

**Weeks:** 7–9 · **Budget:** ~15 hours (over 3 weeks) · **Track:** kernel · **Weight:** 5%

**Syllabus:** IA L6–L8 (address translation,
paging, page faults) · **Reading:** OSTEP ch. 13, 15–17 (address spaces,
translation, segmentation, free space) and ch. 18–21 (paging, TLBs, smaller
tables, swapping); the xv6 book, ch. 3 (page tables) and ch. 4.6 (page-fault
handling). See `../../reading-list.md`.

You already met the trap machinery in Lab 2. This lab lives one level deeper,
in the **page table**: the per-process map from virtual to physical addresses
that the RISC-V MMU walks on every load and store. You will read the page
table, add a kernel-maintained page that user code can read without a system
call, and finally make `fork()` lazy with **copy-on-write**.

> **This xv6 already does lazy allocation.** OSTEP (and the xv6 book)
> describe an eager `sbrk`; the tree you are working in is lazier:
> `sbrklazy(n)` (see `user/ulib.c` — it wraps `sys_sbrk(n, SBRK_LAZY)`)
> just grows `p->sz` and lets the *page-fault handler*
> (`vmfault()` in `kernel/vm.c`) allocate a zeroed page on first touch.
> Task 0 has you dissect that existing machinery, because Task 3
> (copy-on-write) has to share the page-fault path with it. Plain `sbrk(n)`
> from `user/ulib.c` is **eager**.

> **Naming note.** Some names differ from the xv6 book: the kernel print
> routine is `printk` (not `printf`); the internal syscall implementations
> are `kfork`, `kexit`, `kwait`, `kexec`; the return-to-user helper is
> `prepare_return()`. Read the real code — these are the names you will see.

---

## Background

### The RISC-V Sv39 page table

An address space is a three-level radix tree of 4 KiB pages. A 39-bit virtual
address splits into three 9-bit indices (one per level) and a 12-bit offset.
`walk()` in `kernel/vm.c` implements exactly this descent; `mappages()`,
`uvmalloc()`, `uvmcopy()`, and friends build on it. Each leaf **PTE** holds a
physical page number plus the permission bits `V R W X U` (`kernel/riscv.h`),
and two bits (8–9) reserved for software — you will use one of those for
copy-on-write.

The user address space this tree builds looks like (low to high):

```
0x0            text  (R-X)      the program's code
               data/bss (RW-)
               guard page (mapped, PTE_U cleared -> stack overflow traps)
               user stack (RW-)
               heap ... grows up via sbrk ...
   ...
USYSCALL       (Task 2 adds this: R--, U)   at an address you choose
TRAPFRAME      (RW-, no U)                   TRAMPOLINE - PGSIZE
TRAMPOLINE     (R-X, no U)                   MAXVA - PGSIZE
```

### Page faults

When translation fails, the MMU raises an exception. `usertrap()` in
`kernel/trap.c` dispatches on `scause`: **13** is a load page fault, **15** a
store page fault (12 is instruction fetch). The stock kernel routes 13/15 to
`vmfault()`, which is how lazily-allocated pages get their physical frame on
first use. In Task 3 you will make store faults (15) *ambiguous* — a page can
now fault because it is unmapped (lazy) **or** because it is a read-only
copy-on-write page that wants a private copy — and your handler must tell them
apart.

---

## Setup

From this directory, create a fresh working tree:

```
./starter/setup.sh ~/lab4
cd ~/lab4
make qemu          # boots xv6; quit with Ctrl-a x
```

**Pacing.** The lab spans weeks 7–9. Tasks 0–2 belong with week 8's
page-table reading; Task 3 (copy-on-write) is week 9's material — save it
for the week-9 reading.

`setup.sh` refuses to overwrite an existing directory. The starter tree
**builds and boots as-is**, and ships three test programs in `UPROGS`:
`pgtbltest`, `cowtest`, `forkbench`. Out of the box they fail:

- `pgtbltest` — `vmprintme()` is an unimplemented system call (prints
  `unknown sys call 23`), and `ugetpid()` in `user/ulib.c` is a stub returning
  −1, so `ugetpid_test` mismatches.
- `cowtest` — without copy-on-write, forking a process that owns most of
  physical memory fails (`fork() failed`): the eager copy runs out of RAM.
- `forkbench` — this one *does* run (eager fork works); note its tick count as
  your "before" measurement.

Run the autograder at any time:

```
tests/run.sh ~/lab4          # full run (incl. usertests -q)
QUICK=1 tests/run.sh ~/lab4  # skip the long usertests run
CPUS=3 tests/run.sh ~/lab4   # stress COW locking (default is 1)
```

---

## Tasks

### Task 0 — Dissect the existing lazy allocator (15%, written)

Read the lazy-allocation machinery already in your tree and answer the
following, each with a **file:line** reference to the code you are citing.
(Line numbers below are for the pristine tree; verify them.)

1. **`sbrk(-n)`.** `sys_sbrk` (`kernel/sysproc.c:40`) sends *negative* `n`
   down the `SBRK_EAGER` branch (`sysproc.c:50`) to `growproc()`, never the
   lazy branch. Follow `growproc()` into `uvmdealloc()`
   (`kernel/vm.c`): what exactly happens to the physical pages and the PTEs
   when a process shrinks its heap, and why must shrinking be eager?

2. **Why zero the page?** `vmfault()` (`kernel/vm.c:455`) does
   `memset((void*)mem, 0, PGSIZE)` (`vm.c:469`) before mapping. What security
   property would break if it mapped the freshly `kalloc()`'d page without
   zeroing it? (Think about what `kalloc()` last held.)

3. **The limits.** `vmfault()` returns 0 (fault unhandled → process killed)
   unless the address passes two checks: `va >= p->sz` (`vm.c:460`) and
   `ismapped()` (`vm.c:463`). Explain what each rejects, and how the
   `va < p->sz` test interacts with the stack **guard page** that `kexec`
   installs with `uvmclear()`.

4. **Why does `copyin`/`copyout` call `vmfault` at all?** The hardware already
   faults on a bad user access — so why do `copyin` (`vm.c:391`) and `copyout`
   (`vm.c:357`) call `vmfault()` *by hand*? Walk through what page table is
   installed while the kernel runs `memmove` inside `copyin`, and what would
   go wrong for a lazily-allocated, not-yet-faulted user page if they did not.

5. **Load vs. store.** `usertrap` (`kernel/trap.c:71`) passes
   `read = (scause == 13)` to `vmfault`, but the stock `vmfault` ignores it.
   Give one concrete reason a real kernel would care whether the faulting
   access was a load or a store. (Task 3 gives you one for free.)

Deliverable: `answers.md` (human-graded).

### Task 1 — `vmprint` (20%)

Add a debugging routine that dumps a page table as an indented tree, and
expose it as a system call `int vmprintme(void)` that prints the **calling
process's** page table and returns 0. (The syscall number `SYS_vmprintme`,
the `user/user.h` prototype, and the `user/usys.pl` stub are already in the
starter — you only write the kernel side.)

The required format:

```
page table 0x0000000087f25000
 ..0: pte 0x0000000021fc8401 pa 0x0000000087f21000
 .. ..0: pte 0x0000000021fc8001 pa 0x0000000087f20000
 .. .. ..0: pte 0x0000000021fc885b pa 0x0000000087f22000
 .. .. ..1: pte 0x0000000021fc7cd7 pa 0x0000000087f1f000
 ...
 ..255: pte 0x0000000021fc9001 pa 0x0000000087f24000
 .. ..511: pte 0x0000000021fc8c01 pa 0x0000000087f23000
 .. .. ..510: pte 0x0000000021fd10c7 pa 0x0000000087f44000
 .. .. ..511: pte 0x000000002000184b pa 0x0000000080006000
```

(Sample from a stock process; after Task 2 your USYSCALL page appears here
too, at whatever index your chosen address puts it.)

A header line `page table <pa>`, then one line per valid PTE: `..` repeated
once per level of depth, the PTE index, the raw PTE, and the physical address
it points at. Recurse into non-leaf PTEs (a PTE with none of R/W/X set points
to a lower-level table).

**Written part (include in `answers.md`):** run `vmprintme()` from a fresh
process and explain, line by line, every mapping in the printout — the text
pages under top index 0, and the leaves near the top of memory: index
**511 = trampoline**, **510 = trapframe** (plus, after Task 2, your USYSCALL
page). Why is the top index 255 and not 511? (Hint: look at `MAXVA` in
`kernel/riscv.h`.)

### Task 2 — the USYSCALL shared page (25%)

`getpid()` is a full system call for a value that never changes. Map a
**read-only** page into every process at a fixed virtual address `USYSCALL`,
holding a kernel-maintained `struct usyscall { int pid; }`, and implement
`ugetpid()` in `user/ulib.c` so it reads the pid straight from that page — no
trap into the kernel. This is exactly how a real vDSO serves cheap kernel data
to user space.

**Choosing the address is part of the task.** Pick a fixed virtual address
near the top of user space and define it as `USYSCALL` in
`kernel/memlayout.h`. A user-readable page changes what the rest of the
system can assume about the address space — the acceptance test is that
`pgtbltest` *and* the full `usertests -q` regression both pass. If the
regression objects to your first choice, work out why before moving the
page: that debugging is the point of the exercise.

You will:

- Define `USYSCALL` (your chosen address) and
  `struct usyscall` in `kernel/memlayout.h`. (Guard the struct with
  `#ifndef __ASSEMBLER__` — `memlayout.h` is included by `trampoline.S`.)
- In `allocproc()`, `kalloc()` a page, store its pointer on `struct proc`, and
  write the pid into it.
- In `proc_pagetable()`, `mappages()` it at `USYSCALL` with `PTE_R | PTE_U`
  (readable by user, **never** `PTE_W`).
- In `freeproc()` free the page, and in `proc_freepagetable()` `uvmunmap()` it
  (do this the same way `TRAPFRAME` is handled).
- Replace the `ugetpid()` stub in `user/ulib.c` with a read of
  `((struct usyscall*)USYSCALL)->pid`.

`pgtbltest`'s `ugetpid_test` forks 64 children and checks `ugetpid() ==
getpid()` in each.

### Task 3 — copy-on-write fork (40%)

Make `fork()` stop copying. Instead of duplicating every user page,
`uvmcopy()` should map the parent's pages into the child **read-only and
shared**, and defer the real copy until someone writes.

What a working design must provide — the *how* of each is yours to work out:

1. **Shared pages must be write-protected and identifiable as COW.** After a
   fork, no process may write a shared frame through an old mapping, and a
   later fault handler must be able to tell, from a PTE alone, that a page is
   COW rather than genuinely read-only. Background points you at the RSW bits
   the hardware reserves for software; `kernel/riscv.h` is where PTE bits are
   defined. Think carefully about *whose* page tables need which changes.

2. **Physical frames need reference counting.** A frame can now appear in
   several page tables, so it must return to the free list only when the
   **last** reference is gone. The lifecycle is yours to define precisely —
   where a count begins, every event that moves it, what ends it — as is how
   the counter is protected from concurrent update. `kernel/kalloc.c` owns
   frame lifetime today; study it before deciding.

3. **The fault router must disambiguate.** After this task, a store page
   fault (`scause == 15`) can mean *lazy* (no page there yet) **or** *COW*
   (page there, write-protected on purpose) — and the two need different
   treatments. How your `usertrap()` tells them apart, and in what order it
   checks, is the central design point of the lab.

4. **`copyout` must break COW too.** When the *kernel* writes into a user page
   (e.g. `wait()` copying an exit status, a pipe `read()` filling a buffer),
   it walks the user page table by hand and there is **no** hardware fault —
   so `copyout()` must detect a COW target and handle it itself.

Your kernel must pass **`cowtest`** (subtests `simple`, `three`, `file`) and,
crucially, the full **`usertests -q`** regression — copy-on-write interacts
with almost every corner of the VM system, so this is the real gate.

### Measurement deliverable

`forkbench` (shipped complete) grows a parent to ~4 MB, then `fork()`s and
`wait()`s repeatedly with a child that exits immediately. Report its tick
count **before** (starter, eager fork) and **after** (your COW fork) in
`answers.md`, and explain the difference.

### Stretch (unweighted) — `mmap`/`munmap`

Sketch (in `answers.md`; no code required) how you would add file-backed,
lazily-populated `mmap`/`munmap`: what per-VMA state a process needs, how the
page-fault handler would read a page from a file on first touch, how `munmap`
writes back and unmaps, and how this reuses the lazy-allocation and
reference-counting machinery you already have.

*Want to build it for real rather than sketch it? This is offered as a full
final-project option — see **Lab 8, Option 5**.*

---

## Hints

**Files you will touch.** Task 1: `kernel/vm.c` (add `vmprint`),
`kernel/sysproc.c` (`sys_vmprintme`), `kernel/syscall.c` (dispatch),
`kernel/defs.h`. Task 2: `kernel/memlayout.h`, `kernel/proc.h`,
`kernel/proc.c`, `user/ulib.c`. Task 3: `kernel/riscv.h`, `kernel/kalloc.c`,
`kernel/vm.c`, `kernel/trap.c`, `kernel/defs.h`.

**vmprint.** `printk("%p", x)` prints `0x` followed by 16 hex digits, exactly
the format above. Recurse using the same `(pte & (PTE_R|PTE_W|PTE_X)) == 0`
non-leaf test that `freewalk()` uses.

**Stale disk images.** If `usertests` fails oddly in file-system tests (e.g.
`iref`) after you have been rebuilding a lot, `rm fs.img && make` — a stale
image produces spurious failures unrelated to your VM code.

**Reference counts.** Two spots bite if unexamined: how a physical address
maps to a slot in your counter store (`kernel/memlayout.h` tells you the range
of RAM the kernel manages), and what happens at boot, when `freerange()` hands
every page to `kfree()` before any of your counts have ever been set. A
separate spinlock from `kmem.lock` keeps things simple.

**The COW break, carefully.** Consider what a refcount of exactly 1 lets you
skip.

**Don't forget the flags.** When you rebuild a PTE, recompute permissions from
`PTE_FLAGS(*pte)`, then flip `PTE_COW`/`PTE_W`. Losing `PTE_U` or `PTE_X` here
is a classic cause of "works until exec/usertests" bugs.

**Debug with what you built.** When `usertests` wedges, `vmprintme()` (or a
kernel-side `vmprint(p->pagetable)`) is your friend — dump the table and check
the flags on the page that faulted.

**Rebuild and re-test** after each task. `make clean` if the disk image seems
stale. Iterate with `QUICK=1`; run the full `usertests -q` before you call
Task 3 done.

---

## Deliverables

1. Your modified xv6 tree (or a patch of it).
2. `answers.md` — the Task 0 questions, the Task 1 page-table walk-through,
   and the `forkbench` before/after tick counts with a sentence of analysis.
3. `tests/run.sh` passes on your tree — all functional tests **and**
   `usertests -q`.

---

## Rubric (100%)

| Task | Weight | What is graded |
|------|-------:|----------------|
| 0 — lazy-allocator dissection | 15% | Five questions answered precisely with file:line references. Human-graded. |
| 1 — `vmprint` | 20% | Correct indented-tree format; recurses to all 3 levels; syscall prints the caller's table. Plus the written walk-through. (`vmprint` test) |
| 2 — USYSCALL / `ugetpid` | 25% | Page mapped `R+U` at `USYSCALL`; populated at `allocproc`, freed at `freeproc`; `ugetpid()` reads it in userspace (`ugetpid` test). Never writable: read-only mapping confirmed from your `vmprint` output — human-checked (the `USYSCALL` leaf must carry no `W` bit). |
| 3 — copy-on-write fork | 40% | Shares pages COW on fork; refcounts free frames correctly; store-fault and `copyout` both break COW; distinguishes COW from lazy faults. (`cowtest`) |
| Regression | gate | `usertests -q` must pass. COW that breaks the kernel forfeits the Task 3 marks. |
| `forkbench` | — | Runs and reports ticks; before/after numbers appear in `answers.md`. |

The autograder prints a PASS/FAIL table and exits non-zero on any failure.
`QUICK=1` skips `usertests -q` while you iterate; the final check runs it.
