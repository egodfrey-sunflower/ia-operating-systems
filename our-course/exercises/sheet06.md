# Examples Sheet 6 — Memory management

**Attempt in two stages: §§B–C after Week 8; §§D–G after Week 9.** Budget 2–4
hours per stage. Work closed-book first; self-mark against
`answers/sheet06-answers.md` (spoilers — attempt first).

*Covers: address binding, segmentation vs paging, multi-level
page tables, TLBs, demand paging and replacement, copy-on-write, the buddy and
slab allocators.*

Reading: OSTEP ch. 13, 15–24 (address spaces through replacement; ch. 14, the memory-API interlude, is optional); the xv6 book
ch. 3; reading-list papers 17 (Denning, working set), 18 (Belady) and 19
(Bonwick, slab). Work closed-book first, then check against the reading. This
sheet pairs with **Lab 4 (xv6 virtual memory)** — several questions refer to it.

The sheet is split because its two halves need different weeks' material: §§B–C
need only address translation and paging (weeks 7–8), whereas §§D–G lean on week
9's replacement, working-set and copy-on-write material — so attempt the first
half after Week 8 and the second after Week 9.

---

## A. Warm-ups (true / false, with a one-line justification)

**A1.** A TLB miss always causes a page fault.

**A2.** With copy-on-write `fork()`, no physical page is ever copied.

**A3.** The LRU replacement policy can suffer from Belady's anomaly (more
frames giving *more* faults).

**A4.** The buddy allocator suffers from internal fragmentation but never from
external fragmentation.

**A5.** Demand segmentation is as straightforward to implement as demand
paging.

---

## B. Bookwork from the IA sheet (do by citation)

These four are the memory questions from the original IA examples sheet 2.
Answer them in full prose as written there; the notes only say what to watch
for.

**B1.** Do **IA Examples Sheet 2, Q4** (`../../cambridge-course/examples_sheets/examples_sheet2.pdf`)
— the *address-binding problem* and its resolution at compile / load / run
time. *Note:* tie each option to who does the relocation (linker, loader, MMU)
and what it costs; run-time binding is exactly what the rest of this sheet
relies on.

**B2.** Do **IA Examples Sheet 2, Q5** — benefits and drawbacks of giving each
process its own virtual address space via a level of indirection. *Note:* name
at least isolation, relocation/placement freedom, and sharing; for drawbacks
weigh translation cost and per-process page-table space (which B5 quantifies).

**B3.** Do **IA Examples Sheet 2, Q6** — sketch and translate a *paged* versus
a *segmented* virtual address, and say which makes physical allocation easier.
*Note:* the key contrast is fixed-size frames (no external fragmentation, easy
allocation) versus variable-size segments (external fragmentation, but a
meaningful protection/sharing unit).

**B4.** Do **IA Examples Sheet 2, Q7** — internal vs external fragmentation,
the purpose of a page table and its interaction with the TLB, and a two-level
page table. *Note:* your two-level answer feeds directly into B5, which makes
it numeric.

---

## C. Multi-level page-table translation and table space

Consider a 32-bit byte-addressed machine with **4 KiB pages** and a **two-level**
page table. The 32-bit virtual address is split

```
  31            22 21            12 11             0
 +----------------+----------------+----------------+
 | L1 index (10)  | L2 index (10)  | offset (12)    |
 +----------------+----------------+----------------+
```

Each table (the top-level *directory* and each second-level table) has 1024
entries of 4 bytes, so each occupies exactly one 4 KiB page. A process `P` has
only two mapped regions:

* **code/data:** virtual pages at VA `0x0000_0000 … 0x0000_3FFF` (4 pages),
  mapped to physical frame numbers `0x100, 0x101, 0x102, 0x103`;
* **stack:** virtual pages at VA `0xFFFF_C000 … 0xFFFF_FFFF` (4 pages), mapped
  to physical frame numbers `0x200, 0x201, 0x202, 0x203` in ascending address
  order.

Everything else is unmapped (invalid PTE).

**(a)** For each virtual address below, give the L1 index, L2 index, offset,
and either the translated physical address or "page fault":

  (i) `0x0000_2ABC`  (ii) `0xFFFF_D123`  (iii) `0x0040_5000`

**(b)** How many *pages* of physical memory does `P`'s complete two-level page
table occupy? List them (the directory and each second-level table actually
needed).

**(c)** Compare that with the memory a **single-level** page table for the same
32-bit / 4 KiB / 4-byte-PTE machine would need. State the ratio, and explain in
one sentence the property of `P`'s address space that makes the two-level
scheme win here.

