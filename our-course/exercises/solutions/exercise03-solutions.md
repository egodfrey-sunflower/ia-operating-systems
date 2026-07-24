> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 3 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> These are model answers and marking notes, not the only correct responses.
> Where a question is open-ended the notes flag the points a supervisor wants
> to see made, rather than prescribing wording.

---

## A. Warm-ups

**A1. FALSE.** The process supplies a system-call **number**, not an address.
The hardware jumps through the **trap table**, which the OS installed at boot
with a privileged instruction. That indirection is a protection mechanism: if
a process could name the target address it could jump into kernel code just
past a permission check, and the kernel would be running attacker-chosen
sequences.

**A2. TRUE.** `open()` in your program is an ordinary procedure call — into
the C library. Inside the library, hand-written assembly places the arguments
and the system-call number at agreed locations and executes the trap
instruction. This is why you never write assembly to make a system call:
somebody already did, and it must follow the kernel's calling convention
exactly.

**A3. FALSE — the hardware has already done it.** When the interrupt fires,
the *hardware* implicitly saves the user registers of the running process onto
its kernel stack, before any OS code runs. The OS performs a second, distinct
save — kernel registers into the process structure, in `switch()` — and only
if it decides to switch. Distinguishing the implicit hardware save from the
explicit software save is the point of this question; answers that merge them
into one save earn little.

**A4. FALSE.** A buggy or malicious process can spin forever in a loop that
makes no system call and touches nothing illegal. Under the cooperative
approach the OS is then simply never entered, and the chapter is blunt about
the only remedy: reboot the machine. This is precisely why the timer
interrupt — armed at boot, by a privileged operation — is what makes
preemptive scheduling possible.

**A5. TRUE.** With simultaneous arrivals and run-to-completion, any schedule
that runs a longer job before a shorter one can be improved by exchanging
them — the shorter job's turnaround falls by more than the longer job's rises.
Repeat until sorted: that is SJF. (Credit for noting the guarantee evaporates
once arrivals stagger — a short job arriving just after a long one has started
still waits, the convoy effect in miniature — which is what preemption/STCF
fixes.)

**A6. FALSE.** Two errors. First, slice length must amortise the switch cost:
with a 1 ms slice and a 1 ms switch, half of all CPU time is overhead; the
chapter's example (10 ms slice, 1 ms switch) already wastes 10%. Second, the
register save/restore is not the whole cost — a switch also evicts cache, TLB
and branch-predictor state built up by the outgoing job, so the incoming job
runs slow for a while. The right slice is long enough to amortise both, short
enough to stay responsive.

**A7. TRUE.** RR stretches every job: with three 5-second jobs and a 1-second
slice, completions are 13, 14, 15 (average turnaround 14) versus FIFO's 5, 10,
15 (average 10). Any policy that is fair on small time scales pays this
turnaround penalty; RR is close to pessimal on that metric. That is not a bug
in RR — it is the price of optimising response time.

**A8. FALSE.** This is Ousterhout's point, echoed in ch. 6's aside: many OS
operations are **memory-intensive**, and memory performance has not scaled
with processor speed. So OS operations improve more slowly than CPU-bound
code. B1(d) asks you to check the cycle counts yourself — the wall-clock
numbers improved far less than three decades of frequency gains would predict.

---

## B. Measurement and scheduling arithmetic

