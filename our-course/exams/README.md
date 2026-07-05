# Exams

Three sit-down assessments, all closed-book and handwritten (the Tripos is;
train like you fight). The papers are `midterm1.md`, `midterm2.md` and
`final.md`, each with a spoilered `-solutions.md` (model answers + mark
scheme) — do not open a solutions file until you have sat the paper.

## Format conventions

Questions follow the Tripos house style: a 20-mark question in 3–5 parts,
ramping from bookwork ("describe…", ~4 marks) through applied ("calculate the
response time / translate this address…", ~8 marks) to open-ended design or
critique ("to what extent would this scheme work?", ~8 marks). Mark scheme
at 1 mark ≈ 1 minute ≈ one distinct point made.

## Midterm 1 — after week 6 (90 minutes, 4 questions, answer 3)

Covers weeks 1–6: OS structure, protection, processes, scheduling (IA +
extension), concurrency I.

Planned question blueprint:

1. **Protection & syscalls** — dual-mode operation bookwork; trace what
   happens from `read()` in userspace to the driver and back; why can't
   protection be done in software alone (and what if it must be — SFI).
2. **Processes** — lifecycle diagram; PCB contents; context-switch cost
   analysis: what must be saved for a process switch vs a user-level thread
   switch (ties to Lab 5).
3. **Scheduling calculation** — given a process table: draw Gantt charts and
   compute waiting/turnaround/response times under FCFS, SRTF, RR(q=2), and
   lottery-in-expectation; then MLFQ behaviour of a process that alternates
   CPU bursts with I/O.
4. **Concurrency** — identify the race in a given code fragment; fix with a
   spinlock built from test-and-set; explain when a spinlock is the *wrong*
   choice and what to use instead.

## Midterm 2 — after week 11 (90 minutes, 4 questions, answer 3)

Covers weeks 1–11, emphasis on 7–11: memory, virtual memory, I/O, files.
One question may reach back to midterm-1 material (as Tripos Q3/Q4 mix
topics).

Planned question blueprint:

1. **Address translation calculation** — given page size, address width and a
   two-level page table: translate addresses by hand, compute table space
   for a sparse address space, and the TLB-miss cost; compare against an
   inverted table.
2. **Replacement policies** — run a reference string through FIFO, LRU,
   clock, OPT; exhibit Belady's anomaly; working set and thrashing essay
   part; why does COW fork (Lab 4) interact with the replacement policy?
3. **I/O** — polling vs interrupts vs DMA control-flow pseudo-code; when is
   polling *faster*; buffering strategies; a throughput/latency calculation
   for a disk vs an SSD.
4. **File systems** — V7 inode arithmetic (max file size with the Lab 6
   doubly-indirect extension); hard vs soft links and metadata placement;
   critique a metadata-in-the-directory-entry design (CP/M/FAT-style, no
   inode table) — what it simplifies and what it breaks. (The
   hardlink-snapshot critique in the style of IA sheet 3 Q6(b) is reserved
   for the final.)

## Final — after week 17 (the revision week) (3 hours)

Comprehensive: 7 questions, **Q1 (the sealed Tripos paper y2026p2q3) is
compulsory** (~35 minutes), plus any three of Q2–Q7 — four answers in total.
Coverage blocks: (i) Unix & shell internals, (ii) crash consistency,
(iii) concurrency, (iv) virtualization & kernel architecture, (v) design
synthesis, (vi) memory & I/O — with Q7 (memory & I/O) guaranteeing the IA
core is always in the optional pool. CPU scheduling is examined by the two
midterms (and possibly by the compulsory Q1), not by the final's own
optional pool. Likewise protection theory (dual-mode operation, syscall
mechanics, ACLs vs capabilities, covert channels) is examined by Midterm 1,
and reaches the final only indirectly — via the compulsory Q1 or the essay
prompts — not as an optional question of its own.

Planned question blueprint:

1. **Tripos transplant** — y2026p2q3 (sealed until the exam) as the single
   compulsory question, ~35 minutes, marked against the real IA standard.
   (Its sealed partner y2026p2q4 is *not* in this paper — it is sat separately
   as the week-17 timed standalone mock; see the mock-exam protocol below.)
2. **Unix case study** — shell internals: given a command line with pipes and
   redirection, enumerate the exact syscall sequence; signals and process
   groups; what the versioned-hardlink-snapshot scheme (IA sheet 3 Q6(b))
   gets right and wrong, and how modern CoW filesystems solve it properly.
3. **Crash consistency** — order the disk writes for an append under
   data-journaling vs metadata-journaling ext3; what fsck can and cannot
   repair; LFS: how reads, writes, and cleaning work, and when it wins/loses
   vs FFS.
4. **Concurrency II** — condition variables (diagnose and fix a
   subtly-wrong signalling discipline); deadlock: the four
   conditions, an avoidance calculation (banker's-style), and why RCU
   sidesteps reader-side locking.
5. **Virtualization** — trap-and-emulate requirements; shadow vs nested page
   tables (a translation-cost calculation); containers: which kernel
   mechanisms provide isolation, resource control and the filesystem image,
   and what a container *cannot* isolate that a VM can.
6. **Design/synthesis essay** — a choice of two: (A) the "pendulum" essay
   (functionality swings into the kernel for performance and out for safety —
   trace it through monolithic Unix, microkernels, exokernels, unikernels,
   eBPF, and argue where it settles), or (B) design an OS for a specified
   unusual target (thousands of cores; no MMU; formally-verified control
   system) justifying each departure from the Unix design. (The
   microkernel-vs-hypervisor essay lives on sheet 11 as practice; the final
   deliberately uses fresh prompts.)
7. **Memory & I/O (IA core)** — TLB/EAT calculation; a replacement or
   working-set part; a disk-vs-SSD design part — guarantees the IA core in
   the optional pool with fresh numbers.

## Grading bands (all exams)

- 75%+ = comfortable first: applied parts fully correct, design parts show
  judgement (trade-offs, not just lists).
- 60–75% = solid: bookwork and calculations correct, design parts thin.
- 50–60% = passable: targeted revision of the weak questions before moving
  on.
- <50%: revisit the corresponding examples sheets before continuing.

## Mock-exam protocol

Before each exam, one full timed dry run using papers you have not seen:

- **Midterm 1** (dry run in week 5): y2016p2q3 and y2017p2q3 — both
  scheduling (process states and SRTF; the CPU–I/O burst cycle and
  pre-emption), squarely within weeks 1–5 material.
- **Midterm 2** (dry run at the end of week 11; the paper itself is sat at
  the start of week 12): y2017p2q4 and y2016p2q4 — paging/TLB arithmetic and
  file systems (directory lookup costs, inode arithmetic), matching the
  paper's weeks 7–11 emphasis.
- **Final** (week 17): the held-back memory papers y2023p2q3, y2024p2q4 and
  y2025p2q4, plus the sealed y2026p2q4 as the timed standalone mock. These
  four are reserved exclusively for week 17 — nothing else assigns them.
  One honesty note: y2024p2q4's part (d) (emulating referenced/dirty bits
  via the page-fault handler) is drilled by sheet 6 D(d), so that part
  measures revision recall rather than fresh performance — weight your
  self-assessment accordingly.

Mark yourself against the published IA solution notes where available; be
harsh on prose parts — "would a supervisor accept this sentence as a made
point?" Recent Tripos OS papers do not reliably have published solutions
(this includes the final's compulsory Q1): where none exist, self-mark
against the examiners' report remarks if available, otherwise against this
course's own grading bands and the relevant sheet answer keys — and mark
deliberately harshly.

The two sealed y2026 papers serve different purposes: **y2026p2q4** is the
week-17 timed standalone mock (35 minutes, marked to the IA standard);
**y2026p2q3** is the final's compulsory Question 1. Open neither before its
time.
