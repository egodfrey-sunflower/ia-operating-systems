# Exercise Sheet 8 — Paging and TLBs

**Attempt after Week 8.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise08-solutions.md`](solutions/exercise08-solutions.md).

**This sheet leans on:** OSTEP ch. 18–19; xv6 book ch. 3.

**You will need:** the OSTEP `paging-linear-translate.py` simulator from
`ostep-homework/vm-paging/` (§B1 only), and a C compiler with access to a
cycle- or microsecond-resolution timer for §B5 (`gettimeofday()` suffices —
the same harness you built for the ch. 6 measurement homework in week 3).

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** Paging eliminates fragmentation.

**A2.** During address translation, the page table translates the virtual page
number, and the offset is translated by a separate hardware unit.

**A3.** A TLB entry's valid bit and a page-table entry's valid bit mean the
same thing.

**A4.** On a software-managed TLB, the return-from-trap at the end of the
TLB-miss handler resumes at the instruction after the one that missed, just as
a system call's return-from-trap does.

**A5.** Since the page tables in memory are unchanged by a context switch, the
TLB's contents remain correct across one.

**A6.** Larger pages mean a smaller page table and better TLB coverage, so a
system should use the largest page size the hardware offers.

**A7.** The x86 TLB is filled by the operating system's TLB-miss handler.

**A8.** LRU is the best replacement policy for a TLB, since it exploits
locality.

---

## B. Translation, sizing, and measurement

**B1. Linear translation by hand, then by simulator.**
A machine has a 16-bit virtual address space with 4 KB pages, and 128 KB of
physical memory. The page table for the running process contains:
VPN 0 → PFN 7, VPN 1 → PFN 12, VPN 5 → PFN 3, VPN 15 → PFN 31; every other
entry is invalid.
  (a) How many bits of VPN and how many bits of offset? How many entries does
      the linear page table hold, and how many bits are needed for a PFN?
  (b) Translate each of the following virtual addresses, giving the physical
      address in hex, or the outcome if translation fails:
      `0x1234`, `0x5FFF`, `0x4123`, `0xF000`.
  (c) Check your method against the simulator, e.g.
      `python3 paging-linear-translate.py -P 1k -a 16k -p 32k -v -u 50 -s 1`
      (then `-c` to verify). The parameters are smaller than in (a)–(b); that
      is fine — you are checking the *procedure*, not the numbers.
  (d) In the simulator runs, what happens to the fraction of valid entries as
      `-u` falls? State, in one sentence, what property of real address spaces
      this models, and which PTE bit makes a linear table survive it.

**B2. The space bill.**
  (a) A 32-bit virtual address space uses 4 KB pages and 4-byte PTEs. Compute
      the size of one linear page table, and the total for 200 running
      processes.
  (b) Recompute (a) for 16 KB pages. The table shrank — what did you pay for
      that, and where does the cost show up? (One sentence; ch. 18's homework
      asks exactly this.)
  (c) Recompute (a) for a full 64-bit virtual address space with 4 KB pages and
      8-byte PTEs. What do you conclude about linear tables on 64-bit machines?

**B3. Locality in a TLB trace.** *(Fresh instance of ch. 19's §19.2 example.)*
A tiny machine has 16-byte pages. An array of twelve 4-byte integers starts at
virtual address 60. A loop reads `a[0]` through `a[11]` once each, in order.
The TLB starts empty and is large enough to hold every translation. Ignore
instruction fetches and other variables.
  (a) For each access, say hit or miss, and give the resulting hit rate.
  (b) Recompute the hit rate with 32-byte pages. What general claim about page
      size and spatial locality does the change illustrate?
  (c) The loop now runs a second time, immediately. What is the hit rate on the
      second pass, which kind of locality is responsible, and what condition on
      the TLB must hold for your answer?
  (d) Give a memory-access pattern for which an LRU TLB of `n` entries misses
      on *every* access even though the program touches only `n + 1` distinct
      pages, and name the policy ch. 19 suggests to sidestep such behaviour.

**B4. The effective access time of translation.**
A machine has a 50 ns memory access time, a 5 ns TLB lookup, and a linear page
table in physical memory. A TLB hit therefore costs one memory access after the
lookup; a miss costs two (PTE fetch, then the data).
  (a) Write the effective access time (EAT) as a function of hit rate `h`, and
      evaluate it at `h = 98%`.
  (b) What hit rate is needed to keep EAT at or below 60 ns?
  (c) The vendor switches to a software-managed TLB whose miss handler adds
      400 ns of trap-and-handler overhead on top of the memory accesses. Redo
      (b). What does the answer tell you about where engineering effort must go
      on such machines? (Ch. 19 names two mechanisms that protect exactly this
      path — name one.)

**B5. Measuring your own TLB (design before you run).**
Ch. 19's homework has you write `tlb.c`: touch one `int` per page across
`NUMPAGES` pages, loop many times, and time the average access as `NUMPAGES`
grows.
  (a) Sketch the curve you *predict* — axes, shape, and what feature of the
      machine each jump or plateau reveals.
  (b) `gettimeofday()` has microsecond resolution. Explain how that constrains
      the experiment's design, and the standard remedy.
  (c) Name two ways the experiment can silently produce garbage — one caused by
      the compiler and one caused by the scheduler — and the countermeasure for
      each.
  (d) The first pass over a freshly-allocated array is much slower than later
      passes for a reason unrelated to the TLB. What is it, and how does your
      harness avoid contaminating the measurement?
  Now write and run `tlb.c`, and compare reality with your prediction from (a).

---

## C. Discussion and design critique

**C1. What would you measure?**
After a kernel upgrade, a server that context-switches heavily between two
processes gets measurably slower. Engineer A says: *"the upgrade flushes the
TLB on every switch — the refill cost afterwards is what's killing us."*
Engineer B says: *"switches got more expensive in themselves — register
save/restore and scheduler overhead dominate; the TLB is a red herring."*

Design the experiment, or small set of experiments, that settles this. Be
concrete: what program(s) you run, what you vary, what you time, and what
controls you apply. You have the tools this course has already built — a
timing harness (week 3), pipes to bounce control between two processes at a
known rate (week 2), and this week's `tlb.c` methodology. State the *predicted
outcome under each hypothesis*, so that the measurement can actually
distinguish them, and finish with the conditions under which each engineer
would be right.

**C2. Hardware- or software-managed?**
You are on the team defining the memory system of a new ISA. The stated
constraint: the ISA will live for thirty years, and the OS teams that target it
want freedom to redesign their page-table formats over that lifetime. Compare a
hardware-managed TLB (x86-style: hardware walks a fixed-format table) with a
software-managed one (MIPS-style: a miss traps to the OS) *under that
constraint*, covering miss cost, hardware complexity, the failure modes the OS
must now guard against, and what history suggests. Say which you would choose,
and — precisely — what change in the constraint or the workload would flip
your choice.

**C3. Against the guard page.**
xv6 guards each stack against overflow by two different means. Below a
**kernel** stack it leaves an **unmapped** guard page (its PTE invalid, `PTE_V`
clear), so an overflow faults and the kernel panics — its authors write that
"a panic crash is preferable" to silently overwriting adjacent kernel memory.
Below a **user** stack it instead maps a page with the user-access bit (`PTE_U`)
cleared, so a user-mode overflow faults on it. Make the strongest case *against*
that preference for a hard fault — there is at least one serious alternative for
user stacks, which xv6's own chapter mentions — and then state the conditions
under which xv6's choice is nonetheless the right one for a kernel's own stacks.
