> # ⚠️ DO NOT OPEN UNTIL YOU HAVE SAT THE PAPER ⚠️
> ## Final exam solutions — Q2, Q3, Q4 only.
> Reading this before the sitting converts the only unseen final you will ever
> get into a re-sit. Close the file.
>
> **There is no solution for Q1 (`y2026p2q3`), because nobody has read it.**
> The question is sealed; no model answer exists or can exist until you sit
> it. Mark Q1 yourself afterwards, per the protocol in
> [`final.md`](final.md).

---

# Final — Solutions and Mark Scheme (Q2–Q4)

These are the three held-back Tripos questions, answered from the course's
material. General guidance as everywhere in this course: 1 mark ≈ one distinct
made point; method carries most calculation marks; a bare number earns few.

---

## Q2 — `y2023p2q3` (2023 Paper 2 Q3)

### (a) Interrupts and traps [4 marks]

**(i) Interrupt handling in a modern UNIX-like OS [2]:** a device raises an
interrupt line; the CPU finishes the current instruction, switches to kernel
mode, saves the interrupted context (PC, status, registers — the trap frame),
and vectors through the interrupt/trap table to the handler the kernel
registered for that IRQ. The handler acknowledges the device, does the minimal
urgent work (move data, mark a request complete, wake any process blocked on
it), defers anything long to later (bottom-half/softirq-style processing so
interrupts stay masked only briefly), and returns-from-interrupt, restoring
the saved context. The interrupted process is resumed — or, if the handler
made a higher-priority process runnable, the scheduler may switch instead.

**(ii) Why traps are "software interrupts" [2]:** a trap (system call) rides
**exactly the same hardware mechanism** — a vectored, mode-switching transfer
into a kernel-chosen handler with the same context save and the same
privileged return path. The only difference is the cause: an interrupt is
raised **asynchronously by hardware**, a trap **synchronously by an
instruction** the program executes deliberately. Same machinery, software
origin — hence the name.

### (b) EAT with page faults [12 marks]

Let p be the page-fault rate per memory access. Model:
EAT = (1 − p) × 80 ns + p × (fault service). Since 80 ns ≪ 8 ms, the
approximation EAT ≈ 80 + p × service is accurate to a fraction of a
nanosecond — state it and use it.

**(i) Maximum permitted fault rate [4]:**
80 + p × 8,000,000 ns ≤ 100 ns → p ≤ 20 / 8 × 10⁶ = **2.5 × 10⁻⁶** — one
fault per **400,000** accesses.
(Exact form: p ≤ 20/7,999,920 = 2.50003 × 10⁻⁶ — the same answer to three
significant figures; either earns full marks with working.)

**(ii) At double that rate [4]:** p = 5 × 10⁻⁶:
EAT = 80 + 5 × 10⁻⁶ × 8 × 10⁶ = 80 + 40 = **120 ns**.

**(iii) Half the faults find no free frame [4]:** the 8 ms figure assumed a
free frame; when there is none, a victim must first be written out — another
disk operation of the same order, so that fault costs 8 + 8 = **16 ms**
(state the assumption). Mean service = ½ × 8 + ½ × 16 = **12 ms**. At the
same p = 5 × 10⁻⁶:
EAT = 80 + 5 × 10⁻⁶ × 12 × 10⁶ = 80 + 60 = **140 ns**.

*Marking notes:* the marks are for the EAT model stated once and applied three
times, and for the (iii) assumption made explicit (eviction ≈ one extra
full-cost disk write before the read). Common errors: dropping the (1−p) term
*without saying so* (fine if said, sloppy if silent); in (iii), averaging
8 and 16 into "12 ms per fault" but then forgetting the rate is still the
doubled one from (ii).

### (c) The evict-two scheme [4 marks]

The proposal: whenever a fault must evict, evict **two** frames, so the next
fault finds a frame free. Two system-wide implications, stated and discussed
(2 each; any two well-argued points — one benefit and one cost makes the
strongest answer):

- **It shifts eviction I/O off the critical path — a free-frame pool.** The
  fault that pays the double eviction buys the *next* fault the cheap 8 ms
  path: page-outs happen ahead of need, and the two write-backs can be issued
  together (better disk scheduling, amortized positioning). This is exactly
  the watermark/paging-daemon idea: keep a reserve of free frames so fault
  latency stays at the read-only cost. Expected fault *latency* improves.
