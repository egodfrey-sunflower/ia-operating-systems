> # ⚠️ SPOILER — ANSWERS TO EXAMPLES SHEET 4 ⚠️
> **Do not read until you have attempted the sheet closed-book.**
> All numeric results verified with Python. **[ext]** material — no IA equivalent.

---

# Sheet 4 — Answers

## A. Warm-ups

**A1. FALSE.** Lottery gives the expected share 25%; the *realised* share over a
finite interval is a random variable (Binomial), so it is only guaranteed *in
expectation / in the long run*. Over a short window it can deviate substantially
(see B1).

**A2. TRUE (essentially).** Stride is the deterministic cousin of lottery: same
long-run proportional share, but by choosing the smallest-`pass` process each
step it removes the randomness, so it hits the exact ratio quickly with *zero*
variance rather than only in expectation.

**A3. FALSE as stated.** A *lower/nicer* nice value means a *higher weight*, so
its `vruntime` advances *more slowly* per unit of CPU, so CFS picks it *more*
often (it looks "behind"). The statement inverts the direction. (Correct: lower
nice ⇒ higher weight ⇒ slower vruntime growth ⇒ larger CPU share.)

**A4. FALSE.** U ≤ 1 suffices for *EDF*, not RM. RM's sufficient bound is only
`n(2^{1/n}−1)` (≈ 0.693 as n→∞); sets with U between that bound and 1 may or may
not be RM-schedulable (need exact response-time analysis — see D3, where a U =
0.958 set is EDF-feasible but RM-infeasible).

## B. Lottery and stride

**B1. Lottery.**
- (a) A's expected share = 75/100 = **0.75**. Wins over n draws ~ Binomial(n,
  0.75): **mean = 0.75n**, **standard deviation = √(n·0.75·0.25) = √(0.1875 n) ≈
  0.4330 √n**.
- (b)
  | n | mean wins | s.d. (wins) | s.d. as fraction of n (share error) |
  |---:|---:|---:|---:|
  | 100 | 75 | 4.330 | 0.0433 (**4.33%**) |
  | 10 000 | 7 500 | 43.30 | 0.00433 (**0.433%**) |
  The **absolute** spread *grows* (4.33 → 43.3 wins) but the **relative** share
  error *shrinks* as 1/√n (4.33% → 0.433% for a 100× longer run). This is the law
  of large numbers: proportional accuracy improves with √n.
- (c) Lottery is fair **in the limit** (relative error → 0 as n → ∞) but over a
  **short window** the share can be well off (a 20-draw run for a 0.75-ticket
  process has s.d. ≈ 1.94 wins on a mean of 15 — a ~13% swing). This matters for
  a **short-lived interactive task**: it may finish before the average converges,
  so it can get badly under- or over-served. Stride (or CFS) is preferred where
  short-run fairness matters.
- (d) A process subdividing its own tickets is safe because the *total* ticket
  count of *other* users is unchanged — a local currency is exchanged into the
  base currency at a rate fixed by the parent's own allocation, so no process can
  inflate its share at others' expense. The enforcing abstraction is Waldspurger
  & Weihl's **ticket currencies** (a per-owner namespace of tickets backed by a
  fixed base allocation).

**B2. Stride.**
- (a) `stride = STRIDE1 / tickets = 60 / tickets`: **A = 20, B = 30, C = 60.**
- (b) Trace (tie → lowest in order A < B < C):
  | step | pass_A | pass_B | pass_C | chosen |
  |---:|---:|---:|---:|:--:|
  | 1 | 0 | 0 | 0 | A |
  | 2 | 20 | 0 | 0 | B |
  | 3 | 20 | 30 | 0 | C |
  | 4 | 20 | 30 | 60 | A |
  | 5 | 40 | 30 | 60 | B |
  | 6 | 40 | 60 | 60 | A |
  | 7 | 60 | 60 | 60 | A |
  | 8 | 80 | 60 | 60 | B |
  | 9 | 80 | 90 | 60 | C |
  | 10 | 80 | 90 | 120 | A |
  | 11 | 100 | 90 | 120 | B |
  | 12 | 100 | 120 | 120 | A |
  Run counts: **A = 6, B = 4, C = 2.**
