# Lab 5 — Threads & synchronization

**Weeks:** 10–12 · **Budget:** 14–16 hours (over 3 weeks) · **Track:** kernel + host · **Weight:** 5%

**Syllabus:** extension — beyond IA
(threads, concurrency, mutual exclusion, condition synchronisation; Cambridge
defers these to Part IB) ·
**Reading:** OSTEP ch. 26–31 (concurrency and threads, the thread API, locks,
lock-based data structures, condition variables, semaphores); the xv6 book,
ch. 6 (locking) and ch. 7 (scheduling). See `../../reading-list.md`.

This lab is about the two halves of concurrency: **context switching** (how one
CPU juggles several threads) and **synchronisation** (how threads sharing data
stay correct and *fast*). You will build a user-level thread package, cut lock
contention out of two hot kernel subsystems, and implement three classic
synchronisation primitives from a mutex and condition variables.

It has three parts:

- **Part A (25%)** — a user-level cooperative thread library *inside* xv6.
- **Part B (45%)** — remove lock contention from the xv6 physical-memory
  allocator and buffer cache.
- **Part C (30%)** — the bounded buffer, a reusable barrier, and a
  writer-preference read/write lock, in POSIX threads on your Linux box.

---

## Background

### Context switching (Part A)

A *thread* is an independent stream of execution: a program counter, a set of
registers, and a stack. Switching the CPU from one thread to another means
saving the first thread's registers somewhere and loading the second's. A
cooperative context switch happens *at a function call* (`thread_switch`): it
saves only the **callee-saved** registers (`s0`–`s11`) plus the return address
`ra` and stack pointer `sp` — exactly the set the kernel's `kernel/swtch.S`
saves between a process and the scheduler. You will write the user-space twin
of it. (Why that narrow set is enough, rather than the whole register file, is
written question A.1.)

### Lock contention (Part B)

A correct lock serialises access to shared data; a *contended* lock also
serialises the CPUs waiting for it, and that is pure overhead. xv6's stock
physical allocator (`kernel/kalloc.c`) guards one global free list with one
lock, and its buffer cache (`kernel/bio.c`) guards one global LRU list with one
lock. On a multi-core machine every `kalloc`/`kfree` and every `bread`/`brelse`
piles onto that single lock. The fix is the standard one: **split the data
structure so unrelated operations use different locks** — a free list per CPU,
a hash bucket per block — while preserving correctness (you can still find a
free page by stealing; you can still find/evict any buffer).

To *see* contention, this lab wires a pair of counters into every spinlock:
`n` (how many times the lock was acquired) and `nts` (how many of those
acquisitions found the lock already held — the atomic test-and-set "spun").
A new `statistics()` system call reports the totals for the kmem and bcache
locks. **This plumbing is already in your starter tree**, so the very first
time you run `kalloctest`/`bcachetest` you will see large contended-acquire
counts for the stock big locks. Your job is to make them small.

### Condition synchronisation (Part C)

A mutex gives mutual exclusion but cannot make a thread *wait for a condition*
("buffer not empty", "all threads arrived", "no writer active"). That is what
**condition variables** are for: `wait(cond, mutex)` atomically releases the
mutex and blocks; `signal`/`broadcast` wakes waiters. The single most common
bug is the **lost wakeup**: test the condition with `while`, never `if`, and
always re-check after waking, because the wakeup and your re-acquisition of the
mutex are not atomic and the world may have changed.

---

## Setup

### Parts A & B (xv6)

