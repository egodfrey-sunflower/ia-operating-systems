# Exercise Sheet 14 — Semaphores, concurrency bugs, and deadlock

**Attempt after Week 14.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise14-solutions.md`](solutions/exercise14-solutions.md).

**This sheet leans on:** OSTEP ch. 31–32; OSPP §6.5 (Banker's algorithm);
Lu et al. (2008), *Learning from Mistakes*.

**You will need:** nothing but pen and paper. (The `threads-sema/` and
`threads-bugs/` code homework is exercised in lab 5, not here; §B1(d) can be
checked against `threads-sema/rendezvous.c` if you want the satisfaction.)

> **Note.** §B4 is deliberately the heaviest question on the sheet. The
> Banker's algorithm appears only in the OSPP cross-reading — OSTEP names it
> and moves on — and it is classic Cambridge examinable material.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns
nothing.*

**A1.** A good rule for choosing a semaphore's initial value is: the number
of resources you are willing to give away immediately.

**A2.** In every semaphore implementation, a negative value equals the
number of waiting threads.

**A3.** Unlike signalling a condition variable, `sem_post()` before any
thread has called `sem_wait()` is not lost.

**A4.** Deadlock requires all four of: mutual exclusion, hold-and-wait, no
preemption, and circular wait — so defeating any one of them suffices to
prevent it.

**A5.** According to the study underlying ch. 32, most real-world
concurrency bugs are deadlocks.

**A6.** An atomicity-violation bug can always be fixed by protecting every
individual access to the shared variable with a lock.

**A7.** The `trylock()`-based two-lock acquisition protocol removes the
possibility of deadlock, and therefore removes the possibility of threads
failing to make progress.

**A8.** Deadlock avoidance in the Banker's style is a practical
general-purpose technique that mainstream OS kernels use for their internal
locks.

---

## B. Working the mechanisms

**B1. Bounded buffer, three ways to get it wrong.**
Recall ch. 31's producer/consumer with semaphores `empty` (init `MAX`),
`full` (init 0), and a binary `mutex` (init 1).
  (a) In the broken version the mutex is acquired *before* `sem_wait(&empty)`
      / `sem_wait(&full)`. Give the exact two-thread interleaving that
      deadlocks, and name which of the four deadlock conditions each thread
      is exhibiting at the moment of deadlock.
  (b) The fix moves the mutex inside. State precisely which deadlock
      condition the fix removes, and why the `empty`/`full` waits outside
      the mutex cannot themselves create a cycle.
  (c) With `MAX = 1` and a single producer and consumer, is the `mutex`
      needed at all? Justify from what `empty` and `full` already guarantee.
  (d) *Ordering warm-down:* give initial semaphore values and the placement
      of `wait`/`post` calls for a **rendezvous**: threads A and B each
      reach a point P and neither may pass it until the other has arrived.

**B2. Reader-writer locks.**
Using ch. 31's reader-writer lock (Figure 31.13):
  (a) Trace the values of `readers` and the `lock`/`writelock` semaphores
      for the sequence: R1 acquires read, R2 acquires read, W1 requests
      write, R1 releases, R2 releases, W1 acquires. At which step does W1
      actually block, and on what?
  (b) Show, with a schedule, how a continuous stream of readers starves a
      writer indefinitely.
  (c) Sketch the change that prevents (b) — you need only say what a writer
      must be able to do to *arriving* readers, not write the code.
  (d) OSTEP cautions that reader-writer locks "often do not end up speeding
      up performance". Give two distinct reasons why a plain lock can beat
      one in practice.

**B3. Dining philosophers.**
  (a) In the broken all-grab-left-first solution, exhibit the deadlocked
      state and check all four conditions hold in it.
  (b) Philosopher 4 grabs right-then-left; the rest grab left-then-right.
      Prove no deadlock is possible, by arguing that a cycle of waiting
      cannot close.
  (c) With five philosophers, what is the maximum number simultaneously
      eating, and does the fix in (b) reduce it?

**B4. The Banker's algorithm, worked.** *(OSPP §6.5 material.)*
A system has three resource types A, B, C with total units
`E = (6, 5, 7)`. Four processes have declared maxima and current
allocations:

| | Allocation (A,B,C) | Max (A,B,C) |
|---|---|---|
| P1 | (1, 1, 2) | (3, 2, 5) |
| P2 | (2, 1, 1) | (4, 2, 2) |
| P3 | (1, 0, 2) | (3, 1, 4) |
| P4 | (0, 2, 0) | (2, 4, 2) |

  (a) Compute the Available vector and each process's Need matrix.
  (b) Is the state **safe**? If so, exhibit a safe sequence and show the
      Available vector after each hypothetical completion; if not, show why
      no sequence exists. Are there processes that could equally well have
      gone first?
  (c) P2 now requests `(1, 0, 1)`. Run the algorithm: state each check, and
      say whether the request is granted.
  (d) Instead (from the original state in (a)–(b)), P4 requests `(1, 0, 1)`.
      Run the algorithm again and give the verdict.
  (e) One of (c)/(d) is refused even though granting it would not
      *immediately* deadlock anything. Explain the difference between an
      **unsafe** state and a **deadlocked** state, and what the refusal is
      actually insuring against.
  (f) State the two pieces of information the Banker's algorithm requires
      that a general-purpose OS does not have, and name the class of system
      where both are available.

---

## C. Discussion and design critique

**C1. What the bug study buys you.**
Lu et al. examined 105 real concurrency bugs: 74 non-deadlock, of which 97%
were atomicity or order violations; 31 deadlock.
  (a) You have budget to build exactly one automated bug-finding tool for
      your company's large concurrent codebase. Using the numbers, argue
      what the tool should look for and what it may safely ignore.
  (b) A natural fix for an order violation is to make the late thread wait on
      a condition variable until the other has run — the fix OSTEP ch. 32
      recommends (though the Lu study found teams more often reached for ad-hoc
      `while`-flags). Connect this to week 13: which CV rule, if skipped in that
      fix, converts an order-violation bug into a lost-wakeup bug?
  (c) The study drew its bugs from MySQL, Apache, Mozilla, and OpenOffice.
      Give one reason its proportions might *not* transfer to an OS kernel,
      and say what you would measure to check.

**C2.** *An intrepid engineer proposes the following.* "Deadlock keeps
biting us, and lock-ordering documentation rots. I propose we make the
kernel deadlock-free by construction: every subsystem declares, at build
time, the maximum set of locks any of its operations can hold, and at run
time a central allocator applies the Banker's algorithm to every lock
acquisition — the acquire is granted only if the resulting state is safe.
Deadlock becomes impossible, the ordering documentation can be deleted, and
unlike a lock hierarchy, nothing need be redesigned when a new subsystem is
added: it just declares its claims."

Evaluate this proposal. Address specifically: whether "declares the maximum
set of locks" is realistic for kernel code paths (think about what ch. 32
says encapsulation does to lock knowledge); what the per-acquisition safety
check costs on the hot path and how that compares with what a lock acquire
costs now; what conservatism of the safe-state test does to concurrency;
whether deleting the ordering discipline actually removes the thing that
made the codebase auditable; and where a Banker's-style scheme genuinely
does earn its keep. Conclude with a recommendation and the conditions under
which it would flip.

**C3. One primitive to rule them all.**
Ch. 31 notes some programmers use semaphores *exclusively*, shunning locks
and condition variables — and then closes with Lampson's warning against
generalization and the observation that building CVs out of semaphores
defeated even highly experienced programmers. Make the strongest case
**against** offering the semaphore as a system's only synchronization
primitive, and then say what the strongest case *for* it is. Which would you
ship in a new OS's threading API, and what about your intended users would
change your answer?
