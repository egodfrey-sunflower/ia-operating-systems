> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 4 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** The entire point of MLFQ is that it needs no such
declaration: every job enters at the top, and its *observed behaviour* —
whether it burns its allotment or yields for I/O — determines where it
settles. MLFQ uses the recent past to predict the future, like a branch
predictor or a cache.

**A2. FALSE.** That worked under the discarded Rules 4a/4b. The final
Rule 4 charges a job's *cumulative* CPU usage at each level, no matter how
many times it gives up the CPU; once the allotment is spent, the job is
demoted regardless. This is precisely the anti-gaming fix.

**A3. FALSE.** Interactive jobs are already at the top; boosting everyone
to the top helps them not at all. Rule 5 exists for the *other* jobs: it
prevents starvation of CPU-bound work when interactive jobs saturate the
upper queues, and it lets a job that has *changed* behaviour (CPU-bound
phase → interactive phase) be re-evaluated instead of being trapped at the
bottom by its history.

**A4. FALSE.** Lottery scheduling is probabilistically correct only. Over
any finite interval the observed share is a random variable around 25%; the
chapter's fairness study shows short jobs can be far off, with convergence
only as the number of draws grows. Guaranteed proportions need a
deterministic scheme — stride or CFS.

**A5. FALSE.** Stride wins on precision but loses on state: each job
carries a pass value, and a newly arriving job has no obviously correct
one — set it to zero and it monopolizes the CPU until it catches up (B2d).
Lottery needs no global state; adding a job is just adding tickets. Which
matters more depends on churn — "strictly dominates" is exactly wrong.

**A6. FALSE.** CFS computes the slice dynamically: `sched_latency ÷ n` for
n runnable processes (weighted by nice), floored at `min_granularity`, so
the slice shrinks as load rises. A fixed quantum is what MLFQ uses per
queue; CFS deliberately does not.

**A7. TRUE.** The weight table is built so that a constant *difference* in
nice values yields a constant *ratio* of weights (each step is roughly
×1.25). (−5, 0) gives 3121:1024 ≈ 0.753 of the CPU to the first process;
(+5, +10) gives 335:110 ≈ 0.753 — the same split (B3d).

**A8. TRUE.** With one queue there is nothing to demote to and no priority
to differ in; Rules 1–5 collapse to "run everyone in RR with the queue's
quantum". Worth knowing as a sanity check: MLFQ is round-robin *plus* a
priority structure learned from behaviour.

---

## B. Traces, schedules and arithmetic

**B1.**
**(a)** t = 0–10 at Q2 (allotment spent → demote); 10–30 at Q1 (20 ms →
demote); then Q0 in 40 ms quanta: 30–70, 70–110, and 110–120 for the final
10 ms. **Completion at t = 120 ms.** The first boost (t = 200) never fires.
Sanity check: 10 + 20 + 40 + 40 + 10 = 120. ✓

**(b)** A was demoted to Q0 at t = 30, so at t = 100 **A is in Q0**. B
enters Q2 and, being higher priority, preempts A immediately: B runs
100–110 (allotment spent → Q1), then 110–115 at Q1 and finishes.
**Response time 0 ms; turnaround 15 ms.** B was never delayed by A at all —
MLFQ approximated **SJF/STCF**, and it needed no advance knowledge of B's
run time; B's position in the queues *was* the prediction.

**(c)** Under 4a/4b, yielding before the allotment expires resets it. The
attack: run for just under the quantum (say 9.9 of 10 ms), then issue a
trivial I/O. The job stays at Q2 forever and captures arbitrarily close to
**99% of the CPU**, starving everything below. Rule 4 defeats it by
accounting *cumulative* usage per level: after 10 ms of total top-queue CPU
— in one burst or many — the job is demoted, whatever its I/O pattern.

