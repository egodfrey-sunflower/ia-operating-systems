> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 12 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** Mutual exclusion is necessary but nowhere near sufficient. OSTEP
gives three criteria: correctness (mutual exclusion), **fairness** (does every
contender eventually acquire, or can one starve?), and **performance** (what does
it cost uncontended, contended on one CPU, contended across many?). A lock that
grants entry once and never releases satisfies mutual exclusion perfectly.

**A2. FALSE.** Disabling interrupts prevents *preemption on the local core only*.
Another core continues executing and walks straight into the critical section.
It is also unsafe to expose to user code (a program could disable interrupts and
never return) and it loses interrupts that arrive while masked. It remains useful
inside a kernel on a uniprocessor, and — importantly — for excluding *interrupt
handlers* on the local core even on a multiprocessor.

**A3. FALSE.** Test-and-set gives no ordering guarantee whatsoever. Whichever
thread's atomic operation happens to land first wins, so a thread can be
overtaken indefinitely. **Ticket locks**, built on fetch-and-add, hand out
increasing ticket numbers and serve them in order, which is where FIFO fairness
comes from.

**A4. FALSE.** If the critical section is shorter than the cost of two context
switches, spinning is strictly cheaper — you burn a few hundred cycles instead of
paying to deschedule and rewake. This is precisely what **two-phase locks**
exploit (see B5). Sleeping wins when hold times are long or the machine is
oversubscribed.

**A5. FALSE.** It compiles to load, add, store. Two threads can interleave between
the load and the store, and one increment is lost. This is the week-11 shared
counter, and it is the reason this entire chapter exists.

**A6. FALSE.** Chapter 29's central empirical claim is the opposite. Finer
granularity adds a lock acquisition per unit of structure traversed; unless the
resulting parallelism exceeds that cost, throughput drops. The hand-over-hand
linked list is OSTEP's own worked counter-example (see B4).

**A7. FALSE — and this is the most important warm-up here.** Atomicity and
ordering are different guarantees. CAS makes a single read-modify-write
indivisible; it says nothing about whether *other* loads and stores may be
reordered around it by the compiler or the CPU. Without barriers, a store inside
the critical section can be reordered after the store that releases the lock, and
another thread can then observe the release while the data is still stale. This
is xv6 §7.6's subject and OSTEP's most consequential omission — the book assumes
sequential consistency throughout.

**A8. TRUE.** By construction. The global counter lags the true total by whatever
is buffered in the per-CPU locals, so a read can return a value that was never
simultaneously true of the system. The design trades exactness for scalability
and bounds the error (see B3a).

---

## B. Mechanism and measurement

**B1.**
**(a)** With a flag-based lock, both threads execute the *test* before either
executes the *set*. Concretely: T1 loads `flag` (sees 0); interrupt; T2 loads
`flag` (sees 0); T2 stores 1 and enters; interrupt; T1 — which has already made
its decision — stores 1 and also enters. **Both are inside.** The failure is that
test and set are two instructions with a preemptible gap between them, which is
exactly the gap `test-and-set` closes by fusing them.
**(b)** Peterson's is correct, but: it is written for exactly two threads and
generalises awkwardly and expensively; it requires each thread to spin on
variables *other threads write*, so it generates heavy cache-coherence traffic;
and — decisively — **it assumes sequentially consistent memory**, which no modern
CPU provides without explicit barriers. It is a proof that mutual exclusion is
possible with loads and stores alone, not a practical recipe. Credit especially
for the memory-model point.