**(d)** Suppose `P` now also `mmap`s one page at VA `0x4000_0000`. By how many
pages does the page table grow, and why? What does this tell you about the
*worst case* for a two-level table versus the average case?

---

## D. Page-replacement trace and Belady's anomaly

A process generates the reference string

```
  1 2 3 4 1 2 5 1 2 3 4 5
```

(each number is a page). Frames start empty; count a *page fault* whenever a
referenced page is not resident.

**(a)** With **3 frames**, produce the full residency trace and fault count for
each of **FIFO**, **LRU**, and **OPT** (Belady's optimal). Mark every fault.

**(b)** Now run **FIFO with 4 frames** on the same string. State the fault
count. You should find it is *larger* than with 3 frames. Name this phenomenon
and explain why FIFO permits it. What property do LRU and OPT share that makes
it impossible for them? (Reading-list paper 18, Belady 1966.)

**(c)** Run the **clock (second-chance)** algorithm with 3 frames on the same
string, showing the reference bit and clock hand at each step. Compare its
fault count to FIFO and to LRU, and say in one sentence what clock is
approximating and why it is cheap.

**(d)** LRU is expensive to implement exactly. Explain how a kernel running on
hardware with no reference or dirty bit can *emulate* both using only the
page-table valid/protection bits and the page-fault handler.

**(e) Working set.** Denning's **working set** `W(t, Δ)` is the set of distinct
pages referenced in the window of the `Δ` most recent references up to and
including time `t` (reading-list paper 17). Using the *same* reference string
`1 2 3 4 1 2 5 1 2 3 4 5` (number the references `t = 1 … 12`) and a window
`Δ = 4`:

  (i) Give `W(t, Δ)` and its size `|W|` at `t = 4` and at `t = 8`.

  (ii) The working set *shrinks* from one instant to the other. Explain what in
  the reference stream causes that, and what `|W(t, Δ)|` is estimating about the
  process's demand for frames.

**(f) Working set, thrashing, and what the OS does.** As the *degree of
multiprogramming* rises, the OS packs more processes into a fixed amount of
physical memory. Using the working-set principle, explain why there is a point
past which adding one more process makes total throughput *collapse* (the
processes spend nearly all their time paging) — name this phenomenon — and state
what a working-set-based memory manager should do to prevent it (think about
admission control and *whole-process* swap-out, not per-page eviction).

**(g) MRU.** Run **MRU** (most-recently-used: on a fault with all frames full,
evict the page referenced *most* recently) with **3 frames** on the same
reference string, giving the residency trace and fault count, and compare it
with your counts from (a). For what kind of reference pattern is MRU a
sensible policy — and why is that exactly the pattern on which LRU behaves
worst?

---

## E. Copy-on-write fork (Lab 4)

In **Lab 4, Task 3** you made xv6's `fork()` copy-on-write. Answer with
reference to that design.

**(a)** Explain what `uvmcopy()` does to the parent's and child's PTEs for a
writable page instead of copying it, and what per-physical-frame bookkeeping
`kalloc.c` must now maintain so that a shared frame is freed at the right time.

**(b)** After the change, a **store** page fault (RISC-V `scause == 15`) is
*ambiguous*. What two distinct situations can now raise it, and how does the
trap handler tell them apart? (This is the central design point of the lab.)

**(c)** The hardware faults automatically on a bad user store — so why must
`copyout()` *also* contain COW-breaking logic? Give a concrete example of a
kernel write to a user page that would otherwise silently corrupt shared data.

**(d)** `forkbench` grows a parent to ~4 MB and then repeatedly forks a child
that exits immediately. Explain why COW makes this dramatically cheaper, and
name one workload where COW *loses* (the child writes almost everything).

---

## F. Buddy allocator

A binary buddy allocator manages a contiguous **1 MiB** arena with a minimum
block size of **4 KiB** (so block orders 0…8, sizes 4 KiB … 1 MiB).

**(a)** Starting from one free 1 MiB block, service this request stream, showing
the free lists (by order) after each step:

```
  A = alloc(4 KiB)
  B = alloc(60 KiB)
  C = alloc(4 KiB)
  free(A)
  free(C)
```

**(b)** For request `B = alloc(60 KiB)`, which order block is returned, and how
much memory is wasted to **internal** fragmentation? State the general rule for
buddy-allocator internal fragmentation.

**(c)** After `free(A)` then `free(C)` in part (a), can any coalescing happen?
Explain, using the definition of *buddies*, why freeing `C` may or may not merge
even though `A` and `C` were both 4 KiB.

**(d)** Give one reason the Linux kernel uses a buddy allocator for
*page-granularity* allocation despite its internal fragmentation, and one reason
it does **not** use it directly for small kernel objects (which motivates G
below).

---

## G. Slab allocator

Read reading-list paper 19 (Bonwick, "The Slab Allocator", 1994).

**(a)** A slab allocator sits *on top of* the page allocator (e.g. the buddy
allocator). Describe how a *cache* of same-type objects, *slabs*, and the buddy
allocator relate. Where does the memory ultimately come from?

**(b)** Bonwick stresses *object caching* — retaining the constructed/initialised
state of freed objects. Explain the performance argument: what work is saved on
the alloc/free fast path, and why this matters for objects like inodes or
`task_struct`s that are created and destroyed constantly.

**(c)** Explain how per-type slabs largely eliminate **external** fragmentation
for kernel objects, and how they improve **cache** and **TLB** behaviour
compared with a general-purpose `kmalloc`-style allocator.

**(d)** In Lab 5 you will split xv6's single `kmem` free list into a per-CPU list to
cut lock contention. Slab allocators also keep per-CPU (magazine) caches.
Explain in one or two sentences why the *same* concurrency argument applies.
*(You will do exactly this in Lab 5 — answer here from the week-6 lock-contention
principles.)*

---

## H. Program loading and dynamic linking (week 7)

*Week-7 material — attempt this with the **first** stage (alongside §§B–C); it
builds directly on the page-sharing / copy-on-write machinery of §E.*
(reading: Drysdale's LWN linking pieces, "How programs get run" and "…ELF
binaries" / CS:APP ch. 7.)

When you `execve` a dynamically-linked program, the kernel maps the ELF image
but hands control not to the program itself but to its **interpreter**, `ld.so`
(the dynamic linker), named in the ELF's `PT_INTERP` segment.

**(a)** Briefly, what does `ld.so` do before the program's `main` runs? Say how
it finds and maps the shared libraries it needs (e.g. libc), and what **lazy
binding** through the **PLT/GOT** buys — i.e. why a call to `printf` need not be
resolved until the first time it is actually made.

**(b)** Two processes each run a *different* program, but both are dynamically
linked against the **same** libc. The OS keeps **one** physical copy of libc's
*code*, shared read-only between them, yet each process gets its **own** copy of
libc's *writable* data.

  (i) Why is the code shareable but the data not? For one physical copy to be
  correct in two address spaces that may have mapped libc at *different* virtual
  addresses (as ASLR ensures), what must be true of the machine code — the
  property named **PIC/PIE**?

  (ii) libc's writable data starts out *identical* in both processes. Which
  mechanism from **§E** lets the OS still back it with a single physical frame
  until a process first writes to it? And what does the **GOT** have to do with
  why the code pages can stay read-only and shared while that data diverges
  per process?

---

## Past paper questions

Per this directory's `README.md` allocation, this sheet's set is the
genuine memory papers. This sheet lands in the heaviest stretch of the course,
so rather than attempting them the week the sheet is due, **hold them for weeks
10–11 and do them there as Midterm 2 preparation** (~35 minutes each,
closed-book) — they serve better as MT2 revision than as same-week drill:

* **`y2018p2q4`** (`../../cambridge-course/exam_questions/y2018p2q4.pdf`) — page faults, OPT vs
  practical replacement policies, and Belady's anomaly.
* **`y2020p2q3`** (`../../cambridge-course/exam_questions/y2020p2q3.pdf`) — how paging provides
  protection, the TLB, a five-level-page-table effective-access-time
  calculation, and partial-match TLBs.
* **`y2022p2q3`** (`../../cambridge-course/exam_questions/y2022p2q3.pdf`) — segmentation vs
  paging, and two-level page tables.

For extra untimed drill, the **five pre-2016 paging/segmentation questions**
listed on IA examples sheet 2 (`y2015p2q4`, `y2013p2q4`, `y2009p2q3` *(not
(b))*, `y2009p2q4`, `y2011p2q4(a)`) remain good practice. Note that three further
memory papers — `y2023p2q3`, `y2024p2q4`, `y2025p2q4` — are deliberately *held
back* as fresh memory drill for exam revision; resist the temptation to burn
them now.

For mechanical drill on this sheet's calculations, the OSTEP homework
simulators (<https://github.com/remzi-arpacidusseau/ostep-homework>) are ideal:
`relocation.py`, `segmentation.py`, `paging-linear-translate.py` and
`paging-multilevel-translate.py` for section C's translations, and
`paging-policy.py` for section D's replacement traces.
