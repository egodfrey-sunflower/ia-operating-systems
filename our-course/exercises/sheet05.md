# Examples Sheet 5 — Concurrency I: Race Conditions and Mutual Exclusion **[ext]**

**Attempt after Week 6.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet05-answers.md` (spoilers — attempt first). **All new
material — no IA equivalent.**

**Reading this sheet leans on** (see `../reading-list.md`): OSTEP ch. 25–29
(concurrency intro, the thread API, locks, lock-based data structures); the xv6
book ch. 6 (locking). For the memory-model reasoning in Section C, read Jeff
Preshing's *"Memory Barriers Are Like Source Control Operations"* (or an
equivalent short memory-reordering explainer) — the assigned OSTEP chapters
assume sequential consistency and do not cover the reorderings that break
Peterson's algorithm on real hardware. For Section D's lock-free/ABA question
(§D(c)), read Preshing's *"An Introduction to Lock-Free Programming"*, which
names and diagrams the ABA problem and the compare-and-swap loop it breaks —
the assigned OSTEP chapters cover only lock-*based* structures, whereas **A&D
§6.6 (Non-Blocking Synchronization)** gives the lock-free/wait-free foundation
and the CAS-based structures the question builds on. For the multiprocessor-lock
and cache-coherence reasoning in Section F, lean on **A&D §5.7 and §6.1–6.3**
(implementing multiprocessor spinlocks, test-and-test-and-set, cache-coherence
traffic, and MCS locks) — the measured test-and-set vs test-and-test-and-set vs
MCS contention curve there is exactly what §F asks you to explain.
For Section G (priority inversion), the assigned week-6 reading is Mike Jones,
*"What Really Happened on Mars"* — the Mars Pathfinder retrospective, which
describes both the priority-inversion failure and the priority-inheritance fix
that resolved it.

---

## A. Warm-ups (true/false with justification)

**A1.** "A data race can always be removed by making each individual statement in
the critical section a single machine instruction."

**A2.** "A spinlock is strictly worse than a blocking (sleeping) lock, because
busy-waiting wastes CPU."

**A3.** "Peterson's algorithm gives correct mutual exclusion for two threads, so
it is a practical way to build a lock on a modern multicore machine."

**A4.** "Test-and-set (TAS) and compare-and-swap (CAS) are interchangeable: any
lock built from one can be built from the other."

---

## B. Identifying the race (short C fragment)

Two POSIX threads run the following, sharing the global `balance`, which starts
at `0`. Each thread runs the loop 1 000 000 times.

```c
int balance = 0;                 /* shared */

void *worker(void *arg) {
    for (int i = 0; i < 1000000; i++) {
        balance = balance + 1;   /* the critical line */
    }
    return NULL;
}
```

- (a) The final `balance` is almost never `2000000`, and varies run to run.
  Explain precisely *why*, by expanding `balance = balance + 1` into the three
  machine steps (load, add, store) and giving an interleaving of the two threads
  that *loses* an update. What is the smallest possible final value, in
  principle?
- (b) Define *race condition*, *critical section*, and *data race*, and say which
  of the three this code exhibits.
- (c) State the three properties any correct mutual-exclusion solution must
  provide (mutual exclusion, progress/deadlock-freedom, bounded
  waiting/starvation-freedom), and for each, give a one-line test the balance
  example's "fix" must pass.
- (d) Wrapping the critical line in a `pthread_mutex_t` fixes it. But now suppose
  a maintainer "optimises" by taking the lock only *around the store*, not the
  load. Does that fix the race? Explain with an interleaving.

---

## C. Peterson's algorithm

The two-thread algorithm, with shared `flag[2]` (init `{false, false}`) and
`turn`:

```
/* thread i, other = 1 - i */
flag[i] = true;
turn    = other;
while (flag[other] && turn == other)
    ;               /* spin */
