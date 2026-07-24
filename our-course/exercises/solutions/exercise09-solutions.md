> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 9 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** Bigger pages do shrink the table (by exactly the page-size
ratio), but ch. 20's aside is explicit: the main reason architectures support
multiple page sizes is to **reduce pressure on the TLB**, letting a program
cover more address space with one entry. The table-size route to large pages
founders on internal fragmentation, which is why the common-case page stays
small (4 KB or 8 KB).

**A2. FALSE.** The saving comes from *not allocating* pages of the table that
are entirely invalid. A dense address space has no such pages: the multi-level
table then stores everything the linear table stored *plus* the page
directory. In ch. 20's own 16 KB example, a fully-valid address space would
cost 16 pages of table under both schemes, plus one directory page under the
multi-level one. Sparsity, not structure, is where the memory goes.

**A3. FALSE.** That is what the base register meant under pure segmentation.
In the hybrid, the base register holds **the physical address of that
segment's page table**, and the bounds register holds the number of valid
pages in the segment. The segment's actual pages can then live anywhere.

**A4. TRUE.** The table's structure is consulted only on a TLB miss. Ch. 20's
control flow checks the TLB before any table access, and the chapter says it
outright: "in the common case (TLB hit), performance is obviously identical."
The structure taxes *misses* — a four-level walk makes each miss four loads
instead of one.

**A5. FALSE.** The access that faults is perfectly **legal**: the page is
valid, mapped into the address space, and merely not present in physical
memory — ch. 21's aside argues it should really be called a "page miss".
Illegal accesses are the *separate* case in the control flow: an invalid PTE
raises a segmentation fault, and no other PTE bits even matter.

**A6. FALSE.** Page faults are handled in **software on essentially all
systems**, however the TLB is managed. Ch. 21 gives two reasons: a disk-bound
fault is so slow that the overhead of running OS code is noise; and handling
a fault requires knowing about swap space and how to issue disk I/O, which
hardware does not.

**A7. TRUE.** By construction there is one entry per **physical page**, each
recording which process and which virtual page maps there. Ten processes or a
thousand, 32-bit spaces or 64-bit: the table's size tracks the machine's RAM.
The price is that lookup becomes a search (see B3d).

**A8. FALSE.** The faulting process moves to the **blocked** state, and the
OS runs other ready processes while the I/O is in flight. Ch. 21 flags this
overlap as yet another way multiprogramming extracts value from the hardware
— the CPU idles only if *nothing* is runnable.

---

## B. Table sizing, translation, and the fault path

**B1.**
**(a)** Entries: 2³² / 2¹² = **2²⁰ ≈ one million**. Size: 2²⁰ × 4 B = **4 MB**
per process. At 200 processes: **800 MB** of page tables — the number that
motivates the whole chapter.
**(b)** 16 KB pages → offset 14 bits → 2¹⁸ entries × 4 B = **1 MB**. The
factor-of-4 saving mirrors the factor-of-4 page growth exactly. Paid for
with **internal fragmentation**: allocations are rounded up to 16 KB, and
memory fills with barely-used pages.
**(c)** Reach = entries × page size. 64 × 4 KB = **256 KB**; 64 × 4 MB =
**256 MB**. A thousand-fold reach improvement against a factor-4 table
saving in (b): large pages are a **TLB** remedy. (A program with a working
set over 256 KB TLB-misses constantly at 4 KB pages; the same TLB covers a
quarter-gigabyte at 4 MB.)

**B2.**
**(a)** PTEs per page: 4096 / 8 = **512 = 2⁹**, so each level consumes
**9 bits** of VPN. VPN = 48 − 12 = 36 bits = 4 × 9, so **four levels**, and
the top-level directory (2⁹ entries × 8 B = 4 KB) itself exactly fills one
page. Split: `9 | 9 | 9 | 9 | 12`.
**(b)** One load from each of the four levels, then the data access:
**5 memory accesses total, 4 of them page-table accesses.**
**(c)** One leaf page of PTEs covers 2⁹ × 4 KB = **2 MB** of virtual address
space, so each region needs one leaf page (assuming it doesn't straddle a
2 MB boundary — state this). Pages needed: 1 top-level, then per region one
page at each of the three lower levels: 1 + 3 × 3 = **10 pages = 40 KB**.
The linear table for the same machine: 2³⁶ PTEs × 8 B = 2³⁹ B = **512 GB** —
larger than most machines' RAM, which is why 64-bit machines *cannot* use
linear tables at all.
**(d)** Take: TLB probe always paid; data access 50 ns; a miss adds four
table accesses at full memory cost.

```
EAT = 5 + 50 + 0.05 × (4 × 50)
    = 5 + 50 + 10  =  65 ns
```

