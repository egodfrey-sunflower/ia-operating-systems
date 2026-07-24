# Week 18 — File system implementation: vsfs and FFS

> **Part III: Persistence.** Week 18 of 27.

## What you'll learn

Last week ch. 39 gave you the file-system *interface* — files, directories,
descriptors, links. This week ch. 40 opens the box and shows the simplest
implementation that can honour that interface: **vsfs**, a very simple file
system. The chapter's advice is to hold two questions in mind at all times:
what are the **on-disk data structures**, and what are the **access methods**
that map `open()`/`read()`/`write()` onto them? For vsfs the structures are a
superblock, an inode bitmap, a data bitmap, an inode table, and a data region;
the access methods are where the surprises live. Opening `/foo/bar` costs a
pair of reads *per path component*; a single allocating write costs **five**
I/Os; creating a file costs around ten. Two details students reliably get
wrong: reads never touch the allocation bitmaps (bitmaps are consulted only
when allocating), and a file's name is not in its inode — names live in
directory entries, and a directory is just a file whose data blocks hold
(name, inode-number) pairs.

The inode's **multi-level index** — a few direct pointers, then single, double
and triple indirect blocks — is the course's arithmetic centrepiece this week.
It is an imbalanced tree on purpose: measurement says *most files are small*,
so the common case resolves through direct pointers alone, while indirection
buys enormous maximum sizes for the few large files. Cambridge examines this
relentlessly: computing maximum file sizes, counting block reads to reach a
given byte offset, and costing path resolution are the most-drilled
*filesystem* calculation family in the Tripos bank — appearing about as often
as TLB/EAT algebra, behind the scheduling and paging families — and sheet 18 §B
works all three.

Chapter 41 then asks why the original UNIX file system, built exactly along
vsfs lines, delivered **2% of disk bandwidth** — and answers with FFS, the
Berkeley Fast File System. The fix was not a new interface but a disk-aware
layout behind the same API: **cylinder groups** (block groups in ext2/3/4),
each a miniature vsfs, plus placement heuristics — put a directory in a group
with few directories and many free inodes; put files next to their inodes and
their siblings; *except* for large files, which are deliberately chunked
across groups, with chunk size chosen by an **amortization** argument you
should be able to reproduce cold. Sub-blocks, parameterized layout, long
names, symbolic links and atomic `rename()` complete the story: FFS won as
much on usability as on layout.

The cross-reading makes it concrete: xv6 book ch. 10 implements what ch. 40
describes, as **seven layers** — disk, buffer cache, logging, inode,
directory, pathname, file descriptor. Read §10.1–10.6 this week for the layout
and the buffer cache; take the logging sections (§10.4–10.6) at a first-pass
level only, because week 21 returns to them properly when ch. 42 supplies the
theory of write-ahead logging.

**Key ideas:** superblock, bitmaps, inode table, data region · the inode and
its multi-level index · directories as files · access paths and their I/O
counts · caching and write buffering · the durability/performance trade-off ·
cylinder groups · FFS placement heuristics · the large-file exception and
amortization · internal fragmentation and sub-blocks.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 40** | File System Implementation | 18 | 2.6 h |
| 2 | **OSTEP ch. 41** | Locality and The Fast File System | 14 | 2.0 h |
| 3 | **xv6 book §10.1–10.6** | The seven-layer FS; buffer cache; write-ahead log (first pass) | 7 | 1.0 h |

**Paper (required):** ★ McKusick, Joy, Leffler & Fabry (1984), *A Fast File
System for UNIX*, ACM TOCS. OSTEP calls FFS "a watershed moment" in file-system
history. The original FFS was ~1,200 lines of code; read it for the
cylinder-group layout and the measured motivation (the 2% number), and notice
how much of the paper is about usability features.

**Held back:** xv6 §10.4–10.6 (the log) gets its full treatment in **week 21**
alongside ch. 42 — this week you only need to know where the logging layer
sits in the stack. Crash consistency itself (what happens when the five I/Os
of an allocating write are interrupted) is also week 21's problem; this week
takes the disk as reliable.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise18.md`](../exercises/exercise18.md) — budget 3 h. Uses the `vsfs.py` (ch. 40) and `ffs.py` (ch. 41) simulators. Self-mark against [`../exercises/solutions/exercise18-solutions.md`](../exercises/solutions/exercise18-solutions.md) |
| **Lab** | [`../labs/lab07-filesystem/`](../labs/lab07-filesystem/) — **continues** (weeks 17–19). Budget ~5.0 h this week |
| **Timed past paper** | `y2022p2q3` — segmentation vs paging, page-table sizing, and an open design part. Unlocked in week 9; timed here as spaced retrieval, because the sheet already drills this week's own material hard (see below). 35 min closed book, then self-mark |
| **Untimed drill** | This week unlocks the largest filesystem-arithmetic pool in the course: `y2007p1q8`, `y2012p2q4`, `y2016p2q4`. Attempt one or two untimed if §B felt shaky — they are exactly sheet 18 §B2–B3 at exam pressure. **Two more unlock but are reserved: leave them** — `y2018p2q3` is week 21's timed paper and `y2021p2q3` is week 23's |

## Week load

```
OSTEP ch. 40-41     32pp ÷ 7  =  4.6 h
xv6 §10.1-10.6       7pp ÷ 7  =  1.0 h
McKusick FFS [M]              =  1.5 h
Exercise sheet 18             =  3.0 h
Timed paper y2022p2q3         =  1.0 h
Lab 7 (continues)             =  5.0 h
                                ------
                                16.1 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

If the week runs long, let it: do not cut the sheet, and do not cut the
McKusick paper, which week 21's LFS paper argues against and assumes you
have read.

## Notes for the curious

- **Why the timed paper is a memory question.** Unlock week is a floor, not a
  schedule. The five filesystem questions this week unlocks are what sheet 18
  §B already rehearses, so the timed slot is spent keeping week-9 paging
  arithmetic warm instead; the filesystem questions are spread over the
  following weeks as drill.
- Ch. 40's aside on **FAT** is worth a careful read: linked allocation with
  the next-pointers lifted into an in-memory table is the whole design, and it
  explains why FAT cannot support hard links (no inodes — metadata lives in
  the directory entry). Cambridge has asked FAT-vs-inode comparisons more than
  once (`y2007p1q8`, `y2008p1q7`, `y2016p2q4`).
- FFS's **parameterization** (skip-block layout tuned to the drive's rotation)
  is obsolete — modern disks buffer whole tracks — but the *reasoning* is the
  same amortization argument that reappears, with the same equation, in
  week 21's log-structured file system. Learn it once here.
- The measurement table in ch. 40 ("most files are small … most bytes are in
  large files") is load-bearing for sheet 18 §C1. Both halves are true at
  once; which one your design should serve is a genuine judgement call.
- xv6's `mkfs` writes the superblock; the kernel never does. If lab 7 has you
  wondering where the file system comes *from*, that's the answer.
