# Week 3 — Keeping control of the CPU, and deciding who runs

> **Part I: Virtualization.** Week 3 of 27.

## What you'll learn

Last week you met the process and its API. This week OSTEP asks the two
questions those chapters carefully postponed: how does the OS run a program
*at full hardware speed* while still constraining what it can do, and — once
it can switch between programs — which one should it pick?

Chapter 6's answer to the first question is **limited direct execution**: just
run the program on the CPU, but rig the hardware first. Two problems force
the design. *Restricted operations* — a process must be able to ask for I/O
without being allowed to own the disk — are solved by two processor modes
plus the **trap**: user code executes a trap instruction that simultaneously
enters the kernel and raises the privilege level, landing only at addresses
the OS installed in the **trap table** at boot. The process picks a system-call
*number*, never an address — that indirection is a protection mechanism.
*Regaining the CPU* from a process that never traps is solved by the **timer
interrupt**; without it, the cooperative approach's only remedy for an infinite
loop is a reboot. Watch for the chapter's subtlest point: during a switch there
are **two different register saves** — the hardware implicitly saves user
registers to the kernel stack when the trap or interrupt fires, and the OS
explicitly saves kernel registers into the process structure only if it decides
to switch. Confusing these two is the classic exam error.

Chapter 7 then builds scheduling *policy* from deliberately false assumptions,
relaxing them one at a time. With equal, known, simultaneous jobs, FIFO is
fine; unequal lengths produce the **convoy effect**; SJF fixes that until
arrivals stagger; adding preemption gives **STCF**, provably optimal for
average turnaround — and terrible for the new metric that time-sharing
introduced, **response time**, which Round Robin optimises at turnaround's
expense. The chapter is honest that this is an inherent trade-off, not an
engineering failure. It closes with two loose ends: overlapping I/O by treating
each CPU burst as a job, and the fact that real schedulers never know job
lengths — the problem next week's MLFQ exists to solve.

The first cross-reading, OSPP §2.2–2.4, is the definitive treatment of
**dual-mode operation and safe mode transfer** — §2.4 states the contract
between hardware and kernel at the mode boundary (limited entry, atomic
transfer, transparent restartable execution) more carefully than OSTEP's 16
pages have room to. Read it after ch. 6 and the trap protocol will feel less
like a list of steps and more like an argument.

The second cross-reading, OSPP §3.5, answers a question OSTEP never asks:
where should the operating system's own code *live*? A **monolithic** kernel
— Windows, macOS and Linux all qualify — links most of the system together
inside the kernel, where modules call one another directly. That is fast and
tightly integrated, but everything inside runs with full privilege, so a fault
anywhere is a fault everywhere; OSPP cites studies finding 90% of crashes come
from bugs in device drivers rather than in the kernel proper. A **microkernel**
instead runs as much of the system as possible in user-level servers — window
manager, network stack, file system, drivers — leaving the kernel with little
beyond the ability to run those servers and carry requests between them, so a
server that fails corrupts only itself and can be restarted. The price is paid
on every service call: what was a procedure call becomes a round trip through
the kernel. OSPP's verdict is unromantic — the reliability gain is real, the
benefit visible to end users is not, and the extra steps cost performance —
which is why real systems are hybrids, pushing new device drivers out to user
level while keeping the performance-critical services in. Cambridge asks for
this comparison directly, and wants both halves: the isolation argument, and
the communication cost that buys it.

The paper is Ousterhout (1990), *Why Aren't Operating Systems Getting Faster
As Fast As Hardware?* — cited by ch. 6 itself, which calls it a classic. Its
claim: many OS operations are limited by memory (and disk), not by processor
speed, so buying a faster CPU speeds the OS up less than it speeds your
programs up. Sheet 3 asks you to test whether the claim survives on your own
machine.

