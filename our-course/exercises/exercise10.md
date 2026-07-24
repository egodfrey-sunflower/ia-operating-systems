# Exercise Sheet 10 — Replacement policies and complete VM systems

**Attempt after Week 10.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise10-solutions.md`](solutions/exercise10-solutions.md).

**This sheet leans on:** OSTEP ch. 22–23, with TLB and multi-level-table
material from ch. 19–20 (weeks 8–9) in supporting roles.

> ⚠️ **`paging-policy.py` cannot check §B2.** Its `CLOCK` policy does not sweep a
> circular hand — the source carries the comment `# hack: for now, do random` and
> picks candidate pages at random, decrementing reference bits until one is
> clear. It therefore produces a different (and non-deterministic) victim order
> from the textbook clock algorithm §B2 asks you to trace. Work §B2 on paper.

**You will need:** pen and paper only. The OSTEP simulator
`ostep-homework/vm-beyondphys-policy/paging-policy.py` is used solely to *check*
your hand-traces in §B1 and §B3 — every trace here is small enough to work, and
should first be worked, by hand.

> **Note.** Ch. 23 ships **no homework, simulators or code** — §B5 and most of
> §C below are original material. §B is trace- and arithmetic-heavy on purpose:
> replacement traces and fault-rate algebra are perennial Cambridge calculation
> archetypes, and the upstream homework does not drill them by hand.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. The justification is the
answer — a bare verdict earns nothing.*

**A1.** Because LRU uses history and FIFO does not, LRU takes at least as few
misses as FIFO on every reference stream.

**A2.** Whatever the replacement policy, giving a program more physical frames
can never increase its number of page faults.

**A3.** The OS page cache is fully associative, so conflict misses — in the
architects' three-C's sense — do not arise in it.

**A4.** With memory at 100 ns and disk at 10 ms, improving a program's hit rate
from 90% to 99.9% improves its average memory access time by roughly a factor
of a hundred.

**A5.** Evicting a clean page requires no disk I/O; evicting a dirty page
requires a write-back first.

**A6.** The VAX hardware provided no reference bit, so VMS had no way to
approximate least-recently-used behaviour.

**A7.** Under copy-on-write, a `fork()` followed immediately by `exec()` copies
almost none of the parent's address space.

**A8.** KASLR prevents user processes from reading kernel memory.

---

## B. Traces, algebra, and the VAX by the numbers

**B1. Three policies, one trace.**
A process makes the following virtual page references, with **3 physical
frames**, all initially empty:

