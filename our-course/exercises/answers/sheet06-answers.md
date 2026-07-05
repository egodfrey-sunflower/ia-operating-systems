# ⚠️ SPOILER — Examples Sheet 6 model answers ⚠️

> **STOP.** These are full worked solutions. Do the sheet closed-book first.
> Numeric answers were verified with Python; the checks are noted inline.

---

## A. Warm-ups

**A1. False.** A TLB miss only means the translation is not *cached*. The MMU
(or software) then walks the page table; if the PTE is valid the page is
resident and the access completes with no fault. A page fault happens only when
the walk finds an invalid/not-present PTE. TLB miss ⊂ page-table walk; page
fault is a strict subset of walks.

**A2. False.** COW `fork()` copies page-table *structure* and metadata eagerly,
and it copies a data page lazily on the *first write* to a shared writable page.
Read-only pages (text) and pages never written are never copied — but "never
copied at all" is wrong: the first store to any shared page triggers a real
copy.

**A3. False.** LRU is a *stack algorithm*: the set of pages resident with `n`
frames is always a subset of those resident with `n+1` frames, so more frames
can never increase faults. Belady's anomaly requires the non-stack property,
which FIFO has and LRU/OPT do not.

**A4. False (the first half is true, the "never" is not).** The buddy
allocator only ever hands out power-of-two blocks, so a request is rounded up —
internal fragmentation (up to nearly 2×) is real. But it *reduces* external
fragmentation rather than eliminating it: a block can only merge with its one
designated buddy, so a scattering of long-lived allocations whose buddies stay
live blocks coalescing at every level — free memory can be plentiful yet
unmergeable, and a large-order request then fails. That *is* external
fragmentation, and it is why Linux layers migrate-types and memory compaction
on top of its buddy allocator. Buddy's cheap, address-computed coalescing makes
the problem far milder than in a variable-size first-fit allocator, but F(a)'s
`free(A)` step shows the mechanism: a free 4 KiB block that cannot merge
because its buddy is allocated.

**A5. False.** Segments are variable-sized, so faulting one in requires a
contiguous allocation of the right size (external fragmentation, placement
policy), whereas any free frame satisfies a page fault; that asymmetry is why
demand paging won.

---

## B. Bookwork

Model answers are in the reading (OSTEP ch. 13–20; A&D ch. 2/8). Marking notes:

* **B1 (address binding):** compile-time → absolute code, fast but not
  relocatable (MS-DOS `.COM`); load-time → relocatable code fixed up by the
  loader, relocatable once but not after load; run-time → MMU adds base / walks
  page table every access, fully relocatable and the basis of virtual memory,
  at the cost of translation hardware.
* **B2 (indirection):** benefits — isolation/protection, placement freedom
  (any free frames), sharing (map same frame into two spaces), demand paging &
  swapping, COW. Drawbacks — translation latency (mitigated by TLB), page-table
  memory (mitigated by multi-level, see C), TLB-shootdown / context-switch
  cost.
