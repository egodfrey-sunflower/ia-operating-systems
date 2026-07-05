> # ⚠️ SPOILER — ANSWERS TO EXAMPLES SHEET 3 ⚠️
> **Do not read until you have attempted the sheet closed-book.**
> All numeric results below were verified with a Python simulator.
> Metric conventions: turnaround = completion − arrival; waiting =
> turnaround − service; response = first-run − arrival.

---

# Sheet 3 — Answers

## A. Warm-ups

**A1 (IA Sheet 2 Q3).**
- (a) **False.** With the *same* virtual address size, a paged and a segmented
  virtual memory address the same number of bytes; paging vs segmentation is
  about *structure* (fixed pages vs variable segments), not size. (Paging avoids
  external fragmentation; segmentation matches program structure.)
- (b) **True, with caveats.** SJF minimises *average waiting/turnaround* time —
  but only among *non-preemptive* algorithms, only when all jobs are available
  at once (or with SRTF for arrivals), and only *given knowledge of burst
  lengths*. State those assumptions or lose the mark.
- (c) **False.** The convoy effect is a *FCFS* pathology (short jobs stuck behind
  a long one). Round-robin's whole point is to *prevent* it by time-slicing, so a
  long job cannot hold the CPU indefinitely.

**A2. FALSE (subtle).** SRTF minimises average turnaround *given perfect
knowledge of future burst lengths and preemption on arrival*. It is optimal for
average turnaround among all algorithms *with that knowledge*, but "minimises …
among all preemptive algorithms" overstates it if bursts are unknown — and it
ignores context-switch cost and starves long jobs. Credit "true given clairvoyant
burst knowledge and zero switch cost; false in practice".

**A3. FALSE.** A larger quantum makes RR approach FCFS, which can *worsen*
average turnaround (long jobs delay short ones — the convoy returns). A smaller
quantum improves response but adds context-switch overhead. There is no monotone
"bigger is always better".

## B. Scheduling theory (IA Sheet 2 Q1, Q2, Q8) — marking notes

**B1 (Q1).** (a) Static priority: run the highest-priority runnable process;
**ties** broken by a secondary rule — usually round-robin (FCFS) among equal
priorities. (b) *All are priority schedulers* with an implicit key: FCFS keys on
arrival time; SJF on total burst length; SRTF on *remaining* burst; RR on "time
since last run" (cyclic). (c) Major problem: **starvation** of low-priority
jobs; fix: **ageing** (raise priority with waiting time). (d) Favour I/O-bound
jobs because they run briefly then block, so dispatching them promptly keeps the
*devices* busy (overlap) and they rarely hog the CPU — improving overall
utilisation and response. (e) SJF/SRTF need the *burst length*; it is *estimated*
from history via an exponentially-weighted moving average
`τ_{n+1} = α·t_n + (1−α)·τ_n`.

**B2 (Q2).** Single-queue round-robin: from the ready queue you infer the
algorithm is **round-robin** (cyclic time-slicing, no priority key, quantum-based
preemption — fair, starvation-free, favours neither long nor short jobs). Part
(c) computes the response times under quantum 3; the full worked Gantt chart and
per-process figures are given below under **"B2 (IA Sheet 2, Q2) — RR-quantum-3
worked solution"**. Credit the first-CPU reading of "response time" (state the
convention).

**B3 (Q8).** Classic Unix: a process's scheduling priority *decays* as it
accumulates recent CPU time (usage counter that ages/decays each interval). A
process that blocks on I/O accrues little CPU, so its priority stays high and it
is dispatched quickly when its I/O completes — automatically favouring
interactive/I/O-bound work without the scheduler knowing burst lengths in
advance. (This is the multi-level feedback idea of OSTEP ch. 8.)

**B4. Quantum & convoy.**
- (a) Too small: context-switch overhead dominates (a fixed cost paid every
  quantum) — throughput falls. Too large: RR degenerates to FCFS, response time
  suffers. The quantum trades **response time against switch overhead**; rule of
  thumb: quantum ≫ context-switch time but small enough for good interactivity.
- (b) Convoy under FCFS: a CPU-bound job runs; behind it queue many I/O-bound
  jobs that each want a quick burst then the disk. They all wait for the hog, the
  *disk sits idle* the whole time, then all jobs hit the disk together — device
  utilisation collapses and everything is bursty. RR/SRTF break it by letting the
  short I/O jobs run between the hog's slices, keeping the disk fed.

## C. Worked calculation

Process table: **P1**(arr0,burst7) **P2**(arr2,burst4) **P3**(arr4,burst1)
**P4**(arr5,burst4). All CPU-bound, no blocking, free context switches.
Convention: a same-instant arrival joins the ready queue *before* a
simultaneously-pre-empted process is re-queued.

### C1. FCFS
Gantt: `| P1 0–7 | P2 7–11 | P3 11–12 | P4 12–16 |`

| Proc | Completion | Turnaround | Waiting | Response |
|:----:|:----------:|:----------:|:-------:|:--------:|
| P1 | 7 | 7 | 0 | 0 |
| P2 | 11 | 9 | 5 | 5 |
| P3 | 12 | 8 | 7 | 7 |
| P4 | 16 | 11 | 7 | 7 |
| **avg** | | **8.75** | **4.75** | **4.75** |

### C2. SJF (non-preemptive)
At t=0 only P1 is present → P1 runs 0–7. At t=7 available = {P2(4), P3(1),
P4(4)} → shortest P3 → 7–8. At t=8 available = {P2(4), P4(4)} — **a tie**; break
by earlier arrival → **P2** (arr 2) before P4 (arr 5).
Gantt: `| P1 0–7 | P3 7–8 | P2 8–12 | P4 12–16 |`

