# Exercise Sheet 7 — Segmentation and free-space management

**Attempt after Week 7.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise07-solutions.md`](solutions/exercise07-solutions.md).

**This sheet leans on:** OSTEP ch. 16–17; OSPP §8.4; Bonwick (1994), *The Slab
Allocator*. §C2 also draws on week 6's Wahbe SFI paper.

**You will need:** Python 3 and two OSTEP simulators from the `ostep-homework`
repo: `vm-segmentation/segmentation.py` and `vm-freespace/malloc.py`. No C
compiler is needed for this sheet.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** Memory compaction is available both to the OS (for segments) and to the
`malloc` library (for the heap) as a cure for external fragmentation.

**A2.** A segmentation fault can only occur on hardware that supports
segmentation.

**A3.** `free()` needs no size argument because the allocator records each
allocation's size in a header it keeps just before the space handed to the
caller.

**A4.** To satisfy `malloc(N)`, the allocator searches its free list for a
chunk of size `N`.

**A5.** Best fit wastes the least space on each individual allocation, so of
the classic policies it produces the least fragmentation overall.

**A6.** The buddy allocator's power-of-two rounding causes internal
fragmentation, but in exchange, finding the block to coalesce with is a
one-bit address computation.

**A7.** Without coalescing, a heap can be almost entirely free and still unable
to satisfy a request far smaller than the total free space.

**A8.** Only hardware — base/bounds registers or segments — can prevent an
untrusted module from writing outside its allotted region of memory.

---

## B. Translation and allocation, by hand and by simulator

**B1. Segmentation translation.**
A machine has a 14-bit virtual address space (16 KB) with a maximum segment
size of 4 KB, and selects segments **explicitly** from the top two bits. The
segment registers hold:

| Segment | Bits | Base | Size | Grows positive? |
|---------|------|------|------|-----------------|
| Code | 00 | 32 KB | 2 KB | 1 |
| Heap | 01 | 34 KB | 3 KB | 1 |
| Stack | 11 | 28 KB | 2 KB | 0 |

  (a) For each virtual address below, give the physical address it translates
      to, or state that it faults and in which segment: **1200**, **5000**,
      **7300**, **15872**, **13000**. Show the offset arithmetic each time —
      for the stack, that includes how the negative offset is formed and how
      the bounds check works on it.
  (b) Give the lowest and highest *valid* virtual addresses in the stack
      segment, as decimal numbers.
  (c) Now the simulator, which models the two-segment variant (top bit selects;
      segment 0 grows up, segment 1 grows down). Run
      `python3 segmentation.py -s 42 -a 1k -p 16k`, translate all five addresses in
      the trace by hand, then check yourself with `-c`. From the register
      values it prints, also state the highest legal virtual address in
      segment 0 and the lowest legal virtual address in segment 1.

**B2. A free list under load.**
Run `python3 malloc.py -S 100 -b 1000 -H 4 -a 4 -p FIRST -l ADDRSORT
-A +20,+20,+20,-1,-2,+50` — a 100-byte heap at address 1000, 4-byte headers,
4-byte alignment, first fit, address-ordered list, **no coalescing**. The op
list allocates three 20-byte objects, frees the second and third, then requests
50 bytes.
  (a) Before running with `-c`: how many bytes does each `Alloc(20)` actually
      consume, and why? Write out the free list after every operation, and
      predict whether the final `Alloc(50)` succeeds.
  (b) At the point the final request is served or refused, how many bytes of
      the heap are free in total? Reconcile that number with your answer about
      the request's fate, and name the phenomenon.
  (c) Re-run with `-C` added. What changes, at which operation exactly, and
      what does the final free list look like? How many bytes did the
      successful `Alloc(50)` consume?
  (d) Go back to the no-coalescing run and swap `-p FIRST` for `-p BEST`. Does
      the outcome of the final request change? What does this tell you about
      what fit policy can and cannot fix?

**B3. Buddy allocation.**
A binary buddy allocator manages a 64 KB region. Requests arrive: **A = 7 KB**,
**B = 12 KB**, **C = 20 KB**.
  (a) Give the block size each request receives and the internal fragmentation
      of each, and show the recursive splitting that places A (assume each
      split takes the lower half, and each allocation the lowest suitable free
      block).
  (b) After all three allocations, how much of the 64 KB remains free, and in
      what block(s)?
  (c) B is freed first. Does any coalescing happen? Then A is freed. Trace the
      coalescing cascade that follows, step by step, and give the free blocks
      at the end.
  (d) State the rule for computing the buddy of a block of size 2^k at address
      x, and use it: what is the buddy of the 8 KB block at address 40 KB?

**B4. Slab arithmetic and object caching.**
A kernel object cache serves a 200-byte object type from slabs of one 4 KB
page each. (Ignore per-slab metadata for (a) and (b).)
  (a) How many buffers does one slab hold, and what fraction of the slab is
      internal fragmentation? Check your figure against the paper's bound for
      a slab of n buffers.
  (b) Bonwick's implementation caps internal fragmentation at 1/8. For a
      2400-byte object type, what is the smallest whole number of pages per
      slab that meets the cap? Show the waste fraction for each slab size you
      consider.
  (c) The paper's opening claim is that allocating and freeing memory for an
      object is often *cheaper* than initialising and destroying it. What does
      the slab allocator do with a freed object, what work does that save on
      the next allocation, and which kinds of object state does the paper say
      this works for?
  (d) When can the general-purpose page allocator take memory back from an
      object cache, and why is that reclaim cheap? Your L3 `malloc` cannot use
      the constructed-state trick at all — what about its interface makes
      object caching impossible for it?

---

## C. Discussion and design critique

**C1. In defence of segmentation.**
Chapter 16 ends by dismissing the design it has just built: external
fragmentation is "fundamental and hard to avoid", and segmentation "isn't
flexible enough" for a sparse heap, so "we need to find some new solutions".
Make the best honest case *for* pure segmentation. Give the three strongest
arguments you can for a system built on segments — drawing on translation
cost, per-process MMU state, protection and sharing, and anything else the
chapter itself supplies — then concede its genuine weaknesses, and finish by
describing concretely the hardware and workload conditions under which you
would still ship a pure-segmentation design. A defence with no conditions
attached is advocacy, not engineering.

**C2. Segments versus software fault isolation.**
You must run an untrusted third-party codec inside your media server's address
space. It must be unable to read or write anything outside a fixed 16 MB
window you give it. Two designs are on the table: (i) place the window in its
own hardware segment and rely on bounds checks, accepting that entering and
leaving the codec means changing privileged segment state; (ii) software fault
isolation in the style of Wahbe and OSPP §8.4 — rewrite the codec's code so
every store is masked or checked to stay inside the window, and calls cross
the boundary through checked entry points.

Compare the two designs under this constraint: the server makes **thousands of
codec calls per second**, and the codec's inner loops are **dense with memory
accesses**. Identify where each design pays its costs, say which you would
choose under the stated constraint, and — most importantly — say what change
in the workload or the hardware would flip your choice.

**C3. A fragmented broker.**
A long-running message broker allocates and frees messages of many sizes
through a custom allocator: first fit, freed chunks inserted at the head of the
list, **no coalescing** — essentially an unfinished L3. After several days of
uptime, allocations of 4 KB message buffers begin to fail, even though the
allocator's own statistics show **35% of the heap free**. Memory in use by
live messages has not grown.

Diagnose the failure: what has happened to the free list, by what mechanism,
and why does the policy make it worse over time? Say what two measurements of
the free list you would take to confirm the diagnosis before touching any
code. Then propose fixes in the order you would apply them, and state clearly
which proposed remedy merely postpones the failure rather than removing it —
the operations team has suggested restarting the broker nightly.