**B1.**
**(a)** Call `gettimeofday()` in a tight loop storing successive values. Most
deltas are 0; the smallest nonzero delta is the effective resolution —
typically on the order of 1 µs (the interface's granularity) even when the
underlying clock is finer. Consequence: you cannot time one syscall, whose
cost may be well under the resolution. Time **N iterations in one interval**
and divide, choosing N so the whole loop runs for ≫ the resolution — e.g.
aiming for a total of at least 10⁴–10⁵ × resolution (N of order 10⁶ is
typical) so quantisation error is a fraction of a percent. If that is still
too coarse, `rdtsc` gives cycle-granularity timestamps.
**(b)** A 0-byte `read()` enters the kernel, validates arguments, and returns
without touching data — a floor for system-call cost. **Expected magnitude:
a few hundred nanoseconds** on modern hardware (ch. 6: "sub-microsecond");
predictions of ~1 µs are reasonable, milliseconds are off by three orders.
Marking notes: the loop must subtract loop overhead (time an empty loop) or
show it is negligible; optional extra credit for knowing some "system calls"
(notably `gettimeofday` itself) may be answered in userspace via a
vDSO-style fast path without entering the kernel — which is why the 0-byte
`read()` is the right probe, not `gettimeofday`.
**(c)** Round trip: parent writes a byte to pipe 1 and blocks reading pipe 2;
child reads pipe 1, writes pipe 2. Each round trip forces **two** context
switches; time N round trips, divide by 2N.
  (i) **Pinning:** on a multicore machine the two processes would otherwise
  run on *different* cores in parallel, nothing would be switched, and you
  would measure inter-core pipe latency instead. `sched_setaffinity()` to one
  CPU forces every hand-off to be an actual context switch.
  (ii) **Over-estimate.** Each measured switch also includes a `write()` and
  a `read()` system call, pipe buffer management, and wakeup logic — so the
  result is an upper bound on the bare save/pick/restore cost. (Deeper answers
  may note the *indirect* cache-pollution cost is simultaneously
  under-measured, because this microbenchmark has a tiny working set; either
  direction earns credit if argued.)
**(d)** 1996: 4 µs × 200 MHz = **800 cycles** per syscall; 6 µs × 200 MHz =
**1200 cycles** per switch. A modern measurement of, say, 250 ns at 3 GHz is
**750 cycles** — wall-clock improved ~16×, cycle count barely moved. The
gains came almost entirely from clock frequency; per-cycle, these operations
have not improved, because they are dominated by memory traffic, privilege
transitions and state movement rather than by arithmetic the core can speed
up. That is Ousterhout's thesis surviving in miniature. *Marking note: any
plausible measured numbers are acceptable — the marks are for correct cycle
conversion and for drawing the cycles-vs-wall-clock distinction.*

**B2.**
**(a)** FIFO (order 100, 200, 300): completions 100, 300, 600.

```
turnaround = (100 + 300 + 600) / 3 = 1000/3 ≈ 333.3
response   = (0 + 100 + 300) / 3   = 400/3  ≈ 133.3
```

SJF: sorts by length — but the jobs already arrive in ascending order, so the
schedule is **identical**. General condition: SJF and FIFO coincide whenever
jobs (arriving together) are submitted in non-decreasing length order. The
simulator confirms both.
**(b)** RR, q=1: first slices run at times 0, 1, 2 → response times **0, 1, 2**
(average 1). Worst case for N jobs, quantum q: a newly-arrived job waits for
one slice of each of the other jobs ahead of it in the queue:

```
worst-case response = (N − 1) · q
```

**(c)** Think in rounds of 3 units while all three jobs live. Job 1 (100)
takes the first unit of each round; its 100th slice ends at 3·99 + 1 = **298**.
Round 100 completes with job 2's slice (298–299) and job 3's (299–300); job 2
then has 100 left, job 3 has 200. They alternate from t=300: job 2's k-th
remaining slice ends at 300 + 2k − 1, so job 2 finishes at **499**; job 3 runs
alone thereafter and — since total work is 600 and the CPU never idles —
finishes at **600**.

```
turnaround = (298 + 499 + 600) / 3 = 1397/3 ≈ 465.7
```

Versus 333.3 for FIFO/SJF: RR is markedly worse on turnaround even though its
response time (average 1) is spectacular. Fairness on small time scales
stretches every completion — the inherent trade-off, on your own numbers.
**(d)** STCF trace: A runs 0–10. At t=10, B (100) < A's remaining 290 →
**preempt, run B**. At t=20, C (200) > B's remaining 90 → **B continues**,
finishing at 110. Then C (200) < A (290) → C runs 110–310. A finishes 310–600.

```
turnaround: A = 600−0 = 600;  B = 110−10 = 100;  C = 310−20 = 290
            average = 990/3 = 330
response:   A = 0;  B = 0;  C = 110−20 = 90;  average = 30
```

**B3.**
**(a)** Serial: A takes 6+2+6 = 14; B then takes 1+2+1+2+1 = 7. **Total 21.**
No overlap — the CPU sits idle during every I/O.
**(b)** Non-preemptive (run until block or finish; A first):

```
t:    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
CPU:  A  A  A  A  A  A  B  -  A  A  A  A  A  A  B  -  -  B
A:    [cpu 0–6] [io 6–8] [cpu 8–14 → done 14]
B:    [cpu 6–7] [io 7–9] [waits 9–14] [cpu 14–15] [io 15–17] [cpu 17–18 → done 18]
```

A's I/O (6–8) overlaps B's first burst; but once A returns at t=8 it holds the
CPU for its entire 6-unit burst, so B — ready again at t=9 — waits five units.
**Turnaround: A = 14, B = 18. Elapsed 18. Idle: t=7–8 and 15–17, 3 units.**
**(c)** Preemptive, 2-unit slice:

```
t:    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
CPU:  A  A  B  A  A  B  A  A  B  -  A  A  A  A  A  A
A:    cpu 0–2, 3–5, 6–8 (burst 1 done t=8) · io 8–10 · cpu 10–16 → done 16
B:    cpu 2–3 · io 3–5 · cpu 5–6 · io 6–8 · cpu 8–9 → done 9
```

Trace of the decisions: slice expiry at t=2 lets B in; B blocks at 3; A runs
3–5; B's I/O completes at 5 and (ready-before-requeued) B preempts at the
slice boundary; B blocks again at 6; A finishes its first burst at 8 and
blocks; B runs its final unit and **finishes at 9**; the CPU idles 9–10 while
A's I/O completes; A runs 10–16.
**Turnaround: A = 16, B = 9. Elapsed 16. Idle: t=9–10, 1 unit.**
**(d)** Preemption here improved *everything except A*: B's turnaround halved
(18 → 9), total elapsed fell (18 → 16), idle time fell (3 → 1), at a cost of
2 units to A (14 → 16). But **one workload demonstrates nothing general**.
The comparison flips, or narrows, when: the short job's bursts are long
relative to the slice (less to gain from preempting); context switches have a
real cost (here assumed free — with a switch cost c, part (c)'s six switches
cost 6c against a 2-unit gain); or both jobs are pure CPU (then preemption
only redistributes waiting, it cannot reduce elapsed time, which is fixed at
total work). The honest conclusion: preemption wins when short, I/O-bound
jobs would otherwise queue behind long CPU bursts — which is exactly this
workload, and exactly the workload interactive systems face.
*Marking note: full credit needs correct timelines with idle time identified,
not just final numbers — the ready-queue reasoning at t=5 and t=8 is where
errors happen.*

---

## C. Discussion and design critique

*This week's discussion questions are experiment design: the marks are for a
measurement that would actually discriminate between the two positions, with
an explicit decision rule.*

**C1.** A strong answer has four components.

**Operations:** a basket of kernel-dominated microbenchmarks — the 0-byte
`read()`, the pipe ping-pong switch, plus a few compound ones (`fork()+exec()`,
create/delete of a small file) — alongside a **CPU-bound control** (a tight
arithmetic loop) and a **memory-bound control** (pointer-chasing over an array
much larger than cache). The controls are what make the study interpretable.

**Normalisation:** express every cost in *cycles* (cost × clock frequency),
and separately measure the machine's memory latency (the pointer-chase) and
bandwidth in cycles. Ousterhout's thesis predicts OS operation costs track the
*memory* numbers, not the cycle time.

**Varying CPU speed on one machine:** use frequency scaling (DVFS) — rerun
the whole basket at, say, half and full clock speed, with memory speed
unchanged. Decision rule per operation: if halving the clock **doubles** the
wall-clock cost, the operation is CPU-bound (thesis refuted for it); if the
cost barely moves, it is memory/IO-bound (thesis confirmed). Real operations
land between; report the scaling exponent.

**Outcome statement:** thesis *holds* if the OS basket's costs scale with
clock substantially less than the CPU-bound control does (equivalently: OS
costs in cycles rise as frequency rises); thesis *refuted* if OS operations
scale like the arithmetic loop. B1(d)'s cycle comparison (≈800 cycles in 1996,
similar today) is admissible pilot evidence for the first outcome.

