> # ⚠️ SPOILER — ANSWERS TO EXAMPLES SHEET 5 ⚠️
> **Do not read until you have attempted the sheet closed-book.**
> Arithmetic in §E verified with Python. **[ext]** material — no IA equivalent.

---

# Sheet 5 — Answers

## A. Warm-ups

**A1. FALSE.** Even if each *statement* is a single instruction, the critical
*section* usually spans several (load, modify, store), and two of those from
different threads can interleave. And single-instruction atomicity of a
*compound* update (like `x++`) is exactly what ordinary hardware does *not*
guarantee — that is why we need atomic RMW instructions. Removing a race needs
*mutual exclusion over the whole section*, not per-statement brevity.

**A2. FALSE.** For a *short* critical section on a multiprocessor, spinning is
*cheaper* than the two context switches a block costs (see §E): if the expected
wait is below the block overhead, spinning wins. Blocking is better only for
long holds. "Strictly worse" is wrong.

**A3. FALSE.** Peterson's is correct *only under sequential consistency*. Real
multicore CPUs reorder memory operations (store buffers) and compilers reorder
code, both of which break the algorithm unless you add memory fences. It is also
inefficient (busy-waits, two-thread-only) — a teaching tool, not a practical
lock.

**A4. FALSE.** CAS is *strictly more powerful* than TAS. TAS has consensus number
2; CAS is universal (consensus number ∞) and can build arbitrary lock-free
structures. You can build a *lock* from either, but you cannot build a general
lock-free counter/stack from bare TAS the way you can from CAS/LL-SC.

## B. Identifying the race

- (a) `balance = balance + 1` compiles to: **(1) load** `balance` into a register,
  **(2) add** 1, **(3) store** back. Losing interleaving (both threads start with
  balance = k): T1 loads k; T2 loads k; T1 adds → k+1, stores k+1; T2 adds → k+1
  (from its stale load), stores k+1. Two increments, net +1 — one update lost.
  In principle the smallest final value is **2** (not `2000000`): thread A loads
  0, is descheduled for almost the entire run while B does 999 999 increments,
  then A stores 1 (clobbering B), then B does its last increment reading 1 →
  stores 2. (A pathological schedule, but it shows how bad it can get.)
- (b) *Critical section*: code that accesses shared state and must run
  atomically w.r.t. other threads (here, the read-modify-write of `balance`).
  *Race condition*: a bug where the result depends on the *timing/interleaving*
  of threads. *Data race*: two threads access the same location concurrently,
  at least one writing, with no synchronisation. This code exhibits **all three**
  — it is a data race, causing a race condition, on an unprotected critical
  section.
- (c) The three properties, with the test the fix must pass:
  - **Mutual exclusion** — at most one thread in the CS at a time (test: no
    interleaving of two threads' load/store can occur).
  - **Progress / deadlock-freedom** — if the lock is free and someone wants it,
    *some* thread enters (test: the two threads can't both block forever each
    waiting on the other).
  - **Bounded waiting / starvation-freedom** — a thread waiting to enter is not
    passed over forever (test: each thread eventually completes its million
    increments).
- (d) Locking only the *store* does **not** fix it. Interleaving: T1 loads k
  (unlocked); T2 loads k (unlocked); T1 locks, stores k+1, unlocks; T2 locks,
  stores k+1 (from its stale load), unlocks. The lost update happens on the
  *load*, which was left unprotected. The lock must span the entire
  read-modify-write.

## C. Peterson's algorithm

- (a) **Mutual exclusion.** Suppose both are in the CS. Both must have exited
  their `while`, so each saw either `flag[other] == false` or `turn != other`.
  But both set `flag[i] = true` before spinning and never clear it before the CS,
  so each *did* see `flag[other] == true`. Hence each must have exited via
  `turn != other`, i.e. thread 0 saw `turn == 0` and thread 1 saw `turn == 1`.
  But `turn` is a single variable holding one value — it can't be both 0 and 1
  simultaneously. Contradiction ⇒ they cannot both be in the CS. (The *last*
  writer of `turn` yields to the other, who proceeds; the writer waits.)
- (b) **Delete `turn`** (spin only on `flag[other]`): interleave — T0 sets
  `flag[0]=true`, then T1 sets `flag[1]=true`; now each sees the other's flag
  true and both spin forever → **deadlock** (progress/deadlock-freedom breaks).
  `turn` is precisely the tie-breaker that lets exactly one proceed when both
  want in.
- (c) **Delete `flag`** (spin only on `turn != i`): `turn` starts at some value,
  say 0. Now if thread 1 never wants the lock, `turn` is never set to 1, so
  thread 0 (needing `turn == 0`) is fine — but if thread 0 sets `turn = 1`
  (yielding) and thread 1 never runs to yield back, thread 0 spins forever
  waiting for a `turn` change that never comes → **progress breaks** when one
  thread doesn't contend. The `flag` array is what lets a thread proceed when the
  other simply isn't interested.
- (d) **Real hardware.** (i) The CPU has a **store buffer**: each thread's
  `flag[i] = true` (a store) can be buffered while its subsequent *load* of
  `flag[other]` reads old memory — a **store→load reordering** — so both can read
  the other's flag as `false` and both enter (mutual exclusion breaks). (ii) The
  **compiler** may reorder or cache `flag`/`turn` in registers (they look like
  ordinary variables), hoisting the load or eliminating the spin. Fix: a **full
  memory fence** between the `turn` write and the spin loop's reads (or, in C11,
  make `flag`/`turn` atomics with at least acquire/release, and a
  `seq_cst`/fence between the store and the load). The critical pair that must
  **not** be reordered is *the store `flag[i] = true` followed by the load of
  `flag[other]`*.