- **It evicts a page nobody asked to lose — resident memory shrinks.** The
  second victim may be in active use; evicting it manufactures a *future*
  fault that would never have happened, raising the fault **rate** — and the
  (b) arithmetic shows EAT is exquisitely sensitive to the rate (each
  10⁻⁶ of p costs ~8–12 ns here). Under memory pressure the scheme can behave
  like a resident-set tax: more disk traffic, more faults, the opposite of
  what it promised. Whether it wins depends on victim choice quality and on
  how bursty faults are.

*Marking note:* "two implications" wants **system-wide** effects (latency vs
rate, disk traffic, effective memory size), not restatements of the mechanism.
Naming the watermark daemon by analogy is creditable but not required.

---

## Q3 — `y2024p2q4` (2024 Paper 2 Q4)

### (a) Address binding [3 marks]

- **Compile time [1]:** the compiler emits **absolute** addresses, which
  requires knowing, at compile time, exactly where in physical memory the
  program will reside; it must be loaded there and nowhere else, and moving it
  means recompiling.
- **Load time [1]:** the compiler emits **relocatable** code; when the
  program is brought into memory the loader picks a base and **fixes up**
  every address once. No special hardware needed — but once loaded, the
  process cannot be moved.
- **During execution [1]:** binding is deferred to run time: hardware
  translation (base-and-bounds or paging MMU) maps every reference as it
  issues, so the OS can move, swap, and grow the process. Requires MMU
  hardware and OS management of the mappings.

### (b) OPT, LRU, FIFO over `1 2 3 2 4 3 5 1 3 2 3 4`, 3 frames [9 marks]

**(i) OPT [3]** — evict the page whose next use is farthest away:

| Ref | Result | Evict | Frames after |
|----:|--------|-------|--------------|
| 1 | miss | — | 1 |
| 2 | miss | — | 1 2 |
| 3 | miss | — | 1 2 3 |
| 2 | hit | — | 1 2 3 |
| 4 | miss | 2 (next uses: 1@8th, 2@10th, 3@6th → 2 farthest) | 1 3 4 |
| 3 | hit | — | 1 3 4 |
| 5 | miss | 4 (1@8th, 3@9th, 4@12th → 4 farthest) | 1 3 5 |
| 1 | hit | — | 1 3 5 |
| 3 | hit | — | 1 3 5 |
| 2 | miss | 1 or 5 (neither used again — tie; either accepted) | 2 3 5 |
| 3 | hit | — | 2 3 5 |
| 4 | miss | any never-reused page | 2 3 4 |

**7 misses, 5 hits.**

**(ii) LRU [3]** (state most-recent first):

| Ref | Result | Evict | State (MRU→LRU) |
|----:|--------|-------|------------------|
| 1 | miss | — | 1 |
| 2 | miss | — | 2 1 |
| 3 | miss | — | 3 2 1 |
| 2 | hit | — | 2 3 1 |
| 4 | miss | 1 | 4 2 3 |
| 3 | hit | — | 3 4 2 |
| 5 | miss | 2 | 5 3 4 |
| 1 | miss | 4 | 1 5 3 |
| 3 | hit | — | 3 1 5 |
| 2 | miss | 5 | 2 3 1 |
| 3 | hit | — | 3 2 1 |
| 4 | miss | 1 | 4 3 2 |

**8 misses, 4 hits.**

**(iii) FIFO [3]** (queue oldest-first; hits do not touch the queue):

| Ref | Result | Evict | Queue (oldest→newest) |
|----:|--------|-------|-----------------------|
| 1 | miss | — | 1 |
| 2 | miss | — | 1 2 |
| 3 | miss | — | 1 2 3 |
| 2 | hit | — | 1 2 3 |
| 4 | miss | 1 | 2 3 4 |
| 3 | hit | — | 2 3 4 |
| 5 | miss | 2 | 3 4 5 |
| 1 | miss | 3 | 4 5 1 |
| 3 | miss | 4 | 5 1 3 |
| 2 | miss | 5 | 1 3 2 |
| 3 | hit | — | 1 3 2 |
| 4 | miss | 1 | 3 2 4 |

