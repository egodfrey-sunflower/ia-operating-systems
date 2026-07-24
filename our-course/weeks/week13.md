# Week 13 — Condition variables, and the monitors they came from

> **Part II: Concurrency.** Week 13 of 27.

## What you'll learn

Locks let threads *exclude* each other; this week's problem is letting threads
*wait* for each other. A parent wants to sleep until its child finishes; a
consumer wants to sleep until a producer fills a buffer. Spinning on a flag
works but burns a timeslice doing nothing — chapter 30's opening example — so
you need a primitive that atomically releases a lock and sleeps: the
**condition variable**.

The chapter's method is to break things repeatedly until only the correct
pattern is left, and each break is worth internalising. Signal without a state
variable, and a signal delivered before the waiter arrives is simply lost —
CVs have no memory. Check the state without holding a lock, and the waiter can
be interrupted between check and sleep, missing its wakeup forever. Check with
`if` instead of `while`, and a woken thread acts on stale state, because
**signal is only a hint**: between the signal and the woken thread actually
running, some other thread may have changed the world. That hint
interpretation is called **Mesa semantics**, and virtually every system ever
built uses it. Use one condition variable for two different conditions, and a
consumer can wake a consumer while the producer sleeps forever. The surviving
pattern — lock, `while`, wait, two CVs for two conditions — is the
producer/consumer solution you will use for the rest of your career. The
chapter closes with **covering conditions**: when no signaller can know *which*
waiter to wake (Lampson and Redell's memory allocator), wake them all with
`broadcast` and let each re-check.

Appendices C and D supply the history that explains *why* the `while` rule
exists. **Monitors** blended locking into object-oriented languages: every
method of a monitor class implicitly acquires one lock. Hoare's original
semantics made `signal` transfer control — and the lock — to the woken thread
*immediately*, which makes `if` correct and proofs clean, but proved hard to
build. Lampson and Redell, building Mesa's concurrency facilities at Xerox, weakened
`signal` to a hint; the two-line cost is changing `if` to `while`, and the
whole industry followed. Their paper is this week's required reading — it is
where "Mesa semantics" comes from. Appendix D is marked "(Deprecated)" by its
own authors, but only because the book is C/pthreads-based; Java's
`synchronized` methods are monitors, alive and well.

The OSPP cross-reading turns the chapter's scattered tips into a discipline:
a methodology for writing shared objects that are correct by construction
rather than by debugging.

**Key ideas:** condition variables · wait/signal/broadcast · why wait takes
the lock · the lost-wakeup race · Mesa vs Hoare semantics · **always wait in a
`while` loop** · two CVs for producer/consumer · covering conditions ·
spurious wakeups · monitors as language-level locking.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 30** | Condition Variables | 19 | 2.7 h |
| 2 | **OSTEP App. C** | A Dialogue on Monitors | 1 | 0.1 h |
| 3 | **OSTEP App. D** | Monitors (Deprecated) | 13 | 1.9 h |
| 4 | **OSPP §5.5** | Designing and implementing correct shared objects — the methodology *(read-through)* | ~11 | 1.6 h |

**Paper (required):** ★ Lampson & Redell (1980), *Experience with Processes and
Monitors in Mesa*, CACM. OSTEP calls it "a classic paper", and ch. 30's central
rule — always re-check the condition, because signal is a hint — is this
paper's contribution. Read for: why Hoare semantics were abandoned in a real
system, the queues a thread can be on (three from monitor execution, plus the
paper's fourth **fault** queue), and the covering-condition / broadcast idea. The Pilot OS war stories are optional colour.

> **Read Appendix D *before* the paper, not after.** The appendix is
> effectively an extended reader's guide to Lampson & Redell: it works the
> same producer/consumer example under both signalling semantics and shows
> the exact trace where Hoare-style code breaks under Mesa rules.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise13.md`](../exercises/exercise13.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise13-solutions.md`](../exercises/solutions/exercise13-solutions.md) |
| **Lab** | [`../labs/lab05-concurrency/`](../labs/lab05-concurrency/) — **continues** (weeks 11–15). Budget ~3.5 h this week — held light, because the reading is heavy and a timed paper returns |
| **Past paper (timed)** | `y2023p2q4` — 35 min under exam conditions, then self-mark (~1 h total). Scheduling with context-switch overhead: RR and SJF costed against a non-zero switch time. This is week 3–4 material returning as spaced retrieval — see the note below. One part asks you to design a mechanism to predict CPU bursts — OSTEP itself skips this, reaching the same goal via MLFQ in ch. 8, but sheet 4 §B5 covered the ground precisely so this part would be answerable. Revisit that solution *after* sitting the paper if the part went badly |

### Why this week's timed paper is a scheduling question

See week 11 for the full reasoning. Briefly: the concurrency block unlocks almost
no Tripos questions of its own, so weeks 11, 13 and 14 each carry an older paper
to keep the week 3–10 arithmetic rehearsed. `y2023p2q4` is a scheduling paper for
that reason, not because it fits this week's material.

## Week load

```
OSTEP ch. 30 + App. C, D   33pp ÷ 7  =  4.7 h
OSPP §5.5                  11pp ÷ 7  =  1.6 h
Lampson & Redell [M]                 =  1.5 h
Exercise sheet 13                    =  3.0 h
Timed paper (y2023p2q4)              =  1.0 h
Lab 5                                =  3.5 h
                                       ------
                                       15.3 h   — over the 12–14 h band (labs are
                                                 not trimmed to fit)
```

If something must give, give lab hours; do not skip Appendix D — sheet 13's
§B3 and §C2 are built on it, and it is the shortest route into the paper.

## Notes for the curious

- **Ch. 30 ships real homework code** (`ostep-homework/threads-cv/`):
  working and deliberately broken producer/consumer variants
  (`main-two-cvs-while.c`, `main-two-cvs-if.c`, `main-one-cv-while.c`,
  `main-two-cvs-while-extra-unlock.c`) plus a *sleep-string* harness that
  forces a chosen thread to pause at a chosen code point — so you can make a
  race happen on demand instead of waiting for the scheduler to oblige.
  Sheet 13 §B leans on it.
- **Appendices C and D ship no homework** — the historical material is
  exercised by original questions on the sheet instead.
- Appendix D's "Deprecated" tag is about packaging, not ideas. The monitor
  *idea* — data and its lock bound together, compiler-enforced — survives in
  Java's `synchronized`, and the appendix walks Java's original
  one-condition-variable-per-object limitation and why it forces
  `notifyAll()`.
- The chapter notes that **spurious wakeups** — two threads waking from one
  signal, permitted by real implementations — are an independent reason the
  `while` rule is non-negotiable, even if you could somehow reason away Mesa
  semantics.
- Dijkstra's "private semaphores" prefigured CVs, and next week's chapter
  shows the semaphore generalising both locks and condition variables into
  one primitive — and what that generality costs.
