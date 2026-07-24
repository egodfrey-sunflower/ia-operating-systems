# Week 7 — Segmentation, and managing free space

> **Part I: Virtualization.** Week 7 of 27.

## What you'll learn

Chapter 16 takes the base-and-bounds mechanism from week 6 and asks the obvious
question: why only one pair? A base-and-bounds pair **per logical segment** —
code, heap, stack — lets the OS place each segment independently in physical
memory, so the huge empty gap between heap and stack in a **sparse address
space** stops costing physical memory. The chapter then walks the details that
make it real: selecting the segment **explicitly** from the top bits of the
virtual address versus **implicitly** from how the address was formed; the
stack's negative growth direction, which needs its own hardware bit and its own
offset arithmetic; **protection bits**, which buy code sharing across processes
for free; and the fine-grained extreme — Multics and the Burroughs B5000 kept
*thousands* of segments in a memory-resident segment table. This is also where
you learn why every C programmer's least favourite phrase is a **segmentation
violation**.

The chapter's sting is in the tail. Variable-sized segments chop physical
memory into odd-sized holes — **external fragmentation** — and the OS can only
fight back with expensive compaction or clever free-list policy. That is
exactly the problem your allocator faces, which is why chapter 17 follows as a
self-declared detour: how do you manage free space when requests are
variable-sized? It builds the machinery your L3 allocator uses — **splitting**,
**coalescing**, the hidden **header** that lets `free()` take no size argument,
the free list embedded *inside* the free space itself — then surveys the
classic policies (**best fit**, **worst fit**, **first fit**, **next fit**) and
the designs that sidestep list search entirely: **segregated lists**, the
**slab allocator**, and the **buddy allocator**, whose power-of-two structure
makes coalescing a one-bit address computation. The honest conclusion, which
ch. 16's aside states outright ("if 1000 solutions exist, no great one does"):
with hundreds of policies and no best one, fragmentation is minimised, never
solved.

The cross-reading continues week 6's isolation thread. OSPP §8.4 covers
**software fault isolation**: confining an untrusted module *without* relying
on translation hardware, by rewriting its code — the systems descendant of the
Wahbe paper you read last week. Read it against ch. 16: segments are the
hardware way to bound a region of memory; SFI is what you do when you can't or
won't use it.

This week's paper is Bonwick's **slab allocator** — the SunOS 5.4 kernel
allocator, and OSTEP's own pick for how real kernels allocate (its annotation:
"a cool paper about how to build an allocator for an operating system kernel").
Its central
move is **object caching**: keep freed objects in their *constructed* state, so
a hot kernel object (a lock, an inode) skips initialisation and destruction on
every reuse. Slabs — page-multiple chunks carved into equal-size buffers — keep
internal fragmentation provably small and make reclaim trivial. Ch. 17
sketches the idea in two paragraphs; the paper is where it becomes a design.

**Key ideas:** per-segment base/bounds · sparse address spaces · explicit vs
implicit segment selection · negative stack growth · protection bits and code
sharing · fine-grained segmentation and segment tables · external vs internal
fragmentation · compaction · splitting and coalescing · headers · embedded free
lists · best/worst/first/next fit · segregated lists · slab object caching ·
buddy allocation.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 16** | Segmentation | 12 | 1.7 h |
| 2 | **OSTEP ch. 17** | Free-Space Management | 18 | 2.6 h |
| 3 | **OSPP §8.4** | Software fault isolation | ~10 | 1.4 h |

**Paper (required):** Bonwick (1994), *The Slab Allocator: An Object-Caching
Kernel Memory Allocator*, USENIX. OSTEP ch. 17 calls it "a cool paper" and a
great example of specialising for common object sizes. Read for object caching
(constructed state), the slab structure and its fragmentation bound, and the
reclaim path; the statistics and debugging sections at the end can be skimmed.

**Further, not assigned:** Wilson et al. (1995), *Dynamic Storage Allocation: A
Survey and Critical Review* — ch. 17 leans on it throughout and calls it
excellent, but it is nearly 80 pages; consult it if L3 makes you curious, don't
read it through.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise07.md`](../exercises/exercise07.md) — budget 3 h. Work closed-book first, then self-mark against [`../exercises/solutions/exercise07-solutions.md`](../exercises/solutions/exercise07-solutions.md) |
| **Lab** | [`../labs/lab03-allocator/`](../labs/lab03-allocator/) — **continues** (weeks 6–8). Budget ~5.5 h this week. Ch. 17 is this lab's theory chapter: splitting, coalescing, headers and fit policies are exactly what you are building. Read it before your next lab session, not after |
| **Past papers** | **None this week.** No new Tripos questions unlock between week 5 and week 8, so the timed-paper cadence skips this week and resumes in week 8 |

## Week load

```
OSTEP ch. 16-17     30pp ÷ 7  =  4.3 h
OSPP §8.4           10pp ÷ 7  =  1.4 h
Bonwick slab [M]              =  1.5 h
Exercise sheet 7              =  3.0 h
Lab 3                         =  5.5 h
                                ------
                                15.7 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

## Notes for the curious

- **The ch. 17 simulator is `malloc.py`, not `freespace.py`.** The OSTEP
  homework index page lists the free-space simulator under the latter name; the
  file that actually exists in `ostep-homework/vm-freespace/` is `malloc.py`.
  Budget thirty seconds of confusion, not thirty minutes.
- **The `segmentation.py` simulator models two segments, not three.** It uses
  the *top bit* of the virtual address: segment 0 (code+heap) grows positively,
  segment 1 (stack) grows negatively. The chapter's three-segment, top-two-bits
  scheme is the hand-drill version; the simulator is the same arithmetic with
  one fewer bit. Ch. 16 itself notes some systems fold code into the heap
  segment exactly this way.
- Ch. 16's aside on the term *segmentation fault* explains why the name
  survives on machines with no segmentation hardware at all — worth knowing
  before the next time you see one in L3.
- The slab allocator is specialised, not general: it serves fixed-size objects
  from per-type caches and falls back on a general allocator for slab refill.
  For the multiprocessor end of general-purpose allocation, ch. 17 points at
  Hoard (Berger et al., 2000) and jemalloc (Evans, 2006) — both in production
  use today, and both a good browse after L3 is done.
- If you want to see segmentation taken to its baroque extreme, ch. 16's
  references include the Intel architecture manuals' segmentation chapter, with
  the authors' warning that reading it "will hurt your head, at least a little
  bit". x86 carried full segment machinery for decades of backwards
  compatibility.
