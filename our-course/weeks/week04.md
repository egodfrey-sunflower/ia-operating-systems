# Week 4 — Scheduling without an oracle: MLFQ and proportional share

> **Part I: Virtualization.** Week 4 of 27.

## What you'll learn

Week 3 ended on a cliffhanger: SJF and STCF are provably optimal for
turnaround time, but they require knowing each job's length in advance —
an oracle no general-purpose OS has. This week is about the two serious
answers to scheduling *without* that knowledge.

Chapter 8's answer is **feedback**: watch what a job does and treat it
accordingly. The multi-level feedback queue keeps several priority queues;
every new job enters at the top, and a job that burns through its
**allotment** of CPU at a level is demoted one queue. Short and interactive
jobs finish (or block) before they sink; long CPU-bound jobs settle to the
bottom. The result approximates SJF with no prediction at all. The chapter
builds the scheduler the honest way — by breaking it twice. First
**starvation**: enough interactive jobs can pin a long job at the bottom
forever, fixed by a **periodic priority boost** (Rule 5). Second **gaming**:
under the naive rules, issuing an I/O just before your allotment expires
keeps you at high priority indefinitely, fixed by charging cumulative usage
per level regardless of how often the job yields (the final Rule 4). What
remains is a scheduler parameterised by **voo-doo constants** — queue count,
per-queue quanta (short at the top, hundreds of milliseconds at the bottom),
boost interval — which is why real deployments like the Solaris
Time-Sharing class expose them as an administrator-tunable table.

Chapter 9 changes the *goal*. A proportional-share scheduler doesn't
optimize turnaround or response time; it guarantees each job a stated
fraction of the CPU. **Lottery scheduling** represents shares as tickets
and holds a randomized draw each time slice — probabilistically correct,
almost no state, and trivially right when jobs come and go. **Stride
scheduling** is its deterministic twin — exact proportions at the cost of
per-job pass values and an awkward question of what pass to give a newly
arrived job. And then the chapter does something rare for a textbook: it
walks through a production scheduler in real detail. Linux's **CFS** tracks
each process's **vruntime**, always runs the lowest, sizes time slices
dynamically from `sched_latency` and `min_granularity`, maps `nice` values
to weights so that equal nice *differences* give equal share *ratios*, and
keeps runnable processes in a red-black tree so decisions cost O(log n).
Cambridge has examined CFS arithmetic directly, so the sheet drills it.

The cross-reading adds the one analytical tool OSTEP omits: OSPP §7.5's
queueing theory. **Little's Law** (L = λW) ties queue length, arrival rate
and waiting time together no matter what the scheduler does — a five-minute
idea you will use for the rest of your career.

**Key ideas:** feedback vs prediction · MLFQ's five rules · allotment ·
starvation and the priority boost · gaming and cumulative accounting ·
voo-doo constants (Ousterhout's Law) · tickets · lottery vs stride ·
CFS: vruntime, `sched_latency`, `min_granularity`, nice-to-weight table ·
Little's Law.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 8** | Scheduling: The Multi-Level Feedback Queue | 12 | 1.7 h |
| 2 | **OSTEP ch. 9** | Scheduling: Proportional Share (lottery, stride, CFS) | 14 | 2.0 h |
| 3 | **OSPP §7.5** | Queueing theory: Little's Law, response time under load | ~8 | 1.1 h |

**Paper (required):** ★ Waldspurger & Weihl (1994), *Lottery Scheduling:
Flexible Proportional-Share Resource Management*, OSDI. OSTEP calls it "the
landmark paper" — read it for the ticket abstraction (currencies, transfers,
inflation) rather than the mechanism, which the chapter already gives you.
Read it **this week, before Lab 2 asks you to build it**: the lab's
scheduler task is a lottery scheduler inside xv6.

**Not yet:** Anderson's spin-lock paper is cited by ch. 10 but is week 5's
optional reading, and multiprocessor scheduling itself (ch. 10) is next
week. This week completes the single-CPU scheduling story.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise04.md`](../exercises/exercise04.md) — budget 3 h. Work closed-book first, then self-mark against [`../exercises/solutions/exercise04-solutions.md`](../exercises/solutions/exercise04-solutions.md) |
| **Lab** | [`../labs/lab01-shell/`](../labs/lab01-shell/) — **ends this week.** [`../labs/lab02-syscalls-sched/`](../labs/lab02-syscalls-sched/) — **starts this week** (runs to week 6): xv6 system calls, then swap the scheduler for lottery and MLFQ. Budget ~5.5 h combined — the week's single lab slot, split between the two |
| **Past papers** | **`y2017p2q3`, timed** — 35 minutes closed-book, then self-mark (~1 h total). A scheduling trace with blocking I/O; it needs only ch. 7, so everything in it is a week behind you — deliberate spaced practice, since each week carries one timed paper |

## Week load

```
OSTEP ch. 8–9        26pp ÷ 7   =  3.7 h
OSPP §7.5             8pp ÷ 7   =  1.1 h
Lottery paper [M]               =  1.5 h
Exercise sheet 4                =  3.0 h
Timed paper y2017p2q3           =  1.0 h
Lab: L1 ends · L2 starts        =  5.5 h
                                  ------
                                  15.9 h   — over the 12–14 h band (labs are not
                                           trimmed to fit)
```

(Unrounded, the terms sum to 15.86 h — the plan's 15.9.) This week
has little slack; if you must trim, take the lab's optional stretch tasks,
not the paper — Lab 2 assumes you have read it.

## Notes for the curious

- **The road OSTEP doesn't take: burst prediction.** The classical
  alternative to MLFQ's feedback is to *predict* the next CPU burst with an
  exponentially weighted average, τ_{n+1} = α·t_n + (1−α)·τ_n, and feed the
  estimate to SJF. OSTEP argues around it — MLFQ gets SJF-like behaviour
  from demotion alone, with no per-job estimator to tune or mislead — but
  Cambridge has asked candidates to *design* exactly this estimator, so
  sheet 4 drills it once with the formula supplied.
- **`mlfq.py` can reproduce the chapter's own failure modes.** The `-S`
  flag re-enables the old Rules 4a/4b, so you can watch a job with an
  I/O frequency just under the quantum monopolize the CPU — the gaming
  attack — and then see the final Rule 4 kill it.
- **CFS's sleeper rule has a cost.** Setting a waking job's vruntime to the
  tree minimum prevents an all-night sleeper from monopolizing the CPU on
  wake, but it also means jobs that sleep briefly and often never bank
  credit for the CPU they gave up — the chapter notes such jobs may not get
  their fair share. Proportional share and I/O still mix awkwardly.
- **Where tickets went.** Lottery scheduling itself is rarely deployed, but
  the ticket abstraction won: Waldspurger's later ESX Server work uses
  shares to proportion *memory* among virtual machines, and every cloud
  "CPU shares" knob is a descendant. Deterministic virtual-time schedulers
  (stride's family, which includes CFS) won the CPU itself.
- The Solaris Time-Sharing class is the canonical example of voo-doo
  constants in the field: a 60-queue table of quanta increasing from 20 ms
  (highest priority) to a few hundred ms (lowest), boosting roughly every
  second — all shipped as defaults
  an administrator can edit.
