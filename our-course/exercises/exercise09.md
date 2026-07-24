# Exercise Sheet 9 — Smaller page tables, and beyond physical memory

**Attempt after Week 9.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise09-solutions.md`](solutions/exercise09-solutions.md).

**This sheet leans on:** OSTEP ch. 20–21; OSPP §8.3. §B also rehearses the
TLB/EAT arithmetic from ch. 19 (week 8).

**You will need:** Python for the OSTEP simulator
`ostep-homework/vm-smalltables/paging-multilevel-translate.py` (B2 only); a C
compiler and a Linux machine with swap configured for B4(e) — the ch. 21
measurement homework's `vm-beyondphys/mem.c` and `vmstat`.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** Architectures support large pages (e.g. 4 MB) mainly so that page
tables get smaller.

**A2.** For any process, a two-level page table uses less memory than the
linear page table it replaces.

**A3.** In the hybrid paging-and-segmentation scheme, each segment's base
register points to the start of that segment in physical memory.

**A4.** Once the TLB hits, a translation costs exactly the same under a
four-level page table as under a linear one.

**A5.** A page fault means the program touched memory it was not allowed to
access.

**A6.** On a machine with a hardware-managed TLB, the hardware also handles
page faults — it already knows how to walk the page table.

**A7.** An inverted page table's size grows with physical memory, not with the
number of processes or the size of their address spaces.

**A8.** While one process waits for a page to arrive from disk, the CPU
necessarily sits idle.

---

## B. Table sizing, translation, and the fault path

**B1. The size of the problem.**
A 32-bit virtual address space, 4 KB pages, 4-byte PTEs, one linear page table
per process.
  (a) How big is one process's page table? With 200 active processes, how much
      memory do page tables consume in total?
  (b) The pages grow to 16 KB. Recompute the table size, and name the cost
      this saving is paid for with.
  (c) The machine's TLB has 64 entries. Compute its reach (the amount of
      address space it can cover at once) with 4 KB pages, and again if a
      smart application places its working data in 4 MB large pages. Which
      problem — table size or TLB reach — do your numbers suggest large pages
      actually solve?

**B2. Designing and walking a multi-level table.**
A machine has 48-bit virtual addresses, 4 KB pages, and 8-byte PTEs.
  (a) Derive how many levels the page table needs so that *every* piece of it
      — leaf pages and directories alike — fits in one page. Show the split of
      the virtual address into fields.
  (b) On a TLB miss, how many memory accesses does one load instruction now
      cost in total? How many of those are page-table accesses?
  (c) A process uses three small regions of its address space (code, heap,
      stack), each under 2 MB, and so far apart that they share no table page
      below the top level. How much memory do its page tables occupy? Compare
      with the linear table for the same machine.
  (d) The TLB is probed in 5 ns; a memory access takes 50 ns; the TLB hits 95%
      of the time; on a miss, every page-table access is a full memory access.
      Compute the effective access time for a load, and state any assumption
      you make.
  (e) Now check your hand method: run `python3 paging-multilevel-translate.py` with
      random seeds 0, 1 and 2 and verify with `-c` (see the README alongside
      the simulator). For the simulator's two-level scheme, how many memory
      references does each successful lookup perform, and where does each go?

**B3. The rivals: hybrid and inverted.**
Same machine class as B1: 32-bit physical and virtual addresses, 4 KB pages.
  (a) Under the hybrid scheme, a process has a segment layout of: code 8 MB,
      heap 16 MB, stack 4 MB (each segment fully valid). With 4-byte PTEs,
      how much page-table memory does the process need? What does the OS now
      have to find room for that it never did with pure paging, and which old
      enemy does that reintroduce?
  (b) An inverted page table keeps one 8-byte entry per physical frame. How
      big is it, and — the point — what does its size *not* depend on?
  (c) Roughly how many processes with the layout of (a) would the system need
      before the hybrid's tables outweigh the inverted table? What does that
      tell you about when the inverted design earns its keep on space alone?
  (d) Translation with a per-process table is an indexed lookup. What does it
      become with an inverted table, and what structure do real machines
      (ch. 20 names one) add to make that fast?

**B4. The fault path, end to end.**
A machine has a hardware-managed TLB and pages swapped to disk.
  (a) A load misses in the TLB and the hardware walks to the PTE. List the
      three possible outcomes and what each one raises or does next.
  (b) The page is valid but on disk. Trace everything that happens from the
      faulting load to the load finally completing, in order. How many times
      is the instruction retried, and why does the chapter mention a way to
      make it one fewer?
  (c) The page is not in memory, so its PFN field is meaningless. What does
      the OS store there instead, and why is the PTE a natural home for it?
  (d) The OS keeps free-frame watermarks LW = 10 and HW = 50. Describe when
      the swap daemon wakes, what it does, and when it sleeps again. What disk
      efficiency does evicting in batches unlock, and how must the fault
      handler's algorithm change from the chapter's Figure 21.3 to cooperate
      with the daemon?
  (e) *Predict, then run.* Your machine has (say) 8 GB of RAM. You run the
      ch. 21 homework's `mem` with an allocation comfortably beyond free
      memory, with `vmstat 1` in a second terminal. Predict the behaviour of
      the `si`/`so` columns and of the loop bandwidth: first loop versus the
      later loops, and against a run that fits in memory. Then run it and
      reconcile your prediction (the README with `mem.c` has the details).

---

## C. Discussion and design critique

**C1. Two page-table designs, one machine budget.**
You are choosing the page-table structure for a new 64-bit machine that will
ship in two configurations: **(i)** a memory-constrained controller — 256 MB of
RAM, a handful of long-lived processes; **(ii)** a database server — 1 TB of
RAM, thousands of processes, most with large, sparse address spaces. The
candidates are a **multi-level radix table** (hardware-walked) and a **hashed
inverted table**. Compare them under each configuration — space, TLB-miss
service time, and what each design makes awkward — and give a verdict for each
machine. Then state precisely what change of constraint would flip each
verdict. (Ch. 20's summary insists the right structure "depends strongly on
the constraints of the given environment" — your job is to say *how*.)

**C2. Eviction on a multiprocessor.**
Chapter 21's fault-handling algorithm evicts a page by picking a victim frame
and reusing it. On a multiprocessor, OSPP §8.3's TLB-consistency material
shows this simple story is incomplete.
  (a) Explain the hazard: what exactly can go wrong if core 0 evicts a page
      that core 1 has recently touched, and *when* must the danger be removed
      relative to reusing the frame?
  (b) Why does making other cores' TLB entries go away require their
      cooperation, unlike updating the page table itself?
  (c) The swap daemon evicts pages in batches between watermarks. Explain what
      batching does to the cost identified in (b).

**C3. Superpages: transparent or by request?**
Ch. 20's aside notes that using multiple page sizes "makes the OS virtual
memory manager notably more complex", and that large pages are therefore
"sometimes most easily used simply by exporting a new interface to
applications to request large pages directly". You are the OS team: a database
vendor is asking for large-page support. Compare the two routes — fully
transparent promotion by the OS versus an explicit request interface — and say
which you would ship first and why. Use your B1(c) numbers to quantify what is
at stake, address who bears the complexity and who bears the fragmentation
risk under each route, and state the condition under which you would later
change your answer. (The optional Navarro paper is the record of doing it
transparently for real — worth skimming after you've formed your own view.)