**9 misses, 3 hits.**

*Marking notes:* the expected ordering OPT 7 ≤ LRU 8 ≤ FIFO 9 is itself a
sanity check — a script with LRU beating OPT contains an error somewhere.
The classic slips: OPT evicting 3 at the fifth reference (3 is used *sooner*
than 1 and 2 — read the future, not the past); LRU forgetting that the hit on
2 (fourth reference) refreshes 2's recency, so the fifth reference evicts 1,
not 2; FIFO treating the hit on 2 as touching the queue. Per algorithm: 2 for
a correct trace with evictions shown, 1 for the count.

### (c) Bélády's anomaly [4 marks]

**Statement [1]:** under FIFO, *increasing* the number of frames can
*increase* the number of page faults for the same reference string — more
memory, worse performance.

**Why OPT and LRU are immune [2]:** both are **stack algorithms**: they
maintain the **inclusion property** — at every instant, the set of pages held
with k frames is a subset of the set held with k + 1 frames. (For LRU the set
with k frames is always "the k most recently used pages"; for OPT, "the k
best pages to hold given the future"; both are prefix-closed by definition.)
If contents at k are always contained in contents at k + 1, every hit at k is
a hit at k + 1, so faults can only decrease as frames grow — the anomaly is
impossible.

**Why FIFO is not [1]:** FIFO's contents are determined by *arrival order*,
not by a ranking over pages: with different frame counts the eviction
sequences diverge and the smaller memory's contents need not be a subset of
the larger's. With inclusion broken, adding a frame can evict differently and
lose hits the smaller memory happened to keep.

### (d) Emulating reference and dirty bits [4 marks]

Use the protection machinery to make the hardware *tell you* about accesses,
by making them fault:

- **Reference bit [2]:** keep the page resident but mark its PTE **invalid**
  (or protection-deny). The first touch traps; the fault handler recognises
  this as a *soft* fault — the page is in memory — records "referenced" in
  its own software table, marks the PTE valid, and resumes the instruction.
  Periodically the OS re-invalidates pages to re-arm sampling — exactly what
  clock's bit-clearing sweep does, done in software.
- **Dirty bit [2]:** map the page **read-only** even where the segment is
  writable. The first store traps; the handler records "dirty" in software,
  makes the PTE writable, and resumes. From then on writes proceed at full
  speed; the software dirty bit is consulted at eviction to decide on
  write-back (and cleared/re-protected after cleaning).

Cost: one extra trap per page per sampling period — the VAX/VMS position
(week 10) is the historical proof this is workable when hardware omits the
bits.

*Marking note:* the answer must *resume after recording* — a scheme that
leaves the page invalid (faulting forever) or read-only (trapping every
write) misses the re-arm/one-trap structure that makes emulation affordable.

---

## Q4 — `y2025p2q4` (2025 Paper 2 Q4)

Machine: 64-bit architecture, **57-bit** virtual addresses, five-level page
table, 64-bit (8-byte) PTEs, 4096-byte pages.

### (a) Addressable memory [2 marks]

In the SI units the question asks for: 2⁵⁷ B ≈ 1.44 × 10¹⁷ B = **144 PB**.
The binary form, 2⁵⁷ = 2⁷ × 2⁵⁰ = **128 PiB**, is creditable as an
alternative with working — the point is the peta scale — but the question
says SI, and SI prefixes are decimal, so 144 PB is the answer as asked.

### (b) Translating `0x00c0.ffee.ba5e.f00d` [9 marks]

**The split [2]:** 57 = **12 offset bits** (4096-byte page) + **45 VPN bits**
= five groups of **9 index bits** (one per level). Each level's table is
indexed by 9 bits → 2⁹ = **512 entries** × 8 B = **4,096 B — exactly one
page**, which is the design invariant that fixes both the fan-out (512) and
the number of levels (⌈45/9⌉ = 5).

**The indices [3]** — take the address's bits 56..12 in 9-bit groups
(top first), offset = bits 11..0:

| Field | Bits | Value |
|-------|------|-------|
| Level-5 index | 56–48 | 0x0C0 = 192 |
| Level-4 index | 47–39 | 0x1FF = 511 |
| Level-3 index | 38–30 | 0x1BA = 442 |
| Level-2 index | 29–21 | 0x1D2 = 466 |
| Level-1 index | 20–12 | 0x1EF = 495 |
| Offset | 11–0 | 0x00D |

**The walk [3]:** the page-table base register (the CR3 analogue) holds the
physical address of the level-5 table. At each level i: PTE address =
(table base) + 8 × (level-i index); the PTE's frame number, shifted left
12 bits, is the base of the next level's table. So: entry 192 of the L5 table
→ L4 table; entry 511 of it → L3 table; entry 442 → L2 table; entry 466 → L1
table; entry 495 holds the **data page's** frame number F. Physical address =
(F << 12) | 0x00D. Five memory accesses for the walk, plus the data access
itself.

**Full size of the page table [1]:** fully populated, level i (counting the
leaf as level 1) needs 512⁵⁻ⁱ tables of 4 KiB: 1 + 512 + 512² + 512³ + 512⁴
tables ≈ 6.9 × 10¹⁰ tables — dominated by the leaf level's 512⁴ × 4 KiB =
2⁴⁸ B = **256 TiB**; the whole tree is ≈ **256.5 TiB** (512 GiB + 1 GiB +
2 MiB + 4 KiB above it). That absurd total is the *point* of the multi-level
design: a real process allocates only the paths to its mapped regions, a few
tables per region. (An answer giving "5 × 4 KiB = 20 KiB per translation
path, allocated on demand" alongside the fully-populated figure shows exactly
the right understanding; the fully-populated figure is what "full size"
asks for.)

*Marking note:* the reliable slips are byte-indexing (forgetting the × 8 when
locating the PTE), taking the 9-bit groups from the wrong end, and sizing a
level in entries but calling it bytes. The per-level "512 entries = 4 KiB =
one page" sentence is explicitly asked for — leaving it implicit costs marks.

### (c) Effective access time [4 marks]

TLB search 5 ns, memory access 40 ns, hit rate 99%. A hit costs 5 + 40 =
45 ns. A miss costs the search, the five-level walk (5 × 40 = 200 ns), then
the data access: 5 + 200 + 40 = 245 ns.

EAT = 0.99 × 45 + 0.01 × 245 = 44.55 + 2.45 = **47 ns**.

(Equivalently 5 + 40 + 0.01 × 200 = 47 ns. State the assumption: walk
accesses are ordinary uncached memory accesses; TLB probe paid always.)
**[formula/miss-cost 2, evaluation 2]**

### (d) The binary trie proposal [5 marks]

**Worse — decisively.** The five-level radix table **is already a trie**: a
trie over the 45 VPN bits consumed in 9-bit digits, giving fan-out 512 and
depth ⌈45/9⌉ = 5, with each node sized to exactly one page. **[1]**

- The proposal's "log₂(N) lookup" is not an improvement — it is the same
  quantity with a worse base. With N = 2⁴⁵ addressable pages, log₂ N = **45**:
  a binary trie walks **45 levels**, i.e. ~45 dependent memory accesses per
  TLB miss against the radix table's 5 — roughly **9× worse** on the walk,
  turning (c)'s 200 ns miss penalty into ~1,800 ns. **[2]**
- Space and locality also degrade: a binary node holds two 8-byte pointers
  (16 B) — 256 nodes fit where one radix node packed 512 next-pointers — so
  the same mappings cost more nodes, more cache lines, and 45 dependent
  pointer chases that defeat prefetching; the radix node's 512 entries sit in
  one page and its top levels cache superbly. **[1]**
- The confusion to name: log₂(N) is an *asymptotic in the wrong variable* —
  page-table lookup is not a search over the *occupied* mappings but an
  indexed walk over address **bits**; the engineering question is bits **per
  level**, and more bits per level (bounded by keeping a node to one page) is
  strictly fewer memory accesses. **[1]**

(The exam-day correction allowed "trie" to be read as "tree"; an answer
comparing against a binary *search* tree over mapped pages reaches the same
verdict — depth ~log₂ of millions of mappings ≈ 20+ dependent accesses, plus
comparisons and rebalancing — and was marked equivalently.)

---

*End of final solutions (Q2–Q4). Q1 has no solutions — see the banner.*
