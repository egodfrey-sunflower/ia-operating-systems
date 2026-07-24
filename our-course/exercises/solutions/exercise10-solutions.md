> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 10 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** History is a bet, not a guarantee. This sheet's own B3 provides
a counter-example: on the stream `1 2 3 4 1 2 5 1 2 3 4 5` with 3 frames, FIFO
takes 9 misses and LRU takes 10. Nearly-cyclic streams are LRU's worst case —
ch. 22's looping-sequential workload drives LRU (and FIFO) to a 0% hit rate
while Random does measurably better.

**A2. FALSE.** True only for **stack algorithms** (LRU, OPT), whose size-N
cache contents are always a subset of their size-N+1 contents. FIFO has no such
inclusion property and can suffer **Belady's anomaly**: more frames, more
faults (worked in B3).

**A3. TRUE.** A conflict miss is an artefact of set-associativity — a hardware
limit on *where* an item may be placed. The OS page cache can place any page in
any frame; it is fully associative, so only compulsory and capacity misses
remain. (Ch. 22's "Types Of Cache Misses" aside makes exactly this point.)

**A4. TRUE.** This is the chapter's own worked example. AMAT = T_M + P_Miss ×
T_D: at 90%, AMAT = 100 ns + 0.1 × 10 ms ≈ 1 ms; at 99.9%, AMAT = 100 ns +
0.001 × 10 ms ≈ 10.1 µs — roughly 100× better. The disk term dominates so
completely that the miss rate *is* the performance.

**A5. TRUE.** A clean page's contents already exist on disk (or can be
regenerated), so the frame can simply be reused; a dirty page must be written
back first. This is why the hardware dirty/modified bit exists and why
clock variants prefer unused-*and*-clean victims.

