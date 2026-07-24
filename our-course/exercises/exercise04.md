# Exercise Sheet 4 — MLFQ and proportional-share scheduling

**Attempt after Week 4.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise04-solutions.md`](solutions/exercise04-solutions.md).

**This sheet leans on:** OSTEP ch. 8–9; OSPP §7.5 (Little's Law — stated in
the question that uses it); Waldspurger & Weihl (1994).

**You will need:** python3 and the OSTEP simulators `mlfq.py`
(`ostep-homework/cpu-sched-mlfq/`) and `lottery.py`
(`ostep-homework/cpu-sched-lottery/`). Every part of §B can be done with pen
and paper; the simulators are for checking your traces.

---

## A. Warm-ups

*True or false? In each case give a one- or two-sentence justification. A
bare verdict earns nothing — the justification is the answer.*

**A1.** MLFQ must be told, when a job starts, whether it is interactive or
CPU-bound.

**A2.** Under the final MLFQ rules, a job that issues an I/O just before its
allotment expires keeps its high priority.

**A3.** The purpose of MLFQ's periodic priority boost (Rule 5) is to improve
response time for interactive jobs.

**A4.** A lottery-scheduled process holding 25% of the tickets is guaranteed
25% of the CPU over any interval.

**A5.** Stride scheduling achieves exact proportions where lottery only
approximates them, so stride strictly dominates lottery.

**A6.** Under CFS, each process's time slice is a fixed constant.

**A7.** Under CFS, changing two competing processes from nice values (−5, 0)
to (+5, +10) leaves their CPU shares unchanged.

**A8.** An MLFQ configured with a single queue behaves exactly like
round-robin.

---

## B. Traces, schedules and arithmetic

**B1. MLFQ by hand.**
A three-queue MLFQ has queues Q2 (highest) > Q1 > Q0 with quanta of 10 ms,
20 ms and 40 ms respectively. The allotment at each level is one quantum.
Priorities are boosted every 200 ms (at t = 200, 400, …). The final rules
from the chapter apply.
  (a) Job A (CPU-bound, needing 120 ms in total) arrives at t = 0 and runs
      alone. Trace its queue level over time and give its completion time.
  (b) Job A is instead endless, and job B, needing 15 ms of pure CPU,
      arrives at t = 100. What queue is A in at t = 100? Trace what happens
      to B, and give B's response time and turnaround time. What scheduling
      policy has MLFQ just approximated for B, and what knowledge did it
      *not* need to do so?
  (c) Under the chapter's earlier Rules 4a/4b (allotment reset on yield),
      describe precisely how an adversarial job games this scheduler, and
      how much CPU it can capture. Explain how the final Rule 4 defeats the
      attack. *(To watch it happen: run `mlfq.py` with `-S`, which restores
      the old rules, and an I/O pattern of your devising.)*
  (d) Now suppose many interactive jobs keep the top queues busy, and one
      CPU-bound job sits at the bottom. Using the assumption that each boost
      lets the CPU-bound job run exactly one 10 ms top-queue quantum before
      it is demoted again (and that it otherwise never runs), derive the
      largest boost period S that still guarantees it at least 5% of the
      CPU. State whether your bound is conservative or optimistic if the
      job also occasionally runs at lower levels.

**B2. Lottery and stride.**
  (a) Jobs A (100 tickets, numbered 0–99) and B (50 tickets, 100–149)
      compete under lottery scheduling. The first ten winning tickets drawn
      are: 119, 42, 3, 148, 77, 61, 130, 22, 9, 105. Write out the schedule,
      give each job's observed CPU share, and compare it with the
      entitlement. What property of lottery scheduling does the gap
      illustrate, and what happens to it as jobs run longer?
  (b) Two jobs each need 10 quanta: job 0 holds 1 ticket, job 1 holds 100.
      Compute the probability that job 0 runs *at all* before job 1
      completes. (Check with `python3 lottery.py -l 10:1,10:100` over a few
      seeds.) What does the number tell you about starvation under extreme
      ticket imbalance?
  (c) Jobs A, B, C hold 200, 100 and 50 tickets. Using 10,000 as the large
      constant, compute each stride, then trace stride scheduling until all
      pass values are equal, breaking ties alphabetically. Confirm the
      resulting counts match the ticket ratios.
  (d) A new job D (100 tickets) arrives just as your trace ends. If D's
      pass value starts at 0, what goes wrong — and why is it worse the
      longer the system has been running? Propose the standard fix, and name
      the CFS mechanism that is this fix in production form.

**B3. CFS arithmetic.** *(Cambridge has set precisely this style of
calculation.)* Take `sched_latency` = 48 ms, `min_granularity` = 6 ms, and
weights: nice −5 → 3121, nice 0 → 1024, nice +5 → 335, nice +10 → 110.
  (a) Four nice-0 processes are runnable. What time slice does each get?
  (b) Ten nice-0 processes are runnable. What time slice does each get, and
      what happens to the guarantee that every process runs within one
      `sched_latency` window? Why is this the right failure mode?
  (c) Two processes compete: A at nice −5, B at nice 0. Compute each one's
      time slice, and each one's vruntime accumulated over its own slice.
      What do you notice about the two vruntime figures, and why is that the
      point?
  (d) Recompute the CPU shares with A at nice +5 and B at nice +10. What
      property of the weight table does the result demonstrate, and why is
      it a sensible design goal?

**B4. Little's Law.** *(OSPP §7.5. The law: in any stable system, the
average number of requests in the system L, the arrival rate λ, and the
average time in system W satisfy L = λW.)*
  (a) A server receives 200 requests/second and takes 50 ms on average from
      arrival to completion. How many requests are in the system on average?
  (b) After a scheduler change, monitoring shows an average of 30 requests
      in the system at the same arrival rate. What has happened to average
      time-in-system, and what does this tell you about using queue length
      as a health metric?

**B5. The estimator OSTEP skips.** *(Not in OSTEP — formula supplied.
Classical schedulers approximate SJF by predicting the next CPU burst with
an exponentially weighted average: τ_{n+1} = α·t_n + (1−α)·τ_n, where t_n is
the burst just observed and τ_n was its prediction.)*
  (a) With α = 0.5 and τ₀ = 10 ms, a job's successive bursts are 8, 4 and
      12 ms. Compute τ₁, τ₂, τ₃.
  (b) What behaviour do α = 1 and α = 0 produce, and what is the trade-off
      α controls?
  (c) In one or two sentences: how does MLFQ obtain SJF-like behaviour with
      no such estimator at all?

---

## C. Discussion and design critique

**C1. MLFQ vs CFS for a shared login server.**
Your department runs one big shared server: dozens of users, each free to
spawn as many processes as they like; batch compiles and simulations run
alongside editors and shells. The requirements, in order: no user may
starve another; interactive processes must stay responsive; administration
effort should be low. Compare MLFQ and CFS as the scheduler for this
machine and recommend one.

Your answer must address: what each scheduler's unit of fairness actually
is, and what that means when one user spawns fifty processes; how each
resists deliberate manipulation; how each recognises interactive work; and
the tuning burden each imposes. Finish with the conditions under which your
recommendation flips.

**C2. Lottery vs stride for a cloud host.**
A cloud provider sells CPU capacity in contractual shares: a customer who
buys 30% of a host must receive 30%. Virtual machines are created and
destroyed constantly. Compare lottery and stride scheduling as the host's
scheduler under this constraint, and recommend one.

Address specifically: what "must receive 30%" means over a short billing
window under each scheduler (make the lottery case quantitative — with
10 ms quanta, roughly how far off can a 1-second window be?); how each
handles VM arrival and departure; and where the ticket-transfer mechanism
from Waldspurger & Weihl's paper would earn its keep on such a host. State
what property of the contract or workload would reverse your choice, and
say which design family production systems actually chose, and why.