**B2.**
**(a)** Any schedule where the unlucky thread is descheduled while the lock is
free and rescheduled only when it is held. Since test-and-set imposes no queue,
this can repeat forever. It is **not a bug** — it is permitted behaviour, because
test-and-set makes no fairness guarantee at all. Correct answers name starvation
as an accepted property, not a defect.
**(b)** Fetch-and-add issues each arriving thread a distinct, monotonically
increasing ticket, and the lock is granted when `turn` reaches your number. Since
`turn` only advances, every ticket is eventually served — bounded waiting, no
matter how the scheduler behaves.
**(c)** Test-and-test-and-set is preferable when contention is **low** and the
hold time is short. TTAS spins on an ordinary load (cheap, hits in the local
cache, generates no coherence traffic) and only issues the expensive atomic when
the lock looks free. A ticket lock pays the atomic on every arrival. Under low
contention TTAS's uncontended path is cheaper, and fairness is moot when nobody
is queuing.

**B3.**
**(a)** Up to **P × S** — each of the `P` CPUs may be holding as much as `S`
locally at the instant before it flushes. (Accept `P × (S−1)` with the reasoning
that a flush is triggered on reaching `S`; the distinction is a fencepost, not a
concept.)
**(b)** Each CPU performs `N/P` increments and flushes once every `S`, i.e.
`N/(P·S)` flushes; across `P` CPUs that is **`N/S` global-lock acquisitions**.
Global-lock traffic is therefore **inversely proportional to `S`** — doubling `S`
halves contention on the one shared lock.
**(c)** `S` trades **accuracy (staleness) against scalability**: larger `S` means
a lazier, more wrong global value but far less contention on the shared lock.
**(d)** **No, not as stated** — the design gives you a cheap *approximate* read,
and you have asked for an exact one. But the workload is ideal for a fix: make
reads pay. Keep the per-CPU counters, and on a read acquire every local lock,
sum the locals plus the global, and return that. Reads become `O(P)` and
expensive; increments stay local and uncontended. Since reads are a thousand
times rarer, total cost falls. This is essentially Linux's `percpu_counter`.
Credit for spotting that the asymmetry in the workload is what makes the fix
work.

**B4.**
**(a)** (i) Single lock: **`a + (L/2)·t`** — one acquire/release for the whole
traversal. (ii) Hand-over-hand: **`(L/2)·(a + t)`** — a lock operation for every
node examined.
**(b)** Hand-over-hand serialises nothing, so with `n` concurrent lookups its
per-operation cost stands while the single lock serialises all `n`. It wins when

```
(L/2)(a + t)  <  n · (a + (L/2)·t)
```

i.e. above a crossover thread count

```
n*  =  (L/2)(a + t) / (a + (L/2)·t)
```

**Either condition alone can suffice — they are not both required.** Two limits
make this concrete:

- As `a → 0` (lock operations free), `n* → 1`, so **n = 2 already wins**:
  cheap locks alone are enough.
- `n* ≤ L/2` for all `L ≥ 2`, so **`n > L/2` always wins**, whatever `a` is:
  enough concurrency alone is enough.

Credit for deriving `n*` rather than asserting a qualitative condition; the
qualitative claim "you need both large `n` and small `a`" is a common and
incorrect answer, and the algebra above is what refutes it.
**(c)** Because `a` is not small. A lock acquire is an atomic read-modify-write,
which must obtain the cache line in **exclusive** state — invalidating every
other core's copy and forcing a coherence transaction. That costs tens to
hundreds of cycles, whereas `t` (examining a node already in cache) may be a
handful. So `(L/2)·a` dominates, and multiplying it by `L/2` is exactly the wrong
move. Full credit requires naming **cache coherence**, not merely "locks are
slow".
**(d)** A hash table operation touches **one bucket**, so it takes **one** lock
regardless of table size — the lock cost is `O(1)` per operation rather than
`O(L)`. And different threads usually hash to different buckets, so they contend
on *different* cache lines and get genuine parallelism. The structural difference
is that traversal length per operation is constant, so per-node locking never
arises.

**B5.**
**(a)** Spinning for the whole hold time wastes `T` of CPU; sleeping costs `C`
regardless. Spinning is cheaper when **`T < C`**.
**(b)** **Spin for exactly `C`, then sleep.** Worst case: you spin the full `C`
and still sleep, paying `2C`, where an oracle would have paid at most `C`. If
instead `T < C` you spin `T`, which is optimal. So the strategy is never worse
than twice optimal — it is **2-competitive**. This is the standard competitive
analysis and is what two-phase locks implement.