**A6. FALSE — twice over.** VMS's **segmented FIFO** with global clean/dirty
second-chance lists approximates LRU without any reference bit — the bigger the
lists, the closer to LRU. And ch. 23's aside shows reference bits can be
*emulated*: mark pages inaccessible, take the protection trap as evidence of
use, revert the protection (Babaoglu & Joy's trick).

**A7. TRUE.** COW maps the parent's pages read-only into the child instead of
copying; physical copies happen only on write faults. A child that immediately
calls `exec()` writes almost nothing before its address space is replaced, so
almost nothing is ever copied — this is precisely why COW matters for the
`fork()`/`exec()` idiom.

**A8. FALSE.** KASLR randomizes *where* the kernel sits; what stops user code
reading kernel memory is the protection machinery — privileged mappings, and
after Meltdown, KPTI's removal of most kernel mappings from user page tables.
KASLR's job is to frustrate attacks (like ROP) that need to know addresses; it
is secrecy of layout, not inaccessibility of contents — and Meltdown showed
layout secrecy alone is a thin defence.

---

## B. Traces, algebra, and the VAX by the numbers

**B1.**
**(a) OPT — 6 hits, 5 misses, hit rate 54.5%.**

| Ref | H/M | Evict | State |
|-----|-----|-------|-------|
| 1 | Miss | — | 1 |
| 2 | Miss | — | 1,2 |
| 3 | Miss | — | 1,2,3 |
| 4 | Miss | **3** (never used again) | 1,2,4 |
| 2 | Hit | | 1,2,4 |
| 1 | Hit | | 1,2,4 |
| 4 | Hit | | 1,2,4 |
| 2 | Hit | | 1,2,4 |
| 4 | Hit | | 1,2,4 |
| 5 | Miss | **1** (or 2 — neither recurs; tie) | 2,4,5 |
| 4 | Hit | | 2,4,5 |

**(b) LRU — 5 hits, 6 misses, 45.5%.** Evictions: 1 (at the miss on 4), 3 (at
the miss on 1), 1 again (at the miss on 5). The re-fetch of page 1 is LRU's
one avoidable miss: it evicted 1 as least-recent just before 1 was needed.

| Ref | H/M | Evict | State (LRU → MRU) |
|-----|-----|-------|-------|
| 1 | Miss | — | 1 |
| 2 | Miss | — | 1,2 |
| 3 | Miss | — | 1,2,3 |
| 4 | Miss | 1 | 2,3,4 |
| 2 | Hit | | 3,4,2 |
| 1 | Miss | 3 | 4,2,1 |
| 4 | Hit | | 2,1,4 |
| 2 | Hit | | 1,4,2 |
| 4 | Hit | | 1,2,4 |
| 5 | Miss | 1 | 2,4,5 |
| 4 | Hit | | 2,5,4 |

**FIFO — 3 hits, 8 misses, 27.3%.** Evictions in order: 1, 2, 3, 4, 1. FIFO
throws out page 2 while it is hot (it was merely *oldest*), then pays to
re-fetch it — the chapter's core complaint that FIFO "can't determine the
importance of blocks".

| Ref | H/M | Evict | State (first-in → last-in) |
|-----|-----|-------|-------|
| 1 | Miss | — | 1 |
| 2 | Miss | — | 1,2 |
| 3 | Miss | — | 1,2,3 |
| 4 | Miss | 1 | 2,3,4 |
| 2 | Hit | | 2,3,4 |
| 1 | Miss | 2 | 3,4,1 |
| 4 | Hit | | 3,4,1 |
| 2 | Miss | 3 | 4,1,2 |
| 4 | Hit | | 4,1,2 |
| 5 | Miss | 4 | 1,2,5 |
| 4 | Miss | 1 | 2,5,4 |

**(c)** Five distinct pages → five compulsory misses. Hit rate modulo
compulsory = hits ÷ (hits + non-compulsory misses):
**OPT 6/6 = 100%** (zero avoidable misses), **LRU 5/6 = 83%** (one),
**FIFO 3/6 = 50%** (three). The compulsory misses are the same for every
policy — no policy can avoid the first touch — so removing them isolates
exactly the thing the policy controls: the **avoidable** misses. The raw hit
rates (54.5 / 45.5 / 27.3%) understate how close LRU is to perfect here and
how far FIFO is.
**(d)** No model answer — the simulator should reproduce the tables above.

**B2.**
**(a) Clock — 4 hits, 7 misses.** Convention as stated; • marks use bit = 1;
the hand position is given after each miss.

| Ref | H/M | Evict | State / use bits |
|-----|-----|-------|-------|
| 1 | Miss | — | 1• |
| 2 | Miss | — | 1• 2• |
| 3 | Miss | — | 1• 2• 3• |
| 4 | Miss | 1 | 4• 2 3 — hand swept 1,2,3 clearing all bits, wrapped to 1 (now 0), evicted it |
| 2 | Hit | | 4• 2• 3 |
| 1 | Miss | 3 | 4• 2 1• — hand at 2: cleared; at 3: bit 0, evicted |
| 4 | Hit | | 4• 2 1• |
| 2 | Hit | | 4• 2• 1• |
| 4 | Hit | | 4• 2• 1• |
| 5 | Miss | 4 | 5• 2 1 — hand swept 4,2,1 clearing, wrapped to 4, evicted |
| 4 | Miss | 2 | 5• 4• 1 — hand at 2: bit 0, evicted |

**(b)** Clock lands between FIFO (3 hits) and LRU (5 hits): 4 hits. The missing
information is **ordering**. LRU knows the full recency *order* of the resident
pages; the use bit collapses that order to one bit — "touched since the hand
last passed" or not. Whenever several pages have bit 0 (or all had bit 1 and
were mass-cleared), clock chooses by hand position, i.e. essentially FIFO,
which is where its extra misses come from.
**(c)** Page 2 dirty makes clock's **final eviction (of 2)** the expensive one:
the frame cannot be reused until 2 is written back. A clean-preferring variant
scans first for pages with use bit 0 *and* clean, so at that miss it would
evict page 1 (clean, bit 0) instead. The trade: it avoids a synchronous
write-back by tolerating a worse *replacement* choice — keeping a dirty page
that is in fact never referenced again, and evicting a clean one on no better
evidence. The policy is trading I/O cost against prediction quality.

