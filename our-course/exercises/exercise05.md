# Exercise Sheet 5 — Multiprocessor scheduling and the address space

**Attempt after Week 5.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise05-solutions.md`](solutions/exercise05-solutions.md).

**This sheet leans on:** OSTEP ch. 10 and 13. (Ch. 11 and 12 are two-page
summary dialogues; nothing below depends on them. The xv6 ch. 4 reading is
exercised by Lab 2, not by this sheet.)

**You will need:** python3 and the OSTEP simulator
`ostep-homework/cpu-sched-multi/multi.py` — for *checking* §B1 only; the
question is designed to be worked on paper. §B4(c) needs a Linux machine with
`pmap` (any shell will do as the target process). No compiler is needed.

---

## A. Warm-ups

*True or false? In each case give a one- or two-sentence justification. A bare
verdict earns nothing — the justification is the answer.*

**A1.** Hardware cache coherence ensures that two CPUs concurrently removing
elements from a shared linked list will leave the list in a correct state.

**A2.** A process usually reruns faster on the CPU it last ran on.

**A3.** Because every CPU draws work from one shared queue, single-queue
multiprocessor scheduling (SQMS) both balances load well and scales well.

**A4.** Under multi-queue scheduling (MQMS), once a job has been placed on a
queue there is never a reason to move it.

**A5.** The address a C program prints with `%p` is the physical memory
location of the data.

**A6.** The heap must sit just after the code and grow toward the stack, which
grows from the far end of the address space toward the heap; hardware requires
this layout.

**A7.** Memory could be time-shared by saving the running process's entire
memory to disk at each switch; the reason systems don't do this is that it
would be insecure.

**A8.** In a work-stealing scheduler, checking other CPUs' queues more
frequently always improves the schedule.

---

## B. Mechanism and measurement

**B1. The cost of losing your cache.**
Work these on paper using the following model of `multi.py` (all of it stated
here so the arithmetic is self-contained). If you also run the simulator, pass
`-w 10 -r 2`, and read the note in the solutions before comparing: **the model
below and `multi.py` do not agree**, deliberately, and part (d)(i) is where the
difference shows up.

- A job needs `run_time` units of progress. On a **cold** cache it makes 1 unit
  per tick; on a **warm** cache, 2 units per tick.
- A CPU's cache becomes warm for a job after the job has run **10 consecutive
  ticks** on that CPU, *and only if* the job's `working_set_size` fits in the
  cache. Warmth is lost when the job leaves that CPU.
- In every part below, working sets equal the cache size, so a cache can be
  warm for at most one job at a time.

  (a) One CPU, cache size 100, one job `a:30:200` (run time 30, working set
      200). How many ticks to completion, and why?
  (b) Same job, cache size 300. How many ticks now? Give the general formula
      for a run time `R` greater than the warmup time.
  (c) Two CPUs, cache size 100 each, jobs `a:100:100` and `b:100:100`, time
      slice 5 ticks. Compare two dispatchers: (i) an affinity-blind one that
      reassigns both jobs every slice, swapping them between CPUs (this is
      what SQMS's rotation does whenever jobs outnumber CPUs; with equal
      counts we force the swap to isolate its cost); (ii) `-A a:0,b:1`, pinning
      one job per CPU. Give each job's completion time under both.
  (d) Three jobs `a`,`b`,`c`, each `100:100`. (i) One CPU, cache 100,
      round-robin with slice 5: what is the makespan? (ii) Three CPUs, cache
      100 each, one job per CPU: what is the makespan, and what is the speedup
      over (i)? (iii) Your speedup should exceed 3 on 3× the CPUs. Is
      "speedup ≤ number of CPUs" violated? Say precisely what resource grew.
      (iv) Recompute (i) and (ii) with per-CPU caches of size 50, and explain
      what changed.

**B2. Load imbalance, quantified.**
Two CPUs run multi-queue scheduling: Q0 = {A}, Q1 = {B, D}, round-robin within
each queue, all jobs long-running.
  (a) Over any window of 12 time slices (6 per CPU), how many slices do A, B
      and D each receive? State A's share relative to B's.
  (b) A now finishes. Describe the resulting pathology, and give the system's
      CPU utilization.
  (c) Show that a *single* migration fixes (b) permanently. Why is the earlier
      situation in (a) not fixable by any single migration?
  (d) For the three long-running jobs of (a), what CPU share would a perfectly
      fair scheduler give each? Show no static job-to-queue assignment
      achieves it, then exhibit a repeating migration pattern that does
      (compute the share it delivers).
  (e) In one sentence: what does the pattern in (d) cost, in the terms of B1?

**B3. Why memory stays resident.**
A time-sharing system holds one process in memory at a time, ch. 13-style: to
switch, it writes the current process's entire memory image to disk and reads
in the next one. Assume 4 GB images and a disk that sustains 200 MB/s in both
directions.
  (a) How long does one process switch take?
  (b) The system wants to give each of 10 interactive users a response within
      roughly 100 ms. Is that achievable? By what factor is the switch cost
      too large?
  (c) The fix is to leave all processes resident and switch only register
      state. What new obligation does residency place on the OS — and which of
      ch. 13's three VM goals names it?

**B4. Reading an address space.**
  (a) In ch. 13's 16 KB address space, code sits at the bottom, the heap grows
      downward from just after it, and the stack grows upward from the top.
      What failure do both growing regions eventually share, and why does the
      opposite-ends placement postpone it as long as possible?
  (b) Two simultaneous runs of ch. 13's `va.c` print the *same* heap address.
      From week 1's evidence (`mem.c`), what must be true of that address?
  (c) Run `pmap $$` (your shell). Count the mappings, and name two kinds of
      entity present in the output that the code/heap/stack picture omits.
      What does this tell you about the ch. 13 diagram?

---

## C. Discussion and design critique

*Longer-form. Each question presents a system behaving badly. Your job is a
diagnosis: name the mechanism producing the symptoms, propose the fix, and —
this is where the marks are — state the conditions under which your fix itself
fails.*

**C1.** A 4-core build server uses one run queue per core, jobs assigned to
queues at submission and never moved. The operators report: "During busy
periods everything is fine. But when the queue drains at the end of a run, we
routinely see one core at 100% for another twenty minutes while the other three
sit idle. Latency for late-submitted jobs is wildly variable."

Diagnose the mechanism. Propose the standard fix, describing concretely how it
operates. Then state the conditions under which the fix makes things *worse*,
and what parameter controls that risk.

**C2.** A team moves a request-processing service from a 1-CPU machine to a
4-CPU machine, expecting close to 4× throughput. They measure only 1.6×. More
puzzling to them: the *CPU time consumed per request* — same code, same
requests — is measurably higher on the new machine. The service uses a single
global run queue; each worker is dispatched to whichever CPU is free at each
time slice.

There are two distinct mechanisms at work. Identify both, and say which one the
raised per-request CPU time points to and why. Propose a scheduling change that
addresses them, and state the conditions under which your change would be the
wrong trade — including at least one workload property and one machine
property.

**C3.** A colleague working on an embedded board writes: "The datasheet claims
this board has 64 MB of RAM, but my program just printed a pointer with value
`0x7f8a12345678` — far beyond 64 MB. Either the datasheet is wrong or the
allocator is broken."

Diagnose the misreading. Explain what the printed value actually tells you, and
what it tells you nothing about. Then give the condition under which your
colleague's inference *would* have been sound — there is a real class of
systems where it is.