**B6.**
**(a)** The assumption that matters is that a write to a shared line invalidates
**every** cached copy of it, so all `P-1` spinning waiters must re-fetch.
(i) **Test-and-set:** every waiter is not merely reading but issuing an atomic
read-modify-write, each of which acquires the line exclusively. Traffic is
`O(P)` per handover at best, and unbounded in practice, because the waiters keep
generating coherence transactions *while* the lock is held. (ii) **Ticket lock:**
waiters only *read* the now-serving field, so they share the line — but the
release writes it, invalidating all `P-1` copies, so each handover costs `O(P)`
transfers. (iii) **MCS:** the releaser writes exactly one location, its
successor's `locked` flag, which exactly one processor is spinning on:
**`O(1)`** per handover, independent of `P`. That is the paper's central claim,
and it is why MCS works equally well on machines without coherent caches — the
flag can simply be allocated in the successor's local memory.
**(b)** The race is between the two halves of `acquire`. A newcomer executes
`fetch-and-store(L, I)` — so the tail already points at it, and a
`compare-and-swap(L, me, nil)` by the releaser will fail — but it has **not yet**
executed `predecessor->next := I`, so the releaser sees `I->next = nil` and has
no successor to hand off to. Interleaving: releaser reads `I->next` (nil) → newcomer
swaps itself into the tail → releaser's CAS fails → releaser must wait. The spin
is local because the releaser is spinning on `I->next`, a field of **its own**
qnode, which lives in its own memory; only the arriving newcomer ever writes it,
and it does so exactly once.
**(c)** Removing yourself from the queue requires **two** facts to be checked and
acted on indivisibly: that you are still the tail, *and* that the tail is
therefore settable to `nil`. `fetch-and-store` unconditionally overwrites — it
cannot be conditional on the current value — so a lone releaser using it would
clobber a newcomer that had just enqueued. The alternative release path in the
paper (Figure 7) copes without CAS but, as the paper says, gives up strict FIFO
and admits a theoretical starvation.
**(d)** Anywhere the caller has no convenient place to put a per-acquisition
record: recursive or deeply nested acquisition, an interrupt handler with a tiny
stack, or a lock embedded in a data structure whose users are not known in
advance. A ticket lock is a **single word** with no per-waiter state — the
acquire is `fetch-and-add` and nothing must stay live — which is exactly why it
remains the default for short, low-contention kernel locks despite the coherence
traffic.

*Marking note.* (a) is the part that matters: the answer is `O(P)` versus `O(1)`
**per handover**, and full credit requires naming invalidation as the mechanism.
An answer that says only "MCS spins locally" without converting that into a
transaction count has restated the paper's abstract rather than explained it.

---

## C. Discussion and design critique

**C1.** **Atomicity** guarantees that one read-modify-write is indivisible.
**Ordering** guarantees that other memory operations become visible in a
particular sequence. Atomic instructions provide the first and not the second.

Concrete failure — a correctly mutually-excluded critical section that still
yields a wrong result:

```
    lock(&l);
    data = 42;        // (1)
    ready = 1;        // (2)
    unlock(&l);       // (3) stores 0 to l
```

with a reader that checks `ready` before taking the lock (a common fast path). If
the compiler or CPU reorders (2) before (1), or hoists (3) above (1), another
thread can observe `ready == 1`, or observe the lock free, while `data` is still
stale. Mutual exclusion was never violated — no two threads were ever inside —
yet the result is wrong.

