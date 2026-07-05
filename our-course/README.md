# Operating Systems: Principles and Implementation

A self-study course based on the Cambridge CST Part IA *Operating Systems* course
(Lent, Dr Martin Kleppmann), extended with material that Cambridge defers to
Part IB (*Concurrent and Distributed Systems*) and beyond: concurrency, crash
consistency, virtualization, and kernel architecture research.

The IA course is 12 lectures of *concepts*. This course keeps that conceptual
core intact (so the Cambridge examples sheets and Tripos past papers remain
usable as assessment), but adds two things:

1. **Depth** — modern schedulers (CFS/EEVDF), page-table hardware, allocators,
   journaling and log-structured file systems, virtualization internals.
2. **Implementation** — a lab track built around **xv6-riscv** (MIT's teaching
   reimplementation of Unix V6/V7 — the same lineage as the IA Unix case study),
   so every major concept is something you have *built*, not just described.

## Prerequisites

- C programming (pointers, structs, bit manipulation). If rusty, do Lab 0 first
  and read K&R or CS:APP ch. 1–3 alongside.
- Basic computer architecture (registers, memory hierarchy, interrupts at the
  level of the IA Digital Electronics / Intro to Computer Architecture courses).

## Course structure

19 weeks: 16 teaching weeks, a revision week (17) ending in the final exam,
and two dedicated project weeks (18–19) — Lab 8's 25–30 hours get the time
they actually cost. Weeks 1–4, 7–11 and 13 track the IA syllabus
closely; weeks 5–6, 12 and 14–16 are the extension material. The pace targets
roughly 12–14 hours/week all-in (reading + sheet + lab); the chunky labs
(3, 4, 5) are deliberately spread over two to three weeks each.

Each week: do the reading (the table gives the gist; `reading-list.md` has the
full per-week assignment), work the examples sheet if one is due, and put in
your 4–6 lab hours.

| Week | Topic | Reading (chapters — see reading-list.md) | Sheet due | Lab |
|------|-------|--------------------------------------|-----------|-----|
| 1 | Introduction: what an OS is; layering, multiplexing, mechanism vs policy; booting; OS evolution | OSTEP 1–2, 53–55 (security); A&D ch. 1; Saltzer & Schroeder §I + MS Learn/Julia Evans (VMs vs containers) | — | Lab 0: toolchain + Unix warm-up |
| 2 | Protection: dual-mode operation, traps, system calls; kernels vs microkernels; VMs and containers (overview); access matrix, ACLs, capabilities | OSTEP 4–6; A&D ch. 2; microkernel overview (Liedtke read-ahead; Silberschatz §2.8 optional); ★ the Unix paper; Wahbe §1–3; Lampson '73 (covert channels) | 1 | Lab 1: build a shell |
| 3 | Processes: lifecycle, PCB, context switch, IPC; the process as an abstraction | xv6 book ch. 1; Kerrisk ch. 24–27, 44; A&D ch. 3 (IPC & sockets); ★ Lampson's *Hints*; (optional) Multics paper, "A fork() in the road" | 2 | Lab 1 (continued) |
| 4 | Scheduling: CPU–I/O burst cycle; FCFS, SJF, SRTF, priority, RR; MLFQ | OSTEP 7–8; xv6 book ch. 2 & 4; A&D ch. 7 | 3 | Lab 2: xv6 system calls |
| 5 | **Scheduling beyond IA**: lottery and stride scheduling, Linux CFS/EEVDF, multiprocessor scheduling, real-time (RM/EDF) | OSTEP 9–10; ★ lottery paper; Liu & Layland §1–3; CFS→EEVDF articles; xv6 book ch. 7 | 4 | Lab 3: xv6 scheduler |
| 6 | **Concurrency I**: threads (kernel vs user), race conditions, mutual exclusion, hardware primitives (test-and-set, CAS), spinlocks; priority inversion | OSTEP 25–29; xv6 book ch. 6; A&D §5.7, §6.1–6.3, §6.6 (multiprocessor locks, MCS, non-blocking); Preshing on memory reordering + lock-free/ABA; "What Really Happened on Mars" | 5 | Lab 3 (continued) |
| — | **MIDTERM 1** (weeks 1–6; timed dry run in week 5 — see exams/README.md) | | | |
| 7 | Memory I: address binding, logical vs physical addresses, partitions, fragmentation, segmentation, address translation; **static/dynamic linking, ELF, `ld.so`** | OSTEP 13, 15–16; Drysdale LWN (how programs run / ELF) + CS:APP ch. 7 (linking) | — | Lab 3 wrap-up (report); Lab 4 begins |
| 8 | Memory II: paging, free-space management, page tables, TLBs, multi-level tables | OSTEP 17–20; xv6 book ch. 3 | 6 (§§B–C) | Lab 4 (continued) |
| 9 | Virtual memory: demand paging (vs. demand segmentation), replacement (FIFO, OPT, LRU, clock/NRU, LFU/MFU/MRU), working set, thrashing; **plus**: copy-on-write, mmap, buddy & slab allocators | OSTEP 21–24; working-set paper; Belady & slab papers (skim); Chris Down "In defence of swap" (overcommit/OOM) | 6 (rest) | Lab 4 (continued: COW) |
| 10 | I/O subsystem: polling vs interrupts vs DMA; block/char devices; blocking, non-blocking, asynchronous, vectored I/O; buffering, caching, spooling; **plus**: disks vs SSDs, I/O scheduling, io_uring | OSTEP 36–37, 44; io_uring intro; Silberschatz §12.3–12.4 optional ref (buffering/spooling) | 7 | Lab 5: threads & locks |
| 11 | File management: file abstraction, metadata, directories, hard/soft links, access & concurrency control; Unix V7 file system on-disk layout, inodes | OSTEP 39–41; xv6 book ch. 8; ★ FFS paper; optional FreeBSD ch. 7 (VFS/vnode) | 8 | Lab 5 (continued) |
| — | **MIDTERM 2** (weeks 1–11, emphasis 7–11; dry run end of week 11, paper sat at the start of week 12) | | | |
| 12 | **Concurrency II**: condition variables, semaphores, monitors, classic problems; deadlock (prevention, avoidance, detection — incl. the banker's algorithm); lock-free basics and RCU | OSTEP 30–34; A&D §6.5 (deadlock/banker's); RCU intro; Bendersky "Basics of Futexes" | 9 (Concurrency II) | Lab 5 (continued: pthreads) |
| 13 | Unix case study: history, structure, shell internals, signals, pipes, process groups | Kerrisk ch. 20–22, 44 | — | Lab 6: file system |
| 14 | **Crash consistency**: fsck, write-ahead logging, journaling (ext4), log-structured FS, copy-on-write FS (ZFS/btrfs) | OSTEP 42; ★ LFS paper; journaling paper; btrfs-snapshots article; Dan Luu "Files are hard" | 10 (Unix + crash) | Lab 6 (continued: crash experiment) |
| 15 | **Virtualization & isolation**: trap-and-emulate, shadow paging vs nested paging, Xen/KVM; containers for real: namespaces, cgroups, overlayfs, seccomp | OSTEP VMM appendix; ★ Xen paper; Agesen (VT-x + nested/EPT paging); A&D §10.2 (VM page tables); Waldspurger (ballooning/page-sharing); containers paper; namespaces/cgroups articles; read ahead: MirageOS + Firecracker + eBPF | — | Lab 7: containers from scratch |
| 16 | **Kernel architecture & research**: microkernels (Mach → L4 → seL4), exokernels, unikernels; eBPF; capability systems revisited; where OS research is going | seL4 whitepaper (read first); ★ Liedtke µ-kernel; seL4 (Klein); exokernel papers; (optional) Rust kernel (Levy) | 11 | Lab 7 (continued); Lab 8 proposal |
| 17 | **Revision week**: no new material. Exam-revision memory papers (y2023p2q3, y2024p2q4, y2025p2q4); the sealed y2026p2q4 as a timed mock; weak-spot sheets revisited. **FINAL EXAM at week's end** | — | — | — |
| 18–19 | **Project weeks**: Lab 8 main work (deadline: end of week 19) | — | — | Lab 8 |

