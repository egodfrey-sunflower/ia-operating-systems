# Exercise Sheet 12 — Locks and lock-based data structures

**Attempt after Week 12.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise12-solutions.md`](solutions/exercise12-solutions.md).

**This sheet leans on:** OSTEP ch. 28–29; xv6 book §7.6.

**You will need:** the OSTEP `x86.py` simulator from `ostep-homework/threads-locks/`.
No compiler is needed — §B3–B6 are pen-and-paper cost models. §B6 drills the
Mellor-Crummey & Scott paper assigned this week.

> **Note.** OSTEP ch. 29 has a "Homework (Code)" section but ships **no code** —
> there is no `threads-locks-usage` directory in either upstream repo. §B3–B4
> below supply that missing material.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. The justification is the answer.*

**A1.** A lock implementation is correct if it provides mutual exclusion.

**A2.** Disabling interrupts is a valid way to implement mutual exclusion on a
multiprocessor.

**A3.** A test-and-set spinlock is fair: threads acquire it roughly in the order
they arrived.

**A4.** Sleeping is always preferable to spinning, because spinning wastes CPU.

**A5.** `counter++` is a single expression in C, so it executes atomically.

**A6.** Replacing one coarse lock with many fine-grained locks improves
throughput.

**A7.** If the hardware provides an atomic compare-and-swap instruction, a lock
built from it needs no memory barriers.

**A8.** An approximate (sloppy) counter can return a value that was never the
true count.

---

## B. Mechanism and measurement

**B1. Why load/store alone fails.**
Run `python3 x86.py -p flag.s -t 2 -i 2 -R ax,bx -c` and study the trace.
  (a) Give the precise interleaving, in terms of which instruction each thread
      executes at each step, that puts **both** threads in the critical section.
  (b) Peterson's algorithm fixes this using only loads and stores. Given that it
      is provably correct, why does OSTEP nonetheless move immediately to
      hardware atomics rather than recommending it?

**B2. Fairness, measured.**
Run both `test-and-set.s` and `ticket.s` under `x86.py` with two and then four
threads, varying the interrupt interval (`-i`).
  (a) For test-and-set, construct an interrupt schedule under which one thread
      never acquires the lock. Is this a bug or a permitted behaviour?
  (b) Explain how the ticket lock's use of fetch-and-add makes that schedule
      impossible.
  (c) The ticket lock is fairer. Name a workload where you would nonetheless
      prefer test-and-test-and-set, and say why.

**B3. The approximate counter.** *(Material ch. 29 describes but does not supply.)*
An approximate counter keeps one local counter per CPU plus a global counter.
When a local counter reaches threshold `S`, it transfers its value to the global
counter under the global lock and resets to zero. Assume `P` CPUs.
  (a) At any instant, what is the maximum amount by which the global counter can
      understate the true total? Give it in terms of `S` and `P`.
  (b) For `N` total increments spread evenly across CPUs, how many global-lock
      acquisitions occur? How does this scale as `S` grows?
  (c) State the trade-off `S` controls, in one sentence.
  (d) You need a counter that is exact whenever read, but is read a thousand
      times less often than it is incremented. Does this design help? Propose a
      modification if not.

**B4. Hand-over-hand versus one big lock.** *(Also unsupplied by ch. 29.)*
Consider a sorted linked list of length `L` with a lookup that traverses on
average `L/2` nodes. Let `a` be the cost of one lock acquire+release, and `t` the
cost of examining one node.
  (a) Write the cost of one lookup under (i) a single list-wide lock, and
      (ii) hand-over-hand locking where each node has its own lock.
  (b) Hand-over-hand permits concurrency that the single lock does not. Under
      what relationship between `a`, `t` and the number of concurrent threads
      does hand-over-hand actually win?
  (c) OSTEP reports that hand-over-hand is usually slower in practice. Given your
      answer to (b), explain why — and identify which term dominates on real
      hardware and why it is larger than a naive count of instructions suggests.
  (d) Concurrent hash tables with per-bucket locks *do* scale well. What is
      structurally different about them?

**B5. When to spin, when to sleep.**
A two-phase lock spins briefly before sleeping. Let `C` be the cost of a context
switch (out and back), and `T` the time the lock is held by its current owner.
  (a) Derive the condition under which spinning for the whole of `T` is cheaper
      than sleeping.
  (b) The waiter cannot observe `T`. Give a spin bound that is never worse than
      twice the optimal choice, and justify it.

**B6. Where the waiters spin.** *(Mellor-Crummey & Scott, §2.4.)*
Both a test-and-set lock and a ticket lock make every waiter spin on **one
shared** location. The MCS lock instead gives each waiter its own `qnode` record
holding a `next` pointer and a `locked` flag; the lock variable itself is just a
pointer to the queue's **tail**. To acquire, a thread `fetch-and-store`s its own
qnode into the tail, and — if the queue was non-empty — sets its `locked` flag,
links itself behind its predecessor, and spins on **its own** flag until the
predecessor clears it.
  (a) With `P` threads contending, count the number of remote memory references
      (cache-line transfers) generated *per lock handover* under (i) a
      test-and-set lock, (ii) a ticket lock, and (iii) MCS. State any assumption
      you make about how many waiters observe a change to the shared location.
  (b) MCS's release path contains a second spin: if `I->next` is `nil` the
      releaser tries `compare-and-swap(L, I, nil)` and, if that fails, spins
      until `I->next` becomes non-`nil`. What race makes this spin necessary?
      Give the interleaving. Why is it nonetheless a *local* spin?
  (c) The paper notes that without `compare-and-swap` — with `fetch-and-store`
      alone — the lock loses its FIFO guarantee. Say why the release path cannot
      otherwise remove itself from the queue atomically.
  (d) MCS requires the caller to supply a qnode that stays live for the duration
      of the critical section. Name one setting in which that requirement is
      awkward, and say what a ticket lock offers instead.

---

## C. Discussion and design critique

**C1.** xv6 §7.6 argues a lock needs memory barriers even on hardware with atomic
instructions. Explain the distinction between **atomicity** and **ordering**, and
give a concrete failure: a critical section that is correctly mutually excluded
yet still produces a wrong result because of reordering. Say which reordering —
compiler or CPU — you are relying on, and how a barrier prevents it.

**C2.** Chapter 29's headline advice is: start with one big lock, measure, and
only refine if the measurement demands it. This is good engineering advice and
also, in some settings, bad advice. Identify a class of system where following it
would be a serious mistake, and explain what makes that setting different.

**C3.** *An intrepid engineer proposes the following.* "Our kernel has 200
different locks and we keep hitting deadlocks from inconsistent acquisition
order. I propose we delete all of them and replace them with a single global
kernel lock, acquired on entry to the kernel and released on exit. Deadlock
becomes impossible — you cannot have a cycle with one lock. Correctness is
trivially auditable. We lose a little throughput on multiprocessors, but we will
recover it later by reintroducing fine-grained locks only where profiling proves
they are needed."

Evaluate this proposal. Address specifically: is the deadlock claim actually
true; what happens to a system call that blocks while holding the lock; what does
"a little throughput" really mean as core counts grow; and is the incremental
recovery plan credible? Note that this is not a hypothetical — say what you know
about the historical precedent. Conclude with a recommendation and the conditions
that would change it.