## D. TAS vs CAS vs LL/SC

- (a) **test-and-set(addr)**: atomically read the old value at `addr`, set it to
  1, return the old value. **compare-and-swap(addr, expected, new)**: atomically,
  if `*addr == expected` set `*addr = new` and return success (or the old value);
  else leave it and report failure. **LL/SC**: `load-linked(addr)` reads and
  *tags* the location; `store-conditional(addr, v)` writes `v` **only if** the
  location has not been written since the LL, returning success/failure.
- (b) TAS spinlock: `acquire: while (TAS(&lock)) ; release: lock = 0;`.
  CAS spinlock: `acquire: while (!CAS(&lock, 0, 1)) ; release: lock = 0;`.
  TAS builds only the simplest lock because it can only *unconditionally* set a
  bit and report the prior state — it cannot read a rich old value and act on it.
  CAS/LL-SC *read the old value and install a new one conditional on it being
  unchanged*, which lets you implement optimistic lock-free updates: read old,
  compute new, CAS; retry on failure — the basis of lock-free stacks, counters,
  queues.
- (c) **ABA:** thread T1 reads stack top = node A (plans `CAS(top, A, A->next)`).
  T1 stalls. T2 pops A, pops B, then pushes A back (top = A again), with A->next
  now stale/freed. T1 resumes: its `CAS(top, A, A->next)` **succeeds** because top
  == A again — but `A->next` is wrong, corrupting the stack. LL/SC avoids this:
  `store-conditional` fails if the location was **written at all** since the LL
  (the intervening pops/pushes wrote `top`), regardless of whether the value
  happens to equal A again. CAS only checks *value equality*, which A-B-A defeats.
- (d) LL/SC makes **progress/forward-guarantee** hard: an SC can fail
  *spuriously* (e.g. any interfering cache event, context switch, or even an
  unrelated store to the same reservation granule), so LL/SC loops can livelock
  and ISAs restrict what may appear between LL and SC. A single `cmpxchg`
  instruction can't be "interrupted between LL and SC" because it *is* one
  instruction — simpler progress reasoning, at the cost of the ABA problem.

## E. Spinlock vs sleeping lock — the calculation

Costs: one context switch **Cs = 4 µs**; a block-and-resume = **2·Cs = 8 µs** of
wasted CPU; spinning wastes CPU equal to the wait.

- (a) **Break-even.** Spinning wastes `W` (the wait); blocking wastes `2·Cs = 8`.
  Spinning is cheaper iff **`W < 2·Cs = 8 µs`.** So the break-even *wait* is
  8 µs; equivalently (with expected wait `H/2`) the break-even *hold time* is
  `H = 2·(2·Cs) = 16 µs`.
- (b) Contender arrives uniformly during a hold of `H`, so expected wait = `H/2`:
  | H (µs) | expected wait H/2 | spin wastes | block wastes | decision |
  |---:|---:|---:|---:|:--|
  | 2 | 1.0 | 1.0 µs | 8 µs | **spin** |
  | 5 | 2.5 | 2.5 µs | 8 µs | **spin** |
  | 20 | 10.0 | 10 µs | 8 µs | **block** |
  Short holds ⇒ spin (the wait is cheaper than two switches); the long hold
  (H = 20, expected wait 10 > 8) ⇒ block.
