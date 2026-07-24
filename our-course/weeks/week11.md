# Week 11 — Concurrency begins: threads and the thread API

> **Part I: Virtualization ends · Part II: Concurrency begins.** Week 11 of 27.

## What you'll learn

This week the course changes subject. Chapter 24 closes Part I with a short
summary dialogue — the student character recites their mental model of virtual
memory, and it is worth reading precisely to check it against your own: every
address you can observe in a program is virtual; the TLB is what makes paging
survivable, and a working set larger than TLB coverage is painful to watch; a
page table is *just a data structure*, which is why the book could walk from
linear arrays to multi-level trees to page tables paged into kernel virtual
memory. The dialogue also contains OSTEP's cheerfully deflating verdict on Part
I's policy chapters: the best solution to most policy problems is to buy more
memory — but the *mechanisms* you must understand, because they are how systems
actually work.

Chapter 25 then opens Part II with the peach-grabbing analogy: many eaters, many
peaches, and a choice between grabbing concurrently (fast, wrong) and queueing
(correct, slow). The serious content is its answer to "why is this OS material?"
— twice over: the OS must *provide* the primitives multi-threaded programs need
(locks, condition variables), and the OS was itself the **first concurrent
program** — every kernel data structure is accessed by code that can be
interrupted at any moment.

Chapter 26 is the real work. A **thread** is a second (third, …) point of
execution in one process: own program counter, own registers, own stack — but
one shared address space, so switching threads means saving one register set to
a **TCB** and restoring another, with *no page-table switch*. Threads exist for
two reasons: parallelism across CPUs, and overlapping I/O with computation
*within* one program, the way multiprogramming overlaps them *across* programs.
Then the chapter shows what threads cost. A shared `counter = counter + 1`
compiles to load, add, store; a timer interrupt between the load and the store
lets another thread's whole update slide into the gap and be destroyed. Two
increments, net effect one. Master the vocabulary this trace generates —
**critical section**, **race condition** (specifically a data race),
**indeterminate**, **mutual exclusion**, **atomicity** — because the next five
weeks are structured entirely around it. The chapter closes by naming the second
problem, sleeping and waking, and deferring it to the condition-variable
chapter.

Chapter 27 is the pthread interface — creation, join, mutexes, condition
variables — and says of itself that it is best used as a reference. Read it once
now, attentively: its short list of traps (uninitialised locks, unchecked return
codes, returning a pointer to a thread's stack, ad-hoc flag synchronisation —
studied to be buggy about half the time it is tried) is the difference between
this week's lab compiling and this week's lab working.

The cross-reading, OSPP §4.6–4.8, covers what OSTEP skips entirely: how a thread
library is actually *implemented* — kernel threads, user-level threads, and the
design space between them (OSPP's "hybrid model", where some threading is in the
kernel and some in a user-level library).

**Key ideas:** thread · shared address space, per-thread stacks · TCB · context
switch without page-table switch · parallelism vs I/O overlap · critical section
· race condition / data race · indeterminacy · mutual exclusion · atomicity ·
`pthread_create`/`join` · mutex initialisation and error checking · condition
variables as signalling (wait in a `while`) · why ad-hoc flag synchronisation is
harmful.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 24** | Summary Dialogue on Memory Virtualization | 3 | 0.4 h |
| 2 | **OSTEP ch. 25** | A Dialogue on Concurrency | 4 | 0.6 h |
| 3 | **OSTEP ch. 26** | Concurrency: An Introduction | 16 | 2.3 h |
| 4 | **OSTEP ch. 27** | Interlude: Thread API | 12 | 1.7 h *(reads as a reference chapter — by its own advice; read through once now, return to it during the labs)* |
| 5 | **OSPP §4.6–4.8** | Implementing threads: kernel vs user-level, and the hybrid design space | ~10 | 1.4 h |

**Paper (optional):** Dijkstra (1968), *Cooperating Sequential Processes*.
OSTEP says it "outlines all of the thinking that has to go into writing
multi-threaded programs" — Dijkstra coined critical section, mutual exclusion,
and most of the week's vocabulary. Freely available in the EWD archive at UT
Austin (EWD123). Take it only if the week feels light; it is the relief valve.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise11.md`](../exercises/exercise11.md) — budget 3 h. Work closed-book first, then self-mark against [`../exercises/solutions/exercise11-solutions.md`](../exercises/solutions/exercise11-solutions.md) |
| **Lab** | [`../labs/lab04-vm/`](../labs/lab04-vm/) **ends** · [`../labs/lab05-concurrency/`](../labs/lab05-concurrency/) **starts** (runs to week 15). Budget 5.5 h combined this week (3.5 h finishing lab 4, 2.0 h opening lab 5) |
| **Past paper** | **`y2007p1q7` — timed, 35 min closed-book, then self-mark.** Two-level paging translation, PTE protection bits, and segmentation-vs-paging — week 8–9 material, set here deliberately (see below). Part (d), memory-mapping open files into a 64-bit address space, rests on a MULTICS idea OSTEP never teaches: treat it as an open-design stretch and attempt it from ch. 16/18/20 reasoning |

### Why a virtual-memory paper in a concurrency week

Cambridge IA barely examines concurrency — it is Part IB material there — so
weeks 11–14 unlock almost no new questions of their own. Rather than let a month
pass with the week 8–10 paging and replacement arithmetic unrehearsed, weeks 11,
13 and 14 each carry a timed paper drawn from the pool those earlier weeks
unlocked. This is spaced retrieval, not filler: fresh material on Monday,
retrieval of month-old material under exam conditions by Friday.

## Week load

```
OSTEP ch. 24-27     35pp ÷ 7  =  5.0 h
OSPP §4.6-4.8       10pp ÷ 7  =  1.4 h
Exercise sheet 11             =  3.0 h
Timed paper y2007p1q7         =  1.0 h
Lab 4 ends · Lab 5 starts     =  5.5 h
                                ------
                                15.9 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

## Notes for the curious

- **The `x86.py` simulator you meet this week recurs immediately.** Week 12's
  lock chapter drives the same simulator against its own assembly files
  (`threads-locks/`), so time spent learning its flags (`-i` interrupt interval,
  `-r`/`-s` random seeds, `-R` register trace, `-M` memory trace, `-c` check)
  pays twice.
- **helgrind is a pattern detector, not a prover.** The ch. 27 homework includes
  a program (`main-deadlock-global.c`) that helgrind flags even though the
  flagged failure cannot occur — sheet 11 §B4 makes you work out why. Take the
  general lesson: dynamic race detectors report *suspicious structure*, and both
  their alarms and their silences need interpretation.
- **Ch. 27 undersells condition variables on purpose.** It shows the `while`
  loop and the wait-releases-the-lock contract without justifying them; the full
  argument (Mesa semantics, spurious wakeups) is ch. 30's job in week 13. If the
  `while` bothers you now, good — hold the thought.
- `man -k pthread` on Linux lists over a hundred API calls; the seven or so in
  ch. 27 are the load-bearing ones, and the man pages are unusually good.