/* --- critical section --- */
flag[i] = false;
```

- (a) **Mutual exclusion.** Argue that both threads cannot be in the critical
  section at once. (Consider the last writer of `turn`, and the values of
  `flag[other]` and `turn` each thread must have observed to exit its `while`.)
- (b) **The turn variable earns its keep.** Delete the `turn` line and the
  `turn == other` conjunct, leaving only `flag`. Exhibit an interleaving that
  either deadlocks or violates mutual exclusion, and say which property breaks.
- (c) **The flag variable earns its keep.** Now instead keep `turn` but delete
  the `flag` handshake (spin only on `turn != i`). What property breaks now, and
  under what workload (hint: what if one thread never wants the lock)?
- (d) **Why it fails on real hardware.** Peterson's proof assumes every thread
  sees others' reads and writes to `flag`/`turn` in program order (sequential
  consistency). Modern machines do *not* guarantee that. Identify **two
  distinct ways** — one in the hardware, one in the toolchain — that real
  systems violate this assumption for code like the above. State which
  reordering lets both threads enter the critical section, pointing at the
  specific pair of accesses in the code that must not be swapped, and propose
  a fix.

---

## D. Hardware primitives: TAS vs CAS vs LL/SC

- (a) Give the atomic semantics (as if in one indivisible step) of
  **test-and-set**, **compare-and-swap(addr, expected, new)**, and
  **load-linked / store-conditional (LL/SC)**.
- (b) Write a minimal spinlock `acquire`/`release` using TAS, and another using
  CAS. Why can TAS build only the simplest lock, whereas CAS (or LL/SC) can build
  lock-free stacks, counters, and queues? Give one thing CAS can express that a
  bare TAS cannot.
- (c) CAS has the *ABA problem*; LL/SC does not. Describe an ABA interleaving on
  a lock-free stack pop, and explain why LL/SC's "was this location written since
  I linked it?" semantics avoids it where CAS's "does it still equal A?" does
  not.
- (d) Consensus/expressiveness aside, name one *practical* reason real ISAs
  differ: RISC-V and ARM offer LL/SC-style primitives while x86 centres on a
  CAS-like `cmpxchg`. What does LL/SC make hard to guarantee (progress) that a
  single `cmpxchg` instruction sidesteps?

---

## E. Spinlock vs sleeping lock: the trade-off calculation

A critical section is protected by a lock. When a thread finds the lock held it
must either **spin** (busy-wait, burning CPU until the holder releases) or
**block** (sleep; the scheduler runs someone else, then wakes it later). Use
these costs:

- One context switch costs **Cs = 4 µs** of CPU.
- Blocking therefore costs **2·Cs = 8 µs** of wasted CPU (switch out on the way
  to sleep, switch back on wake-up).
- A waiter that spins burns CPU equal to the time it waits.

- (a) **Break-even.** For what expected wait time `W` is spinning cheaper (in
  wasted CPU) than blocking? Derive the threshold.
- (b) The lock is held for an average of `H` µs, and a contending thread arrives
  at a uniformly random point during the hold, so its expected remaining wait is
  `H/2`. For each of `H = 2 µs`, `H = 5 µs`, `H = 20 µs`, decide spin-or-block
  from your threshold and give the wasted CPU each way. Tabulate.
- (c) On a *uniprocessor*, spinning to wait for a lock held by *another,
  descheduled* thread is not just wasteful but can be catastrophic. Explain why,
  and state the rule this implies: *never spin on a uniprocessor for a lock held
  by a preempted thread.* Why does the calculus change on a multiprocessor?
- (d) Real locks (e.g. Linux mutexes, `pthread` adaptive mutexes) *spin then
  block*. Explain how spinning briefly *before* blocking captures the best of
  your table in (b) — for short holds you pay `≈ H/2`, and you only pay the
  `2·Cs` block cost when the hold turns out to be long.

---

## F. Test-and-test-and-set (TTAS)

A plain TAS spinlock spins by *repeatedly executing the atomic TAS* on the lock
word.
- (a) On a cache-coherent multiprocessor, why is that catastrophic when `N`
  cores contend? Describe the cache-coherence traffic each spinning TAS
  generates and how it scales with `N`.
- (b) **Test-and-test-and-set** spins on an ordinary *read* (`while (locked)
  ;`) and only issues the atomic TAS when the read says the lock looks free.
  Explain how this cuts the coherence traffic while spinning (the read hits in
  the local cache in the *Shared* state; no bus transaction until the lock is
  released). What traffic storm still happens *at the moment of release*, and
  which lock design (name one: ticket lock, MCS lock) fixes even that?

---

## G. Priority inversion

- (a) **Define** priority inversion. Give the minimal three-task scenario
  (high-, medium-, low-priority tasks and one shared lock) in which a *high*-
  priority task is blocked, indirectly, by an *unrelated medium*-priority task.
  Draw the timeline.
- (b) **Mars Pathfinder (1997).** Recount, in a few sentences, what happened on
  the Pathfinder lander: a high-priority bus-management task, a low-priority
  meteorological task sharing a mutex, medium-priority comms tasks, and a
  watchdog that kept resetting the spacecraft. Which of the three tasks in your
  (a) scenario maps to which real task?
- (c) **Priority inheritance** was the fix (uploaded to Mars). State the rule: a
  task holding a lock temporarily inherits the priority of the highest-priority
  task waiting on that lock. Show, on your (a) timeline, how inheritance bounds
  the high-priority task's blocking to (at most) one low-priority critical
  section. Name one alternative protocol (the *priority ceiling* protocol) and
  one line on how it differs.
- (d) Connect back to Sheet 4: EDF/RM schedulability tests (Sheet 4, §D) assume
  tasks are *independent*. Priority inversion is exactly what breaks that
  assumption. In one sentence, what must a schedulability analysis add once tasks
  share locks (a *blocking term* `Bᵢ` in the response-time equation)?

---

## Past paper questions

This is extension material (**[ext]**) with no IA or Tripos equivalent, so
this directory's `README.md` allocates no past-paper questions here. For mechanical
drill on locks and races, re-run the OSTEP concurrency homework (ch. 25–28
`x86.py` race simulator) and carry the mutual-exclusion reasoning forward into
**Lab 5** (threads & locks) and Sheet 9 (concurrency II).