(★ = a do-not-skip paper from the reading list.)

## Components

- [`reading-list.md`](reading-list.md) — ordered reading list: one spine
  textbook, supporting texts, 20-plus classic papers, and a handful of articles,
  all keyed to weeks.
  The spine (OSTEP, including the website's security chapters) is assigned
  near-completely; the deliberate omissions are RAID and data integrity
  (optional extras alongside weeks 10–11 and 14), the distributed chapters
  NFS/AFS and distributed-system security (IB territory), cryptography
  (optional),
  the memory-API interlude (assumed C prerequisite), and the LFS chapter
  (replaced by the original Rosenblum & Ousterhout paper in week 14).
- [`labs/`](labs/README.md) — 8 labs + final project, xv6-based kernel track
  plus userspace systems programming.
- [`exercises/`](exercises/README.md) — weekly examples sheets in the Cambridge
  supervision style, including pointers into the original IA sheets and past
  Tripos questions for the overlapping weeks.
- [`exams/`](exams/README.md) — two midterms and a final: format, coverage,
  and marking scheme.

## Assessment weights (suggested)

| Component | Weight |
|-----------|--------|
| Labs (8 × 5%) | 40% |
| Final project | 10% |
| Midterm 1 | 10% |
| Midterm 2 | 15% |
| Final exam | 25% |

## How to study each week

1. Do the assigned reading (spine text first, then the paper if one is listed).
2. Work the examples sheet *closed-book first*, then self-mark against its
   notes in `exercises/answers/` (spoilered — attempt first).
3. Labs run in parallel — budget 4–6 hours/week for them; iterate with the
   autograders' `QUICK=1` tier and pay for the full `usertests` run once,
   at submission.
4. Each sheet ends with its timed Tripos past-paper allocation (~35 min per
   question, closed book) — do those before moving on. Three memory papers
   are held back for exam revision, and the sealed y2026 papers are reserved
   for week 17 (sat as timed mocks in revision).