- (c) 6 : 4 : 2 = **3 : 2 : 1 = tickets exactly.** Stride is exact because `pass`
  is a deterministic accumulator: a process is picked precisely often enough to
  keep its pass advancing in lock-step with the others' (inversely proportional
  to tickets). Lottery only matches the ratio *in expectation* because each pick
  is an independent random draw with Binomial variance (B1).
- (d) A late joiner set to `pass = 0` would win every lottery until its pass
  caught up, monopolising the CPU. Real stride implementations set a joining
  process's `pass` to the **current global minimum pass** (or the current
  scheduler pass value), so it starts "level" with the incumbents. Lottery needs
  *no* special handling for joiners: it holds a fresh independent draw each time,
  with no historical state to be stale.

## C. CFS / EEVDF

**C1.**
- (a) `vruntime` = weighted, normalised accumulated CPU time of a task. Rule:
  **run the runnable task with the smallest `vruntime`** (leftmost in the
  red-black tree). This approximates proportional share because a task that has
  run less (relative to its weight) has a smaller `vruntime` and is picked next,
  self-balancing all tasks toward equal *weighted* progress — without random
  draws or explicit strides.
- (b) `Δvruntime = Δt_wallclock × (W_0 / W_i)` where `W_i` is the task's weight
  and `W_0` a reference weight (weight of nice 0). Higher weight ⇒ smaller
  multiplier ⇒ `vruntime` grows more slowly ⇒ the task stays "leftmost" longer ⇒
  more CPU. This confirms the correction to **A3**: lower nice = higher weight =
  slower vruntime = *more* CPU.
