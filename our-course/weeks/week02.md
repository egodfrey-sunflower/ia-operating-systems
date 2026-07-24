# Week 2 — The process, and the API that creates it

> **Part I: Virtualization.** Week 2 of 27.

## What you'll learn

This week the course's first big abstraction arrives: the **process**, a running
program. The program itself is a lifeless thing on disk; the OS takes those
bytes and gets them running — and, by stopping one process and starting
another, creates the illusion of a nearly endless supply of CPUs from one or a
few physical ones. That trick, **time sharing**, is the whole of CPU
virtualization in one sentence; the next four weeks are the machinery and the
policy that make it efficient and fair.

Chapter 3 is a four-page dialogue introducing virtualization by metaphor — read
it in ten minutes and don't overthink it. Chapter 4 does the real work: a
process is defined by its **machine state** — the memory it can address, its
registers (program counter and stack pointer above all), and its I/O state such
as open files. You'll meet the small API every OS provides (create, destroy,
wait, status), what actually happens when a program is loaded (and why modern
OSes do it lazily), and the **state machine** every process lives in: running,
ready, blocked — plus the initial and final states real systems add, including
the wonderfully named **zombie**. The chapter closes with the OS's own
bookkeeping: the process list and the per-process structure everyone calls a
**PCB**. Note also chapter 4's design tip — the separation of **policy** (which
process runs?) from **mechanism** (how do we switch?) — which recurs all
course.

Chapter 5 is an interlude on the UNIX answer to process creation, and it is
genuinely odd: **`fork()`** duplicates the caller, **`exec()`** transforms it
into a different program, and **`wait()`** lets a parent collect a child. The
chapter's central argument is that this strangeness pays: because fork and exec
are *separate*, the shell can run code in the child *between* the two calls —
which is precisely how redirection and pipes work, with no cooperation from the
programs involved. The chapter also gives your first taste of
**nondeterminism** (parent and child race unless `wait()` intervenes), and ends
with signals, users, and the superuser. Read the aside on Lampson's "get it
right" — the exercise sheet asks you to argue with it.

Alongside the book you'll read the paper that started it all: Ritchie &
Thompson's *The UNIX Time-Sharing System*. Chapter 2 cites it, but it is
assigned here, where the process abstraction and the shell give its design
arguments something to attach to. Watch for how much of chapters 4–5 was
already in place in 1974 — and how small the system that carried it was.

**Key ideas:** process = running program · machine state (address space,
registers, PC, open files) · time sharing vs space sharing · mechanism vs
policy · process states (running / ready / blocked, plus initial and zombie) ·
process list and PCB · `fork()` / `wait()` / `exec()` · nondeterminism · the
lowest-free-descriptor rule and redirection · the shell as an ordinary user
program · signals, users, the superuser.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 3** | A Dialogue on Virtualization | 4 | 0.6 h |
| 2 | **OSTEP ch. 4** | The Abstraction: The Process | 13 | 1.9 h |
| 3 | **OSTEP ch. 5** | Interlude: Process API | 15 | 2.1 h |
| 4 | **TLPI ch. 24–28, 20–22, 34** | fork/exec/wait internals; signals; process groups & job control | — | 1.0 h *(reference — consult during Lab 1, don't read cover to cover)* |

**Paper (required):** ★ Ritchie & Thompson (1974), *The UNIX Time-Sharing
System*, CACM. OSTEP calls it a "great summary", and it is the system every
modern OS descends from. Read it for the file and process model and for the
design-economy argument in its conclusions; the exercise sheet's discussion
section draws on it.

> **Why TLPI is reference, not reading.** OSTEP ch. 5 gives you the shape of
> the process API; the shell you start building this week needs the details —
> exact `wait()` semantics, signal handling, terminal job control. Those live
> in TLPI (ch. 24–28 for fork/exec/wait, 20–22 for signals, 34 for process
> groups), and signals and job control are genuine OSTEP gaps. Look things up
> as the lab demands them.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise02.md`](../exercises/exercise02.md) — budget 3 h. Work it closed-book, then self-mark against [`../exercises/solutions/exercise02-solutions.md`](../exercises/solutions/exercise02-solutions.md) |
| **Lab** | [`../labs/lab01-shell/`](../labs/lab01-shell/) — **starts this week** (weeks 2–4). Budget ~5.0 h this week: REPL, builtins, and first `fork`/`exec`/`wait` plumbing |
| **Past papers** | **None this week.** The earliest usable Cambridge paper is week 3 — nothing is examinable until limited direct execution and scheduling are taught. This is an expected consequence of following OSTEP's order |

## Week load

```
OSTEP ch. 3-5       32pp ÷ 7  =  4.6 h
TLPI (reference)              =  1.0 h
Ritchie & Thompson [M]        =  1.5 h
Exercise sheet 2              =  3.0 h
Lab 1 (starts)                =  5.0 h
Past papers          none     =  0.0 h
                                ------
                                15.1 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

## Notes for the curious

- **Chapter 3 is a boundary chapter**: OSTEP's web page files it under the
  Intro, but the printed table of contents opens Part I "Virtualization" with
  it. This course counts it as week 2 material either way — it's four pages.
- Both chapters ship homework. Ch. 4's `cpu-intro/process-run.py` simulates
  the process state machine under different switching policies; ch. 5's
  `cpu-api/fork.py` draws process trees, and its **Homework (Code)** section is
  the seed of Lab 1. Sheet 2 §B uses both.
- Ch. 5 closes by citing Baumann et al. (2019), *A fork() in the road* (HotOS)
  — a spirited modern argument that `fork()` is no longer a good API. It is
  not assigned (sheet 2 §C supplies the argument you need), but it is short and
  fun if the C1 question hooks you.
- The **zombie** state is not morbid decoration: a dead process's exit status
  has to live somewhere until the parent asks for it.
- TLPI ch. 34 (process groups, sessions, job control) explains what `Ctrl-C`
  actually does — a question OSTEP never answers. You will need it for the
  final weeks of Lab 1.