*Marking notes: the DVFS idea (or an equivalent single-machine manipulation)
is the discriminating insight — without some way to vary CPU speed while
holding memory constant, the study measures one point and settles nothing.
Also credit: controlling for what 1990 could not have anticipated (multicore —
pin everything to one core), and honesty that a single machine tests the
mechanism, not the decades-long trend.*

**C2.** The disagreement is really about **indirect** switch cost, so the
experiment must make cache damage visible.

**Workload:** two processes pinned to one core — a *batch* job that repeatedly
walks a working set of adjustable size W (measure its throughput in
iterations/sec), and an *interactive* job that sleeps, wakes, does a tiny
amount of work, and records its wakeup-to-completion latency (report p95, not
the mean).

**Sweep:** quantum q ∈ {1, 3, 10, 30, 100} ms, crossed with W below, near,
and above the cache size.

**Predictions and decision rule:** the 1 ms engineer's model (direct cost
only) predicts batch throughput flat in q — 0.1% overhead everywhere. The
100 ms engineer predicts throughput climbing with q whenever W is
cache-sized, because each switch forces a refill costing W/bandwidth, which
at q = 1 ms can be a large fraction of the slice. Interactive p95 latency
grows roughly linearly in q. Plot both against q: if throughput is flat, take
1 ms and the responsiveness for free; if throughput craters at small q, the
knee of the curve *is* the answer.