**Key ideas:** limited direct execution · user/kernel mode · trap and
return-from-trap · the trap table and system-call numbers · cooperative vs
timer-driven preemption · context switch, and the two kinds of register save ·
turnaround vs response time · FIFO, convoy effect, SJF, STCF, RR ·
amortising the time slice · I/O overlap · monolithic vs microkernel structure.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 6** | Mechanism: Limited Direct Execution | 16 | 2.3 h |
| 2 | **OSTEP ch. 7** | Scheduling: Introduction | 13 | 1.9 h |
| 3 | **OSPP §2.2–2.4** | Dual-mode operation, mode-transfer types, and safe mode transfer | ~15 | 2.1 h |
| 4 | **OSPP §3.5** | Operating system structure: monolithic kernels and microkernels | ~5 | 0.7 h |

**Paper (required):** Ousterhout (1990), *Why Aren't Operating Systems Getting
Faster As Fast As Hardware?*, USENIX Summer. Short and readable — OSTEP ch. 6
cites it as "a classic paper on the nature of operating system performance."
Read it before attempting sheet 3 §B1(d) and §C1, which build on it.

**Not yet:** ch. 8–9 (MLFQ and proportional share) are next week; ch. 10
(multiprocessor scheduling) lands in week 5 — OSTEP itself flags it as an
advanced chapter best read once you know more.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise03.md`](../exercises/exercise03.md) — budget 3 h. Work closed-book first, then self-mark against [`../exercises/solutions/exercise03-solutions.md`](../exercises/solutions/exercise03-solutions.md) |
| **Lab** | [`../labs/lab01-shell/`](../labs/lab01-shell/) — **continues** (weeks 2–4). Budget ~4.5 h this week |
| **Past paper** | **`y2013p2q3`, timed — the first past paper of the course.** Sit it closed-book in 35 minutes, then self-mark; budget 1 h total |

### The first timed paper

Nothing was examinable in weeks 1–2 because the Cambridge questions assume
material OSTEP's ordering defers: every question in the bank needs at least
limited direct execution or scheduling. With ch. 6 and 7 read, the earliest
unlockable questions — `y2013p2q3`, `y2017p2q3`, `y2019p2q3` — become
attemptable; this week sits the first, and the other two are scheduled in
weeks 4–5. From here on, most weeks carry one timed paper.

## Week load

```
OSTEP ch. 6-7        29pp ÷ 7  =  4.1 h
OSPP §2.2-2.4        15pp ÷ 7  =  2.1 h
OSPP §3.5             5pp ÷ 7  =  0.7 h
Ousterhout [S]                 =  0.75 h
Exercise sheet 3               =  3.0 h
Timed paper y2013p2q3          =  1.0 h
Lab 1 (continues)              =  4.5 h
                                 ------
                                 16.15 h  — over the 12–14 h band (labs are not
                                           trimmed to fit)
```


## Notes for the curious

- **Ch. 6 ships no simulator.** Its homework is a *measurement* exercise: you
  write your own timing harness for system-call and context-switch cost. Sheet
  3 §B1 is that homework, with methodology guidance — it is the course's first
  encounter with the craft of measuring an OS, which lmbench (cited in the
  chapter) turned into a standard tool.
- Ch. 6's aside on context-switch cost is worth memorising as a calibration
  point: in 1996, roughly 4 µs per system call and 6 µs per context switch on
  a 200 MHz P6; modern machines are sub-microsecond. Sheet 3 asks what those
  numbers look like *in cycles* — the answer is more interesting.
- The RR time slice must be a multiple of the timer-interrupt period — policy
  is quantised by the mechanism underneath it, a small but telling example of
  the mechanism/policy layering from week 1.
- Cambridge vocabulary: past papers speak of the "four conditions triggering
  scheduling" (a Silberschatz framing). OSTEP teaches the same content as
  traps, timer interrupts, yields, and blocking I/O — reconcile the two
  vocabularies before you sit papers, which lean on the process-state framing.
- If Ousterhout's paper grabs you: his measurement-driven scepticism reappears
  in this course repeatedly, and his later essay on threads is assigned in
  week 15.
