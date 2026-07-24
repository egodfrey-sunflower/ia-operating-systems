> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 8 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** Paging eliminates **external** fragmentation by construction —
fixed-size units mean a free frame always fits a page. **Internal**
fragmentation remains: the last page of any region is partly wasted, and the
waste grows with page size. "Fragmentation" without a qualifier is the trap.

**A2. FALSE.** The offset is not translated by anything. Only the VPN is looked
up; the offset names a byte *within* the page and is copied unchanged into the
low bits of the physical address. This is precisely why pages must be
contiguous, aligned units of physical memory.

**A3. FALSE — ch. 19 flags this as a common mistake.** A PTE marked invalid
means the process never allocated that page: an access is illegal, traps, and
likely kills the process. A TLB entry marked invalid merely means no
translation is cached in that slot — the normal state after boot or a flush,
and no comment at all on the legality of any access.

**A4. FALSE.** A system call's return-from-trap resumes at the *next*
instruction — the syscall completed. A TLB-miss handler must re-execute the
instruction that *caused* the trap, so it succeeds on retry; the hardware must
therefore save a different PC depending on what kind of trap occurred.

**A5. FALSE.** Correctness of the page tables is not the issue — *which* page
table a cached translation came from is. TLB entries map VPNs of the old
process; the new process uses the same VPNs for different frames. Either the
TLB is flushed on the switch (correct but pay full refill), or entries carry an
address-space identifier and the hardware matches on ⟨ASID, VPN⟩.

**A6. FALSE.** Larger pages do shrink the table and stretch TLB coverage, but
internal fragmentation grows in proportion — xv6's chapter gives the honest
example of handing a 4 MB superpage to a program using 8 KB. Page size is a
trade-off; the pragmatic design is a small default with large pages reserved
for large, randomly-accessed structures (ch. 19 names DBMSs).

**A7. FALSE.** x86 is the standard example of a **hardware-managed** TLB: on a
miss the hardware itself walks the fixed-format page table pointed to by CR3
and refills the TLB with no OS involvement. OS-filled TLBs are the
**software-managed** design (MIPS R4000, SPARC v9), where a miss raises an
exception and a trap handler does the work.

**A8. FALSE.** Ch. 19's own counter-example: a loop cycling over `n + 1` pages
with an `n`-entry LRU TLB misses on *every* access — LRU always evicts exactly
the entry needed next. Random replacement does much better there. No policy is
"best" independent of the reference stream; LRU is merely a good default when
locality is genuine.

---

## B. Translation, sizing, and measurement

**B1.**
**(a)** 4 KB pages ⇒ 12 offset bits; 16-bit VA ⇒ **4 VPN bits**, so the linear
table has **2⁴ = 16 entries**. 128 KB physical = 2¹⁷ bytes = **32 frames**, so
a PFN needs **5 bits**.
**(b)** Split each address as ⟨VPN | 12-bit offset⟩:

| VA | VPN | offset | PTE | PA |
|----|-----|--------|-----|-----|
| `0x1234` | 1 | `0x234` | PFN 12 = `0xC` | `0xC000 + 0x234` = **`0xC234`** |
| `0x5FFF` | 5 | `0xFFF` | PFN 3 | `0x3000 + 0xFFF` = **`0x3FFF`** |
| `0x4123` | 4 | `0x123` | **invalid** | trap to the OS (segmentation fault); the process is likely killed |
| `0xF000` | 15 | `0x000` | PFN 31 = `0x1F` | **`0x1F000`** |

**(c)** No fixed answer — the check is that your hand procedure (mask off the
offset, shift down, index the table, concatenate PFN and offset) reproduces the
simulator's translations, including the invalid ones.
**(d)** As `-u` falls, most PTEs are invalid — modelling that real address
spaces are **sparse**: code and heap at one end, stack at the other, a hole in
between. The **valid bit** is what lets a linear table represent the hole
without backing it with physical frames. (The table itself still spends an
entry per hole page — the space problem week 9 addresses.)

**B2.**
**(a)** 32-bit VA, 12-bit offset ⇒ 2²⁰ PTEs × 4 B = **4 MB per process**;
× 200 processes = **800 MB** of translations. State both numbers.
**(b)** 16 KB pages ⇒ 14-bit offset ⇒ 2¹⁸ PTEs × 4 B = **1 MB**. The payment
is **internal fragmentation**: every partly-used page now wastes up to 16 KB
rather than up to 4 KB, and the waste sits inside allocated regions where no
allocator can reclaim it.
**(c)** 64-bit VA ⇒ 2⁵² PTEs × 8 B = 2⁵⁵ B = **32 PiB per process**. A linear
table is not merely wasteful there — it is physically unrealisable, so 64-bit
machines *must* use a different page-table structure. (Ch. 20, next week, is
exactly that story; xv6's three-level Sv39 tree is one answer you have already
seen in code.)