**Conditions attached:** small W (both jobs cache-resident) → 1 ms is right;
W comparable to cache → q must exceed several refill times, favouring tens of
ms; latency target of ~100 ms (human-facing) tolerates a larger q than a
10 ms target. The number of runnable jobs matters too — response degrades as
(N−1)·q (B2(b)), so bigger N argues for smaller q. *Marking note: the key
required element is a workload in which the two models make different
predictions; a design that only measures direct switch cost more precisely
settles nothing.*

**C3.** The claim to test is "our event loop yields constantly", and it is
measurable: **instrument the firmware to record the distribution of intervals
between yields/blocking calls** under realistic *and* adversarial load
(malformed packets, full queues, error paths). Secondarily, fault-inject a
wedged handler and measure time-to-recovery with and without the timer.

Decision rules: if the **maximum** observed inter-yield gap (not the mean)
stays below the device's latency requirement across all paths, and every loop
in the code is provably bounded, cooperative-only is defensible — this is how
many small embedded runtimes genuinely ship, and the team is right that it
buys determinism and a simpler kernel. The reviewer wins outright the moment
any measurement shows an **unbounded** gap — one code path that can loop
without yielding (a retry loop on a wedged device, a parser on hostile input).
A single such observation ends the argument, because ch. 6's point is
qualitative, not statistical: without the timer the OS's control of the CPU
depends on *every* path in *every* future firmware update yielding, and the
failure mode is a bricked appliance in the field.

Condition to state: the verdict also flips if the device will ever run
third-party or field-updated code — auditing "all paths yield" is only
possible while the team owns every line.
*Marking note: credit answers that measure the worst case rather than the
average, and that identify the asymmetry: the proposer must prove a universal
property, the reviewer needs one counterexample.*