(Equivalently 5 + 0.95×50 + 0.05×(200+50) = 65 ns.) Assumptions worth
stating: page-table accesses go to memory (no caching of table entries), no
page faults, probe and access not overlapped. Any clearly-stated consistent
convention earns full credit.
**(e)** Each successful simulator lookup makes **two** table references —
one into the page directory (indexed by the high VPN bits, based at the
PDBR), one into the selected page of the page table (indexed by the low VPN
bits, based at the PDE's frame) — before the data access itself. Worth
noticing en route (the chapter's homework asks): however many levels, the
hardware still needs only **one register** to find the whole structure — the
base of the top level.

**B3.**
**(a)** Code 8 MB / 4 KB = 2048 PTEs → 8 KB; heap 16 MB → 4096 PTEs → 16 KB;
stack 4 MB → 1024 PTEs → 4 KB. Total **28 KB** per process — a huge win over
4 MB. But the three tables are **variable-sized objects** (any multiple of
the PTE size, set by each segment's bounds), and the OS must find
*contiguous* free memory for each. Finding room for arbitrary-sized objects
is the allocation problem of week 7, and its old enemy returns:
**external fragmentation**.
**(b)** 2³² / 2¹² = 2²⁰ frames × 8 B = **8 MB**, once, for the whole machine.
It does **not** depend on the number of processes or on how large or sparse
their address spaces are — only on physical memory.
**(c)** 8 MB / 28 KB ≈ **293 processes**. So on pure space the inverted table
only starts winning against *these* per-process tables at around three
hundred active processes — the space argument for inversion is strongest
when address spaces are huge (64-bit) or processes very numerous, which is
precisely where per-process tables balloon.
**(d)** Translation becomes a **search**: the VPN no longer indexes the
table, since entries are keyed by physical frame. A linear scan of 2²⁰
entries per miss is hopeless, so a **hash table** is built over the
structure to make the search fast; ch. 20 names the **PowerPC** as an
architecture that does this.

**B4.**
**(a)** From ch. 21's hardware control flow:
1. **Valid and present** — extract the PFN, insert into the TLB, retry the
   instruction; it now hits.
2. **Valid but not present** — raise **PAGE_FAULT**; the OS page-fault
   handler runs (the legal-but-absent case).
3. **Invalid** — raise **SEGMENTATION_FAULT**; the OS trap handler runs and
   likely terminates the process. No other PTE bits mean anything here.
**(b)** Load → TLB miss → walk finds valid, present = 0 → trap to OS →
handler reads the **disk address from the PTE** → finds a free frame (or has
one freed — see (d)) → issues the disk read; the process **blocks** and the
OS runs others → I/O completes → handler sets present = 1 and writes the
frame number into the PTE → **retry 1**: TLB miss again, but now the walk
succeeds and the translation is inserted → **retry 2**: TLB hit, data
fetched. **Two retries.** The chapter notes the OS could instead insert the
TLB entry while servicing the fault, skipping the first retry's miss — at
the cost of the handler manipulating the TLB directly.
**(c)** The page's **swap-space (disk) address**, stored in the bits that
normally hold the PFN. Natural because those bits are otherwise meaningless
when present = 0, and because the PTE is exactly what the handler already
has in hand when the fault arrives — no second lookup structure needed.
**(d)** When free frames drop **below LW = 10**, the swap daemon wakes and
evicts until **HW = 50** frames are free, then sleeps. Batching lets the OS
**cluster** the evicted pages' writes into one large sequential transfer to
swap, paying the disk's positioning cost once instead of per page. The
handler changes from evict-inline to: check for a free page; if none,
**wake the daemon and sleep**; when the daemon has freed frames it
re-awakens the handler, which then pages in as before.
**(e)** Run that fits: after the first loop touches (and zero-fills) every
page, `si`/`so` sit near zero and later loops run at memory speed, faster
than loop 0. Run beyond memory: during loop 0, `so` climbs as the daemon
pages out to make room (the fresh pages are zero-filled, not read from
disk, so `si` stays low at first); by later loops the array's *own* early
pages have been evicted, so every stride both swaps one page in and forces
one out — sustained, roughly balanced `si` and `so`, and bandwidth
collapses from memory-like to disk-like. Ch. 21's number for getting this
wrong: **10,000–100,000× slower**. Credit for any prediction that
distinguishes loop 0 from steady state and says why.

---

## C. Discussion and design critique

**C1.** The dimensions that matter, then the verdicts.

- **Space.** Radix: proportional to *used virtual address space, summed over
  processes* (B2c: tens of KB per sparse process; but pathological for huge
  dense spaces). Inverted: proportional to *physical memory only* (B3b) —
  8 B per frame is a fixed ~0.2% tax regardless of process count.
- **TLB-miss service.** Radix: a fixed number of dependent loads (four at
  48 bits), walkable by hardware; the upper levels are few, shared and hot,
  so they cache well (the simulator homework's caching question is exactly
  this point). Hashed inverted: one probe when the hash is kind, but chains
  make the tail unpredictable, and hash probes have no locality for the
  cache to exploit.
- **What each makes awkward.** Radix: nothing much at these scales — it is
  the default for a reason. Inverted: each entry names *one* (process,
  virtual page) owner for the frame, so sharing a frame between processes
  needs extra machinery; and any per-process operation (tear-down,
  enumeration) is a sweep of the whole table.

**Verdicts.** *(i) Controller:* 256 MB of RAM makes the inverted table tiny
(~512 KB), but a handful of processes makes radix tables tiny too — and radix
keeps hardware-walked misses cheap and predictable. **Radix**, with the honest
note that at this scale the decision barely matters — which is itself the
chapter's "constraints decide" point. *(ii) Server:* thousands of sparse
64-bit processes is where linear tables are impossible and per-process radix
overhead sums to perhaps tens of MB against 1 TB — negligible. Miss latency
dominates: hardware-walked radix with hot upper levels beats hash probing,
and databases lean on shared memory, which inversion handles poorly.
**Radix again — the inverted table loses even where its space advantage is
largest, because space stopped being the binding constraint.**

**What flips it:** a software-managed TLB (the walk is OS code either way, so
the hash's average-case probe count can win); virtual:physical ratios so
extreme that even sparse radix paths dominate memory; or a design brief where
per-process table memory must be strictly bounded. Marking note: the question
asks for judgement *with conditions*; an answer that picks a winner without
naming what would reverse it earns little, and an answer that notices both
configurations pick the same winner *for different reasons* is doing exactly
what the chapter's summary asks.

**C2.**
**(a)** The TLB is a cache of PTEs, and core 1 may still hold the evicted
page's translation. Core 0 can rewrite the PTE (present = 0), but core 1's
TLB will keep translating the old VPN to the reused frame — core 1 then reads
or writes **another process's data** with no fault raised. The stale entries
must be gone **before the frame is reused for anything else**; invalidating
after reuse leaves a window in which the corruption has already happened.
**(b)** The page table lives in shared memory, so core 0 can simply write to
it. Each TLB is **private to its core** — no other core can reach into it —
so removal requires interrupting the other cores and having each invalidate
the entry itself, then confirming completion. That is the **TLB shootdown**
of OSPP §8.3: an operation whose cost scales with the number of cores
interrupted, and which the evictor must *wait* for.
**(c)** The shootdown's cost is dominated by interrupting the other cores and
waiting for their acknowledgements — a cost per *round*, not per page. A
daemon evicting a batch between watermarks can invalidate the whole batch's
translations in **one shootdown round**, amortising the interruption across
dozens of pages, just as clustering amortises the disk's positioning cost.
Marking note: full credit needs the ordering constraint in (a) stated
crisply (invalidate *before* reuse), and (c) explicitly connecting batching
to per-round amortisation — "shootdowns are slow" alone is not an answer.

**C3.** The stake, from B1(c): reach 256 KB versus 256 MB — three orders of
magnitude in how much address space the TLB covers, which for a large working
set is the difference between running from the TLB and missing continuously.

- **Transparent promotion.** Every unmodified program can benefit. But the
  OS must find or create *contiguous, aligned* runs of physical memory,
  decide when a region deserves promotion, and demote when memory tightens —
  this is the complexity ch. 20's aside warns about, and the Navarro paper
  exists precisely because getting it right (promotion policy, fragmentation
  control, demotion) is a research-grade effort. The OS also silently takes
  on the **fragmentation risk**: a promoted-but-sparsely-used superpage
  wastes megabytes where a page wasted kilobytes, and the *application*
  can't see it happening.
- **Explicit request interface.** The OS's job shrinks to reserving and
  mapping large pages on demand — simple, testable, shippable. The
  complexity moves to the application: it must know its own access pattern
  and ask. The fragmentation risk also moves to the requester — an
  application that asks for 4 MB pages and uses them thinly wastes its own
  budget, visibly.

**Verdict: ship the explicit interface first.** The parties who benefit most
— the chapter names database systems — are exactly the sophisticated
applications able to ask, so the interface captures most of the value at a
small fraction of the engineering and risk. Transparent promotion is the
right *eventual* destination for a general-purpose OS, because most code
will never ask. **Condition to revisit:** when profiling shows substantial
TLB-miss time in *unmodified* applications — or when memory is abundant
enough that fragmentation waste is acceptable — the transparent machinery
starts paying for its complexity, and the explicit interface remains as its
fallback. Marking note: credit answers that quantify the stake with the
B1(c) arithmetic, place both the complexity and the fragmentation risk
under each route, and end with a conditional recommendation rather than a
preference.
