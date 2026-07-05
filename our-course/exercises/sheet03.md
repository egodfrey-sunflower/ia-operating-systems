# Examples Sheet 3 — CPU Scheduling

**Attempt after Week 4.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet03-answers.md` (spoilers — attempt first).

**Reading this sheet leans on** (see `../reading-list.md`): OSTEP ch. 7
(scheduling metrics, FCFS/SJF/STCF/RR) and ch. 8 (MLFQ); Anderson & Dahlin
ch. 7. The OSTEP `scheduler.py` and `mlfq.py` simulators are the best mechanical
drill for the arithmetic in Section C.

IA pointers are into `../../cambridge-course/examples_sheets/examples_sheet2.pdf`.

**Metric conventions used throughout** (state them in every answer — Cambridge
notes vary):
- **Turnaround time** = completion − arrival.
- **Waiting time** = turnaround − CPU service time (time spent *ready but not
  running*).
- **Response time** = first-time-scheduled − arrival (time to first CPU).
- On a same-instant race (an arrival at the exact tick a quantum expires), the
  **new arrival joins the ready queue *before* the pre-empted process is
  re-queued.** State whichever convention you use.

---

## A. Warm-ups (true/false with justification)

**A1. (from IA Sheet 2, Q3.)** Do the three true/false parts of **IA Examples
Sheet 2, Q3** in `../../cambridge-course/examples_sheets/examples_sheet2.pdf`: (a) a paged
virtual memory is smaller than a segmented one; (b) SJF is an optimal scheduling
algorithm; (c) round-robin can suffer the convoy effect. State your assumptions
for (b) precisely — *optimal for what metric, under what knowledge?* *(Part (a)
is on paging vs segmentation — defer it to Sheet 6 if you have not met paging
yet; do parts (b)–(c), the scheduling parts, now.)*

**A2.** "Shortest-Remaining-Time-First minimises average turnaround time among
all preemptive scheduling algorithms."

**A3.** "Increasing the RR time quantum can only improve (never worsen) average
turnaround time."

---

## B. Scheduling theory (IA foundations — do by citation)

**B1.** Do **IA Examples Sheet 2, Q1** in `../../cambridge-course/examples_sheets/examples_sheet2.pdf`:
- (a) how the CPU is allocated under static priority, *including tie-breaking*;
- (b) the claim "all scheduling algorithms are essentially priority scheduling",
  discussed against FCFS, SJF, SRTF, RR — i.e. state the implicit priority key
  each one sorts on;
- (c) the major problem with static priority (starvation) and its fix (ageing);
- (d) why schedulers favour I/O-bound jobs;
- (e) what SJF/SRTF need to know about each job and how that is *estimated* in
  practice (exponentially-weighted moving average of past bursts).

**B2.** Do **IA Examples Sheet 2, Q2** in `../../cambridge-course/examples_sheets/examples_sheet2.pdf`:
from a single round-robin ready queue, **infer the properties of the scheduling
algorithm**, then compute the processes' **response times** under a **time
quantum of 3**. *Note:* part (c) is the response-time calculation — before you
compute, state whether you read "response time" as *first-CPU − arrival* or as
*turnaround* (Cambridge solutions differ; this sheet's convention list uses
first-CPU).

**B3.** Do **IA Examples Sheet 2, Q8** in `../../cambridge-course/examples_sheets/examples_sheet2.pdf`:
how Unix scheduling favours I/O-intensive jobs. Answer in terms of the classic
Unix multi-level feedback design — priority decaying with recent CPU usage, so a
process that sleeps on I/O keeps a high priority.

**B4. Quantum choice and the convoy effect.**
- (a) A too-small RR quantum and a too-large one are both bad. Describe each
  failure and name the quantity the quantum trades off against (context-switch
  cost).
- (b) Describe the *convoy effect* under FCFS: one CPU-bound job ahead of many
  short I/O-bound jobs. Why does throughput of the *devices* collapse? How does
  RR (or SRTF) break the convoy?

---

## C. Worked calculation (new — the core drill)

Four processes arrive at a single CPU. **No process ever blocks.** Times are in
abstract *ticks*; context switches are free.

| Process | Arrival | CPU burst |
|:-------:|:-------:|:---------:|
| P1 | 0 | 7 |
| P2 | 2 | 4 |
| P3 | 4 | 1 |
| P4 | 5 | 4 |

For **each** of the following policies, (i) draw the Gantt chart, (ii) tabulate
completion, turnaround, waiting, and response time per process, and (iii) give
the averages. Then answer the discussion parts.

- **C1. FCFS** (ties broken by arrival order).
- **C2. SJF**, non-preemptive (on a tie in burst length, run the
  earlier-arriving process — this tie *will* occur; say where and why).
- **C3. SRTF** (preemptive SJF; re-evaluate on every arrival).
- **C4. RR with quantum = 2** (new arrivals join the tail before a
  simultaneously-pre-empted process; see the convention above).

**C5. Discussion.**
- (a) Rank the four policies by *average turnaround* and by *average response*.
  Do the two rankings agree? Explain the divergence in terms of what each metric
  rewards.
- (b) In your SRTF chart, identify every point where a *preemption* happened and
  name the arriving process that caused it. Which process suffers the worst
  waiting time under SRTF, and why is that the price SRTF pays for its optimal
  average turnaround?
- (c) In your SJF chart, the tie between P2 and P4 was broken one way. Recompute
  *only* the two affected processes' turnaround times if you had broken it the
  other way. Does the *average* change? Explain.
- (d) Which single policy here would you deploy on an interactive multi-user
  machine, and which on a batch cluster maximising throughput? Justify from your
  numbers.

*(Self-check: OSTEP's `scheduler.py -p SJF/FIFO/RR` reproduces C1–C4. The
answer key gives the full tables.)*

---

## D. Extending the model

**D1. Priority with I/O.**
Re-imagine P2 and P3 as *I/O-bound*: each runs 1 tick of CPU, then blocks for 3
ticks on I/O, repeatedly, until its total CPU service (from the table) is
consumed; P1 and P4 stay CPU-bound. Without drawing a full chart, argue
qualitatively how a multi-level feedback queue (OSTEP ch. 8) would place these
four processes across its levels after a few hundred ticks, and why the
I/O-bound pair gets better response time without anyone assigning explicit
priorities.

**D2. What the numbers can't show.**
SJF/SRTF need to *know or estimate* future burst lengths (B1(e)). Explain why
the C2/C3 results are therefore an *upper bound* on what a real scheduler
achieves, and describe the estimation error's direction when a process changes
phase (CPU-bound → I/O-bound).

---

## Past paper questions

Per this directory's `README.md`, attempt these two under time pressure (~35 min
each) now that you have finished Sheet 3 (files in `../../cambridge-course/exam_questions/`):

- **y2019p2q3** — the process state machine, an FCFS waiting-time calculation,
  and the convoy effect.
- **y2023p2q4** — context switching, RR vs SJF turnaround with switch overhead,
  and predicting burst lengths.

Hold the third for **week 5** and attempt it alongside Sheet 4, so week 4 is not
buried in timed papers:

- **y2021p2q4** — FCFS/SJF/SRTF average waiting times and RR fairness. Its
  scheduling content is still fresh a week on, and its Unix file-permissions part
  is revision of Sheet 1. (Sheet 4's past-paper section flags this too.)

(**y2024p2q3**'s context-switch and RR parts fit here, but its central RR-vs-CFS
comparison is week-5 (proportional-share) material, so that question is allocated
to **Sheet 4** instead.)

For extra, untimed scheduling drill, these pre-2016 questions fit (files in
`../../cambridge-course/exam_questions/`):

- **y2013p2q3** — I/O-bound vs CPU-bound processes, preemptive vs
  non-preemptive scheduling, and elapsed-time calculations for two interleaved
  CPU/I/O processes — Section B theory plus Section C-style arithmetic.
- **y2010p2q3 [not (c)]** — SJF, SRTF and RR definitions plus
  average-waiting-time calculations for a four-process set — a direct
  companion to Section C. (Part (c), on file-system caches, is later
  material.)
