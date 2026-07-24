# Week 8 — Paging, and making it fast

> **Part I: Virtualization.** Week 8 of 27.

## What you'll learn

This is the week the course arrives at the mechanism every modern machine
actually uses. Chapter 18 makes the case for **fixed-size** pieces: segmentation
chops the address space into variable-sized chunks and inherits external
fragmentation as a permanent tax, so paging chops it into fixed-size **pages**
instead, maps them into physical **page frames**, and records the mapping in a
per-process **page table**. Translation becomes mechanical: split the virtual
address into a virtual page number and an offset, look the VPN up, glue the
frame number onto the untouched offset. Free-space management collapses to
keeping a free list of frames, and a sparse address space costs nothing but
invalid entries.

Then the chapter honestly presents the bill, and it is steep — twice over. In
**space**: a linear page table for a 32-bit address space with 4 KB pages is
2²⁰ entries, some 4 MB per process, before you have run a single instruction.
In **time**: every load, store and instruction fetch now requires an *extra*
memory reference to fetch the translation first — the chapter's closing memory
trace shows a four-instruction loop generating ten memory accesses per
iteration, half of them page-table overhead. Sit with that trace; it is the
best picture in the book of what translation really costs.

Chapter 19 answers the time problem (week 9 answers the space problem). The
**TLB** — really an address-translation cache in the MMU — works because
programs exhibit **spatial and temporal locality**: walk an array and only the
first touch of each page misses. The chapter then opens two doors that matter
for the rest of the course. First, *who handles a miss?* — hardware that walks
the page table itself (x86, via CR3), or the OS in a trap handler
(MIPS/SPARC), which buys the OS total freedom over page-table structure at the
price of care (the handler must never itself TLB-miss — hence unmapped or
**wired** entries). Second, *what happens on a context switch?* — the cached
translations belong to the old address space, so either flush the whole TLB and
pay the refill on every switch, or tag entries with an **address space
identifier** and pay in hardware. That flush-vs-ASID trade is exactly what this
week's timed paper examines.

The cross-reading is the canonical pairing: OSTEP gives you the theory, and
**xv6 book ch. 3** gives you the real thing — RISC-V **Sv39**, where a 39-bit
virtual address is translated through a three-level tree of 512-entry
page-table pages (9 + 9 + 9 index bits), the root installed in the `satp`
register, `sfence.vma` doing the TLB flush. The chapter's `walk()` function is
the hardware's page-table walk transcribed into twenty lines of C, and it is
the code you will instrument and extend in Lab 4, which starts this week.
You will also meet two elegant page-table tricks — the double-mapped trampoline
page, and unmapped **guard pages** that turn stack overflow into a clean fault.

**Key ideas:** pages and frames · VPN/offset split · linear page table · PTE
bits (valid, protection, present, dirty, accessed) · the space and time costs of
translation · TLB, hit rate, locality · TLB coverage · hardware- vs
software-managed TLBs · context switch: flush vs ASIDs · wired entries · Sv39
three-level walk · `satp` · guard pages.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 18** | Paging: Introduction | 15 | 2.1 h |
| 2 | **OSTEP ch. 19** | Paging: Faster Translations (TLBs) | 16 | 2.3 h |
| 3 | **xv6 book ch. 3** | Page tables — Sv39, `satp`, `walk()`, kernel address space | 12 | 1.7 h |

**No paper this week.** The reading is heavy (43 pp across three sources) and
ch. 19's homework is a measurement project you write yourself, which absorbs
the slack. OSTEP's own reference lists for ch. 18–19 are thin; the one survey
it praises (Wiggins 2003, on TLB/cache/pipeline interaction) is optional depth
— see the notes below.

> **Reading order.** OSTEP ch. 18 → xv6 ch. 3 → OSTEP ch. 19. xv6's three-level
> tree lands better once linear tables have shown you the problem, and ch. 19's
> flush-vs-ASID discussion lands better once you have seen `sfence.vma` in real
> code.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise08.md`](../exercises/exercise08.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise08-solutions.md`](../exercises/solutions/exercise08-solutions.md) |
| **Lab** | [`../labs/lab03-allocator/`](../labs/lab03-allocator/) — **ends this week.** [`../labs/lab04-vm/`](../labs/lab04-vm/) — **starts this week** with page-table printing on exactly the Sv39 structures xv6 ch. 3 describes. Budget 6.0 h across the two (4.0 h finishing lab 3, 2.0 h opening lab 4) |
| **Timed past paper** | **`y2024p2q3`, parts (a)–(b) only** — 35 min pro-rata, closed book, then self-mark. (a) is context-switch state including what must be *flushed* (this week's TLB material); (b) is RR and CFS schedules with context-switch counts (weeks 3–4 material). Part (c) is a serverless-computing critique; the week 26 paper block covers that ground, and the full question becomes attemptable then |

## Week load

```
OSTEP ch. 18-19     31pp ÷ 7  =  4.4 h
xv6 book ch. 3      12pp ÷ 7  =  1.7 h
Exercise sheet 8              =  3.0 h
Timed paper y2024p2q3(a-b)    =  1.0 h
Lab 3 ends · Lab 4 starts     =  6.0 h
                                ------
                                16.1 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

## Notes for the curious

- **Why ch. 19's homework is measurement, not simulation.** There is no
  simulator for TLBs upstream — the homework is to *write* `tlb.c` and detect
  your own machine's TLB sizes by timing page touches, a method due to
  Saavedra-Barrera's 1992 dissertation. This is deliberate: a TLB is invisible
  to correct programs and shows up only in timing, so measuring is the only
  honest way to meet one. Sheet 8 §B5 and §C1 build directly on it.
- **xv6 ch. 3 runs one week ahead of OSTEP on purpose.** Sv39's page table is a
  three-level tree, and OSTEP doesn't explain *why* multi-level tables win
  until ch. 20 (week 9). Meeting the mechanism in working code first, then the
  design-space argument, is the right order — week 9's reading will feel like
  an explanation of something you have already touched. Sheet 8 stays on linear
  tables and the Sv39 walk as code; the design comparison waits for sheet 9.
- **"RAM isn't always RAM."** Ch. 19 names this Culler's Law: if your working
  set exceeds TLB coverage, nominally uniform memory accesses aren't uniform at
  all. Databases exploit larger page sizes for exactly this reason. Keep the
  phrase; it resurfaces in the I/O and file-system weeks whenever a "flat"
  abstraction hides a cost cliff.
- **Where the TLB came from.** Couleur invented the associative translation
  buffer at GE in 1964; the name descends from the Atlas machine's "lookaside
  buffer" for its cache. The 1968 patent (Couleur & Glaser) is cited at the end
  of ch. 19 — a reminder that the core mechanisms of this course were settled
  before 1970, and everything since is engineering the constants.
- **Optional depth:** Wiggins (2003), *A Survey on the Interaction Between
  Caching, Translation and Protection* — OSTEP calls it "an excellent survey".
  Read it if the physically- vs virtually-indexed cache aside at the end of
  ch. 19 grabbed you; nothing later depends on it.
