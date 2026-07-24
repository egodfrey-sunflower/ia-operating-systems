# Week 9 — Smaller page tables, and going beyond physical memory

> **Part I: Virtualization.** Week 9 of 27.

## What you'll learn

Last week's linear page table had one glaring cost: at 4 KB pages and 4-byte
entries, a 32-bit address space needs a **4 MB table per process** — hundreds of
megabytes across a busy system, spent mostly on entries marked *invalid*.
Chapter 20 is a tour of the data-structure design space that fixes this, and
each stop teaches a general lesson. **Bigger pages** shrink the table but buy
internal fragmentation (and, as the chapter's aside explains, real machines
support large pages mainly to relieve the *TLB*, not to shrink tables). The
**hybrid** — one page table per segment, with a bounds register saying how many
valid pages it has — is clever and real (Multics), but it inherits
segmentation's assumptions and makes page tables arbitrary-sized objects, so
external fragmentation sneaks back in. The winner is the **multi-level page
table**: chop the table into page-sized pieces, and use a **page directory** to
record which pieces exist. Space now scales with the address space you *use*,
at the price of a genuine **time–space trade-off** — a TLB miss on a two-level
table costs two loads instead of one, and deeper trees cost more.

Two things in ch. 20 deserve a slow read. First, the worked example (14-bit
address space, 64-byte pages): do the 0x3F80 translation yourself before
reading the answer. Second, the "more than two levels" argument — the rule
that *every piece of the table, including the directory, must fit in a page*
is what mechanically determines how many levels a machine needs, and it is the
kind of derivation exams love. The chapter closes with **inverted page
tables** (one entry per *physical* frame, searched via a hash — PowerPC), a
reminder that page tables are just data structures, and the right one depends
on the machine's constraints.

Chapter 21 then knocks away the course's oldest standing assumption: that
every address space fits in memory. The machinery is small and precise —
**swap space** on disk, a **present bit** in the PTE, and a **page-fault
handler** that runs in the OS (always in software, even on hardware-walked
machines — the chapter's aside on *why* is short and worth internalising).
Study the two control-flow figures until you can reproduce the three TLB-miss
outcomes — present-and-valid, valid-but-not-present, invalid — and the full
life of a fault: trap, find the disk address *in the PTE itself*, block the
process while the I/O flies, then retry. The chapter ends with how systems
actually behave: they don't wait for memory to fill, but keep a **swap
daemon** running between **low and high watermarks**, evicting in batches so
writes can be clustered.

Ch. 21 is unusually short (11pp) and deliberately stops at *mechanism* — which
page to evict is the **policy** question, and it is next week's subject. The
cross-reading (OSPP §8.3) covers what OSTEP compresses: **superpages** as
implemented, what a multiprocessor must do to keep TLBs consistent (**TLB
shootdown**), and virtually-addressed caches.

**Key ideas:** page-table size arithmetic · internal vs external fragmentation
(again) · hybrid paging-over-segments · page directory · multi-level
translation · levels-from-page-size derivation · time–space trade-off ·
inverted page tables · swap space · present bit · page fault vs segmentation
fault · page-fault handler · watermarks and the swap daemon · clustering ·
TLB shootdown.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 20** | Paging: Smaller Tables | 15 | 2.1 h |
| 2 | **OSTEP ch. 21** | Beyond Physical Memory: Mechanisms | 11 | 1.6 h |
| 3 | **OSPP §8.3** | Superpages, TLB consistency/shootdown, virtually-addressed caches *(read through)* | ~12 | 1.7 h |

**Optional:** Navarro et al. (2002), *Practical, Transparent Operating System
Support for Superpages*, OSDI. OSTEP calls it "a nice paper showing all the
details you have to get right" — the honest engineering record behind ch. 20's
one-paragraph aside on large pages. This is the week's relief valve: skip it
without guilt if the week runs long.

**Not yet:** both chapters cite Levy & Lipman's VAX/VMS paper — ch. 20 for
swapping page tables themselves to disk, ch. 21 for page clustering. It is
**week 10's** required paper, where ch. 23 gives it a full case study to
attach to. Likewise, *which* page to evict (replacement policy) is ch. 22,
also week 10.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise09.md`](../exercises/exercise09.md) — budget 3 h. Work closed-book first, then self-mark against [`../exercises/solutions/exercise09-solutions.md`](../exercises/solutions/exercise09-solutions.md) |
| **Lab** | [`../labs/lab04-vm/`](../labs/lab04-vm/) — **continues** (weeks 8–11). Budget ~5.5 h this week. The multi-level walk you drill on paper in §B is the same walk the lab has you implement |
| **Past paper** | **`y2008p1q8` — timed: 35 min closed-book, then self-mark.** A *scheduling* question, set here deliberately: weeks 8–10 unlock almost only memory questions, and the scheduling material from weeks 3–5 needs to stay warm ahead of Midterm 1 next week. One caveat: part (b) asks for a Unix vs **Windows NT** comparison, and the course teaches only the Unix half — attempt the NT side from first principles (what must any dynamic-priority scheduler track?) and self-mark that part leniently |

## Week load

```
OSTEP ch. 20-21     26pp ÷ 7  =  3.7 h
OSPP §8.3           12pp ÷ 7  =  1.7 h
Exercise sheet 9              =  3.0 h
Timed paper y2008p1q8         =  1.0 h
Lab 4                         =  5.5 h
                                ------
                                14.9 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

If the week runs long, the Navarro paper is optional by design; do not trim the
sheet's §B — page-table walk and sizing arithmetic is among the two or three
most frequently examined calculation styles in the Cambridge bank, alongside
scheduling metrics and demand-paging/replacement.

## Notes for the curious

- **Ch. 20's simulator is `vm-smalltables/paging-multilevel-translate.py`.**
  The chapter's homework asks for runs with seeds 0, 1 and 2, checked with
  `-c`. Do at least one translation fully by hand before letting `-c` confirm
  it — the exam version of this task comes with no `-c` flag.
- **Ch. 21's homework is measurement, not simulation**: the supplied
  `vm-beyondphys/mem.c` plus `vmstat`. It needs a real Linux machine with swap
  configured. Check `swapon -s` and `/proc/meminfo` before you start, and size
  runs with care — the chapter's own question 6 has you push past available
  swap to see allocation fail, and a machine deep in swap will crawl for
  everything else you're running on it.
- OSTEP's references admit, charmingly, that "we have yet to find a good first
  reference to the multi-level page table" — the cited treatment is Bryant &
  O'Hallaron's coverage of x86 (CS:APP §9.6, and §9.6.3 for the Core i7's
  four-level table specifically), an early mainstream user of the structure. For
  a survey of real designs (x86, PowerPC, MIPS), ch. 20 points at Jacob &
  Mudge (1998), *Virtual Memory: Issues of Implementation*.
- Ch. 20 §20.5 drops one more assumption in passing: page tables themselves
  can live in *kernel virtual memory* and be swapped to disk. The system that
  did this seriously is VAX/VMS — week 10's case study.
- The word **daemon** (as in swap daemon) traces to Project MAC's CTSS work on
  the IBM 7094, named for Maxwell's demon — Corbató's team fancied a tireless
  background agent sorting things. Ch. 21's references have the delightful primary
  source.