```
1  2  3  4  2  1  4  2  4  5  4
```

  (a) Trace **OPT**, showing for each reference: hit or miss, any eviction, and
      the resulting cache state. Give the hit rate.
  (b) Do the same for **LRU** and for **FIFO**.
  (c) For each policy, also give the hit rate *modulo compulsory misses* (the
      chapter's term — ignore the first miss to each page). What does this
      second figure isolate, and why is it the fairer basis for comparing
      policies?
  (d) Check any trace you are unsure of with `paging-policy.py` (see its
      README for flags).

**B2. Clock on the same trace.**
Repeat the trace of B1 under the **clock algorithm** with a single use bit per
page. Use exactly this convention: pages are loaded with use bit = 1; a hit
sets the use bit to 1; on a miss the hand examines frames in fill order,
starting where it last stopped — a use bit of 1 is cleared and the hand
advances, a use bit of 0 selects the victim, and after replacement the hand
advances one frame.
  (a) Give the full trace and hit count.
  (b) Where does clock land relative to your B1 results, and what single piece
      of information — present in LRU, absent in clock — accounts for the gap?
  (c) Now suppose page 2 is written (not just read) at its first reference.
      Under a clock variant that prefers evicting *clean* pages, which specific
      eviction in your trace becomes more expensive to make, and what does the
      policy trade to avoid it?

**B3. Belady's anomaly, worked and explained.**
Take the chapter's stream:

```
1  2  3  4  1  2  5  1  2  3  4  5
```

  (a) Trace FIFO with **3** frames, then with **4** frames. Give both miss
      counts.
  (b) Trace LRU at both sizes. Give both miss counts.
  (c) State precisely the property LRU has and FIFO lacks that makes the
      anomaly impossible for LRU, and sketch the argument in two or three
      sentences.
  (d) Your B3(a)/(b) numbers contain a small surprise about FIFO versus LRU at
      3 frames. Identify it, and explain what feature of this particular
      stream produces it.

**B4. Fault-rate algebra.**
A machine has a 50 ns memory access time and a page-fault service time of
5 ms. (Work in nanoseconds; state any assumption you make about whether the
memory access proceeds in parallel with anything.)
  (a) Write the effective access time as a function of the page-fault rate
      `p`, and give its **slope** — how many nanoseconds each unit of fault
      probability costs. How much EAT does one extra fault per *million*
      accesses (Δp = 10⁻⁶) add?
  (b) The performance target is an effective access time of at most 75 ns.
      What fault rate does that permit? Give the answer three ways: as a
      probability, as "at most one fault per ___ accesses", and as the
      required hit rate written out as a percentage.
  (c) A new target tightens the EAT to at most 55 ns. Recompute all three
      forms. Then compare (b) and (c): the EAT target moved by well under
      2× — what happened to the permitted fault rate, and why does this make
      a bare "our hit rate is 99%, which sounds high" claim meaningless for
      paging?
  (d) Relate your formula to ch. 22's AMAT equation, and state in one sentence
      the design lesson these numbers teach.

**B5. VAX/VMS by the numbers.** *(Original — ch. 23 supplies no homework.)*
The VAX-11 has 32-bit virtual addresses and 512-byte pages. Assume 4-byte
PTEs throughout.
  (a) How many bits of offset, and how many of VPN? The top two VPN bits
      select a segment (P0 program, P1 stack, S system): how many bits index
      within a segment?
  (b) How large would a single linear page table for one process's *entire*
      address space be? How large for one 2³⁰-byte segment? Compare with the
      same machine redesigned around 4 KB pages, and state what this says
      about where VMS's page-table problem came from.
  (c) VMS attacks the problem twice over: per-segment base/bounds registers,
      and user page tables placed in kernel *virtual* memory. For each, say
      what it saves and what new cost it introduces (for the second, be
      precise about what now happens on a TLB miss to a user page).
  (d) A process has resident-set limit 3 and pages `[a, b, c]` on its FIFO
      list (`a` first-in); the global clean list holds `[x]`, the dirty list
      is empty, and `a` and `b` are clean while `c` is dirty. The process
      touches page `d` (a miss), then touches `a` again; then another process
      takes one frame from the clean list. Trace the lists after each step.
      Which of these five events cause disk I/O, and which fault is satisfied
      *without* one? What does the answer tell you about what the second-chance
      lists are actually for?

---

## C. Discussion and design critique

**C1. Global versus per-process replacement.**
Ch. 23 records the VMS designers' worry about **memory hogs**, and their
verdict that LRU "is a global policy that doesn't share memory fairly among
processes"; their segmented FIFO instead gives each process its own resident
set. Yet plain Linux replacement works over a global pool. Under what workload
and machine conditions is each of the two designs the right one? Name the
failure mode of each — what the global policy does to a well-behaved process
under a hog, and what the per-process limit wastes when there is no hog — and
state, with justification, which you would choose for a single-user laptop and
which for a multi-tenant server.

**C2.** Ch. 23 endorses mapping the kernel into every process's address space —
"the kernel appears almost as a library, albeit a protected one" — noting it
makes user–kernel data movement and kernel bookkeeping natural, and that the
construction is widely used. The same chapter later describes KPTI, which
largely abandons the construction. **Argue the strongest case against the
chapter's endorsement** — as a matter of design principle, not merely by citing
Meltdown: what did the design assume, which of this course's own protection
ideas does it strain against, and what does it cost when the assumption fails?
Then state the conditions under which the endorsement remains correct, and
what KPTI's cost profile tells you about them.

**C3.** Ch. 22 closes by recording the long-standing engineering answer to
paging pressure: replacement policy had stopped mattering much, because "the
best solution was a simple (if intellectually unsatisfying) one: buy more
memory." **Argue the strongest case against this advice.** Build the argument
from this week's own material — the AMAT arithmetic, the workloads of §22.6,
what the page cache caches, and the multi-process concerns of ch. 23 — rather
than from appeal to authority; the chapter's own one-line concession about
fast SSDs is the *start* of an answer, not the whole of one. Then be fair:
state the class of systems for which "buy more memory" was, and remains,
exactly the right call. Conclude with the conditions that decide between the
two verdicts.
