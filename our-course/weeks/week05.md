# Week 5 — Multiprocessor scheduling, and the address space

> **Part I: Virtualization.** Week 5 of 27.

## What you'll learn

Four chapters this week, but two of them (11 and 12) are two-page dialogues, so
the week is lighter than the chapter count suggests. The real work is chapter 10,
which closes the CPU-virtualization story, and chapter 13, which opens the
memory-virtualization one.

Chapter 10 asks what happens to everything you learned in weeks 3–4 when there
is more than one CPU. The answer turns on **hardware caches**. Each CPU has its
own, caches exploit temporal and spatial locality, and a process that runs
builds up state in the caches (and TLBs) of the CPU it ran on. Three
consequences follow. First, **cache coherence**: hardware (e.g. bus snooping)
keeps the caches' copies of memory consistent — but coherence alone does *not*
make concurrent updates safe; the chapter's shared-list example still
double-frees without a lock. Second, **cache affinity**: a job reruns faster on
the CPU it last used, so placement is now a performance decision. Third, the
scheduler's structure itself becomes a design axis: a **single queue** (SQMS) is
simple and balances load by construction, but every CPU contends on one lock and
jobs bounce between CPUs, destroying affinity; **multiple queues** (MQMS) scale
and preserve affinity, but drift into **load imbalance** and need **migration**
— typically by **work stealing**, whose peek-frequency knob trades balance
against the very overhead multiple queues were meant to remove. Linux, tellingly,
has never settled this: O(1) (priority-based, MLFQ-like) and CFS (week 4) use
multiple queues; BFS uses a single one.

Chapter 13 then restarts the course's main move — virtualize a resource — for
memory. Early machines gave the one running program all of physical memory;
time sharing made that untenable, and saving *all* of memory to disk on each
switch is hopelessly slow (sheet 5 makes you put a number on it). So processes
stay resident, protection becomes a problem, and the OS's answer is the
**address space**: the running program's private view of memory, holding code,
heap and stack, with the heap and stack placed at opposite ends so both can grow
— a convention, not a hardware requirement. The goals are **transparency** (the
program can't tell), **efficiency** (hardware will have to help), and
**protection** (isolation between processes). One slogan to carry for the rest
of the course: *every address your program can print is virtual*.

**Key ideas:** cache coherence vs. synchronization · cache affinity · SQMS vs
MQMS · load imbalance · migration and work stealing · the address space ·
code/heap/stack layout as convention · transparency, efficiency, protection ·
every address you see is virtual.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 10** | Multiprocessor Scheduling (Advanced) | 13 | 1.9 h |
| 2 | **OSTEP ch. 11** | Summary Dialogue on CPU Virtualization | 2 | 0.2 h |
| 3 | **OSTEP ch. 12** | A Dialogue on Memory Virtualization | 2 | 0.2 h |
| 4 | **OSTEP ch. 13** | The Abstraction: Address Spaces | 9 | 1.3 h |
| 5 | **xv6 book ch. 4** | Traps and system calls | ~8 | 1.1 h |

**Optional:** Anderson (1990), *The Performance of Spin Lock Alternatives for
Shared-Memory Multiprocessors*, IEEE TPDS — OSTEP calls it a classic. This week,
skim it only for its conclusion: naive spin locks collapse under contention,
which is why chapter 10 worries about a single scheduler lock. The full reading
pays off in week 12, where you build the lock alternatives it evaluates (OSTEP
cites Anderson only in ch. 10, but its lessons are exactly what ch. 28–29's
lock-building draws on). Skip it now if time is tight; it is not costed into the week.

> **Why xv6 ch. 4 is here.** The grounding rates it the strongest single chapter
> in any of the course texts on trap mechanics — and Lab 2, which continues this
> week, has you inside exactly that code: the trampoline, the trapframe, and the
> path from `ecall` to the syscall dispatch table. Read it beside the lab rather
> than as sheet preparation; the sheet does not examine it.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise05.md`](../exercises/exercise05.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise05-solutions.md`](../exercises/solutions/exercise05-solutions.md) |
| **Lab** | [`../labs/lab02-syscalls-sched/`](../labs/lab02-syscalls-sched/) — **continues** (weeks 4–6). Budget ~6.0 h this week — this lab's heaviest |
| **Past papers** | **Two timed papers this week:** `y2019p2q3` and `y2016p2q3` — 35 min each, closed book, then self-mark (~1 h each all-in). See the note below |

### Sitting two papers this week

The Cambridge scheduling questions unlock early — weeks 2 and 6 carry no timed
paper, so week 5 carries two, keeping the timed practice spaced rather than
bunched. Both papers are scheduling questions whose material you have held since
weeks 3–4; this week's lighter reading makes room to sit them.

Two vocabulary mappings to know before you start (Cambridge phrases things over
material OSTEP teaches under other names):

- **y2016p2q3** asks for the "four conditions triggering scheduling" — a
  Silberschatz idiom OSTEP never uses. Before you sit it, work out for yourself
  the occasions on which ch. 6's mechanisms return control to the kernel; that
  list *is* the answer, in different clothes.
- **y2019p2q3** uses a six-state Unix process diagram whose state names differ
  from ch. 4's — reconcile the two vocabularies yourself before sitting it.

## Week load

```
OSTEP ch. 10-13     26pp ÷ 7  =  3.7 h
xv6 book ch. 4      8pp ÷ 7  =  1.1 h
Exercise sheet 5              =  3.0 h
Timed papers (two)            =  2.0 h
Lab 2 (continues)             =  6.0 h
                                ------
                                15.8 h   — over the 12–14 h band (labs are not
                                         trimmed to fit)
```

## Notes for the curious

- **Chapter 10 is marked "Advanced", and OSTEP itself suggests covering it
  after the concurrency part.** This course keeps it in chapter position, and
  the honest cost is that §10.2 ("Don't Forget Synchronization") is a sketch:
  it shows you *that* coherence doesn't replace locking, via the list example,
  but the machinery of locks is weeks 11–15. Take the section's conclusion on
  trust for now; every claim in it is re-derived properly in Part II.
- **The Anderson paper is optional here because its subject arrives later.**
  Ch. 10 cites it for why a single scheduler lock stops scaling, which is the
  narrow reason it appears this week. But the paper is a study of *spin-lock
  alternatives* — test-and-test-and-set, queue locks, backoff — and none of that
  machinery exists for you until week 12. Read it now for the scaling argument
  alone, or save it until you are building locks and it will land properly.
- **`multi.py`'s cache model is deliberately coarse.** A cache is simply
  "warm" or "cold" for a job, warming after a fixed run duration if the working
  set fits, and a warm cache scales execution rate by a constant. No coherence
  traffic, no partial warmth. That is the right level for studying *placement*
  effects — affinity, migration cost, super-linear speedup — which is what
  sheet 5 §B1 uses it for.
- The heap-grows-down, stack-grows-up picture of ch. 13 describes a
  single-threaded process. Once a process has many threads (Part II), each
  needs its own stack, and the tidy two-ends layout no longer works — the
  chapter flags this in passing; it becomes real in week 11.
