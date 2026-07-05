# Examples Sheets

Eleven sheets, Cambridge supervision style: short true/false-with-justification
warm-ups, then discussion and worked-calculation questions, then past Tripos
questions where the topic overlaps the IA course. Work each sheet closed-book
after finishing the week's reading; write answers out in full prose as if for
a supervisor.

The original IA sheets remain part of the course — they are woven in (by
citation) rather than replaced. Sheets marked **[ext]** cover extension
material with no IA equivalent. The sheets live in `sheet01.md … sheet11.md`,
with self-marking notes in `answers/` (spoilered — attempt first).

| Sheet | After week | Coverage | Source material |
|-------|-----------|----------|-----------------|
| 1 | 2 | OS structure, protection, dual-mode, syscalls, ACLs vs capabilities | IA sheet 1 Q1–2, Q5–6; original questions on VMs vs containers and covert channels |
| 2 | 3 | Processes: lifecycle, PCB, context switch, interrupts, IPC | IA sheet 1 Q3–4; original questions on IPC mechanism trade-offs |
| 3 | 4 | Scheduling algorithms: FCFS/SJF/SRTF/RR/priority, quantum choice, convoy effect | IA sheet 2 Q1–3, Q8 |
| 4 | 5 | **[ext]** Proportional-share & modern scheduling: lottery/stride worked examples, CFS virtual runtime, EDF schedulability test | original |
| 5 | 6 | **[ext]** Concurrency I: race conditions, Peterson's algorithm, TAS/CAS, spinlock vs sleeping lock trade-offs | original |
| 6 | 9 | Memory: address binding, segmentation vs paging, multi-level page tables, TLBs, replacement policy traces, working set, COW, buddy/slab | IA sheet 2 Q4–7; original questions on COW and slab/buddy allocators |
| 7 | 10 | I/O: polling vs interrupt vs DMA control flow, blocking/non-blocking/async, buffering; disk vs SSD scheduling | IA sheet 3 Q1(a–b), Q3–4; new SSD/io_uring questions |
| 8 | 11 | Files: directory service, links, metadata, V7 layout, inode arithmetic, mount points | IA sheet 3 Q1(c–d), Q2, Q5–6 |
| 9 | 12 | **[ext]** Concurrency II: condition variables, semaphores, monitors, deadlock & the banker's algorithm, RCU | new (extension — no IA equivalent) |
| 10 | 14 | Unix case study + **[ext]** crash consistency: shell/syscalls, signals, pipes; fsck vs journaling vs LFS, what `fsync` guarantees | IA sheet 3 Q7; new crash-consistency questions |
| 11 | 16 | **[ext]** Virtualization & kernel architecture: trap-and-emulate, nested paging, namespaces/cgroups, micro- vs monolithic kernels, seL4 capabilities | original |

Each sheet is self-contained: it carries its own timed Tripos past-paper
allocations (from `../../cambridge-course/exam_questions/`, topics verified by reading each
paper), pointers to the relevant OSTEP homework simulators for mechanical
drill, and cross-references to the labs it feeds. Three memory papers
(`y2023p2q3`, `y2024p2q4`, `y2025p2q4`) are held back for exam revision, and
`y2026p2q3`/`y2026p2q4` stay sealed until week 17 — one as the timed
standalone mock, one inside the final itself (see `../exams/`).