- (c) **Uniprocessor:** if you spin waiting for a lock held by *another* thread,
  that holder is *not running* (only one CPU, and you're on it) — so it cannot
  release the lock while you spin, and you burn your entire quantum
  accomplishing nothing; the holder only runs after you're descheduled. So on a
  uniprocessor you must **block** (yield to let the holder run), never spin, for
  a lock held by a preempted thread. On a **multiprocessor** the holder may be
  *running on another core* and about to release, so a brief spin can beat a
  block — the holder makes progress *in parallel* with your spin.
- (d) **Adaptive (spin-then-block):** spin for a bounded time first; if the lock
  frees quickly (short hold), you paid only ≈ `H/2` of spinning and avoided both
  context switches — the cheap row of the table. If it's still held after the
  spin bound, you *then* block, paying the `2·Cs` once. So you get the low cost
  of spinning for short holds and cap the worst case at (spin bound + block cost)
  for long holds — the best of both columns of (b).

## F. Test-and-test-and-set

*MESI in brief (so you can self-mark without outside sources).* Cache coherence
tracks each cached line in one of four states: **M**odified (this cache has the
only copy and it is dirty), **E**xclusive (only copy, clean), **S**hared (clean,
possibly cached by others), **I**nvalid (not usable). A **read** may load a line
in S (many caches can hold it in S at once); a **write** requires the line in M
or E, which forces the coherence protocol to **invalidate every other cache's
copy** first, so only the writer holds it. That is the whole reason a spinning
plain TAS is expensive and a spinning TTAS is not: TAS *writes* the lock word on
every attempt, so each spin yanks the line to M and invalidates all other
spinners (their next spin must re-fetch it — continuous O(N) coherence traffic);
TTAS *reads* the lock word while it is held, and a read is satisfied from the
local copy in **S** with no bus transaction, so idle spinners are silent. Traffic
appears only at the release (one invalidation, then a re-read storm as everyone
races to TAS).

- (a) A plain TAS spin *executes the atomic RMW every iteration*. On a
  cache-coherent multiprocessor, each TAS is a *write* (it sets the bit), so it
  must obtain the cache line in **Modified/Exclusive** state, invalidating every
  other spinner's copy. With `N` cores spinning, each iteration by each core
  generates coherence traffic to steal the line — **O(N) bus/interconnect
  transactions continuously**, saturating the interconnect and *slowing the
  holder's release*.
- (b) **TTAS** spins on an ordinary **read** (`while (locked) ;`) and only issues
  the atomic TAS when the read shows the lock free. While spinning, the line sits
  in each core's cache in the **Shared** state; the reads *hit locally* and
  generate **no** bus traffic. Traffic occurs only when the lock is released
  (one invalidation) and the spinners re-read. But at release there is still a
  **thundering herd**: all `N` spinners see it free and race to TAS, causing an
  O(N) burst. **Queue locks — a ticket lock, or better an MCS lock** (each waiter
  spins on its *own* cache line, handed the lock in order) — eliminate even that
  release storm, giving O(1) traffic per handoff.

## G. Priority inversion

- (a) **Priority inversion** = a high-priority task is blocked by a lower-priority
  task, and (unbounded inversion) is further delayed by *medium*-priority tasks
  that preempt the low-priority lock holder. Minimal scenario: **L** (low) takes
  lock X; **H** (high) becomes runnable, needs X, blocks on L; **M** (medium,
  doesn't use X) becomes runnable and *preempts L* (M > L). Now L can't run to
  release X, so H is stuck behind M — a high-priority task effectively waiting on
  an unrelated medium one, *for as long as M runs*. Timeline: `L holds X → H
  blocks on X → M preempts L → M runs (H still blocked) → M finishes → L runs,
  releases X → H finally runs`.
- (b) **Mars Pathfinder (1997):** a high-priority **bus management** task and a
  low-priority **meteorological/data** task shared a mutex on the information bus;
  medium-priority **communications** tasks would preempt the low task while it
  held the mutex, so the high task blocked long enough that a **watchdog** timer
  fired and reset the spacecraft repeatedly. Mapping: bus-management = **H**,
  meteorological = **L** (lock holder), comms = **M** (unrelated preemptor).
- (c) **Priority inheritance:** while L holds a lock that H is waiting on, **L
  temporarily inherits H's priority**, so M can no longer preempt L; L runs at H's
  priority, releases X quickly, then drops back — H's blocking is bounded to *one*
  low-priority critical section (the time to finish L's use of X), not "until M
  is done". On the timeline, at "H blocks on X" L is boosted to H's priority, so
  M never gets to preempt. Alternative: the **priority ceiling protocol** — each
  lock has a *ceiling* = the priority of the highest task that can ever use it; a
  task may acquire a lock only if its priority exceeds the ceilings of all locks
  currently held by others. It prevents inversion (and deadlock) *proactively* by
  admission control, rather than reacting by boosting.
- (d) Once tasks share locks they are **not independent**, so schedulability
  analysis must add a **blocking term `Bᵢ`** (the worst-case time task *i* can be
  blocked by a lower-priority lock holder) to its response-time equation:
  `Rᵢ = Cᵢ + Bᵢ + Σ_{j higher} ⌈Rᵢ/Tⱼ⌉ Cⱼ`. Priority inheritance/ceiling
  protocols are what make `Bᵢ` *bounded* (and hence analysable).

*(Verified in Python: Cs = 4 µs, block = 8 µs; break-even wait 8 µs / hold 16 µs;
the H = 2/5/20 µs table decisions spin/spin/block.)*
