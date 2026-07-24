# Week 10 — Replacement policies, and two complete VM systems · **MIDTERM 1**

> **Part I: Virtualization** — its final week. Week 10 of 27.

## What you'll learn

This week finishes memory virtualization, and with it the whole of Part I —
which is why Midterm 1 sits here. It is also the heaviest reading week of the
virtualization block (37pp), so the lab is cut to almost nothing.

Chapter 22 answers the question week 9 left open: when memory is full, *which*
page goes? The chapter's method is worth absorbing, not just its policies. It
first turns the question into a caching problem — memory is a cache for pages,
and **AMAT** = T_M + P_Miss × T_D tells you immediately why policy matters: with
disk five orders of magnitude slower than DRAM, even a 1-in-1000 miss rate is
ruinous. It then establishes **OPT** (Belady's evict-the-furthest-in-future) not
as a real policy but as the yardstick every real policy is measured against.
FIFO and Random fail because they are blind to history; **LRU** wins by betting
on locality — and then turns out to be unimplementable at memory speed, because
perfect recency-ordering needs accounting on *every* reference. So real systems
approximate: a hardware **use bit** plus the **clock algorithm** gets most of
LRU's benefit at a fraction of its cost, and a **dirty bit** makes the policy
prefer free (clean) evictions over expensive (write-back) ones. The chapter
closes with the failure mode policies cannot fix — **thrashing** — and the blunt
instruments used against it, from admission control to Linux's out-of-memory
killer.

Chapter 23 then shows two complete systems, and it repays close reading because
it is where ten weeks of mechanisms finally assemble into something real.
**VAX/VMS** is the historical masterclass: 512-byte pages made linear page
tables ruinously large, the hardware had *no reference bit*, and the same
architecture had to run on tiny and huge machines alike — and every one of
those constraints was met in software: segmented address spaces with per-segment
base/bounds, page tables kept (and swapped!) in kernel virtual memory,
**segmented FIFO** with global second-chance lists standing in for LRU, plus
**demand zeroing** and **copy-on-write** — the lazy optimizations every modern
OS inherited. **Linux** brings the story to the present: kernel logical vs
kernel virtual addresses, the four-level x86-64 page table, **huge pages** (the
cure for TLB coverage, priced in internal fragmentation), the unified **page
cache** with 2Q-style active/inactive lists — and then the security material:
buffer overflows, the NX bit, return-oriented programming, ASLR and KASLR, and
finally **Meltdown and Spectre**, which forced **KPTI** — the partial undoing of
a kernel-mapping design the chapter itself praises. That tension is deliberate,
and sheet 10 makes you argue it.

By the end of this week you can take a virtual address on a real machine and
say what happens to it, from TLB lookup through multi-level walk to page fault,
eviction choice, and write-back. That is the skill Midterm 1 examines.

**Key ideas:** memory as a cache · AMAT · OPT/MIN as yardstick · FIFO, Random,
LRU · locality · Belady's anomaly and stack algorithms · clock and the use bit ·
dirty bit · prefetching · clustering · thrashing · segmented FIFO ·
second-chance lists · demand zeroing · copy-on-write · huge pages · the page
cache and 2Q · NX, ASLR/KASLR, Meltdown/Spectre, KPTI.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 22** | Beyond Physical Memory: Policies | 18 | 2.6 h |
| 2 | **OSTEP ch. 23** | Complete Virtual Memory Systems | 19 | 2.7 h |

**No cross-reading this week.** At 37pp with a midterm, nothing else fits — and
nothing else is needed: ch. 23 *is* the case-study material a cross-reading
would normally supply.

**Paper (required):** ★ Levy & Lipman (1982), *Virtual Memory Management in the
VAX/VMS Operating System*, IEEE Computer. OSTEP's verdict: "read the original
source" — the chapter's whole VAX/VMS section is drawn from it, and the chapter
calls the paper excellent and short. Read it after ch. 23's VAX section and the
paper becomes a worked example you already half-know; look especially at how
each design choice is justified by a hardware limitation the designers could
not change.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise10.md`](../exercises/exercise10.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise10-solutions.md`](../exercises/solutions/exercise10-solutions.md). **Ch. 23 ships no homework at all** — the sheet's ch. 23 material is fully original |
| **Assessment** | 🎓 **MIDTERM 1** — sat this week. 90 minutes, closed book. Covers weeks 1–10 — all of virtualization. See [`../exams/README.md`](../exams/README.md) |
| **Lab** | [`../labs/lab04-vm/`](../labs/lab04-vm/) — **continues**, finishing week 11. Budget only ~1.0 h this week; the midterm takes the slack |
| **Past papers** | **None this week.** **Midterm weeks.** Weeks 10 and 20 carry no timed past paper; the midterm is that week's timed practice |

**Preparing for the midterm.** The highest-value revision is re-working, timed
and closed-book, the calculation sections of sheets 3–9: Gantt charts with
average turnaround and response time, multi-level translation of a hex
address, EAT algebra with a TLB, and (from this week) a replacement trace.
Those four archetypes are where the marks are.

## Week load

```
OSTEP ch. 22-23     37pp ÷ 7  =  5.3 h
Levy & Lipman [M]             =  1.5 h
Exercise sheet 10             =  3.0 h
Midterm 1 (sit + prep)        =  3.0 h
Lab 4 (trimmed)               =  1.0 h
                                ------
                                13.8 h   ✅ within 12-14 h target
```

**This week has no slack.** If you are behind, trim lab hours to zero first —
lab 4 has week 11 still to run — and do not touch the sheet or the paper: the
sheet is the only place ch. 23 gets exercised, and the paper is short. The
midterm figure assumes ~90 minutes of preparation; treat that as a floor, not
an estimate.

## Notes for the curious

- **Ch. 23 is the only chapter in Part I with no homework of any kind** — no
  simulator, no code, no measurement. That is why sheet 10's §B5 and two of its
  three discussion questions are original material, and it is also, perversely,
  what makes this 37pp week fit: the reading is heavy but the upstream homework
  burden is light. Ch. 22's `paging-policy.py` simulator covers the policy half.
- **The chapter argues against itself, on purpose.** §23.1 (VAX/VMS) praises
  mapping the kernel into every address space ("the kernel appears almost as a
  library, albeit a protected one"); §23.2 (Linux) then describes KPTI —
  separating the kernel page table precisely because Meltdown showed the
  mapping leaks. Sheet 10 C2 asks
  you to take the argument further than the chapter does.
- **VMS's reference-bit emulation matters beyond VMS.** The Babaoglu–Joy trick —
  mark pages inaccessible, count the traps, revert protections — is the standard
  answer whenever hardware lacks a bit the policy needs, and it reappears for
  dirty bits and for working-set estimation. It is a mechanism worth having at
  your fingertips.
- **Where replacement research went next:** the chapter's summary names **scan
  resistance** as the theme of modern policies, with ARC (Megiddo & Modha 2003,
  a FAST Test-of-Time winner) as the exemplar — and notes that fast SSDs have
  made replacement policy matter again after years in which "buy more memory"
  was a defensible answer. Sheet 10 C3 asks you to attack that answer properly.
- Ch. 24, the three-page summary dialogue on memory virtualization, is folded
  into week 11's reading as the bridge out of Part I.
