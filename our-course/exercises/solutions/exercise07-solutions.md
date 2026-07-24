> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 7 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** The OS *can* compact segments — stop the processes, copy the
data, rewrite the base registers — though it is expensive and, as ch. 16 notes,
ironically makes segment-growth requests harder to serve. The `malloc` library
**cannot compact at all**: once a pointer has been handed to a C program, the
library has no way to find and update every copy of it, so allocated chunks can
never move. That asymmetry is why ch. 17 rules compaction out from its first
page.

**A2. FALSE.** Ch. 16's aside is explicit: the term arose from illegal accesses
on segmented machines, but it persists — "humorously, or not so humorously" —
on machines with no segmentation support at all. Any illegal memory access that
the memory-protection hardware catches gets reported under the historical name.

**A3. TRUE.** The header (minimally the region's size, often plus a magic
number) sits immediately before the returned pointer; `free(ptr)` does pointer
arithmetic (`(header_t *)ptr - 1`) to find it, reads the size, and returns
header-plus-region to the free list. This is why the interface can be
`free(void *)` with no length.

**A4. FALSE.** It searches for a chunk of at least `N` **plus the header size**
(and in practice rounded up to the alignment). Ch. 17 flags this as the "small
but critical detail": a request for 100 bytes consumes 108 in its running
example. Sheet question B2 makes you count this.

**A5. FALSE.** Best fit's exact-as-possible matches systematically leave tiny
leftover slivers that satisfy nothing, and it pays an exhaustive list search to
do so. Ch. 16's aside states the underlying truth: with a thousand policies,
there is no best one — "a good algorithm attempts to minimize" fragmentation,
none avoids it. (Credit answers that also recall the chapter's empirical note
that *worst* fit performs badly; the point being drilled is that no fit policy
dominates.)

