# Week 14 — Semaphores, concurrency bugs, and deadlock

> **Part II: Concurrency.** Week 14 of 27 — the last full week of new
> concurrency mechanism.

## What you'll learn

Chapter 31 introduces Dijkstra's **semaphore**: one object with an integer
value and two operations, `wait()` (decrement, sleep if the result demands
it) and `post()` (increment, wake one waiter). Initialise it to 1 and you
have a lock; to 0 and you have an ordering primitive that — unlike a
condition variable — *remembers* a post delivered before anyone waits. The
chapter rebuilds the bounded buffer with `empty`/`full` semaphores, and its
best moment is a wrong version: put the mutex *outside* the empty/full waits
and a consumer sleeps holding the lock the producer needs — a two-line
deadlock. Then reader-writer locks (and why they often aren't worth it),
dining philosophers (deadlock again, broken by making one philosopher grab
forks in the other order), throttling, and building a semaphore from a lock
plus a CV. The closing warning is Lampson's: semaphores generalise locks and
CVs, but "generalizations are generally wrong" — going the other way,
building CVs from semaphores, is notoriously error-prone.

Chapter 32 asks what concurrency bugs *actually* look like, leaning on Lu et
al.'s study of MySQL, Apache, Mozilla and OpenOffice — this week's required
paper, and the basis for the whole chapter. Of 105 real bugs, 74 were not
deadlocks, and 97% of those were just two patterns: **atomicity violations**
(a check and its use, separated by an unlocked gap) and **order violations**
(B assumed A had happened; nothing enforced it). Then deadlock properly: the
four conditions — mutual exclusion, hold-and-wait, no preemption, circular
wait — and the strategy menu: *prevent* (break one condition, most
practically by a total or partial **lock order**), *avoid* (schedule so an
unsafe state is never entered), or *detect and recover*.

OSTEP names deadlock avoidance and gestures at **Banker's algorithm** in one
paragraph; it never works it. OSPP §6.5 does — and Cambridge examines this
material, so the cross-reading and sheet §B4 give it the weight the spine
text doesn't. The algorithm grants a request only if some completion order
provably exists afterwards; the price is declared maximum claims and
conservatively refused concurrency, which is why it lives in embedded
systems rather than general-purpose kernels.

**Key ideas:** semaphore = value + wait/post · initial value = resources you
can give away · binary semaphore · ordering with 0 · bounded buffer with
semaphores · reader-writer locks and starvation · dining philosophers ·
atomicity vs order violations (97% of non-deadlock bugs) · the four deadlock
conditions · lock ordering · trylock and livelock · **Banker's algorithm,
safe vs unsafe states** · detect-and-recover.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 31** | Semaphores | 20 | 2.9 h |
| 2 | **OSTEP ch. 32** | Common Concurrency Problems | 16 | 2.3 h |
| 3 | **OSPP §6.5** | Deadlock; the Banker's algorithm worked properly — read **§6.5.4** closely, skim §6.5.1–6.5.3/6.5.5 (they overlap ch. 32) | ~10 | 1.4 h |

**Paper (required):** ★ Lu, Park, Seo & Zhou (2008), *Learning from Mistakes —
A Comprehensive Study on Real World Concurrency Bug Characteristics*,
ASPLOS. OSTEP calls it the "basis for this chapter". Read for the taxonomy
and the numbers; skim the per-bug case studies. It is that rare thing, an
empirical paper about software that changed how people build tools — the
sheet's §C1 asks what its numbers *imply*, not what they are.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise14.md`](../exercises/exercise14.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise14-solutions.md`](../exercises/solutions/exercise14-solutions.md). §B4 (Banker's) is the centrepiece — it is the one part of this week Cambridge actually examines |
| **Lab** | [`../labs/lab05-concurrency/`](../labs/lab05-concurrency/) — **continues** (weeks 11–15), one week from finishing. Budget ~3.5 h this week, among the lightest in the block — the reading is heavy and a timed paper runs |
| **Past paper (timed)** | `y2009p2q3` — 35 min timed, then self-mark (~1 h). Virtual address spaces, fragmentation, a 48-bit multi-level page-table design, then pipes and `fork`/`exec`/`wait` shell pseudo-code. Spaced retrieval of week 2 and week 9 material — see week 13's note on why the concurrency block carries memory papers |

## Week load

```
OSTEP ch. 31–32     36pp ÷ 7  =  5.1 h
OSPP §6.5           10pp ÷ 7  =  1.4 h
Lu et al. [M]                 =  1.5 h
Exercise sheet 14             =  3.0 h
Timed paper (y2009p2q3)       =  1.0 h
Lab 5 (held light)            =  3.5 h
                                ------
                                15.5 h   — over the 12–14 h band (labs are not
                                         trimmed to fit)
```

No slack. If you must shed load, skim the Lu paper's case-study middle
rather than cutting the sheet, and reclaim lab time next week when lab 5
wraps up.

## Notes for the curious

- **Both chapters ship code homework**: `threads-sema/` (skeletons for
  fork/join, rendezvous, barriers, reader-writer with and without
  starvation) and `threads-bugs/` (a `vector_add()` family that deadlocks,
  or avoids it by global ordering, trylock, or going lock-free). The sheet
  samples them lightly; lab 5 is where the coding time lives.
- Ch. 32 is stamped VERSION 1.20 — the most recently revised chapter in the
  book.
- The chapter's lock-ordering tip — acquire multiple locks in **address
  order** — is a beautifully cheap trick worth remembering: it turns "we
  need a global ordering" into three lines of code with no registry.
- Dijkstra called deadlock the "deadly embrace"; the name didn't stick, the
  four-condition analysis (Coffman et al., 1971 — cited by ch. 32) did.
- Semaphore trivia with content: in Dijkstra's definition a negative value
  equals the number of waiters. OSTEP's own lock-and-CV implementation
  (the "Zemaphore") deliberately drops that invariant — its value never
  goes negative — and Linux does the same.
- Where the Banker's algorithm actually earns its keep: systems where the
  full task set and worst-case claims are known at build time — the same
  niche (embedded, hard-real-time) where week 13's Hoare-semantics
  trade-offs also flip.