| Proc | Completion | Turnaround | Waiting | Response |
|:----:|:----------:|:----------:|:-------:|:--------:|
| P1 | 7 | 7 | 0 | 0 |
| P2 | 12 | 10 | 6 | 6 |
| P3 | 8 | 4 | 3 | 3 |
| P4 | 16 | 11 | 7 | 7 |
| **avg** | | **8.00** | **4.00** | **4.00** |

### C3. SRTF (preemptive)
- t=0: P1 runs (only job).
- t=2: P2 arrives, remaining P1=5 vs P2=4 → **preempt P1**, run P2.
- t=4: P3 arrives, remaining P2=2, P3=1, P1=5 → **preempt P2**, run P3 (1 tick),
  done t=5.
- t=5: P4 arrives; remaining P2=2, P4=4, P1=5 → run P2 to completion (5–7).
- t=7: remaining P4=4, P1=5 → run P4 (7–11).
- t=11: run P1 to completion (11–16).

Gantt: `| P1 0–2 | P2 2–4 | P3 4–5 | P2 5–7 | P4 7–11 | P1 11–16 |`

| Proc | Completion | Turnaround | Waiting | Response |
|:----:|:----------:|:----------:|:-------:|:--------:|
| P1 | 16 | 16 | 9 | 0 |
| P2 | 7 | 5 | 1 | 0 |
| P3 | 5 | 1 | 0 | 0 |
| P4 | 11 | 6 | 2 | 2 |
| **avg** | | **7.00** | **3.00** | **0.50** |

### C4. RR, quantum = 2
Queue trace (arrival at a quantum boundary enters before the pre-empted job):
Gantt: `| P1 0–2 | P2 2–4 | P1 4–6 | P3 6–7 | P2 7–9 | P4 9–11 | P1 11–13 |
P4 13–15 | P1 15–16 |`

| Proc | Completion | Turnaround | Waiting | Response |
|:----:|:----------:|:----------:|:-------:|:--------:|
| P1 | 16 | 16 | 9 | 0 |
| P2 | 9 | 7 | 3 | 0 |
| P3 | 7 | 3 | 2 | 2 |
| P4 | 15 | 10 | 6 | 4 |
| **avg** | | **9.00** | **5.00** | **1.50** |

### C5. Discussion
- (a) **By avg turnaround:** SRTF (7.0) < SJF (8.0) < FCFS (8.75) < RR (9.0).
  **By avg response:** SRTF (0.5) < RR (1.5) < SJF (4.0) < FCFS (4.75). The
  rankings **disagree**: RR is 2nd-best on response (everyone gets the CPU
  quickly, in ≤ one quantum) but *worst* on turnaround (frequent switching
  stretches completion of every job). SRTF wins both here, but only because it is
  clairvoyant. Turnaround rewards *finishing fast*; response rewards *starting
  fast* — RR optimises the latter at the former's expense.
- (b) SRTF preemptions: **at t=2, P2's arrival preempts P1** (5 > 4); **at t=4,
  P3's arrival preempts P2** (2 > 1). Worst waiting time is **P1 = 9**: the
  longest job is repeatedly shunted to the back and only finishes last. That is
  the price SRTF pays — optimal *average* turnaround at the cost of starving/
  delaying the longest job.
- (c) Breaking the P2/P4 tie the *other* way (P4 at 8–12, P2 at 12–16): P4 →
  completion 12, TAT 7; P2 → completion 16, TAT 14. Their TAT sum is 7+14 = **21**
  — identical to the original 10+11 = 21. So the **average is unchanged** (8.00);
  only the distribution between the two tied jobs moves. Ties in equal-length jobs
  redistribute waiting but conserve the total, because swapping two equal-burst
  jobs just exchanges their finish slots.
- (d) **Interactive multi-user machine → RR** (bounded, low response time — no
  user waits long for first service; here best response after clairvoyant SRTF).
  **Batch/throughput cluster → SJF/SRTF** (minimum average turnaround, and
  context-switch overhead is amortised over long jobs). FCFS is simplest but is
  dominated on both metrics here.

## D. Extending the model
**D1.** With P2/P3 as I/O-bound (1 tick CPU then block), an MLFQ keeps them near
the **top** queues: each uses only a fraction of its quantum before *voluntarily*
blocking, so it is never demoted; the CPU-bound P1/P4 use full quanta and sink to
**lower** queues. Result: the I/O-bound pair gets dispatched immediately whenever
their I/O completes (good response) while the hogs share the leftover CPU — MLFQ
*infers* the I/O-bound nature from behaviour, no priority numbers needed.

**D2.** SJF/SRTF assume the burst length is *known*; a real scheduler must
*estimate* it (EWMA of past bursts), so C2/C3's averages are an **optimistic
bound** — real turnaround is worse because estimates err. When a process changes
phase (CPU-bound → I/O-bound), the EWMA lags: it over-estimates the now-short
bursts for a while, mis-prioritising the process until the average catches up.

## B2 (IA Sheet 2, Q2) — RR-quantum-3 worked solution
RR quantum 3; P1(0,9), P2(1,4), P3(7,2).
Gantt: `| P1 0–3 | P2 3–6 | P1 6–9 | P2 9–10 | P3 10–12 | P1 12–15 |`.
- **Response** (first-run − arrival): **P1 = 0, P2 = 2, P3 = 3.**
- (For completeness — turnaround: P1 = 15, P2 = 9, P3 = 5.)
Note the definitional subtlety: some Cambridge solutions read "response time"
here as *turnaround*; state which you use. The first-CPU reading is the more
standard "response time" and is what makes RR attractive for the multi-user
setting the question is about.

*(All C1–C4 and the Q2(c) figures were reproduced by a Python discrete-event
simulator; you can cross-check with OSTEP's `scheduler.py`.)*
