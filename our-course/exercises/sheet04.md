# Examples Sheet 4 — Proportional-Share and Modern Scheduling **[ext]**

**Attempt after Week 5.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet04-answers.md` (spoilers — attempt first). **All new
material — no IA equivalent.**

**Reading this sheet leans on** (see `../reading-list.md`): OSTEP ch. 9
(proportional-share: lottery *and* stride) and ch. 10 (multiprocessor);
Waldspurger & Weihl (1994), *Lottery Scheduling* (reading item 14 — you
implement it in **Lab 3**, `labs/lab3-scheduler/README.md`); the LWN articles on
Linux CFS → EEVDF; standard real-time scheduling theory — Liu & Layland's
rate-monotonic bound (item 15) and, for Section D's response-time recurrence,
Joseph & Pandya (1986) (item 16, optional; the answer notes also derive it).
OSTEP's `lottery.py` is good drill for Section B.

---

## A. Warm-ups (true/false with justification)

**A1.** "With lottery scheduling, a process holding 25% of the tickets is
*guaranteed* 25% of the CPU over any interval."

**A2.** "Stride scheduling is just lottery scheduling with the randomness
removed, and gives the same *long-run* share but lower variance."

**A3.** "Under CFS, giving a process a lower (nicer) `nice` value multiplies the
*rate* at which its virtual runtime advances, so it is scheduled less often."

**A4.** "If a set of periodic tasks has total utilisation ≤ 1, rate-monotonic
scheduling can always meet every deadline."

---

## B. Lottery and stride scheduling (worked)

**B1. Lottery: expected share and variance.**
Two CPU-bound processes share one core under lottery scheduling. **A holds 75
tickets, B holds 25** (total 100). One ticket is drawn per scheduling decision;
the winner runs for that slice.
- (a) What is A's *expected* share of slices? Over `n` independent draws, A's
  number of wins is a Binomial(`n`, 0.75) random variable. Write down its mean
  and standard deviation as functions of `n`.
- (b) Evaluate the mean and standard deviation for `n = 100` and `n = 10 000`
  draws. Express the standard deviation as a *fraction of n* (the share error)
  in each case. What happens to the *absolute* spread and to the *relative*
  share error as `n` grows?
- (c) Hence explain, in one sentence, the sense in which lottery scheduling is
  fair "in the limit" but can be markedly unfair over a short window — and why
  that matters for a short-lived interactive task.
- (d) *Ticket inflation / currencies:* Waldspurger & Weihl let a process
  subdivide its tickets among its own threads without affecting other processes.
  Why is that safe (i.e. why does it not let one user steal CPU from another),
  and what abstraction enforces the boundary?

**B2. Stride scheduling: trace by hand.**
Stride scheduling makes the proportional share *deterministic*. Each process has
`stride = STRIDE1 / tickets`; the scheduler runs the process with the smallest
`pass`, then adds that process's `stride` to its `pass`. Use **STRIDE1 = 60**
and three processes:

| Process | Tickets | Stride |
|:-------:|:-------:|:------:|
| A | 3 | ? |
| B | 2 | ? |
| C | 1 | ? |

- (a) Compute each stride.
- (b) All passes start at 0. Trace **12 scheduling steps**, breaking ties in the
  fixed order A < B < C. Produce a table with columns `step, pass_A, pass_B,
  pass_C, chosen`. Then count how many times each process ran.
- (c) Show that the run-counts over these 12 steps are *exactly* in the ratio of
  the tickets, and explain why stride achieves this exactness whereas lottery
  only achieves it in expectation.
- (d) Stride has one wart lottery does not: a process that *joins late* (with
  `pass = 0`) monopolises the CPU until its pass catches up. State how real
  implementations fix this (what value a joining process's `pass` is set to).
  What does lottery need to do for a joining process? (Nothing — say why.)

---

## C. CFS / EEVDF (conceptual)

**C1. Virtual runtime.**
- (a) Define *virtual runtime* (`vruntime`) and state the one-line rule CFS uses
  to pick the next process. Why does this rule *approximate* proportional share
  without drawing any random numbers or computing strides explicitly?
- (b) A process's `vruntime` advances at a rate scaled by its weight (derived
  from `nice`). Write the relationship between wall-clock time run and
  `vruntime` charged, in terms of the process's weight and a reference weight,
  and use it to explain warm-up **A3** (correcting it if it is wrong).
- (c) *Sleeper fairness.* A process that blocks on I/O for a long time returns
  with a stale (small) `vruntime` and would otherwise monopolise the CPU. What
  stops a long sleeper from monopolising the CPU on wake, and how do real
  schedulers bound it?

**C2. Why weight-by-nice, and EEVDF.**
- (a) `nice` values map to weights *geometrically* (each nice level is ~1.25×),
  not linearly. What user-visible property does the geometric mapping give (hint:
  the ratio of shares between two tasks depends only on the *difference* of their
  nice values, not their absolute values)?
- (b) CFS optimises long-run share but gives weak *latency* guarantees to a task
  that wants a small slice often. In one or two sentences, state what EEVDF adds
  (a per-request *eligible time* and *virtual deadline*) and what problem with
  CFS it is designed to fix.

**C3. Slice arithmetic (drill).**
CFS turns its policy into concrete slices with two parameters: a **target
latency** — the period within which every runnable task should run once, split
among the runnable tasks in proportion to weight — and a **minimum
granularity** — a floor below which no slice may shrink, so slices do not
dissolve into pure context-switch overhead. Take equal-weight tasks, target
latency **20 ms**, minimum granularity **4 ms**.
- (a) With **5** runnable tasks, what time slice does each task get, and how
  long does any one task wait between the end of one of its slices and the
  start of the next?
- (b) Repeat for **10** runnable tasks. Which of the two parameters determines
  the slice in each of (a) and (b), and what scheduling property is sacrificed,
  and by how much, when they conflict?

---

## D. Real-time scheduling: RM/EDF schedulability

Standard model: independent periodic tasks, task *i* has worst-case compute time
`Cᵢ` and period `Tᵢ`, relative deadline = period, utilisation `Uᵢ = Cᵢ / Tᵢ`.

**D1. The utilisation bounds.**
- (a) State the Liu–Layland sufficient bound for **rate-monotonic (RM)**
  scheduling of `n` tasks, `U ≤ n(2^{1/n} − 1)`, and evaluate it for
  `n = 1, 2, 3` (to 4 dp). What value does it approach as `n → ∞`?
- (b) State the exact **EDF** schedulability condition for this model. In one
  sentence, why is EDF's bound `1` while RM's is lower — what freedom does EDF's
  *dynamic* priority give it that RM's *static* priority lacks?

**D2. A schedulable set (RM).**
Tasks: **T1 (C=1, T=4), T2 (C=1, T=5), T3 (C=1, T=10)**.
- (a) Compute total utilisation and compare to the RM bound for `n = 3`. Is the
  set RM-schedulable by the sufficient test?
- (b) Confirm with **response-time analysis**: priorities by period (T1 highest).
  Iterate `Rᵢ = Cᵢ + Σ_{j higher} ⌈Rᵢ/Tⱼ⌉ Cⱼ` to a fixed point and check each
  `Rᵢ ≤ Tᵢ`.

**D3. A set that separates EDF from RM.**
Tasks: **T1 (C=1, T=4), T2 (C=2, T=6), T3 (C=3, T=8)**.
- (a) Compute total utilisation `U`. Is it ≤ 1? Hence: is the set
  EDF-schedulable?
- (b) Is `U` below the RM bound for `n = 3`? What does the *sufficient* RM test
  tell you (and what does it *not* tell you)?
- (c) Run response-time analysis for RM (priorities by period). Show that **T3's
  response time exceeds its deadline** — so the set is *not* RM-schedulable even
  though `U < 1`. This is the classic demonstration that EDF is strictly more
  powerful than RM for meeting deadlines.
- (d) Give the price EDF pays for that power: two practical disadvantages of EDF
  versus RM (think about overload behaviour and implementation cost).

---

## E. Tie to Lab 3 (discussion)

In **Lab 3** you implement lottery scheduling in xv6 and measure a 30:10 ticket
split converging to roughly 3:1 tick-share, and an MLFQ whose periodic *boost*
un-starves a CPU hog (`labs/lab3-scheduler/README.md`).
- (a) The lab's `LOTTERY` check passes if the measured ratio lands in
  **[2.0, 4.5]** for a 30:10 (expected 3:1) split, "because lotteries are
  noisy". Using your Section B1 variance reasoning, explain *why the acceptance
  window is so wide*, and predict how it could be narrowed (what would you change
  about the measurement, not the scheduler?).
- (b) Lab 3's MLFQ needs a periodic boost to prevent starvation; stride/lottery
  do *not* starve anyone with ≥ 1 ticket. Explain the structural difference:
  which property of proportional-share scheduling rules out starvation by
  construction, and what does MLFQ give up to instead approximate SJF-like
  responsiveness?

---

## Past paper questions

This is extension material (**[ext]**), but one recent Tripos question reaches
directly into it. Attempt it under time pressure (~35 min, closed-book) after
finishing this sheet (file in `../../cambridge-course/exam_questions/`):

- **y2024p2q3** — what process state must be saved on a **context switch**; a
  comparison of **round-robin against CFS** schedules; and how you would
  schedule short-lived **cloud functions**. The context-switch part is Sheet-2/3
  revision; the RR-vs-CFS and cloud-function parts are exactly this sheet's
  proportional-share and `vruntime` material (Section C).

Also attempt **y2021p2q4** this week — it is allocated here rather than with
Sheet 3 so that week 4 does not drown in timed papers. Its FCFS/SJF/SRTF
waiting-time and RR-fairness content is a week fresh, and its Unix
file-permissions part is Sheet-1 revision.