**B3.**
**(a)** With 16-byte pages, `a[i]` sits at address 60 + 4i:

| access | address | page | outcome |
|--------|---------|------|---------|
| a[0] | 60 | 3 | **miss** |
| a[1] | 64 | 4 | **miss** |
| a[2]–a[4] | 68–76 | 4 | hit ×3 |
| a[5] | 80 | 5 | **miss** |
| a[6]–a[8] | 84–92 | 5 | hit ×3 |
| a[9] | 96 | 6 | **miss** |
| a[10]–a[11] | 100–104 | 6 | hit ×2 |

4 misses, 8 hits: **hit rate 8/12 ≈ 67%**.
**(b)** With 32-byte pages the array spans pages 1 (32–63), 2 (64–95) and
3 (96–127): misses at `a[0]`, `a[1]`, `a[9]` only — **9/12 = 75%**. Doubling
the page size lets **spatial locality** amortise each miss over more accesses:
one compulsory miss per page touched, and fewer pages touched.
**(c)** **12/12 = 100%** — **temporal locality**: every translation was cached
on the first pass. Condition: the TLB must hold all four translations
simultaneously and nothing evicted them in between (TLB capacity ≥ the loop's
page working set).
**(d)** Stride through pages `0, 1, …, n` and repeat: `0, 1, …, n, 0, 1, …`.
With an `n`-entry LRU TLB, the entry evicted at each step is exactly the one
needed soonest, so every access misses despite only `n + 1` distinct pages.
Ch. 19's suggestion: **random replacement**, valued for avoiding precisely
such corner cases.

**B4.**
**(a)** Hit: 5 + 50 = 55 ns. Miss: 5 + 50 (PTE fetch) + 50 (data) = 105 ns.

```
EAT(h) = h·55 + (1−h)·105  =  55 + (1−h)·50 ns
```

At h = 98%: 55 + 0.02 × 50 = **56 ns** — 12% over raw memory speed, which is
why ch. 19 says TLBs "in a real sense make virtual memory possible".
**(b)** 55 + (1−h)·50 ≤ 60 ⇒ (1−h) ≤ 0.1 ⇒ **h ≥ 90%**.
**(c)** Miss now costs 5 + 400 + 50 + 50 = 505 ns, so EAT = 55 + (1−h)·450.
For 60 ns: (1−h) ≤ 5/450 ⇒ **h ≥ 98.9%**. The lesson: on a software-managed
TLB the miss *rate* must be driven near zero and the handler itself must be
bulletproof-fast — which is why such machines keep the handler where it cannot
itself TLB-miss: in unmapped physical memory, or covered by **wired** TLB
entries reserved for the OS (either mechanism earns the mark).

**B5.**
**(a)** A staircase. x-axis: pages touched (log scale helps); y-axis: average
ns per access. Flat and cheap while the pages fit the first-level TLB; a jump
where its capacity is exceeded (the step's *position* reveals the L1 TLB entry
count, its *height* the L1-miss cost); a second plateau if a second-level TLB
exists; a final rise to full page-walk cost when that too is exceeded. The
chapter's own worked run shows ~5 ns up to ~8–16 pages, ~20 ns to ~512, and
~70 ns past ~1024 — evidence of a two-level TLB.
**(b)** One access costs nanoseconds; the timer resolves microseconds. A single
measurement is pure noise, so the loop must run enough total accesses (hundreds
of millions) that the elapsed time is seconds, then divide. In general: the
measured interval must be several orders of magnitude larger than the timer's
resolution.
**(c)** *Compiler:* the loop's results are never used, so the optimiser may
delete the loop entirely — defeat it by making the result observable (print the
accumulated sum, or qualify the array `volatile`). *Scheduler:* the thread may
migrate between CPUs, each with its own TLB hierarchy, corrupting the trace —
**pin** the thread to a single CPU for the run. Both are in the chapter's own
homework questions.
**(d)** The first touch of each page pays one-off allocation costs — notably
**demand zeroing** of freshly-allocated memory — which have nothing to do with
TLB geometry. Touch the whole array once *before* starting the timed passes
(a warm-up pass), so the steady-state runs measure only translation.

---

## C. Discussion and design critique

**C1.** A strong answer has four parts: an instrument, a variable, predictions
stated *before* the run, and controls.

**The instrument.** Two processes ping-pong a byte over a pair of pipes —
each round trip forces two context switches at a known rate, and total time /
iterations gives cost per round trip. Between receiving and replying, each
process strides one `int` per page across `W` pages (`tlb.c`'s access
pattern). Everything here was built in weeks 2–3 plus this week.