**B3.**
**(a) FIFO: 9 misses with 3 frames, 10 misses with 4 frames.** More memory,
more faults — Belady's anomaly, on the exact stream Belady's group published.
(3 frames: hits on the **third** 1 and the **third** 2 — positions 8 and 9 — plus the final 5. 4 frames: after the four
compulsory misses and hits on 1 and 2, the stream `5 1 2 3 4 5` evicts each
page just before re-use — every remaining reference misses.)
**(b) LRU: 10 misses with 3 frames, 8 with 4.** Monotone improvement, as a
stack algorithm must give.
**(c)** LRU has the **stack property**: at every instant, the size-N cache's
contents are a subset of the size-(N+1) cache's contents. This holds because
LRU's contents are defined by the reference stream alone — the N most recently
used pages — and the N most-recent are always a subset of the N+1 most-recent.
Inclusion means any hit at size N is also a hit at size N+1, so misses cannot
increase with capacity. FIFO's contents depend on its own past eviction
decisions, which differ *between* capacities, so no inclusion holds and the
anomaly becomes possible.
**(d)** The surprise: **FIFO beats LRU at 3 frames** (9 vs 10 misses). The
stream is nearly cyclic, and cyclic reuse at a period wider than the cache is
LRU's worst case: LRU's recency bet keeps the just-re-hit pages 1 and 2 and
evicts 5 — which is exactly what the tail `3 4 5` then punishes. FIFO, blind
to the hits on 1 and 2, leaves 5's queue position alone, and 5 survives to
score the final hit. History helps only when the past predicts the future;
here it anti-predicts.

**B4.** Assume every access pays the 50 ns memory cost, with the fault service
added on faults (the AMAT convention of eq. 22.1). 5 ms = 5 × 10⁶ ns.
**(a)** EAT(p) = 50 + p × 5×10⁶ ns (strictly 50(1−p) + p(5×10⁶ + 50); the
−50p is negligible and stating the approximation is part of the answer).
Slope ≈ **5 × 10⁶ ns per unit of fault probability** — the fault-service /
memory-access gap is the multiplier on everything. Δp = 10⁻⁶ adds
5×10⁶ × 10⁻⁶ = **5 ns**: one extra fault in a million accesses costs a tenth
of an entire memory access time, on *every* access on average.
**(b)** The fault budget is 75 − 50 = 25 ns ⟹ p ≤ 25 / 5×10⁶ = **5 × 10⁻⁶**:
at most **one fault per 200,000 accesses**, a hit rate of **99.9995%**.
**(c)** Budget = 55 − 50 = 5 ns ⟹ p ≤ 5 / 5×10⁶ = **10⁻⁶**: one fault
per **1,000,000 accesses**, hit rate **99.9999%**. The EAT target tightened
only 1.36× (75 → 55 ns) but the permitted fault rate fell **5×** — the
50 ns memory floor is untouchable, so only the headroom above it was ever
available to faults, and the new target keeps just a fifth of that headroom
(25 ns down to 5 ns). And as percentages, 99.9995 and 99.9999 look
interchangeable while permitting 5× different fault rates: hit-rate
percentages compress exactly the digits that carry the performance. Fault
talk must be in faults-per-N-accesses (or counted nines), never a rounded
percentage — which is why "99% sounds high" is meaningless: at p = 10⁻²,
EAT = 50 + 10⁻² × 5×10⁶ = 50,050 ns ≈ **50 µs** — nearly seven hundred
times the (b) target.
**(d)** This is eq. 22.1, AMAT = T_M + (P_Miss × T_D). Lesson: EAT is linear
in the fault rate with a slope of ~10⁵ × T_M — so the fault rate, which the
replacement policy controls, dominates everything else; no plausible
improvement in T_M can buy back what the last decimal place of the hit rate
gives away.

**B5.**
**(a)** 512 = 2⁹ → **9 offset bits**, leaving **23 VPN bits**; two select the
segment, so **21 bits** index within a segment.
**(b)** Whole space: 2²³ PTEs × 4 B = 2²⁵ B = **32 MB per process** — absurd on
machines of the era (and still absurd: it is per-process overhead). One 2³⁰
segment: 2²¹ × 4 B = **8 MB**. Redesigned with 4 KB pages: 2³²⁻¹² = 2²⁰ PTEs ×
4 B = **4 MB** for the whole space — 8× smaller, because table size scales
inversely with page size. The arithmetic locates the problem exactly where
ch. 23 does: the **512-byte page**, "chosen for historical reasons", is what
made VMS's tables monstrous.
**(c)** **Per-segment base/bounds:** the bounds register caps each segment's
table at the number of pages actually used, so the table costs O(pages in use),
not O(address-space size), and the unused gulf between heap (P0) and stack (P1)
costs nothing at all. New cost: two tables and two register pairs per process
to manage, and each table must still be *contiguous* and grown as its segment
grows. **Page tables in kernel virtual memory:** the tables themselves become
swappable — under pressure the kernel can page out page tables, reclaiming
physical memory. New cost: on a TLB miss to a user page, the hardware may
first have to translate the *page table's own* virtual address via the system
page table (in physical memory) before it can read the user PTE — a two-step
walk, and potentially a nested fault if the table page is swapped out. The
TLB exists precisely so this laborious path is usually skipped.
**(d)** Stepwise:

1. **Touch `d` (miss):** process is at its RSS, so first-in `a` is evicted;
   `a` is clean → appended to the clean list: `[x, a]`. `d` is read from disk —
   **disk I/O**. FIFO now `[b, c, d]`.
2. **Touch `a` (miss):** `a` is still on the clean list → **reclaimed with no
   disk I/O** (a "soft" fault: just re-map the frame). The process is at RSS
   again, so first-in `b` is evicted; clean → clean list `[x, b]`. FIFO
   `[c, d, a]`.
3. **Other process allocates:** takes the head of the clean list, `x`. No I/O
   (`x` was already clean; a page arriving via the *dirty* list would have
   been written back by the clustered writer before becoming reclaimable).

Only event 1 does disk I/O. That is the design's whole point: **the
second-chance lists are a victim cache**. Per-process FIFO is a crude policy
that evicts good pages; the global lists make those evictions *provisional*,
so FIFO's mistakes are forgiven at re-map cost instead of disk cost. The
bigger the lists, the more mistakes are caught — which is exactly why ch. 23
says segmented FIFO approaches LRU as the lists grow.

*Marking note (B5): the arithmetic in (a)–(b) must show working; in (d), full
credit requires identifying step 2 as I/O-free and saying why — a trace that
merely shuffles list contents without the "victim cache" observation misses
the question's point.*

---

## C. Discussion and design critique

**C1.** The axis is **isolation versus utilisation**.

- **Global replacement** maximises whole-machine hit rate: frames flow to
  whichever pages are hottest, nothing is idle. Its failure mode is the one
  the VMS designers named — a **memory hog** (or just a large sequential job)
  flushes every other process's resident pages; a well-behaved interactive
  process pays the hog's bill in latency, and nothing in the policy prevents
  it. There is no fault isolation between workloads.