- (c) A long-sleeping task returns with a stale small `vruntime` and would
  monopolise the CPU to "catch up". CFS clamps a waking task's `vruntime` to at
  least **`min_vruntime − a small threshold`** (the runqueue's minimum), so it
  gets a *modest* dispatch boost for responsiveness but cannot hog the CPU. The
  trade-off: some *sleeper fairness* (long-term accumulated deficit) is given up
  to bound the wake-up burst (latency vs strict long-run fairness).

**C2.**
- (a) Geometric (~1.25× per level) mapping makes the *ratio* of two tasks' shares
  depend only on the **difference** of their nice values, not their absolute
  values: `nice 0` vs `nice 5` gives the same ratio as `nice 5` vs `nice 10`.
  This gives users a consistent, composable "each niceness step changes my share
  by a fixed factor (~10% per level, ~1.25× weight)".
- (b) EEVDF adds, per scheduling request, an **eligible time** (a task becomes
  eligible only once it has "earned" the right to run, by the passage of virtual
  time) and a **virtual deadline** derived from its requested slice; it runs the
  *eligible* task with the earliest virtual deadline. This fixes CFS's weak
  **latency** guarantee: a task wanting a *small slice often* (low latency) gets a
  near virtual deadline and is served promptly, which pure min-vruntime CFS could
  not guarantee.

**C3. Slice arithmetic.** *Verified in Python.*
- (a) 5 equal-weight tasks: raw slice = 20 ms / 5 = **4 ms**, exactly at the
  4 ms granularity floor, so it stands. Round-robin over 5 tasks at 4 ms each
  gives a full period of 5 × 4 = **20 ms**; any one task waits 4 × 4 = **16 ms**
  between turns. The target latency is met with nothing to spare.
- (b) 10 tasks: raw slice = 20 ms / 10 = 2 ms < 4 ms, so the **minimum
  granularity binds**: each slice is clamped to **4 ms** and the effective
  period stretches to 10 × 4 = **40 ms** (each task waits 9 × 4 = 36 ms) —
  double the 20 ms target. In (a) the *target latency* determines the slice
  (slices shrink so the period fits); in (b) the *granularity floor* does. The
  property sacrificed is the **latency bound**: once the floor binds,
  scheduling latency grows linearly with the number of runnable tasks (period
  = n × granularity) instead of staying pinned at the target — here 40 ms vs
  20 ms, a 2× overshoot. This is the arithmetic behind y2024p2q3(b)(iii)–(iv).

## D. RM/EDF schedulability

**D1.**
- (a) RM sufficient bound `U ≤ n(2^{1/n} − 1)`: **n=1 → 1.0000, n=2 → 0.8284,
  n=3 → 0.7798.** As n → ∞ it decreases to **ln 2 ≈ 0.6931**.
- (b) EDF: a set of independent periodic tasks with deadline = period is
  schedulable **iff U ≤ 1**. EDF's bound is 1 because its *dynamic* priority
  (earliest absolute deadline first) always devotes the CPU to the most urgent
  job; RM's *static* priority (by period) can leave the CPU running a
  short-period task while a long-period task with an imminent deadline waits,
  wasting the slack — so RM needs headroom below full utilisation.

**D2. Schedulable set.** T1(1,4), T2(1,5), T3(1,10).
- (a) U = 1/4 + 1/5 + 1/10 = 0.25 + 0.20 + 0.10 = **0.55 ≤ 0.7798** → passes the
  sufficient RM test ⇒ RM-schedulable.
- (b) RTA (priorities by period, T1 highest): **R1 = C1 = 1 ≤ 4 ✓.**
  R2 = 1 + ⌈R/4⌉·1 → 1+1 = 2, fixed point **R2 = 2 ≤ 5 ✓.**
  R3 = 1 + ⌈R/4⌉·1 + ⌈R/5⌉·1: start 1 → 1+1+1 = 3, fixed point **R3 = 3 ≤ 10 ✓.**
  All responses ≤ deadlines — confirmed RM-schedulable.

**D3. EDF beats RM.** T1(1,4), T2(2,6), T3(3,8).
- (a) U = 1/4 + 2/6 + 3/8 = 0.25 + 0.3333 + 0.375 = **0.9583 ≤ 1** ⇒
  **EDF-schedulable.**
- (b) RM bound for n=3 is 0.7798, and 0.9583 > 0.7798, so the **sufficient RM
  test fails**. But "fails the sufficient test" ≠ "not schedulable" — the test is
  only *sufficient*, not necessary, so we must do exact RTA.
- (c) RTA for RM (T1 highest, then T2, T3):
  - R1 = 1 ≤ 4 ✓.
  - R2 = 2 + ⌈R/4⌉·1: 2 → 2+1 = 3, fixed point **R2 = 3 ≤ 6 ✓.**
  - R3 = 3 + ⌈R/4⌉·1 + ⌈R/6⌉·2:
    3 → 3+1+2 = 6; 6 → 3+2+2 = 7; 7 → 3+2+4 = **9 > deadline 8** ✗.
    T3's response time reaches 9 (> 8) before converging → **T3 misses its
    deadline; the set is NOT RM-schedulable** even though U < 1. This is the
    canonical demonstration that **EDF is strictly more powerful than RM**.
- (d) EDF's price: (i) **worse overload behaviour** — near/above U=1 EDF can
  cascade (a domino of missed deadlines across many tasks) whereas RM degrades
  predictably (only the lowest-priority tasks miss); (ii) **higher runtime cost /
  harder implementation** — dynamic priorities mean re-sorting the ready set by
  absolute deadline every release, and it is harder to reason about which task
  misses. RM's static priorities are simpler and analysable.

## E. Tie to Lab 3
- (a) The `LOTTERY` acceptance window [2.0, 4.5] around an expected 3:1 is wide
  because, per B1, a lottery's realised ratio has Binomial variance that is large
  over a *short* measurement window — with modest total ticks the observed ratio
  can swing well away from 3. To narrow the window you would **lengthen the
  measurement** (more ticks ⇒ relative error ∝ 1/√n) and pin to one CPU for
  determinism — no change to the scheduler, just more samples.
- (b) Proportional-share scheduling gives **every process with ≥ 1 ticket a
  non-zero probability (lottery) or bounded pass gap (stride) of running**, so no
  runnable process is ever *permanently* skipped — starvation is impossible by
  construction. MLFQ instead approximates SJF-like responsiveness by *demoting*
  CPU-bound work, which *can* starve a low-queue hog — so it must add a **periodic
  boost** to restore progress. MLFQ trades guaranteed non-starvation for better
  approximation of "favour short/interactive jobs".

*(Verified in Python: lottery mean/s.d. for n = 100, 10 000, 20; the full 12-step
stride trace and 6:4:2 counts; the LL bounds; and both RTA computations including
T3's deadline miss in D3.)*