Which reordering: **both** are real. The compiler may reorder because it cannot
see other threads and the accesses are independent to its analysis; the CPU may
reorder because store buffers retire writes out of order on any weakly-ordered
architecture (ARM, POWER, RISC-V; x86 is stronger but still permits store-load
reordering). A **release barrier** before the unlock forces (1) and (2) to be
visible before (3); an **acquire barrier** after the lock stops subsequent
accesses hoisting above it. In xv6 these are `__sync_synchronize()`.

**C2.** Any well-argued class. The two strongest:

- **Systems where scalability is the product requirement** — a kernel targeting
  high core counts, a database engine, a network dataplane. Here concurrency
  structure is not an optimisation to be discovered by profiling; it is a design
  constraint. Retrofitting fine-grained locking into a codebase whose invariants
  assume one big lock is not a refactor but a multi-year rewrite, because every
  site must rediscover what it actually needed protecting. (See C3.)
- **Real-time systems.** A coarse lock makes worst-case blocking time a function
  of the longest critical section anywhere in the system, which destroys the
  schedulability analysis. Priority inversion becomes unbounded. Here the
  requirement is a *bound*, and "measure the average and refine later" cannot
  produce one.

Marking note: the point is not that ch. 29 is wrong — it is right for the common
case. Credit answers that identify *what property of the setting* invalidates
the advice, rather than merely asserting an exception.

**C3.** A strong answer separates four claims.

**Is deadlock actually impossible?** Largely but not entirely. A single
non-recursive lock cannot produce a lock-order cycle, so that whole bug class
does go away — the engineer is right about this. But deadlock is not only about
lock cycles: a thread that blocks on I/O, or waits on a condition, *while holding*
the lock, halts everything; and if the lock is not recursive, any path that
re-enters the kernel deadlocks against itself. Starvation and livelock are
untouched. So: one bug class eliminated, others created.

**Blocking while holding the lock** is the fatal operational problem. Any system
call that sleeps — a disk read, a page fault, waiting on a socket — would stall
the entire kernel for every core. So you *must* release around every sleep. But
the moment you do, state can change across the gap, and you are back to reasoning
about exactly what each critical section protects. The simplification is largely
illusory; you have replaced explicit lock ordering with implicit
"what-changed-while-I-slept" reasoning, which is harder to audit, not easier.

**"A little throughput" is Amdahl's law, and it is not little.** If a fraction
`s` of execution is spent inside the kernel under the lock, maximum speedup is
bounded by `1/s` *no matter how many cores you add*. At 20% kernel residency —
unremarkable for I/O-heavy workloads — the ceiling is **5×**, on any core count.
This is a hard asymptote, not a constant factor, and it worsens as core counts
grow. Boyd-Wickizer et al.'s Linux scalability study (cited by OSTEP ch. 10) is
the empirical version of this argument.

**Is incremental recovery credible?** **Historically, no — and this is not
hypothetical.** Linux's Big Kernel Lock was introduced for SMP support around
1996 and was not fully removed until **2.6.39 in 2011** — roughly fifteen years,
with sustained effort by many developers. FreeBSD's equivalent "Giant" lock took
a comparable multi-year campaign. The reason is structural: once code is written
assuming one lock protects everything, the information about *what each site
actually needed* is lost, and every removal must reconstruct it under threat of
subtle races. The plan reverses the cheap and expensive directions — coarsening
is easy, refining is not.

**Recommendation.** Reject as a permanent architecture. It is defensible as a
*temporary* bring-up strategy for a brand-new kernel on low core counts, provided
the removal cost is budgeted honestly up front rather than assumed away — which
the proposal does not do. Conditions that would change the verdict: a genuinely
single-core or few-core target; a kernel with very low kernel-residency; or a
system where the correctness risk of the current 200 locks demonstrably exceeds
the scalability cost, in which case the right move is usually a documented lock
*ordering* discipline and automated verification, not a single lock.

*Marking note: the strongest answers concede the engineer's deadlock point rather
than dismissing it, and locate the real objections in blocking, Amdahl, and the
asymmetry of the migration path. An answer that only says "BKL was bad" without
the mechanism earns little.*
