> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 13 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** Under Mesa semantics — which is what pthreads and virtually
every built system provide — `signal()` merely moves one waiter from the
condition queue to the ready queue. It is a *hint* that the state changed.
The signaller keeps running (and keeps the lock, if held); when the woken
thread eventually runs, the state may have changed again. This is exactly why
the woken thread must re-check. (Hoare semantics *do* run the woken thread
immediately — the point of §C2b — but no mainstream system implements them.)

**A2. FALSE.** The two cases differ in kind. Holding the lock while
*signalling* is a tip — usually right, occasionally optional. Holding it
while *waiting* is mandated by the semantics of `wait()`: the call assumes
the lock is held, releases it while putting the caller to sleep
(atomically), and re-acquires it before returning. Without that contract
the check-then-sleep window reopens and wakeups are lost (see A8, B1).

**A3. FALSE.** Condition variables are stateless: a signal with no waiter is
lost. That is ch. 30's "no state variable" broken join (Figure 30.4): the
child signals first, the parent then waits forever. It is precisely why every
CV must be paired with an explicit state variable (`done`, `count`, …) that
records what happened. (Contrast with next week's semaphore, whose internal
value *does* remember posts.)

**A4. FALSE.** The `while` fix and the two-CV fix address two different bugs.
With one CV, a consumer's `signal()` can wake *another consumer* rather than
the producer; the woken consumer re-checks (thanks to `while`), finds the
buffer empty, and sleeps again — and now every thread is asleep (Figure
30.11). `while` protects each waiter against stale state; it cannot make the
signal reach the right *kind* of thread. Directed signalling needs two CVs
(or a broadcast — see A5).

**A5. TRUE.** Because every waiter re-checks its condition in a `while`,
waking too many threads is harmless: the extras re-check, find the condition
false, and go back to sleep. Appendix D says this outright — under Mesa
semantics broadcast is always correct. The cost is performance: needless
wakeups and context switches (the thundering herd, costed in B4). OSTEP's
advice: if your program only works when signals become broadcasts, you
probably have a bug — but for covering conditions broadcast is the right
tool.

**A6. FALSE.** Real thread packages permit two threads to wake from one
signal, as ch. 30 notes; the pthreads specification allows it. Correct code
must tolerate it, and does so for free if it waits in a `while` loop — the
spuriously woken thread re-checks and sleeps again. This is an independent
reason for the `while` rule, on top of Mesa semantics.