**A6. TRUE.** Rounding a 7 KB request up to an 8 KB block is internal
fragmentation, and the buddy scheme accepts it deliberately. In exchange, the
buddy of any block differs from it in exactly one address bit (the bit given by
the block's level/size), so the freed block's coalescing partner is found by
arithmetic, not search — and coalescing recurses cheaply up the tree.

**A7. TRUE.** Ch. 17's own worked example: free chunks that are adjacent in
memory but never merged remain separate list entries, so a heap that is 100%
free can appear as three 10-byte chunks and refuse a 20-byte request. B2
reproduces exactly this failure at slightly larger scale.

**A8. FALSE.** Software fault isolation (Wahbe et al.; OSPP §8.4) confines an
untrusted module without hardware support: the module's code is rewritten so
that its memory accesses are checked or masked into its permitted region
before they execute. Hardware makes the check free per access; software makes
it possible without the hardware. (This is the subject of C2.)

---

## B. Translation and allocation, by hand and by simulator

**B1.**

**(a)** Max segment size 4 KB ⇒ the two segment-selector bits are followed by a
12-bit offset; the quadrants of the 16 KB space are 00: 0–4095, 01: 4096–8191,
11: 12288–16383.

| VA | Segment | Offset arithmetic | Result |
|----|---------|-------------------|--------|
| 1200 | 00 code | offset 1200 < 2048 ✓ | PA = 32768 + 1200 = **33968** |
| 5000 | 01 heap | offset 5000 − 4096 = 904 < 3072 ✓ | PA = 34816 + 904 = **35720** |
| 7300 | 01 heap | offset 7300 − 4096 = 3204 ≥ 3072 | **violation (heap)** |
| 15872 | 11 stack | offset 15872 − 12288 = 3584; negative offset 3584 − 4096 = −512; check \|−512\| ≤ 2048 ✓ | PA = 28672 − 512 = **28160** |
| 13000 | 11 stack | offset 712; negative offset 712 − 4096 = −3384; \|−3384\| > 2048 | **violation (stack)** |

For the stack the rule is: subtract the *maximum* segment size (4 KB) from the
in-quadrant offset to get the negative offset, add it to the base, and bounds-
check the absolute value against the segment's current size.

**(b)** Valid stack negative offsets run from −1 to −2048, i.e. in-quadrant
offsets 2048–4095: virtual addresses **14336** (lowest) through **16383**
(highest).

**(c)** With `-s 42 -a 1k -p 16k` the simulator reports segment 0 base 4506
(0x119a), limit 419; segment 1 base 3919 (0xf4f), limit 262. Top bit of a
10-bit address selects: VAs ≥ 512 are segment 1.

| VA | Result |
|----|--------|
| 754 | seg 1; 754 − 1024 = −270; 270 > 262 → **violation (seg 1)** |
| 692 | seg 1; −332 → **violation (seg 1)** |
| 913 | seg 1; −111; PA = 3919 − 111 = **3808** |
| 89 | seg 0; 89 < 419; PA = 4506 + 89 = **4595** |
| 432 | seg 0; 432 ≥ 419 → **violation (seg 0)** |

Highest legal VA in segment 0: **418**. Lowest legal VA in segment 1:
1024 − 262 = **762**.

**B2.**

**(a)** Each `Alloc(20)` consumes **24 bytes**: 20 is already 4-byte aligned,
plus the 4-byte header. The evolution (no coalescing):

```
start                  [ addr:1000 sz:100 ]
Alloc(20) -> ptr[0]    [ addr:1024 sz:76 ]
Alloc(20) -> ptr[1]    [ addr:1048 sz:52 ]
Alloc(20) -> ptr[2]    [ addr:1072 sz:28 ]
Free(ptr[1])           [ addr:1024 sz:24 ] [ addr:1072 sz:28 ]
Free(ptr[2])           [ addr:1024 sz:24 ] [ addr:1048 sz:24 ] [ addr:1072 sz:28 ]
Alloc(50)              FAILS (returns -1); list unchanged
```

`Alloc(50)` needs 50 rounded to 52, plus 4 of header = **56 contiguous
bytes**; the largest free chunk is 28.

**(b)** Free bytes at the failure: 24 + 24 + 28 = **76** — considerably more
than the 56 required, yet the request fails because no single chunk is big
enough. This is **external fragmentation**, manufactured entirely by the
allocator's own refusal to merge: chunks at 1024, 1048 and 1072 are physically
adjacent.

**(c)** With `-C`, the second free changes everything: when chunk 1048 is
freed, it is adjacent to free chunks on both sides and the three merge into
`[ addr:1024 sz:76 ]`. `Alloc(50)` then succeeds, consuming **56 bytes**
(52 aligned + 4 header) and leaving `[ addr:1080 sz:20 ]`.

*(Simulator quirk worth knowing: in `-A` list mode this simulator reports the
allocated chunk's base address — e.g. "returned 1024" — whereas ch. 17's
convention, and the simulator's own random mode, hand the caller
base + header. The free-list states are the thing to check yourself against.)*

**(d)** No — best fit fails identically (it searches all three chunks and
still finds nothing ≥ 56). A fit policy only chooses *among* the chunks the
free list offers; it cannot repair a list whose chunks are wrongly split. The
lesson: coalescing is **mechanism** — without it, no amount of policy
cleverness recovers the lost contiguity.

**B3.**

**(a)** Requests round up to powers of two: A = 7 KB → **8 KB** block (1 KB
internal fragmentation); B = 12 KB → **16 KB** (4 KB waste); C = 20 KB →
**32 KB** (12 KB waste). Placing A: 64 KB splits into 32+32; the lower 32 KB
splits into 16+16; the lower 16 KB splits into 8+8; A takes **[0, 8 KB)**.
Free after A: [8, 16) as an 8 KB block, [16, 32) as a 16 KB block, [32, 64) as
a 32 KB block.

**(b)** B takes the 16 KB block [16, 32); C takes the 32 KB block [32, 64).
Free: exactly **one 8 KB block, [8 KB, 16 KB)** — 64 − 56 = 8 KB.

**(c)** Freeing B releases the 16 KB block [16, 32). Its buddy is [0, 16) —
but that region is not a free 16 KB block (A occupies [0, 8)), so **no
coalescing occurs**. Freeing A releases [0, 8); its buddy [8, 16) is free, so
they merge into [0, 16); *that* block's buddy [16, 32) is now free, so they
merge into [0, 32); the 32 KB buddy [32, 64) is held by C, so the cascade
stops. Free blocks at the end: **one 32 KB block [0, 32 KB)**, with C still
occupying [32, 64).

**(d)** The buddy of a block of size 2^k at address x is **x XOR 2^k** — the
addresses differ in exactly the bit selected by the block's size. For the 8 KB
block at 40 KB: 40960 XOR 8192 = 32768 = **32 KB**.

**B4.**

**(a)** ⌊4096 / 200⌋ = **20 buffers**, using 4000 bytes; the 96 unusable bytes
are internal fragmentation: 96/4096 ≈ **2.3%**. The paper's bound for a slab
of n buffers is at most 1/n — here 1/20 = 5%, and 2.3% ≤ 5% ✓ (the leftover is
by construction smaller than one buffer).

**(b)** For 2400-byte objects: one page (4096) holds 1 object, wasting
1696/4096 ≈ **41.4%** — far over. Two pages (8192) hold 3 objects (7200
bytes), wasting 992/8192 ≈ **12.1%** ≤ 12.5% ✓. So **2 pages per slab**
suffices.

**(c)** A freed object is returned to its cache **still constructed**: the
invariant part of its state — the paper lists initialised mutexes, condition
variables, reference counts, lists of other objects, and read-only data — is
preserved. The next allocation of that type skips the constructor entirely
(the paper's example: `mutex_init()` runs once, at the object's first
allocation, not once per use; `mutex_destroy()` likewise vanishes from the hot
path). The saving matters because, as the paper measures, constructing a
complex object often costs more than allocating its memory.

**(d)** When a slab's count of in-use buffers falls to zero, the whole slab is
eligible for reclaim, and under memory pressure the general allocator takes
complete slabs back. It is cheap because the allocator unlinks **one slab**,
not each of its buffers from a freelist. Your L3 allocator cannot cache
constructed objects because its interface erases type: `malloc(size)` and
`free(void *)` traffic in anonymous byte ranges, so the allocator cannot know
what "constructed state" would even mean for the memory it hands back, nor
that the next request of the same size is the same kind of object.

---

## C. Discussion and design critique

**C1.** *(Register-C form: defend an unfashionable design.)* A strong defence
makes roughly these arguments, all available from ch. 15–17:

- **Translation is nearly free.** A segmented access costs an add and a
  compare — no memory references, no tables to walk, no translation state to
  cache. The chapter concedes this itself: segmentation "is also fast", with
  "overheads of translation minimal".
- **Per-process MMU state is tiny.** A handful of base/bounds/permission
  registers per process; a context switch saves and restores a few registers.
  Any scheme with per-process translation *tables* pays memory and
  switch-time costs that segmentation simply does not have.
- **Protection and sharing fall at semantic boundaries.** Read-execute on
  code, read-write on data, per logical unit of the program — and marking a
  code segment read-only gives cross-process **code sharing** while preserving
  the isolation illusion. The protection unit matches what the programmer
  means, not an arbitrary fixed size.
- **Sparse address spaces are (coarsely) handled.** Only used segments consume
  physical memory — the original motivation — and the fine-grained variant
  (Multics, B5000) extends this: with thousands of compiler-produced segments,
  the OS learns which logical pieces are actually in use.

Honest concessions: **external fragmentation is fundamental** — variable-sized
segments chop physical memory into odd holes, and compaction is expensive and
self-defeating for growth; and sparsity *within* a segment is unsupported — a
large, sparsely used heap must be entirely resident, so the model breaks when
program behaviour doesn't match the segment model.

Conditions under which pure segmentation is still the right call: a machine
with **simple MMU hardware and tight memory** (a few register pairs are cheap
to build); **few, small, predictable address spaces** whose segments rarely
grow — embedded and single-purpose systems — where external fragmentation has
no time to develop; workloads where **cheap sharing and per-region protection**
are the main goals rather than dense utilisation. Marking notes: full credit
needs (i) at least three distinct affirmative arguments, not one repeated;
(ii) the concessions stated fairly — a defence that denies external
fragmentation is wrong, not bold; (iii) explicit conditions. Answers that
appeal to mechanisms not yet taught should instead argue on the chapter's own
terms; "segmentation lost, therefore it was worse" earns nothing — the
question asks under what conditions the verdict flips.

**C2.** *(Compare two designs under a stated constraint.)* Where the costs
land:

- **Hardware segments:** the *steady-state* cost is essentially zero — every
  load and store is bounds-checked by the MMU for free. The cost concentrates
  at the **boundary**: segment registers are privileged state, so entering and
  leaving the codec means trapping into the OS to swap protection state, at
  thousands of crossings per second. There is also a granularity rigidity: the
  window is whatever the segment hardware can express.
- **SFI:** the boundary is cheap — a crossing is a checked jump in user space,
  no kernel involvement — but the cost is smeared across execution: every
  store (and indirect jump) inside the codec carries masking/checking
  instructions, so code that is dense with memory accesses pays a percentage
  tax on its whole inner loop. You also shift trust from hardware to the
  rewriter/verifier that produced the sandboxed code.

Under the stated constraint the two costs pull opposite ways, so the answer
must be argued, not asserted. A defensible choice: the crossing rate
(thousands/sec) is small in absolute terms — thousands of traps per second is
a modest kernel load — while "dense with memory accesses" taxes *every
iteration* of the codec's hot loops; that favours **segments**. The verdict
flips if: crossings become very frequent relative to work per call (chatty,
fine-grained calls — SFI wins on boundary cost); the deployment hardware lacks
usable segment support (SFI is then the only option — its historical
motivation); the codec's loops are compute-dense rather than store-dense
(SFI's tax shrinks); or several sandboxes with odd-sized windows are needed
(software wins on flexibility). Marking notes: credit requires locating the
costs correctly — per-access vs per-crossing — and naming at least two
condition changes that flip the choice. Either final choice is acceptable
with that structure; a choice with no flip conditions is not.

**C3.** *(Diagnosis.)* **Mechanism:** every free inserts an unmerged chunk at
the head; every allocation that splits a chunk manufactures a smaller
remainder. With no coalescing, chunk sizes only ever go *down* — adjacent free
chunks are never reunited — so the free list drifts monotonically toward a
large population of small chunks. First fit then serves small requests from
the head, where the freshly freed small chunks congregate, re-splitting them
further; ch. 17 notes first fit "pollutes the beginning of the free list with
small objects". After days of mixed-size traffic, the 35% of the heap that is
free exists as thousands of sub-4 KB fragments: classic **external
fragmentation**, not a leak — which is consistent with live-message memory
being flat.

**Confirming measurements:** (1) the free-list **size distribution** — in
particular the largest free chunk versus the 4 KB + header needed: if
max-chunk < request, the diagnosis is proven; (2) **free-list length over
uptime** — a monotonically growing count of ever-smaller chunks is the
signature of no-coalescing (both measurements are exactly what B2 showed in
miniature).

**Fixes, in order:** (1) **coalesce on free** — with an address-ordered list
so neighbours are found cheaply; this removes the mechanism of decay and is
the fix the allocator was always missing. (2) **Segregate the hot size**: give
4 KB message buffers their own free list (slab-style), so the size class that
matters cannot be starved by other traffic and its alloc/free path gets
faster. (3) Only then consider fit-policy tuning — B2(d) showed policy alone
cannot recover contiguity. **The nightly restart is a mitigation, not a fix**:
it resets the free list to one large extent and the same decay begins again —
acceptable as a stopgap, an admission of defeat as an architecture. Marking
notes: the answer must name the mechanism (splitting without merging, plus
head-insertion concentrating small chunks where first fit looks) and must
distinguish fragmentation from a leak using the given evidence; a fix list
that omits coalescing, or one that proposes "use best fit" as the primary
remedy, misses the mechanism/policy distinction the sheet has been building.