- **Per-process limits** (VMS's RSS) buy isolation: a hog can only thrash
  itself. The failure mode is stranded memory — a process whose working set
  exceeds its RSS thrashes *while free frames sit idle* inside other
  processes' allowances; and sizing RSS is guesswork that must be re-done as
  workloads change.

**Laptop:** global. One principal, cooperative workloads, and the machine-wide
hit rate is the user's experience; per-process limits would only strand memory.
**Multi-tenant server:** per-tenant limits. Isolation is the product — one
tenant's scan must not become every tenant's latency — and the operator would
rather waste some memory than sell unpredictable performance. What flips the
verdict is the trust structure of the workload mix, not the hardware: the
moment co-resident workloads are mutually adversarial (or just mutually
indifferent), fairness stops being free and must be enforced by policy.

*Marking note: credit requires naming both failure modes concretely. Answers
that just say "global is efficient, local is fair" without the stranded-memory
and hog mechanisms earn little.*

**C2.** The strongest case against the endorsement, argued from principle:

- **It rests on a single mechanism with no fallback.** The construction places
  the kernel's code and data *inside* every untrusted address space and
  interposes exactly one defence: the per-page privilege check. The design
  assumption is that this check is a perfect information barrier — not merely
  that it denies access, but that denied accesses reveal *nothing*. That is a
  much stronger property than the architecture ever promised, and the course's
  own framing since ch. 2 — protection through isolation — argues for not
  making reachability the default in the first place. What is not mapped
  cannot be probed; what is mapped is one mechanism-failure away from being
  read.
- **The benefit was convenience, the exposure was total.** What the mapping
  buys is real but modest — cheap user-pointer dereference in syscalls, no
  table switch on kernel entry. What it stakes is the kernel's entire memory,
  including every other process's data resident in kernel structures. An
  asymmetric bet: small recurring saving against catastrophic tail risk.
- **When the assumption failed, the cost came back with interest.** Meltdown
  demonstrated the leak (via speculative side effects), and the retrofit —
  KPTI — pays a page-table switch on *every* kernel crossing, plus the TLB
  consequences, applied as an emergency patch to systems designed around the
  opposite assumption. A design that had kept unrelated kernel data unmapped
  would have had a smaller emergency and a cheaper fix.

**When the endorsement stands:** on hardware where the privilege check is not
subvertible by microarchitectural side channels (in-order, non-speculative
cores — many embedded parts), or where kernel memory holds nothing the
resident processes may not see (single-application appliances). And KPTI's
cost profile — proportional to kernel-entry rate — identifies exactly the
workloads for which the construction was most valuable: syscall-heavy ones.
The honest conclusion is conditional: map the kernel where the hardware's
barrier deserves the trust, and only the minimal trampoline where it does not
— which is precisely the point KPTI landed on.

*Marking note: the question demands an argument beyond "Meltdown happened."
Full credit needs the single-mechanism/defence-in-depth observation (or an
equivalent principled objection), the asymmetry of benefit versus exposure,
and honest conditions for the other side. A pure recitation of §23.2's KPTI
paragraphs earns little.*

**C3.** A strong answer builds four independent objections, then concedes
fairly.

- **The AMAT slope makes "enough memory" a cliff, not a slope.** EAT is linear
  in miss rate with slope T_D/T_M ≈ 10⁵ (B4). Buying memory helps only by
  moving the miss rate; if the working set still does not fit — and one
  workload class always exists whose working set exceeds any RAM you can buy —
  the machine still runs "at the rate of the disk". Conversely §22.6's curves
  are flat in places: for the looping workload, LRU gains *nothing* from more
  frames until the cache holds the entire loop, then everything. Spending on
  memory has threshold economics; a scan-resistant policy improves matters at
  every size.
- **Replacement never stopped running, because of the page cache.** The advice
  quietly equates "replacement" with "swapping anonymous memory". But ch. 23's
  page cache means every byte of file I/O competes for the same frames:
  relative to the contents of storage, memory is *always* oversubscribed, on
  any machine, at any RAM size. Linux's 2Q lists exist because a single large
  cyclic file scan defeats plain LRU regardless of how much memory you bought
  — the fix is policy (confine the scan to the inactive list), not capacity.
- **Multi-process fairness is a policy property, not a capacity property.**
  VMS's memory-hog problem does not go away with bigger memory; the hog grows
  to fill it. Isolation between competing processes (C1) has to be enforced by
  the replacement design — RSS limits, second-chance lists — and no amount of
  purchased DRAM supplies it.
- **The performance ratios shifted under the advice.** The chapter's own
  concession, extended: paging to a fast SSD shrinks T_D by orders of
  magnitude, which re-balances AMAT so that moderate miss rates are survivable
  — and then *which* pages miss (the policy's choice) becomes visible in
  performance again rather than being hidden behind a uniform catastrophe.
  "Renaissance in page replacement" is the chapter's phrase for exactly this.

**The concession:** for a single machine whose total working set demonstrably
*can* fit at feasible cost — the common desktop and workstation case for two
decades of cheap DRAM — every curve in §22.6 converges to 100% once everything
fits, all policies become equivalent, and a memory purchase eliminates the
problem outright for less than the engineering cost of being clever. There,
"buy more memory" was and is exactly right.

**Deciding conditions:** (i) can the working set fit at acceptable cost — if
yes, buy; (ii) is the workload scan-heavy or cyclic — if yes, policy matters
at *every* memory size; (iii) is memory shared between mutually indifferent
workloads — if yes, policy must supply the isolation capacity cannot; (iv) how
fast is the backing store — the faster it is, the more the choice of victim,
rather than the raw miss count, is what shows.

*Marking note: full credit requires the AMAT/threshold argument made
quantitatively and the page-cache observation — those two are the spine. The
SSD line alone (the chapter's own words handed back) earns a single point.
The steelman is compulsory: an answer that refuses to concede the fitting-
working-set case has not engaged with why the advice was given.*