**(d)** Per boost period S the starving job receives 10 ms (one top-queue
quantum). Requiring 10/S ≥ 5% gives **S ≤ 200 ms** — boost at least every
200 ms. The bound is **conservative**: any CPU the job scrounges at lower
levels, or extra quanta before re-demotion, only raises its share above 5%.
(This is the chapter's own homework question 5, worked symbolically.)

**B2.**
**(a)** Tickets ≥ 100 belong to B, so the draws give
**B A A B A A B A A B** — A runs 6/10 (60%), B runs 4/10 (40%), against
entitlements of 66.7% and 33.3%. The gap illustrates **probabilistic
correctness**: lottery guarantees proportions only in expectation. As run
length grows the law of large numbers takes over — the chapter's fairness
metric F approaches 1 (Figure 9.2).

**(b)** Job 0 runs before job 1 completes exactly when it wins at least one
of the first 10 draws. Each draw, job 0 wins with probability 1/101. So

```
P = 1 − (100/101)^10 ≈ 1 − 0.905 = 0.095
```

— about **one run in ten even sees job 0 execute** before job 1 has
finished entirely. Extreme ticket imbalance thus produces near-serial
execution in practice; lottery prevents *indefinite* starvation (job 0
eventually wins) but offers no useful bound on the wait. Fairness under
imbalance is a policy you chose, not a defect of the mechanism.

**(c)** Strides = 10,000 / tickets: **A = 50, B = 100, C = 200.** Trace
(run lowest pass; ties alphabetical):

```
pass:  A     B     C     runs
       0     0     0     A  → A=50
       50    0     0     B  → B=100
       50    100   0     C  → C=200
       50    100   200   A  → A=100
       100   100   200   A  → A=150   (tie A,B)
       150   100   200   B  → B=200
       150   200   200   A  → A=200
```

All passes now equal 200: the cycle is complete after 7 quanta with counts
**A 4, B 2, C 1 = 200 : 100 : 50.** ✓

**(d)** With pass 0 against incumbents at 200, D runs **2 quanta back to
back** before anyone else. Harmless here — but pass values grow without
bound, so on a system that has run for hours the incumbents' passes are
enormous and D would monopolize the CPU for as long as it took its pass to
catch up. The fix: initialize a new (or waking) job's pass to the
**minimum pass currently in the system** (a global virtual time), so it
joins the rotation rather than replaying history. CFS's production form of
exactly this rule: a waking task's **vruntime is set to the minimum
vruntime in the red-black tree**.

**B3.**
**(a)** 48 ms ÷ 4 = **12 ms each.**
**(b)** 48 ÷ 10 = 4.8 ms < `min_granularity`, so each gets **6 ms**; a full
rotation now takes 60 ms and the every-process-within-48 ms guarantee is
knowingly missed. This is the right failure mode because the alternative —
ever-thinner slices as load grows — makes context-switch overhead consume
the machine. CFS chooses to stretch the fairness horizon, which degrades
gracefully, rather than efficiency, which doesn't.
**(c)** Total weight 3121 + 1024 = 4145.

```
A's slice = 3121/4145 × 48 ≈ 36.1 ms
B's slice = 1024/4145 × 48 ≈ 11.9 ms
A's vruntime over its slice = 36.1 × (1024/3121) ≈ 11.9 ms
B's vruntime over its slice = 11.9 × (1024/1024) = 11.9 ms
```

The two vruntime figures are **equal** — that is the point. Weighting slows
A's virtual clock by exactly the factor its slice was stretched, so every
rotation advances both processes' vruntime by the same amount and the
alternation is stable. Weights turn into CPU proportions *because* vruntime
advances equally.
**(d)** Shares become 335/(335+110) = 335/445 ≈ **0.753** for A — the same
split as 3121/4145 ≈ 0.753 (slices ≈ 36.1 ms and 11.9 ms again). The table
preserves ratios under a constant nice *difference* (it is close to
geometric, ~×1.25 per step). Sensible because "5 nicer than you" should
mean the same relative penalty anywhere on the scale — otherwise absolute
nice values would need system-wide coordination to mean anything.

**B4.**
**(a)** L = λW = 200 × 0.050 = **10 requests** in the system on average.
**(b)** W = L/λ = 30/200 = **150 ms** — average time-in-system has tripled.
Little's Law makes queue length a *latency measurement in disguise*: at a
known arrival rate, watching L is watching W. A dashboard showing queue
depth creeping up is reporting a latency regression whether or not anyone
instrumented latency. (It also cuts the other way: no scheduler change can
lower W at fixed λ without lowering L — the law holds regardless of
policy.)

**B5.**
**(a)** τ₁ = 0.5·8 + 0.5·10 = **9 ms**; τ₂ = 0.5·4 + 0.5·9 = **6.5 ms**;
τ₃ = 0.5·12 + 0.5·6.5 = **9.25 ms**.
**(b)** α = 1: the prediction is simply the last burst — maximally
responsive, maximally noisy. α = 0: the prediction never changes — all
evidence is ignored. α trades **responsiveness to genuine phase changes
against smoothing of one-off noise**.
**(c)** MLFQ never predicts a number at all: demotion sorts jobs by
observed CPU appetite, and a job's **queue level is the prediction** —
feedback replaces estimation, with no α to mistune and nothing for a noisy
burst to corrupt.

---

## C. Discussion and design critique

**C1.** A strong answer is organised around one observation the question is
engineered to surface: **both schedulers' default unit of fairness is the
process, and the stated requirement is fairness between users.**

- **Unit of fairness.** Under either scheduler, a user who spawns fifty
  compiler processes holds fifty claims on the CPU; per-process fairness
  *is* the starvation vector. What decides the comparison is who has an
  answer: CFS can schedule over **groups** of processes (per-user
  hierarchies — the chapter notes CFS "can schedule across large groups"),
  making the share per-user and dividing it within; classic MLFQ has no
  equivalent knob — its priorities encode behaviour, not ownership.
- **Manipulation.** MLFQ's history includes a working gaming attack
  (Rule 4a/4b); the fix helps, but the boost and I/O handling remain
  behavioural heuristics an adversary can court. CFS accounts actual
  runtime — to look deserving you must actually not run. Its soft spot is
  the sleeper rule (feigned sleeping to reset vruntime to the minimum), a
  smaller surface.
- **Interactivity.** MLFQ recognises it structurally (I/O-bound jobs stay
  high); CFS relies on sleepers waking with minimum vruntime and hence
  prompt scheduling. Both are adequate for editors-beside-compiles; MLFQ is
  arguably sharper here, which is the honest concession to make.
- **Tuning.** MLFQ: queue count, per-queue quanta, allotments, boost
  period — voo-doo constants by the chapter's own account, cf. the Solaris
  60-row table. CFS: two headline parameters plus nice values users already
  understand.

**Recommendation:** CFS, driven by per-user group fairness and the lower
tuning burden. **Flips if:** the machine is single-user or all users are
trusted (per-user fairness moot — MLFQ's simplicity and interactivity bias
win); or the requirement hardens from fairness into latency *ceilings* for
interactive work (weighted fairness gives no ceiling; explicit priority
tiers or a real-time class do); or the box is a saturated many-core server
where per-decision O(log n) and scheduler overhead — ~5% of datacenter CPU
in the study OSTEP cites — are the binding cost.

*Marking note: answers that compare "fairness" in the abstract without
spotting the process-vs-user mismatch earn little; the question hands the
requirement over ("each free to spawn as many processes as they like") and
expects it used. Full credit needs a recommendation plus at least two
conditions that flip it.*

**C2.** The shape of a good answer:

- **What the contract means under each.** Stride delivers 30% *exactly* at
  every cycle boundary — auditable over any window longer than a cycle.
  Lottery delivers 30% in expectation; over a 1-second billing window at
  10 ms quanta there are only n = 100 draws, so the standard deviation of
  the observed share is √(p(1−p)/n) = √(0.3·0.7/100) ≈ **4.6 percentage
  points** — a customer can measure 25% against a 30% contract with no one
  cheating. Over an hour (n = 360,000) the deviation is negligible. So the
  deciding constraint is the **horizon over which the share must be
  certifiable**.
- **Churn.** VMs arriving and departing is lottery's home ground: no per-VM
  state beyond a ticket count, and admission is trivially correct. Stride
  must answer the new-pass question (B2d) — solvable with a global virtual
  time, but that is precisely the extra machinery lottery exists to avoid.
- **Ticket transfer** earns its keep whenever a VM blocks on a shared host
  service (I/O daemon, virtual switch): the client VM transfers its tickets
  to the server so its purchased share keeps working on its behalf —
  otherwise a premium customer's requests queue behind a budget customer's
  at an unweighted service. This is the paper's client/server scenario, and
  it applies to either scheduler family.
- **Verdict and flip.** Short billing windows or SLA-grade certification →
  stride-family determinism, paying the virtual-time bookkeeping. Long
  windows with heavy churn → lottery is defensible and simpler. **What
  production chose:** the deterministic virtual-time family — Linux CFS,
  and proportional *shares* in VMware's ESX (Waldspurger's own later
  system) — because contracts are exactly what randomness is bad at over
  short horizons. Lottery's surviving legacy is the ticket abstraction
  itself.

*Marking note: the quantitative step — some estimate of lottery's
short-window variance, even rough — separates strong answers from
hand-waving; so does noticing that churn and certifiability pull in
opposite directions, which is what makes the question a judgement rather
than a lookup.*