**A7. TRUE.** Under Hoare semantics `signal()` immediately transfers control
— and the monitor lock — to the woken thread, which runs before anything else
can enter the monitor. So when the woken consumer resumes at C1, the
condition established by the signaller still holds; no third party can sneak
in between. That is why Hoare's original solution uses `if` and is correct.
The same code breaks the moment the semantics weaken to Mesa (B3's trace).

**A8. FALSE.** The library protects its own queue internally. The mutex
argument protects the *shared program state the condition is about* — it
closes the gap between checking the state and going to sleep. Without it, a
waiter can test `done == 0`, be preempted, miss the signal, then sleep
forever (Figure 30.5). The reason `wait()` takes the mutex as a parameter is
so it can release it and sleep *atomically*, then re-acquire before
returning.

---

## B. Forcing the races

**B1.**
**(a) No — it cannot fail with one producer and one consumer.** With only two
threads there is no third thread to change the state between a signal and the
woken thread running. If the consumer waits at c3, only the producer can wake
it, and the producer wakes it only having just made `count == 1`; nobody else
can consume that item first. (The `if` is a loaded gun, but with 1p/1c nobody
pulls the trigger.) The harness confirms: the failure needs `-c 2`.

**(b)** The Figure 30.9 interleaving, with two consumers Tc1, Tc2 and
producer Tp:

1. Tc1: c1, c2 (finds `count == 0`), c3 — **sleeps on `fill`**, releasing
   the lock.
2. Tp: p1, p2 (buffer not full), p4 — `put()`, `count = 1`; p5 — signals:
   **Tc1 moves to the ready queue** (not running); p6; loops: p1, p2 —
   buffer full — p3, sleeps.
3. Tc2: c1, c2 — finds `count == 1`, **skips the wait entirely** — c4
   `get()`, `count = 0`; c5, c6; exits or loops.
4. Tc1 finally runs: it returns from `wait()` at c3 and — because the check
   was an `if` — proceeds straight to c4, calling `get()` on an empty
   buffer. Assertion fires.

The staleness point is **step 3**: the instant Tc2 executes c4, the state
that Tp's signal reported to Tc1 ("there is an item") is no longer true —
but Tc1, already past its check, has no way to notice.

**(c)** Any sleep string that reproduces the (b) staleness on demand: it must
let a second consumer take the single item in the gap between the producer's
signal and the woken consumer actually running. One reliable recipe with two
consumers and a one-slot buffer (`-p 1 -c 2 -m 1 -l 3`):
`-C 0,0,1,0,0,0,0:0,0,0,0,0,0,0` — the first consumer (the harness calls it
**C0**) pauses 1 s at **c2**, the point right after the empty-buffer check
(index 2 in the string, per the README's `c0…c6` numbering); the second
consumer and the producer run eagerly. The pause delays a consumer *around*
its wait so that the producer and the other consumer queue on the mutex, the
other consumer barges in and consumes the item, and the consumer that returns
from `Cond_wait` at c3 then walks straight into `do_get()` on the now-empty
buffer — `error: tried to get an empty buffer`, ten runs out of ten (the
`0,0,0,1,0,0,0` placement at c3 is equally reliable). Note there is no sleep
point *inside* `Cond_wait` itself, so the pause works by delaying a consumer
around its wait, never by freezing it mid-wait. The inserted sleep stands in
for anything that delays a runnable thread in production: a timeslice
expiring, an interrupt, running on a loaded machine, a page fault. Credit for
the observation that the bug's probability, not its possibility, is what the
sleep changes — which is why such bugs pass tests and fail in the field.

**B2.**
**(a)** *One producer + one consumer:* with `while` re-checking, the only
hazard left is a signal reaching the wrong kind of thread — and with only one
other thread, there is no wrong kind. Every signal from the producer can only
wake the consumer, and vice versa; and the state variable means an early
signal is never needed (the other thread checks `count` first).
*Two consumers:* the Figure 30.11 schedule. Tc1 and Tc2 both sleep (c3).
Tp fills the buffer, signals — wakes Tc1 — then loops, finds the buffer full,
and sleeps. Tc1 wakes, re-checks (fine), consumes (c4), then signals — and
the signal, on the single shared CV, **wakes Tc2 instead of Tp**. Tc2
re-checks, finds `count == 0`, sleeps. Tc1 loops, finds `count == 0`, sleeps.
All three threads are now asleep; nobody can ever signal. Deadlock by
mis-directed wakeup.

**(b)** With the sleep at **c3** the consumer sleeps *just after waking,
while holding the lock* (it re-acquired it inside `wait()`). Everything
serialises behind that lock: no other consumer can consume, the producer
cannot produce. Each item costs one serialised 1 s sleep — and so does each
of the three END_OF_STREAM markers the harness pushes through the same buffer
to shut the three consumers down, because consuming an EOS also wakes a
consumer at c3. Ten items plus three shutdown markers ⇒ ≈ **13 s**
(occasionally 12, when a consumer finds the buffer already non-empty and skips
its wait); `-m 3` changes essentially nothing, because the bottleneck is the
lock hold time, not buffer space. (Accept ~12–13 s. A student who forgets the
shutdown markers predicts ~10 s — worth flagging the ~3 s of shutdown cost
explicitly.)

**(c)** With the sleep at **c6** the consumer sleeps *after releasing the
lock*. Sleeps now overlap: three consumers can be sleeping their 1 s
simultaneously while a fourth item is produced and consumed. Counting the
three END_OF_STREAM shutdown markers alongside the ten items, 13 wakeups
spread over three consumers means the busiest consumer does ⌈13/3⌉ = 5 of
them ⇒ ≈ **5 s** (accept ~4–5 s). Buffer size `-m 3` again makes little
difference — the producer was never the bottleneck. The principle: **time
spent holding a lock serialises the whole system; the same work moved outside
the critical section parallelises.** This is ch. 29's guidance to keep
critical sections short — its move-`malloc()`-outside-the-lock discussion —
made measurable, and the marking note is that the *qualitative* split
(serialised vs overlapped) matters more than the exact seconds.

**B3.**
**(a)** Trace (M = monitor-lock queue; `full`/`empty` = CV queues; FE =
`fullEntries`; `MAX = 1`):

| t | Running | Line | M | full | empty | FE | Comment |
|---|---------|------|---|------|-------|----|---------|
| 1 | Con1 | C0 | – | – | – | 0 | buffer empty |
| 2 | Con1 | C1 | – | Con1 | – | 0 | Con1 waits on `full` |
| 3 | Prod | P0 | – | Con1 | – | 0 | no wait needed |
| 4 | Prod | P2–P4 | – | Con1 | – | 1 | fills buffer |
| 5 | Prod | P5 | – | – | – | 1 | signal: **Con1 → ready**, Prod keeps running (Mesa) |
| 6 | Con2 | C0 | – | – | – | 1 | finds FE = 1, skips wait |
| 7 | Con2 | C2–C4 | – | – | – | 0 | **consumes** — Con1's item |
| 8 | Con2 | C5–C6 | – | – | – | 0 | signals `empty` (no waiter), returns |
| 9 | Con1 | C2 | – | – | – | 0 | resumes after C1 — `if` means **no re-check** |
| — | | | | | | | reads `buffer[use]` with FE = 0: garbage/underflow |

The failing step is **t = 9**: Con1 acts on the world as it was at t = 5.
Between its wakeup and its running, Con2 (t = 7) invalidated the condition.

**(b)** Change P0 and C0 from `if` to `while` (Figure D.5). Under Hoare
semantics the `while` merely re-tests a condition that is guaranteed still
true — one wasted comparison, no behavioural change. So `while` is correct
under *both* semantics, which is why "always use `while`" is stated
unconditionally.

**(c)** The three monitor queues: the **ready queue** (runnable threads), the
**monitor-lock queue** (threads waiting to enter the monitor), and a
**condition-variable queue per CV** (threads that waited). (Lampson & Redell
list a fourth — the **fault queue**, for a process temporarily unable to run
because of an OS fault; it is not part of monitor semantics, so it plays no
role here.) Con1's journey:
running → `full`'s CV queue (t = 2–4) → ready queue (t = 5–8, after the
signal) → running (t = 9). Note Con1 never visits the monitor-lock queue in
this schedule; it would if another thread held the monitor when it woke —
re-acquiring the lock inside `wait()` is what that queue is for.

**B4.**
**(a)** `signal()` wakes *one* thread, chosen by the queue, not by fitness.
`free(50)` may wake T₁ (needs 100); T₁ re-checks `100 > 50`, sleeps again —
and T₂, which needs only 10 and could have proceeded, was never woken. No
correct single choice is possible *by this code* because the signaller does
not know the waiters' sizes: the information needed to choose lives in each
waiter's local `size` argument, invisible to `free()`. This is Lampson and
Redell's covering-condition situation.

**(b)** Each `free()` broadcasts: **N wakeups**, of which 1 proceeds and
**N − 1 re-check and re-sleep**. Each wakeup is a context switch in and (for
the losers) back out: ≈ N switches in, N − 1 back to sleep — Θ(N) switches
per free, so **Θ(M · N)** for M frees, versus Θ(M) with perfectly targeted
signals. (Accept any count within a constant factor with correct reasoning.)

**(c)** Good deal: N is small, frees are rare, or correctness/simplicity
dominates — a memory allocator in an application with a handful of threads
loses nothing measurable. Bad deal: N is large and the wakeup path is hot —
e.g. a server where thousands of request threads block on one resource pool;
Θ(N) switches per release collapses throughput. The property that flips the
verdict is the **product of waiter count and release frequency** — the herd
size times how often it stampedes.

**(d)** Give the monitor the information it lacks: track waiters' requests.
Two standard shapes: (i) an ordered list of (size, private CV) pairs — on
`free()`, scan and signal only threads whose requests now fit (this is
"specific notification": one CV per waiter or per size class); (ii) maintain
waiters sorted (say smallest-first) and signal the head while it fits. New
requirements: per-waiter state registered before sleeping, and a policy
decision (smallest-first starves nobody? largest-first?) that the broadcast
design never had to make. Credit for noticing the fix trades wakeup cost for
bookkeeping cost plus a *policy* obligation.

---

## C. Discussion and design critique

**C1.** A serviceable checklist (wording may vary; substance should not):

1. **One lock guards the object's state; hold it for every access** — check
   or update, fast path or slow.
2. **Hold that lock when calling `wait()`** — required by its semantics —
   **and when signalling.**
3. **Always wait in a `while` loop** re-testing the condition (Mesa
   semantics, spurious wakeups).
4. **Pair every CV with explicit state**; never encode "it happened" in the
   signal itself — signals are lost if nobody waits.
5. **One CV per logical condition**, so signals reach the right kind of
   waiter; use `broadcast` when the signaller cannot know whom to wake.
6. **Do the waiting and signalling inside the object** (monitor style), so
   callers cannot get the protocol wrong.

Applied to the queue:

- **E1–E2**: waits with an `if` (breaks rule 3) and — worse — calls
  `wait()` **without holding the mutex** (breaks rule 2): undefined
  behaviour in pthreads, and semantically a lost-wakeup window between the
  E1 check and the E2 sleep. Also, when `wait()` "returns" it re-acquires
  `m` — which E3 then tries to lock again: self-deadlock on a
  non-recursive mutex.
- **E1 before E3**: the state check itself is done without the lock
  (rule 1) — `count` can change under it.
- **E4–E5**: the enqueue path never signals at all (rule 4/5's dual):
  a consumer sleeping for data is never woken. Producers wake nobody.
- **D2**: spins with the lock held, releasing nothing — it should
  `pthread_cond_wait(&c, &m)`. As written, a waiting `dequeue()` holds `m`
  forever, so no `enqueue()` can ever run: system-wide deadlock the first
  time the queue is empty.
- **D4** signals `c`, but with only one CV for two conditions (rule 5) a
  dequeuer's signal may wake another dequeuer once the other bugs are
  fixed — the Figure 30.11 failure. Two CVs (`notEmpty`, `notFull`) finish
  the job.

*Marking note: full credit requires naming the rule per defect, not just
spotting defects; and the E2/E3 self-deadlock is the detail most miss.*

**C2.** Model answers should end in judgements with conditions, not verdicts.

**(a) The monitor as language construct.** The compiler pairing lock with
data makes three whole bug classes from this reading impossible: forgetting
to acquire the lock (every entry acquires it), forgetting to release it on
some exit path (every return releases it), and acquiring the wrong lock for
the data (there is only one, and it is attached). The evidence that the idea
won is that the most-used managed languages shipped it: Java's
`synchronized` methods *are* monitors (Appendix D works the example), and
the pattern persists in C#'s `lock` and in designs that bind data to its
lock. What Java's history also shows is the design's real cost: the original
one-implicit-CV-per-object could not express producer/consumer's two
conditions, forcing `notifyAll()` and its thundering herd, until an explicit
`Condition` class was added. Judgement: prefer monitor-style constructs
wherever the language offers them and the object's synchronization is
self-contained; prefer bare locks and CVs when you need multiple conditions,
fine-grained or hand-over-hand locking across objects, or control of lock
scope narrower than a method — the cases where the automation stops
matching the need.

**(b) Hoare semantics.** What improves: the woken thread runs *immediately*
with the lock, so the condition established by the signaller **still holds**
— the waiter may assume it, `if` suffices, and invariant-based proofs are
clean ("more amenable to proofs", as Appendix D concedes). There is also a
latency and fairness argument: under Mesa the interval between a condition
becoming true and the waiter acting is unbounded — other threads may barge
in and consume the condition arbitrarily many times (Con2 in B3 did exactly
that); under Hoare the waiter is served first, giving bounded handoff — a
real virtue where response bounds matter. The costs that made Mesa win:
signal becomes an immediate context switch (expensive precisely on the hot
path), the signaller is descheduled mid-critical-section and its state must
be parked, the implementation must transfer lock ownership atomically, and
the scheduler loses freedom — a thread is forced to run out of turn.
Lampson and Redell, building a real OS, found the hint cheap and robust:
a hint may be wrong, and the `while` makes wrong hints harmless. A setting
to accept Hoare's costs today: a small hard-real-time kernel where bounded
condition-handoff latency is a requirement and formal verification of the
synchronization is planned — low thread counts make the switch cost
negligible and the proof leverage is worth it. *Marking note: strong answers
name both the assumption transfer (what the waiter may assume) and the
barging/latency point; weak answers say only "Hoare is cleaner".*
