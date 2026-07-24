# Week 12 — Locks, and the data structures built from them

> **Part II: Concurrency.** Week 12 of 27.

## What you'll learn

This is the densest mechanism week in the book. Chapter 28 is the longest chapter in
Part II (22pp) and it earns the length: it builds a lock from nothing, discarding
each attempt as you discover why it fails.

The arc is worth knowing in advance, because it's easy to lose the thread.
Disabling interrupts works on a uniprocessor and nowhere else. Load/store alone
gets you Peterson's algorithm — correct, but it doesn't scale and it assumes
memory ordering you don't have. So the hardware must help: **test-and-set** gives
a working spinlock, **compare-and-swap** generalises it, **load-linked/store-conditional**
splits it in two, and **fetch-and-add** buys you a ticket lock with actual
fairness. Then a second problem appears — spinning burns a whole timeslice doing
nothing — so you go to `yield`, then to sleeping queues with `park`/`unpark`, and
finally to the **futex**, which is what Linux really does: uncontended locks never
enter the kernel at all.

Chapter 29 then asks the question that matters in practice: given a lock, how do
you build a *correct and scalable* data structure? The answer is deflating and
important — start with one big lock, measure, and only then refine. The chapter's
own examples show finer granularity often failing to pay (the hand-over-hand
linked list), with one clear success (per-bucket hash table locks) and one clever
compromise (the approximate/sloppy counter, which trades exactness for
scalability under a threshold).

The cross-reading is short but load-bearing. xv6 §7.6 covers **memory ordering** —
the fact that both compiler and CPU reorder your loads and stores, so a lock
needs barriers to work at all. OSTEP assumes sequential consistency throughout and
mentions relaxed memory only once, in passing (ch. 28); real hardware doesn't provide it.

**Key ideas:** mutual exclusion, fairness, performance as the three criteria ·
test-and-set · CAS · LL/SC · ticket locks · two-phase locks · futex ·
approximate counters · hand-over-hand locking · why premature refinement loses ·
memory barriers.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 28** | Locks | 22 | 3.1 h |
| 2 | **OSTEP ch. 29** | Lock-based Concurrent Data Structures | 16 | 2.3 h |
| 3 | **xv6 book §7.6** | Instruction and memory ordering (short but dense) | ~2 | 0.3 h |

**Paper (required):** Mellor-Crummey & Scott (1991), *Algorithms for Scalable
Synchronization on Shared-Memory Multiprocessors*, ACM TOCS. OSTEP calls it an
"excellent and thorough survey". MCS queue locks are what you build when ticket
locks stop scaling — each waiter spins on its *own* cache line instead of
everyone hammering one. Read for the queue-lock construction (§2.4); skim the
rest. **Sheet 12 §B6 drills it directly**, and lab 5 offers building one as a
stretch goal.

**Paper (optional):** Herlihy (1991), *Wait-free Synchronization*, ACM TOPLAS —
OSTEP calls it "a landmark paper". It defines the lock-free/wait-free hierarchy
this course otherwise only gestures at. Genuinely hard; take it if week 12 feels
light, which it won't.

> **Why xv6 §7.6 and not more OSPP.** OSPP ch. 5 is excellent but 78pp — it can't
> fit beside a 38pp OSTEP week. Memory ordering is the one thing OSTEP actually
> omits rather than merely compresses, so that's what the cross-reading buys.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise12.md`](../exercises/exercise12.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise12-solutions.md`](../exercises/solutions/exercise12-solutions.md) |
| **Lab** | [`../labs/lab05-concurrency/`](../labs/lab05-concurrency/) — **continues** (weeks 11–15). Budget ~5.0 h this week — lab 5's largest single-week share, since weeks 13–14 are held light for the reading |
| **Past papers** | **None this week.** Week 12 is the heaviest reading week in Part II; weeks 11, 13 and 14 each carry a timed paper instead — see the note below |

### On the missing past paper

Week 11 explains the concurrency block's timed-paper pattern in full. In short:
weeks 11–14 unlock almost nothing of their own, so weeks 11, 13 and 14 carry
older papers as spaced retrieval. **Week 12 carries none** — its reading load is
the highest of the four.
## Week load

```
OSTEP ch. 28-29     38pp ÷ 7  =  5.4 h
xv6 §7.6             2pp ÷ 7  =  0.3 h
MCS paper [M]                 =  1.5 h
Exercise sheet 12             =  3.0 h
Lab 5                         =  5.0 h
Past papers          none     =  0.0 h
                                ------
                                15.2 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

This week has no slack. If something has to give, give on the optional Herlihy
paper, then on lab hours — **not** on the sheet, because §B is the only place
you'll actually execute a race.

## Notes for the curious

- **Ch. 29 has a "Homework (Code)" section but ships no code.** Neither
  `ostep-homework` nor `ostep-code` has a `threads-locks-usage` directory — the
  chapter expects you to build the approximate counter and hand-over-hand list
  yourself. Sheet 12 §B supplies that missing material.
- **Anderson's spin-lock paper belongs to this week, as flagged in week 5.**
  OSTEP cites it only in ch. 10, for why one scheduler lock stops scaling — but
  its actual subject is the thing you are studying now: it measures
  test-and-set against test-and-test-and-set and queue locks on real hardware,
  and it is where the MCS paper's argument starts. If you skipped it in week 5,
  read §1–3 before the MCS paper.
- Ch. 28 *does* have a simulator: `threads-locks/x86.py`, driving `flag.s`,
  `test-and-set.s`, `ticket.s`, `yield.s`, `test-and-test-and-set.s`.
- The futex material at the end of ch. 28 is the bridge to week 13's sleeping
  primitives, and it underlies real pthread mutexes. If Drepper's *Futexes Are
  Tricky* tempts you, it's better read after week 13's condition variables.
- xv6 §7.6's memory-ordering discussion is the shortest route into a genuinely
  deep topic. If it grabs you, the full treatment is the Linux-Kernel Memory
  Model paper (Alglave et al., ASPLOS 2018) — but it is long and hard, and
  nothing later in this course depends on it.