From this directory, create a fresh xv6 working tree (a copy of the vendored
xv6 with this lab's starter files overlaid):

```
./starter/setup.sh ~/lab5
cd ~/lab5
make qemu          # boots xv6 with CPUS=3; quit with Ctrl-a x
```

`setup.sh` refuses to overwrite an existing directory. The tree **builds and
boots as-is.** Try the three programs — they are already in the shell:

```
$ uthread        # Part A: prints nothing useful yet (you haven't written it)
$ kalloctest     # Part B.1: runs, but reports huge kmem contention -> FAIL
$ bcachetest     # Part B.2: runs, but reports huge bcache contention -> FAIL
```

### Part C (host pthreads)

Part C does **not** use xv6. It builds on your Linux machine. It is also
deliberately **week-12** work — the course table schedules it alongside
Concurrency II, so don't feel behind if you reach it last. Copy
`starter/hostsync` out of the course tree first and work on the copy, so
your edits don't dirty the course checkout:

```
cp -r starter/hostsync ~/lab5-hostsync
cd ~/lab5-hostsync
make             # builds bbuffer, barrier, rwlock (-Wall -Wextra -Werror -std=c11 -pthread)
./bbuffer        # prints "bbuffer: FAIL" until you implement it
```

### Running the autograder

```
tests/run.sh ~/lab5 ~/lab5-hostsync            # everything
QUICK=1 tests/run.sh ~/lab5 ~/lab5-hostsync    # skip the long usertests run
NOXV6=1 tests/run.sh unused ~/lab5-hostsync    # Part C only
```

`run.sh <xv6-workdir> [hostsync-dir]` runs the xv6 parts (A+B, via QEMU) and
then Part C. It prints a combined PASS/FAIL table and exits non-zero on any
failure. **Part B must be tested with more than one CPU** (`CPUS=3`, the
default) — with one CPU there is no contention to remove and no stealing to
exercise.

---

## Tasks

### Task A — User-level threads (25%)

Finish the cooperative thread package in `user/uthread.c` and write its context
switch in `user/uthread_switch.S`. The scheduler (round-robin over a fixed
array of threads, switching only when a thread calls `thread_yield()`) is
provided; three things are missing:

1. **`user/uthread_switch.S`** — write `thread_switch(struct context *old,
   struct context *new)`: save the callee-saved registers plus `ra` and `sp`
   into `*old`, load them from `*new`, and `ret`. This is the user-space
   analogue of `kernel/swtch.S` — read that first. The register offsets must
   match `struct context` in `uthread.c`.
2. **`thread_create()`** — set up a brand-new thread's saved context so that
   the first switch into it begins running its start function on its own stack.
3. **`thread_schedule()`** — actually call `thread_switch()` to switch from the
   outgoing thread to the next runnable one.

When it works, `uthread` runs three threads that count to 100, yielding to each
other, and prints their interleaving followed by `uthread: all threads
finished`.

**Written questions** (put the answers in `answers.md`):

1. `thread_switch` saves only `ra`, `sp`, and `s0`–`s11`. Why is it safe to
   ignore the caller-saved registers (`a0`–`a7`, `t0`–`t6`) and every other
   register the CPU has?
2. A brand-new thread has never run. When `thread_schedule()` switches to it
   for the first time, `thread_switch` ends in `ret` — where does that `ret`
   jump, and how did the value get there?
3. Contrast this with a *process* context switch in xv6 (`swtch` plus
   everything around it). Name at least two things a process switch must save
   or change that `thread_switch` does **not**, and explain why user-level
   thread switching is therefore much cheaper.

### Task B.1 — Per-CPU kmem free lists (20%)

Redesign `kernel/kalloc.c` so that **each CPU has its own free list and its own
lock**. `kalloc`/`kfree` should use the current CPU's list in the common case.
When a CPU's list is empty, `kalloc` must **steal** pages from another CPU's
list (otherwise a process that happens to run where the free list is empty
would fail to allocate even though memory is free elsewhere).

`kalloctest` (provided) forks several processes that hammer `sbrk` up and down,
then reports the kmem contended-acquire count via `statistics()`; it also
checks that stealing works (every child must be able to allocate) and that all
memory is recoverable. It prints `kalloctest: OK` on success.

Hints: read the current CPU with `cpuid()`, but only with interrupts off — use
`push_off()`/`pop_off()` (or `myproc`-style pinning) around it, since a timer
interrupt could otherwise migrate you to another CPU mid-operation. Two design
questions to settle before you code: how often should cross-CPU stealing happen
if the common-case fast path is to stay fast, and what does that imply about
how much to take per steal? And what invariant about how many kmem locks a CPU
holds at once would make deadlock impossible *by construction*? Update
`kmem_lockstat()` to sum the counters across all CPUs' locks.

### Task B.2 — Bucketed buffer cache (25%)

Replace the single lock and global LRU list in `kernel/bio.c` with a **hash
table of buckets** (13 is a good prime), each with its own lock. Key each block
by `blockno % NBUCKET`. A cache **hit** must take only its bucket's lock. On a
**miss**, recycle the least-recently-used buffer with `refcnt == 0` — searching
across *all* buckets if the target bucket has none free. The global LRU list is
gone, so you need some other way to know which free buffer was least recently
used: a per-buffer timestamp updated on release is one workable option, but the
mechanism is yours to design.

The subtlety is **not deadlocking** when eviction crosses buckets (you may need
a buffer that currently lives in a different bucket), and **not double-caching**
a block if two CPUs miss on it at once. Think carefully about lock ordering
before you write any code. `bcachetest` (provided) stresses the cache from several processes,
checks the data it reads back, forces eviction, and reports the bcache
contended-acquire count. It prints `bcachetest: OK` on success. `bpin`,
`bunpin`, and `brelse` must keep working; update `bcache_lockstat()` to sum
across all bucket locks.

**Regression:** `usertests -q` must still pass with `CPUS=3`. Subtle bugs in
the allocator or cache (a lost page, a double-free, an evicted-but-in-use
buffer) show up there, not in the targeted tests.

**Deliverable numbers:** report the kmem and bcache contended-acquire counts
**before** (stock big locks) and **after** (your redesign), at `CPUS=3`.

### Task C — Classic problems in pthreads (30%)

Implement three primitives in your copy of `hostsync/`. Each `.c` file has a header
comment stating the invariant to maintain and the classic bug to avoid, and a
built-in stress test that prints `<name>: PASS` or `<name>: FAIL`. Use only
`pthread_mutex_t` and `pthread_cond_t` (plus atomics for instrumentation) — no
`pthread_rwlock_t`, no `pthread_barrier_t`.

1. **`bbuffer.c` — bounded buffer.** A fixed-capacity ring shared by N
   producers and M consumers, guarded by a mutex and two condition variables
   ("not full", "not empty"). *Invariant:* the number of items in the ring is
   always in `[0, capacity]` and no item is lost or duplicated. *Bug to avoid:*
   the lost wakeup.
2. **`barrier.c` — reusable cyclic barrier.** `barrier_wait()` blocks until all
   T threads have called it, then releases them all; and it must work **round
   after round**. *Bug to avoid:* the reuse race — a fast thread charges into
   the next round and "uses up" the barrier before slow threads have left the
   previous one. Surviving reuse is the whole exercise; the stress test runs
   thousands of rounds precisely to catch it.
3. **`rwlock.c` — writer-preference read/write lock.** Many readers may hold it
   together, or one writer exclusively. Build it from a mutex and condition
   variables with **writer preference**: a waiting writer blocks *new* readers,
   so a steady stream of readers cannot starve a writer. *Invariant:* never a
   writer together with any reader or another writer. *Property to demonstrate:*
   readers still share (the test observes >1 concurrent reader) **and** a
   writer's wait is bounded (it is not starved).

---

## Hints

- **Part A.** `kernel/swtch.S` and `struct context` in `kernel/proc.h` are your
  template; `thread_switch` is the same shape. If a new thread faults the
  instant it starts, re-ask yourself written question 2: where does the first
  `ret` into it land, and what stack is it standing on when it gets there?
  Check the values you planted in `thread_create()` against your answer.
- **Part B, `cpuid()`.** It is only valid with interrupts disabled. The pattern
  is `push_off(); int c = cpuid(); ...; pop_off();`.
- **Part B, deadlock.** The golden rule is a consistent global lock order.
  Per-CPU kmem: settle your answer to Task B.1's lock-holding invariant
  question before you write the steal path. Bucketed bcache: the cross-bucket
  LRU scan is where a deadlock hides — settle your ordering rule before you
  write it, then check every path obeys it.
- **Part B, measuring.** `statistics()` gives cumulative counts since boot; the
  tests read it before and after their workload and report the *difference*.
  You can call it yourself from a small user program while debugging.
- **Part C, condition variables.** Always `while(!predicate) pthread_cond_wait`.
  Hold the mutex when you call `signal`/`broadcast` (simplest and always
  correct here).

---

## Deliverables

1. Your modified xv6 tree (or a patch): `user/uthread.c`,
   `user/uthread_switch.S`, `kernel/kalloc.c`, `kernel/bio.c`, `kernel/buf.h`.
2. Your `hostsync/{bbuffer,barrier,rwlock}.c` (from the copy you worked in).
3. `answers.md` — the three Part A written questions, plus your **before/after**
   kmem and bcache contended-acquire numbers at `CPUS=3`.
4. `tests/run.sh` passes on your tree (all three parts, including
   `usertests -q`).

---

## Rubric (100%)

| Task | Weight | What is graded |
|------|-------:|----------------|
| A — uthread | 25% | Threads interleave and each reaches 100 and exits; correct `thread_switch` (callee-saved + ra/sp); written questions. |
| B.1 — kmem | 20% | `kalloctest: OK` at CPUS=3: per-CPU lists, working steal, kmem contention far below stock; memory fully recoverable. |
| B.2 — bcache | 25% | `bcachetest: OK` at CPUS=3: bucketed cache, correct cross-bucket LRU eviction, no deadlock/double-cache, bcache contention far below stock. |
| C — pthreads | 30% | Each of bbuffer / barrier / rwlock passes its stress test (10% each): correct invariants, no lost wakeup, no barrier reuse bug, writer not starved. |
| Regression | gate | `usertests -q` must still pass at CPUS=3 — a kalloc/bcache change that breaks the kernel forfeits the Part B marks. |

The autograder prints a PASS/FAIL table and exits non-zero on any failure.
`QUICK=1` skips `usertests -q` while you iterate; the final check runs it.