**The variable.** Sweep `W` from 1 to well past the TLB capacity measured in
§B5. Also run a *no-switch control*: the identical page-striding loop in a
single process, same `W`, no pipes — subtracting it isolates the
switch-induced excess from the cost of touching pages at all.

**The predictions.** Under A (flush/refill dominates), the switch-induced
excess *grows with `W`* — every page touched since the last switch must be
re-faulted into the TLB after each flush — rising roughly linearly and then
flattening once `W` exceeds TLB capacity (beyond that, the loop evicts its own
entries anyway, so the flush adds nothing extra). Under B (fixed switch cost
dominates), the excess is *flat in `W`*. The shape of one curve distinguishes
the hypotheses; that is what makes this an experiment rather than a debate.
Running the same sweep on the old kernel makes it decisive: a regression whose
delta grows with `W` convicts the flush; a `W`-independent delta convicts the
switch path itself.

**The controls.** Pin both processes to one core (a real switch per ping-pong,
and no cross-CPU TLB confusion); warm-up pass before timing (demand zeroing);
iterations sized for timer resolution; repeated trials with variance reported.

**The conditions.** A is right for workloads with working sets that are large
but within TLB reach, switching frequently — and A's mechanism disappears
entirely on hardware using ASIDs, so "does the new kernel still tag entries?"
is the first question to ask. B is right when working sets are tiny (little to
refill) or switches are rare.

*Marking note: the discriminating power lives in "grows with `W`" versus "flat
in `W`". An answer that measures only total slowdown, with no variable that
separates the hypotheses, has designed a benchmark, not an experiment, and
earns little. Credit the no-switch control and pinning explicitly.*

**C2.** The constraint — thirty years of OS freedom over page-table format —
is the whole question, because the two designs differ exactly there.

**Hardware-managed** bakes the table's format into the ISA: the walker is
circuitry, so every future OS on every future implementation must lay out its
translations the way the first silicon expected (x86's fixed multi-level walk
via CR3 is the canonical case). Misses are handled without any trap — faster,
typically by a large factor — and the OS cannot get the machinery wrong.
**Software-managed** asks the hardware only to raise an exception; the OS owns
the format outright (ch. 19: the primary advantage is *flexibility*, the
second *simplicity* — the hardware does almost nothing). The price: every miss
pays trap-and-handler cost (§B4c showed how punishing that is), the
return-from-trap must retry the faulting instruction rather than the next one,
and the handler must be arranged never to TLB-miss itself — unmapped physical
placement or wired entries, a real and recurring source of kernel subtlety.

Under the stated constraint, **software-managed** is the defensible choice:
it converts a hardware contract into an OS implementation detail, which is
precisely what a thirty-year horizon wants. What flips it: a workload profile
with intrinsically high miss rates (sparse, huge working sets), where §B4's
algebra says handler cost dominates everything; or a market where single-thread
performance and a stable binary ecosystem outweigh OS experimentation — which
is, historically, roughly what happened, since the hardware-managed x86 line
absorbed RISC techniques and thrived. A middle path worth credit: a hardware
walker for a *defined* format plus an architected escape to software for
anything else.

*Marking note: the judgement must be conditional on the constraint, not a flat
verdict. Answers reciting CISC-vs-RISC history without connecting miss cost to
the EAT algebra, or without naming the infinite-miss/wired-entry hazard, are
incomplete.*

**C3.** The strongest case against: for **user** stacks the guard-page fault is
a *recoverable* event, and treating it as fatal wastes the very mechanism the
page table provides. The trap identifies the faulting address; if it lies just
below the current stack, the obvious response — which xv6's own chapter
concedes a real-world OS would make — is to allocate another page, map it, and
resume the process, invisibly growing the stack on demand. Killing (or worse,
panicking the whole machine for) a process that did nothing wrong except
recurse deeply converts a routine condition into unavailability;
"crash preferable to corruption" is a false dichotomy when the fault is
precisely detectable and fixable.

When xv6's choice is right: for the **kernel's own stacks**, and for a teaching
kernel generally. A kernel-stack overflow means kernel invariants may already
be violated — growing the stack from inside the exception path is delicate
(the handler itself needs stack), and continuing risks exactly the silent
corruption the guard page exists to prevent; fail-fast with a clean panic is
what production kernels do for kernel stacks too. And in a system whose
purpose is to be understood, a loud, immediate, located failure is worth more
than availability. The honest summary: guard page *detection* is right
everywhere; what you do with the fault should differ between user stacks
(grow) and kernel stacks (panic).

*Marking note: full credit requires separating user from kernel stacks — the
question's "at least one serious alternative" is demand-growth, and the
conditions part is where the marks are. An answer that only attacks or only
defends the claim misses the judgement being trained.*
