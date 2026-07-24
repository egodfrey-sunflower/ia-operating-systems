# Exercise Sheet 3 — Limited direct execution and scheduling

**Attempt after Week 3.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise03-solutions.md`](solutions/exercise03-solutions.md).

**This sheet leans on:** OSTEP ch. 6–7; OSPP §2.2–2.4; Ousterhout (1990).

**You will need:** a C compiler on a Linux machine (§B1), and python3 with the
OSTEP simulator `cpu-sched/scheduler.py` from `ostep-homework/` (§B2).

> **Note.** Ch. 6 ships no simulator — its homework is *measurement*, and §B1
> is that homework. Ch. 7's `scheduler.py` covers simultaneous arrivals only,
> so §B2(d) and §B3 are hand-worked; Cambridge papers put 9–15 marks on
> exactly this arithmetic.

---

## A. Warm-ups

*True or false? In each case give a one- or two-sentence justification. A bare
verdict earns nothing — the justification is the answer.*

**A1.** Executing a system call transfers control to a kernel address chosen by
the calling process.

**A2.** The actual trap instruction for a system call such as `open()` lives in
the C library, not in the user's own code.

**A3.** When a timer interrupt fires, the operating system's first job is to
save the interrupted process's registers.

**A4.** Under purely cooperative scheduling, the OS is still guaranteed to
regain the CPU eventually, because every process eventually makes a system
call or performs an illegal operation.

**A5.** For jobs of different lengths that all arrive at time 0, SJF minimises
average turnaround time.

**A6.** The shorter the round-robin time slice, the better: response time
improves, and the cost of a context switch is only saving and restoring a few
registers.

**A7.** For jobs of equal length arriving together, round robin gives worse
average turnaround time than FIFO.

**A8.** Because processors have become enormously faster since 1990, system
calls and context switches have become proportionally faster too.

---

## B. Measurement and scheduling arithmetic

**B1. Measure the machinery.** *(Ch. 6's measurement homework.)* Write a small
C harness and measure your own machine.

  (a) `gettimeofday()` returns microseconds — but is it *precise* to the
      microsecond? Measure back-to-back calls to find its real resolution, and
      state how that resolution determines the number of iterations you need
      in (b) for a trustworthy result.
  (b) Measure the cost of a system call by timing a large number of 0-byte
      `read()` calls and dividing. **Before running:** predict the order of
      magnitude. Then run and compare.
  (c) Measure the cost of a context switch with the lmbench trick described in
      the chapter: two processes bound to the **same** CPU (see
      `sched_setaffinity()`), ping-ponging one byte through two pipes.
      Explain (i) why both processes must be pinned to one CPU, and (ii) in
      which direction your result errs — is it an over- or under-estimate of a
      bare context switch?
  (d) In 1996, lmbench measured ~4 µs per system call and ~6 µs per context
      switch on a 200 MHz P6. Convert those to **cycles**, convert your own
      measurements to cycles, and compare. What does the comparison say about
      Ousterhout's thesis?

**B2. Policies under the simulator.** Three jobs of lengths 100, 200, 300
arrive together at time 0.

  (a) By hand, compute average turnaround and average response time under
      FIFO (arrival order 100, 200, 300) and under SJF. Explain the
      relationship between your two answers, and state the general condition
      under which SJF and FIFO produce identical schedules. Check with
      `python3 scheduler.py -p FIFO -l 100,200,300 -c` and `-p SJF`.
  (b) Under RR with quantum 1 (`-p RR -q 1`), write down each job's response
      time, then derive a formula for the **worst-case** response time of a
      job when N jobs share the CPU under RR with quantum q.
  (c) Still RR, quantum 1, no switch cost: compute each job's completion time
      and the average turnaround time. What does the result illustrate about
      RR and turnaround?
  (d) Now stagger the arrivals — A (length 300) at t=0, B (100) at t=10,
      C (200) at t=20 — and hand-compute average turnaround and response time
      under **STCF**, showing the preemption decisions at t=10 and t=20.

**B3. Preemption with blocking I/O.** *(The Cambridge trace idiom — hand-drawn
timeline with a ready queue.)* Two jobs:

- **A**: `[cpu 6, io 2, cpu 6]`
- **B**: `[cpu 1, io 2, cpu 1, io 2, cpu 1]`

I/O from different jobs can proceed in parallel with the CPU and with other
I/O. When a job blocks, the scheduler runs another ready job. Ties at t=0 go
to A. Draw a per-unit timeline for each part, marking CPU, I/O, and idle time.

  (a) Both jobs run **serially**, A to completion then B, with no overlap of
      one job's CPU with the other's I/O. What is the total elapsed time?
  (b) **Non-preemptive** scheduling: a running job keeps the CPU until it
      blocks or finishes. Give the timeline, each job's turnaround time, the
      total elapsed time, and the CPU idle time.
  (c) **Preemptive** RR with a 2-unit slice (a job that becomes ready is
      queued ahead of a job re-queued at the same instant). Same questions.
  (d) Do these results demonstrate that preemptive scheduling is superior?
      Answer with reference to what the workload would have to look like for
      the comparison to flip.

---

## C. Discussion and design critique

*Longer-form. This week's discussion questions are **experiment design**: each
gives you a disagreement, and your answer is the measurement that
would settle it — what you'd run, what you'd record, and what each outcome
would mean. A design with no decision rule earns little.*

**C1.** Ousterhout argued in 1990 that operating system performance was
failing to track processor speed because many OS operations are limited by
memory (and disk), not by the CPU. Thirty-five years later, a colleague claims
the paper is obsolete: "CPUs, caches and memory have all changed beyond
recognition — nobody has checked in decades." Design the measurement study
that settles whether Ousterhout's conclusion still holds **on hardware you can
actually get access to** (you cannot requisition a museum of machines). Specify:
the operations you would measure and why; the hardware parameters you would
normalise against; at least one practical technique for varying "processor
speed" while holding memory constant on a single machine; and precisely which
numerical outcome would confirm, and which would refute, the thesis. Your §B1
results are admissible as pilot data.

**C2.** Two engineers are fixing a time slice. One wants **1 ms**: "response
time matters, and B1 shows a context switch costs about a microsecond —
0.1% overhead, essentially free." The other wants **100 ms**: "the register
save is not the real cost; you're forgetting what a switch does to the
caches." Design the experiment that settles it. Specify the workload(s), the
metrics for each side of the trade-off, the parameter sweep, the result that
would vindicate each engineer, and — since the answer is workload-dependent —
the *conditions* under which each choice is right.

**C3.** A team building a single-purpose network appliance proposes shipping
with the timer interrupt disabled: "our event loop yields constantly;
cooperative scheduling avoids preemption's overhead and nondeterminism." A
reviewer objects that this resurrects the exact failure ch. 6 warns about.
Neither side has data. What would you measure — on the appliance's real
firmware — to decide, and what results would make each side right? State the
condition under which shipping cooperative-only is defensible, and the single
observation that should immediately end the argument in the reviewer's favour.
