# Exercise Sheet 13 — Condition variables and monitors

**Attempt after Week 13.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise13-solutions.md`](solutions/exercise13-solutions.md).

**This sheet leans on:** OSTEP ch. 30, App. C–D; OSPP §5.5; Lampson & Redell
(1980), *Experience with Processes and Monitors in Mesa*.

**You will need:** a C compiler and the `ostep-homework/threads-cv/` programs
for §B1–B2 (`main-two-cvs-while.c` and its deliberately broken siblings, plus
the sleep-string harness — read the README for the `-p/-c/-m/-l/-C/-P` flags).
§B3–B4 and §C are pen-and-paper.

> **Note.** Appendices C and D ship no homework in either upstream repo —
> §B3, §B4 and §C2 are original material built on Appendix D and the Lampson
> & Redell paper.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns
nothing — the justification is the answer.*

**A1.** `pthread_cond_signal()` guarantees that the woken thread is the next
thread to run.

**A2.** Holding the lock while calling `wait()` is a style recommendation,
like holding it while calling `signal()`.

**A3.** A condition variable remembers a signal: if a thread signals before
any thread is waiting, the next thread to call `wait()` returns immediately.

**A4.** Once every wait is wrapped in a `while` loop, a single condition
variable suffices for a correct multi-producer/multi-consumer bounded buffer.

**A5.** Under Mesa semantics, replacing every `signal()` in a correct program
(one that waits in `while` loops) with `broadcast()` can change its
performance but not its correctness.

**A6.** Spurious wakeups are a bug in the threading library, and correctly
written application code may assume they do not happen.

**A7.** Under Hoare semantics, the `if`-based bounded-buffer monitor of
Appendix D (Figure D.3) is correct as written.

**A8.** The mutex passed to `pthread_cond_wait()` exists to protect the
condition variable's internal queue of sleeping threads.

---

## B. Forcing the races

**B1. The `if` bug, on demand.**
Study `main-two-cvs-if.c` — the two-CV producer/consumer with `if` instead of
`while`.
  (a) With one producer and **one** consumer, can this code fail? Argue from
      the code, then check your answer with the harness.
  (b) With one producer and **two** consumers, reconstruct on paper the
      interleaving from ch. 30 (Figure 30.9) in which a consumer returns from
      `wait()` and calls `get()` on an empty buffer. Give the sequence of
      labelled lines (p1–p6, c1–c6) executed by each thread, and mark the
      exact moment the woken consumer's view of the world becomes stale.
  (c) Now force it: give sleep strings (`-C` per consumer) that make the
      failure happen reliably, and explain what each inserted sleep is
      standing in for in a production system.

**B2. One condition variable, and timing predictions.**
  (a) `main-one-cv-while.c` waits in `while` loops but has a single condition
      variable. Argue why one producer plus **one** consumer cannot deadlock,
      then give the schedule (à la Figure 30.11) by which one producer plus
      **two** consumers puts every thread to sleep forever.
  (b) Now the working code, `main-two-cvs-while.c`, with one producer, three
      consumers, ten items (`-l 10`), and each consumer forced to sleep 1 s
      at point c3 (just after waking). **Predict** the total run time, for a
      single-entry buffer (`-m 1`) and then for `-m 3`. Does the larger
      buffer help? Why or why not?
  (c) Move the 1 s sleep to point c6 (after the unlock). Predict both timings
      again, then run all four configurations and reconcile prediction with
      observation. State the general principle your reconciliation reveals
      about *where* a thread spends time relative to the lock.

**B3. Hoare vs Mesa, by hand.** *(Appendix D material — no code supplied
upstream.)*
Take Appendix D's bounded-buffer monitor with `if` checks (Figure D.3),
`MAX = 1`, threads Con1, Con2, Prod, and run it under **Mesa** rules
(signal moves a waiter to ready; signaller keeps the lock and keeps running).
  (a) Complete a queue trace in the style of Figure D.6 — columns for the
      running thread, the monitor-lock queue, each CV queue, and
      `fullEntries` — for the schedule: Con1 runs first, then Prod runs until
      it gives up the CPU, then Con2 runs to completion, then Con1 resumes.
      Identify the exact step at which Con1's assumption fails.
  (b) State the two-line fix, and explain why the same fixed code also runs
      correctly (merely wastefully) under Hoare semantics.
  (c) Lampson and Redell list four kinds of queue a thread can occupy; three
      arise from monitor execution (the fourth is an OS **fault** queue, not
      exercised here). Name those three, and say which queue Con1 is on at each phase of your trace
      in (a).

**B4. Covering conditions, costed.**
Consider Appendix D's memory allocator monitor: `allocate(size)` waits while
`size > available`; `free()` adds to `available` and wakes.
  (a) With `available = 0`, thread T₁ calls `allocate(100)`, then T₂ calls
      `allocate(10)`; then T₃ calls `free(50)`. Explain precisely what can go
      wrong if `free()` uses `signal()`, and why no correct choice of *which
      single thread to wake* can be made by the code as written.
  (b) The Lampson–Redell fix is `broadcast()`. Suppose `N` threads are
      blocked in `allocate()` and each `free()` lets exactly one of them
      proceed. Count the wakeups and the re-sleeps per successful allocation,
      and give the total number of context switches caused by `M` frees as a
      function of `M` and `N`.
  (c) Give one condition under which that cost is a perfectly good deal, and
      one system where it would not be — with the property of the system that
      flips the verdict.
  (d) Propose a design that gets the correctness of broadcast without waking
      `N` threads, and state what new information your design requires the
      monitor to track.

---

## C. Discussion and design critique

**C1. A discipline, applied.**
This week's reading — ch. 30's tips, Appendix D, and OSPP §5.5 — amounts to a
checklist for writing monitor-style shared objects. Write down your checklist
(aim for five or six rules, each one sentence). Then apply it to this buggy
blocking queue, identifying **which rule** each defect violates and what
failure it produces:

```c
int queue[MAX]; int count = 0;
pthread_mutex_t m; pthread_cond_t c;

void enqueue(int x) {
    if (count == MAX)              // E1
        pthread_cond_wait(&c, &m); // E2
    pthread_mutex_lock(&m);        // E3
    queue[count++] = x;            // E4
    pthread_mutex_unlock(&m);      // E5
}
int dequeue(void) {
    pthread_mutex_lock(&m);        // D1
    while (count == 0)
        ;                          // D2  (spin, releasing nothing)
    int x = queue[--count];        // D3
    pthread_cond_signal(&c);       // D4
    pthread_mutex_unlock(&m);      // D5
    return x;
}
```

**C2. Defend the deprecated design.**
OSTEP files monitors in an appendix stamped "(Deprecated)", and its dialogue
calls them "an old concurrency primitive". Hoare semantics fared even worse:
"hard to realize in a real system", abandoned by Mesa, and used by
essentially nobody since.

Make the best case **for** the losing side — in two parts, each ending in a
judgement with conditions attached:
  (a) *For the monitor as a language construct.* What classes of bug from
      this week's reading become impossible when the compiler, rather than
      the programmer, pairs the lock with the data? What evidence from a
      language you know suggests this design in fact won? When would you
      still prefer bare locks and CVs?
  (b) *For Hoare semantics.* Identify what genuinely gets better when
      `signal` transfers control immediately — think about what the woken
      thread may assume, what that does to reasoning and proofs, and what it
      does to the *latency* between a condition becoming true and the waiter
      acting on it. Then state the implementation and scheduling costs that
      made Mesa the winner, and describe one setting where you would accept
      those costs today.