* **B3 (paged vs segmented):** paging → (page#, offset), fixed frames, no
  external fragmentation, allocation is trivial (any free frame), internal
  fragmentation ≤ 1 page. Segmentation → (segment#, offset) via a segment table
  giving base+limit, variable size, external fragmentation, but segments are
  natural protection/sharing units. Physical allocation is *easier under
  paging*.
* **B4 (fragmentation / page table / TLB / two-level):** internal =
  allocated-but-unused inside a fixed unit (page/block); external = free memory
  split into unusable gaps between allocations. A page table maps VPN→PFN and
  holds valid/protection/reference/dirty bits; the TLB caches recent
  translations so the walk is skipped on a hit. Two-level: the top table indexes
  second-level tables that are allocated only for populated regions — the point
  quantified in C.

---

## C. Multi-level translation

*Verified in Python.* Split: `L1 = VA[31:22]`, `L2 = VA[21:12]`,
`offset = VA[11:0]`. Stack pages `0xFFFFC000..0xFFFFF000` land at L2 indices
1020, 1021, 1022, 1023 (all under L1 index 1023), mapped to frames
0x200, 0x201, 0x202, 0x203 respectively.

**(a)**

| VA | L1 | L2 | offset | result |
|----|----|----|--------|--------|
| `0x0000_2ABC` | 0 | 2 | `0xABC` | frame `0x102` → **PA `0x0010_2ABC`** |
| `0xFFFF_D123` | 1023 | 1021 | `0x123` | frame `0x201` → **PA `0x0020_1123`** |
| `0x0040_5000` | 1 | 5 | `0x000` | L1 index 1 is **invalid** → **page fault** |

(`0x102ABC = (0x102<<12) | 0xABC`; `0x201123 = (0x201<<12) | 0x123`.)

**(b)** Three pages:

1. the top-level **directory** (1 page);
2. one **second-level table** for L1 index 0 (covers the code/data region);
3. one **second-level table** for L1 index 1023 (covers the stack region).

The two mapped regions fall under two *different* top-level entries, so two
second-level tables are needed. Total = **3 × 4 KiB = 12 KiB**.

**(c)** A single-level table needs one PTE per virtual page: `2^20` PTEs ×
4 bytes = **4 MiB**, resident whether or not pages are mapped. Ratio
≈ 4 MiB / 12 KiB ≈ **341×** smaller. The win comes from `P`'s address space
being **sparse**: almost all top-level entries are invalid, so their
second-level tables are simply never allocated.

**(d)** `0x4000_0000` has `L1 = 0x100 = 256`, a *third* distinct top-level
entry, so a new second-level table is allocated: the page table grows by **one
page** (to 4 tables / 16 KiB). Worst case a process touches one page under every
top-level entry, forcing all 1024 second-level tables plus the directory
(≈ 4 MiB + 4 KiB) — *worse* than single-level. Multi-level tables help only
when the address space is sparse, which real ones overwhelmingly are.

---

## D. Replacement trace

Reference string `1 2 3 4 1 2 5 1 2 3 4 5`. *All fault counts verified in
Python.*

**(a) 3 frames.**

**FIFO (9 faults).** `*` marks a fault; the queue evicts oldest-loaded.

| ref | 1 | 2 | 3 | 4 | 1 | 2 | 5 | 1 | 2 | 3 | 4 | 5 |
|-----|---|---|---|---|---|---|---|---|---|---|---|---|
|fault| * | * | * | * | * | * | * |   |   | * | * |   |
|frames|1|1 2|1 2 3|2 3 4|3 4 1|4 1 2|1 2 5|1 2 5|1 2 5|2 5 3|5 3 4|5 3 4|

Faults at refs 1,2,3,4,1,2,5,3,4 → **9**.

**LRU (10 faults).** Evict the least-recently-*used*.

Faults occur at every reference except the `1` and `2` that immediately follow
the first `5`; total **10**. (E.g. after `1 2 3 4 1 2` the set is `{4,1,2}`;
`5` evicts 4; then `1`,`2` hit; `3` evicts 5; `4` evicts 1; `5` evicts 2.)

**OPT (7 faults).** Evict the page used furthest in the future.
`1 2 3` (3 faults) fill; `4` evicts 3 (3 not needed until much later) → set
`{1,2,4}`... worked forward the optimal choices give faults at
`1,2,3,4,5,3,4` = **7 faults** — the minimum achievable.

Ordering: **OPT (7) < FIFO (9) ≤ LRU (10)** on this particular string. (LRU
beating FIFO is the *usual* case; this string is a deliberate counter-example
where FIFO happens to do better — a good discussion point.)

**(b) FIFO, 4 frames → 10 faults.** With 3 frames FIFO faulted **9** times;
with 4 frames it faults **10**. More frames, more faults: this is **Belady's
anomaly** (paper 18). It is possible because FIFO is *not* a stack algorithm —
the set of pages resident with 4 frames is **not** guaranteed to contain the
set resident with 3 frames, so adding a frame can change *which* page is the
oldest and evict something that is about to be reused. LRU and OPT satisfy the
inclusion (stack) property `resident(n) ⊆ resident(n+1)`, so their fault count
is monotone non-increasing in the number of frames and they can never exhibit
the anomaly. (Verified: LRU 3→4 frames goes 10→8; OPT 7→6.)

**(c) Clock, 3 frames → 9 faults.** Each frame carries a reference bit; on a
fault the hand advances, clearing set bits (giving a "second chance") until it
finds a 0, which it replaces. On this string clock matches FIFO at **9 faults**
(with so few reuses there is little for the reference bits to protect). Clock
approximates LRU — it distinguishes "referenced since the hand last passed"
from "not" — using a single bit per frame and no per-access bookkeeping, which
is why real kernels use it (or NRU/aging) instead of true LRU.

**(d) Emulating reference/dirty bits (`y2024p2q4d`).** Start every resident
page with its PTE marked **invalid** (or read-only). The *first read* traps;
the handler sets a software **reference** bit and makes the PTE valid-read-only,
then resumes. The *first write* traps again; the handler sets a software
**dirty** bit, makes the PTE writable, and resumes. Periodically the pager
clears the software bits and re-protects the pages, so the next access re-traps
and re-marks them. Thus one page fault per page per sampling interval buys
approximate reference/dirty information with no hardware support — the same
protection-fault trick COW and demand-zero use.

**(e) Working set (Δ = 4).** *Verified in Python* over the string
`1 2 3 4 1 2 5 1 2 3 4 5` (references numbered `t = 1 … 12`); the window is the
`Δ = 4` most recent references up to and including `t`.

- (i) At **`t = 4`** the window is references `1 2 3 4`, so
  **`W(4, 4) = {1, 2, 3, 4}`, `|W| = 4`**. At **`t = 8`** the window is
  references `1 2 5 1` (positions 5–8), so **`W(8, 4) = {1, 2, 5}`, `|W| = 3`**.
- (ii) The set shrinks because between the two instants the stream stops touching
  new pages and instead **re-references a small locality** `{1, 2, 5}` — the `1`
  at `t = 8` is a repeat, so the window holds only three *distinct* pages. `|W(t,
  Δ)|` estimates the number of frames the process is *actively* using right now:
  it is the resident-set size that would let it run this window fault-free, i.e.
  its live demand for memory. (For reference, the size over the whole run is
  `1,2,3,4,4,4,4,3,3,4,4,4` — it sits at 4 during the fresh-page phase, dips to 3
  during the `{1,2,5}` locality, and returns to 4.)

**(f) Thrashing and admission control.** Each process has a working-set size
`WSSᵢ = |W(t, Δ)|`. The system runs efficiently only while `Σ WSSᵢ` fits in
physical memory. As the degree of multiprogramming grows, that sum eventually
**exceeds** the available frames: the pager must steal a frame that is *in* some
process's working set, that process faults almost immediately to get it back,
which steals a frame from another working set, and so on. CPU utilisation
collapses because every process is blocked on paging and the disk, not the CPU,
is the bottleneck — this is **thrashing**. The working-set principle says the fix
is *admission control*: the OS should only run a process if its working set fits
alongside the others' (`Σ WSSᵢ ≤ frames`), and when memory is over-committed it
should **swap out an entire process** — suspending it and reclaiming *all* its
frames at once — rather than nibbling pages off everyone. Removing one whole
working set lets the survivors run fault-free; the suspended process is resumed
later when its working set can be re-admitted. (This is why the classic answer to
thrashing is *reduce multiprogramming*, not *buy a faster disk*.)

**(g) MRU, 3 frames → 7 faults.** *Verified in Python.* Evict the
most-recently-used page when a fault finds all frames full:

| t | ref | fault | frames after (recency order, most-recent last) |
|--:|:---:|:-----:|:-----------------------------------------------|
| 1 | 1 | * | 1 |
| 2 | 2 | * | 1 2 |
| 3 | 3 | * | 1 2 3 |
| 4 | 4 | * | 1 2 4 (evict 3) |
| 5 | 1 |   | 2 4 1 |
| 6 | 2 |   | 4 1 2 |
| 7 | 5 | * | 4 1 5 (evict 2) |
| 8 | 1 |   | 4 5 1 |
| 9 | 2 | * | 4 5 2 (evict 1) |
| 10 | 3 | * | 4 5 3 (evict 2) |
| 11 | 4 |   | 5 3 4 |
| 12 | 5 |   | 3 4 5 |

**7 faults** — on this particular string MRU matches OPT (7) and beats FIFO (9)
and LRU (10), because the string's tail re-uses pages (4, 5) that MRU's early
evictions happened to leave resident. MRU is a *sensible* policy for **cyclic
(sequential-scan) access to a data set larger than memory** — repeated linear
passes over a big file or table. Under LRU such a loop is pessimal: with `n`
frames and a cycle of more than `n` pages, the page LRU evicts is always the
one the scan will want soonest, so *every* reference faults. Evicting the
most-recently-used page instead keeps a stable prefix of the cycle resident,
so a fixed fraction of each pass hits. The workload on which LRU collapses is
exactly the one MRU is built for — which is why database buffer managers use
MRU-style eviction for large table scans.

---

## E. Copy-on-write fork (Lab 4)

**(a)** For each writable page, `uvmcopy()` **clears `PTE_W` and sets `PTE_COW`
in both** the parent's and the child's PTE, maps the child's PTE to the *same*
physical frame, and **increments that frame's reference count**. Read-only
pages are simply shared read-only. `kalloc.c` gains a per-physical-page
reference counter (indexed by `(pa-KERNBASE)/PGSIZE`, protected by its own
spinlock): `kalloc` sets it to 1, `uvmcopy` bumps it, and `kfree` *decrements*
and returns the frame to the freelist **only when the count hits 0**.

**(b)** A store fault (`scause 15`) can now mean either **(i) a lazily-allocated
page** that has never been given a physical frame (unmapped), or **(ii) a
copy-on-write page** — mapped, valid, `PTE_U`, `PTE_COW`, but not writable. The
handler disambiguates by walking to the PTE: if it is present with `PTE_COW`
set, do the COW break (copy + remap writable); otherwise fall through to
`vmfault()`'s lazy-allocation path. Checking the COW marker **first** is
essential.

**(c)** When the *kernel* writes into a user page — e.g. `wait()` copying an
exit status out, or a pipe/`read()` filling a user buffer — it walks the user
page table by hand in `copyout()` and does `memmove` directly; **no hardware
fault occurs**. If that target page is still a shared COW page, the kernel would
scribble on the frame the *parent* also sees, corrupting it. So `copyout()`
must itself detect `PTE_COW` and run the copy-and-remap path before writing.
Concrete case: parent forks, child blocks in `read()` on a pipe, parent writes
the pipe — the kernel's `copyout` into the child's still-shared buffer must
break COW or both processes' pages alias.

**(d)** `forkbench`'s child exits immediately, writing almost nothing. Eager
fork copies all ~4 MB every iteration; COW copies only page tables and shares
the frames, so the per-fork cost drops from O(address-space) to O(page-table),
and the tick count falls sharply. COW *loses* when the child immediately
rewrites most of its address space (e.g. a compute kernel that dirties every
page right after `fork`): then every page faults and is copied anyway, and you
pay the fault + bookkeeping overhead **on top of** the copy you could not avoid.

---

## F. Buddy allocator

**(a)** Orders 0..8 = 4 KiB..1 MiB. (Blocks named by their base offset in KiB.)

* `alloc(4 KiB)` = order 0. Split 1 MiB→512K→…→4 KiB, taking the leftmost 4 KiB.
  `A` = `[0,4K)`. Free list now holds one buddy at each split: order-0 `[4K,8K)`,
  order-1 `[8K,16K)`, order-2 `[16K,32K)`, order-3 `[32K,64K)`, order-4
  `[64K,128K)`, order-5 `[128K,256K)`, order-6 `[256K,512K)`, order-7
  `[512K,1M)`.
* `alloc(60 KiB)` rounds up to **64 KiB = order 4**. Take the order-4 free block
  `[64K,128K)`. `B` = `[64K,128K)`.
* `alloc(4 KiB)` = order 0. The order-0 free block `[4K,8K)` is available →
  `C` = `[4K,8K)`. Now `[0,8K)` is fully allocated (`A`+`C`).
* `free(A)` returns `[0,4K)`. Its buddy `[4K,8K)` is `C`, still allocated → **no
  coalescing**; `[0,4K)` sits on the order-0 free list.
* `free(C)` returns `[4K,8K)`. Now its buddy `[0,4K)` is free → coalesce to
  `[0,8K)` (order 1); its buddy `[8K,16K)` is free → coalesce to `[0,16K)`
  (order 2); continue up: `[16K,32K)` free → `[0,32K)`; `[32K,64K)` free →
  `[0,64K)` (order 6). It stops there because the buddy of `[0,64K)` is
  `[64K,128K)` = `B`, still allocated. Free lists: order-6 `[0,64K)`, order-6
  `[256K,512K)`, order-7 `[512K,1M)`, i.e. 1 MiB minus the 64 KiB held by `B`.

**(b)** `B` gets an **order-4, 64 KiB** block for a 60 KiB request → **4 KiB
wasted** to internal fragmentation. General rule: a request of `s` bytes is
served by a block of `2^⌈log2 s⌉`, so waste is up to *just under 2×* (worst
case a request one byte over a power of two nearly doubles).

**(c)** *Buddies* are the two halves that a single split created, i.e. addresses
differing only in the bit for that order. `A=[0,4K)` and `C=[4K,8K)` **are**
order-0 buddies. So freeing `A` alone cannot coalesce (its buddy `C` is live);
freeing `C` afterwards makes both halves free and triggers the cascade in (a).
Coalescing depends on the buddy being free, not merely on some other block of
the same size being free.

**(d)** Page-granularity: the buddy allocator gives O(log n) alloc/free with
guaranteed physically-contiguous power-of-two runs (needed for DMA and huge
pages) and clean coalescing — internal fragmentation is tolerable at page
granularity. Small objects: rounding a 40-byte `task_struct` field up to a
whole page (or even to the next power of two) is ruinous, and the alloc/free
rate is enormous — hence a slab allocator layered on top (G).

---

## G. Slab allocator (paper 19)

**(a)** A **cache** is created per object *type* (one for inodes, one for
`task_struct`, …). Each cache owns a set of **slabs**; a slab is one or a few
**pages obtained from the buddy allocator**, carved into an integral number of
same-size object slots plus a little bookkeeping. Allocation hands back a slot;
the memory ultimately comes from the page allocator, but only in slab-sized
chunks, amortised over many objects.

**(b)** Bonwick's key idea is **object caching**: a freed object keeps its
constructed/initialised state (locks initialised, embedded lists valid), so
alloc from a slab returns a ready-to-use object with **no constructor run** and
free needs **no destructor** — only when a whole slab is reclaimed do the
constructor/destructor run. On the fast path alloc/free are a few pointer
operations. For inodes, `task_struct`s, dentries — created and destroyed
constantly — this removes repeated initialise/teardown cost and keeps the
object layout cache-warm.

**(c)** Because every object in a cache is the **same size**, a slab packs them
with no per-object gaps and freed slots are immediately reusable by the next
same-type alloc — so **external fragmentation is essentially eliminated** within
a cache. Same-size packing also gives predictable alignment: objects can be
coloured to spread them across cache lines/sets, reducing conflict misses, and
keeping hot objects dense improves **cache and TLB** hit rates versus a
general-purpose allocator that intermixes sizes.

**(d)** A single global slab freelist would be a contended lock on every
alloc/free across all CPUs — exactly the `kmem` bottleneck you removed in Lab 5
by giving each CPU its own free list. Slab allocators keep per-CPU
**magazines** (small caches of ready objects) for the same reason: the common
alloc/free hits a CPU-local structure with no cross-CPU locking, and only
refills/drains touch the shared depot.

---

## H. Program loading and dynamic linking (week 7)

**(a)** `ld.so` **finishes the link at load time**. It reads the executable's
dynamic section to find the list of needed shared objects (`DT_NEEDED`, e.g.
`libc.so.6`), searches for each along the library path (`RUNPATH`/`RPATH`,
`LD_LIBRARY_PATH`, the `/etc/ld.so.cache` index, then default dirs), and for
each object **`mmap`s its ELF `PT_LOAD` segments** into the process — text
read-only + executable, data read-write — recursing into that library's own
dependencies. It then applies **relocations**, filling each object's **GOT
(global offset table)** with the runtime addresses of the symbols the code
refers to, and only then jumps to the program's entry point. (So `execve` maps
the ELF and the interpreter; `ld.so`, running in the new process, maps
everything else.)

**Lazy binding** defers the *function* relocations. Rather than resolve every
imported function up front, a call to e.g. `printf` goes through a **PLT
(procedure linkage table)** stub that jumps indirectly through a GOT slot. That
slot initially points back into the stub itself, which pushes the symbol's
relocation index and jumps into `ld.so`'s resolver; the resolver locates
`printf`, writes its real address into the GOT slot, and jumps to it. Every
later call finds the resolved address and jumps straight there. So a symbol is
bound only *if and when* it is first called — startup never pays to resolve
functions a given run does not use.

**(b)(i)** libc's *code* is the same bytes for every process, so a single
physical copy — the file's **page-cache** pages, mapped read-only into each
address space and never written — is correct and saves memory. Its *writable
data* (globals, and the GOT) is mutated independently by each process, so it
cannot be shared. For that one code copy to run correctly in
two processes that may have loaded libc at **different virtual base addresses**
(ASLR does exactly this), the code must be **position-independent** — **PIC**
for a shared library, **PIE** for the main executable. PIC code contains **no
absolute addresses**: it reaches its own globals and its callees **PC-relative**
or **indirectly through the GOT**, so the identical instruction bytes are
correct wherever the segment lands. Non-PIC (load-time-relocated) code would
need its absolute address fields patched to the actual load address; that
patching differs per process, which would make the text pages *writable and
unshareable* — defeating the whole point.

**(b)(ii)** Copy-on-write — the **§E** mechanism. libc's data segment is mapped
**`MAP_PRIVATE`** from the file: as loaded, its pages are the file's
initialised-data bytes, identical in every process, so the OS need not give each
a private frame immediately — it backs them all with the *same* page-cache
frame, mapped **COW / read-only-shared**, and only on a process's first
**write** does the store fault and the kernel make that process a private copy.

The **GOT** is precisely what lets the code pages stay read-only and shared
while the data diverges. All the per-process, load-address-dependent patching
that `ld.so` does lands in each process's **own, writable** GOT — never in the
code. (Those relocation stores are themselves the first writes: they COW the
GOT's pages at load time, and under ASLR their contents genuinely differ per
process, since the addresses written depend on where that process's libraries
landed.) The shared text refers to globals and imported functions *through* the
GOT, so the text bytes are never modified and remain identical (hence
shareable) across every process using libc; the only thing that differs
per process is the small writable GOT/data, which COW keeps cheap. Net cost:
roughly *one* shared physical copy of the (large) code, plus only as many
private data frames as processes actually dirty.

---

*Python verification summary:* two-level translation → PA `0x00102ABC`,
`0x00201123`, and a fault at `0x00405000`; sparse table = 3 pages (12 KiB) vs
4 MiB single-level (≈341×). FIFO faults 9 (3 frames) → 10 (4 frames) = Belady's
anomaly; LRU 10→8, OPT 7→6, clock 9 (3 frames), MRU 7 (3 frames). Working set (Δ=4):
`W(4,4)={1,2,3,4}` (|W|=4), `W(8,4)={1,2,5}` (|W|=3); full sizes
`1,2,3,4,4,4,4,3,3,4,4,4`. Buddy: 60 KiB → 64 KiB block, 4 KiB internal waste.
