# Reading List

The week-by-week table at the bottom is the reading order; item numbers are
stable identifiers used in cross-references throughout the course.

## Spine text (read cover to cover)

1. **Arpaci-Dusseau & Arpaci-Dusseau — *Operating Systems: Three Easy Pieces*
   (OSTEP)** — free at <https://pages.cs.wisc.edu/~remzi/OSTEP/>.
   The primary text. Its three parts (virtualization of CPU/memory,
   concurrency, persistence) map almost exactly onto this course. Short
   chapters, homework simulators, and a running xv6 connection. Where the IA
   course and OSTEP disagree on emphasis, follow OSTEP for depth and the IA
   notes for exam vocabulary. Include the **Security** chapters from the
   website (by Peter Reiher, ch. 53–57): intro to OS security, authentication,
   and access control (ch. 53–55) are assigned in week 1; cryptography and
   distributed-system security (ch. 56–57) are optional extras (the latter is
   IB territory).

## Supporting textbooks (reference — read assigned chapters only)

2. **Anderson & Dahlin — *Operating Systems: Principles and Practice* (2nd ed.)**
   — the best treatment of protection, dual-mode operation, and the kernel/user
   boundary (Part I, ch. 1–2) and of scheduling theory (ch. 7). The course also
   draws on it for IPC (ch. 3), multiprocessor locks / MCS / non-blocking
   synchronization (§5.7, §6.1–6.3, §6.6), deadlock and the banker's algorithm
   (§6.5), and virtual-machine page tables (§10.2). On the official IA list.
3. **Silberschatz, Galvin & Gagne — *Operating System Concepts* (10th ed.)**
   — **optional reference only** (its chapters read as encyclopedia entries
   rather than a narrative). Reach for it for a second, drier take: **§2.8** (the
   OS-structure taxonomy) as a week-2 backstop, and **§12.3–12.4** (the
   application-I/O interface — buffering, spooling, block-vs-char) as a week-10
   backstop. Any edition works for the concepts, but the section numbers here
   (§2.8, §12.3–12.4) are 10th-ed and shift in earlier editions.
4. **Kerrisk — *The Linux Programming Interface*** — the userspace view.
   Chapters on processes (24–28), signals (20–22), and pipes (44) are the
   reference for Labs 1 and 7.
5. **McKusick, Neville-Neil & Watson — *The Design and Implementation of the
   FreeBSD Operating System* (2nd ed.)** — the modern successor to the 4.3BSD
   book on the IA list. **ch. 7** (the VFS / vnode layer — how one kernel hosts
   ext4, procfs, NFS and tmpfs behind one `open`/`read`) and **ch. 9** (the Fast
   Filesystem) both fit week 11: ch. 7 for the structural multi-filesystem abstraction, ch. 9 for a
   production-grade FFS contrast with xv6. Dip into
   **ch. 10** (ZFS) during week 14 for the copy-on-write/snapshots angle. (In the
   2nd ed., ch. 8 is *Devices* and the filesystem chapters are 9–11.)
6. **CS:APP (Bryant & O'Hallaron), ch. 7–9** — **ch. 7 (Linking)** is the
   week-7 depth backstop for static/dynamic linking and ELF (paired with the LWN
   explainers below); ch. 8–9 only if exceptional control
   flow or address translation feel shaky; it explains both from the
   hardware up.

## xv6 companion (for the labs)

7. **Cox, Kaashoek & Morris — *xv6: a simple, Unix-like teaching operating
   system* (the xv6 book)** — free at
   <https://pdos.csail.mit.edu/6.1810/2024/xv6/book-riscv-rev4.pdf>.
   Read one chapter before each kernel lab; it documents exactly the code you
   will be modifying.

## Papers (★ = do not skip)

8. ★ **Ritchie & Thompson (1974), "The UNIX Time-Sharing System"** (CACM) —
   week 2. The design worldview of everything in this course, in 11 pages.
9. **Wahbe et al. (1993), "Efficient Software-Based Fault Isolation"**
   (SOSP) — week 2, §1–3 only. How to confine untrusted native code without
   hardware protection.
10. **Saltzer & Schroeder (1975), "The Protection of Information in Computer
    Systems"** (Proc. IEEE) — week 1, §I.A–B only. The canonical source of the
    protection-principles vocabulary — least privilege, fail-safe defaults,
    ACLs vs capabilities — that the course leans on.
11. **Lampson (1973), "A Note on the Confinement Problem"** (CACM 16(10)) —
    week 2. Three pages; the paper that first named **covert channels** (storage
    vs timing) and the confinement problem — the only week-1–2 source that covers
    them. (Distinct from item 12, Lampson's 1983 *Hints*.)
12. ★ **Lampson (1983), "Hints for Computer System Design"** — week 3. The
    mechanism-vs-policy, keep-it-simple mindset the IA course gestures at.
13. **Corbató & Vyssotsky (1965), "Introduction and Overview of the Multics
    System"** — week 3, optional. What Unix was reacting against; segments
    taken seriously.
14. ★ **Waldspurger & Weihl (1994), "Lottery Scheduling: Flexible
    Proportional-Share Resource Management"** (OSDI) — week 5. You implement
    this in Lab 3.
15. **Liu & Layland (1973), "Scheduling Algorithms for Multiprogramming in a
    Hard-Real-Time Environment"** (JACM) — week 5, §1–3 only. The source of
    the rate-monotonic utilisation bound.
16. **Joseph & Pandya (1986), "Finding Response Times in a Real-Time System"**
    (The Computer Journal 29(5)) — week 5, optional. The origin of
    **response-time analysis**: the fixed-point recurrence
    `Rᵢ = Cᵢ + Σⱼ ⌈Rᵢ/Tⱼ⌉·Cⱼ`. Liu & Layland (15) gives the utilisation
    *bounds*; this gives the exact per-task response time. Supplementary.
17. **Denning (1968), "The Working Set Model for Program Behavior"** (CACM) —
    week 9. Source of the working-set / thrashing story in the IA syllabus.
18. **Belady (1966), "A Study of Replacement Algorithms for a Virtual-Storage
    Computer"** — week 9. OPT, and why FIFO misbehaves (Belady's anomaly).
19. **Bonwick (1994), "The Slab Allocator"** (USENIX) — week 9. Kernel memory
    allocation as it is actually done.
20. **Anderson, Bershad, Lazowska & Levy (1992), "Scheduler Activations:
    Effective Kernel Support for the User-Level Management of Parallelism"**
    (TOCS) — week 10, optional. The definitive treatment of the user-level vs
    kernel-thread trade-off; read alongside Lab 5's user-level threads part.
21. ★ **McKusick et al. (1984), "A Fast File System for UNIX"** (TOCS) —
    week 11. Reads as a direct sequel to the V7 filesystem you study and
    build on in Lab 6.
22. ★ **Rosenblum & Ousterhout (1991), "The Design and Implementation of a
    Log-Structured File System"** (SOSP) — week 14. The most influential
    storage paper of its era; the intellectual ancestor of SSDs' FTLs, ZFS,
    and every journaling design.
23. **Prabhakaran et al. (2005), "Analysis and Evolution of Journaling File
    Systems"** (USENIX ATC) — week 14. How ext3 actually orders its writes;
    pairs with OSTEP's crash-consistency chapter.
24. ★ **Barham et al. (2003), "Xen and the Art of Virtualization"** (SOSP) —
    week 15. Paravirtualization; written down the road from the CL.
25. **Soltesz et al. (2007), "Container-based Operating System
    Virtualization"** (EuroSys) — week 15. The case for OS-level
    virtualization, pre-Docker.
26. **Madhavapeddy et al. (2013), "Unikernels: Library Operating Systems for
    the Cloud"** (ASPLOS) — week 15 (read ahead; the week-16 material draws
    on it). The unikernel argument; pairs with the ebpf.io "What is eBPF?"
    explainer.
27. **Agesen, Garthwaite, Sheldon & Subrahmanyam (2010), "The Evolution of an
    x86 Virtual Machine Monitor"** (ACM SIGOPS Operating Systems Review 44(4)) —
    week 15. The VMware retrospective that carries x86 virtualization
    past software shadow paging into **hardware CPU virtualization (Intel VT-x /
    AMD-V)** and **nested / two-dimensional page tables (EPT / NPT)**, which
    OSTEP's VMM appendix and the (2003) Xen paper both predate.
28. ★ **Liedtke (1995), "On µ-Kernel Construction"** (SOSP) — week 16. Why
    first-generation microkernels were slow and second-generation ones are
    not.
29. **Klein et al. (2009), "seL4: Formal Verification of an OS Kernel"**
    (SOSP) — week 16. Capabilities (from week 2) reappear, now with proofs. Read
    the Heiser whitepaper (34) first for the plain-prose version, then dip into
    §1–2 + §4.5 here for how the proof was actually done.
30. **Engler et al. (1995), "Exokernel: An Operating System Architecture for
    Application-Level Resource Management"** (SOSP) — week 16. The other
    answer to "what should a kernel be?"; from the group that later wrote xv6.
31. **Baumann, Appavoo, Krieger & Roscoe (2019), "A fork() in the road"**
    (HotOS) — week 3, optional. A short, provocative critique of `fork()`: its
    copy-on-write cost (Lab 4), why it fights threads and containers, and the
    `posix_spawn`/`vfork` alternatives. Turns the fork/exec mechanics you build
    on into a design judgement.
32. **Waldspurger (2002), "Memory Resource Management in VMware ESX Server"**
    (OSDI, best paper) — week 15. The memory-reclamation half of virtualization:
    **ballooning**, the **idle-memory tax**, and **content-based transparent
    page sharing** — the overcommit story that address translation alone never
    reaches. Same author as the lottery paper (14).
33. **Agache et al. (2020), "Firecracker: Lightweight Virtualization for
    Serverless Applications"** (NSDI), §1–3 — week 15 (read ahead). The modern
    KVM **microVM**: a minimal device model built *because* container isolation
    wasn't enough for multi-tenant serverless — the industrial answer in the
    VM-vs-container-vs-unikernel debate.
34. **Heiser (2020), "The seL4 Microkernel: An Introduction"** (seL4 Foundation
    whitepaper) — week 16. A plain-prose on-ramp to item 29 (Klein et al.):
    capabilities, what the proof does and does not cover, and performance. Read
    this first, then dip into Klein for how the proof was actually done.
35. **Levy, Campbell, Ghena, Pannuto, Dutta & Levis (2017), "The Case for
    Writing a Kernel in Rust"** (APSys) — week 16, optional (~6 pp). The
    memory-safe-kernel data point for "where OS research is going": how Rust's
    ownership types buy kernel memory safety without a GC or SFI's runtime
    checks, framed explicitly against SFI (9), microkernels (28) and verification
    (29).

## Articles & explainers (assigned inline by the week table)

- **Preshing, "Memory Barriers Are Like Source Control"** — week 6; memory
  reordering and the four barrier types.
- **Preshing, "An Introduction to Lock-Free Programming"** — week 6. Names and
  diagrams the ABA problem and the compare-and-swap loop it breaks (OSTEP ch.
  25–29 cover only lock-*based* structures).
- **Mike Jones, "What Really Happened on Mars"** — week 6; priority inversion.
- **Microsoft Learn, "Containers vs. virtual machines"** — week 1. The
  VM-vs-container layering and isolation-strength contrast.
- **Julia Evans, "What even is a container: namespaces and cgroups"** — week 1.
  The resource-partitioning (cgroups) and naming/isolation (namespaces)
  mechanisms behind a container (deep dive is week 15).
- **LWN's CFS → EEVDF coverage** — week 5; how Linux actually schedules.
- **David Drysdale, "How programs get run" + "…ELF binaries" (LWN)** — week 7;
  what a shared library is and how `execve` + the dynamic linker (`ld.so`) load
  an ELF and resolve symbols. CS:APP ch. 7 is the optional depth. <https://lwn.net/Articles/630727/>, <https://lwn.net/Articles/631631/>.
- **Chris Down, "In defence of swap: common misconceptions"** — week 9; what
  Linux actually does under memory pressure (overcommit, swap as a reclaim
  class, the OOM killer) — the modern counterpart to Denning's working-set
  theory. <https://chrisdown.name/2018/01/02/in-defence-of-swap.html>.
- **The LWN io_uring introduction, or Axboe's "Efficient IO with io_uring"**
  — week 10; Linux's ring-buffer asynchronous I/O interface.
- **McKenney, "What is RCU?" (LWN)** — week 12; read-copy-update.
- **Eli Bendersky, "Basics of Futexes"** — week 12; how a real sleeping lock is
  built (uncontended fast path in userspace, kernel only on contention) — the
  bridge from week-6 spinlocks to week-12 sleeping primitives; also underlies
  Lab 8's uthread mutexes. Drepper's "Futexes Are Tricky" is optional depth.
  <https://eli.thegreenplace.net/2018/basics-of-futexes/>.
- **LWN's btrfs subvolumes and snapshots coverage** — week 14; what a
  copy-on-write filesystem snapshot copies versus shares — the source for the
  copy-on-write-filesystem topic.
- **Dan Luu, "Files are hard"** — week 14; the application-side of crash
  consistency — how even a correct journaling FS lets apps lose data if they
  order writes/`fsync`s wrong. <https://danluu.com/file-consistency/>.
- **LWN's namespaces & cgroups series** — week 15; Lab 7.
- **ebpf.io, "What is eBPF?"** — week 15 (read ahead for week 16).

## Week-by-week mapping

| Week | Reading |
|------|---------|
| 1 | OSTEP 1–2 (intro, dialogue) and 53–55 (security intro, authentication, access control; ch. 55 works ACLs vs capabilities); A&D ch. 1; paper 10 §I.A–B (Saltzer & Schroeder — pairs with ch. 55's ACLs-vs-capabilities); Microsoft Learn "Containers vs. virtual machines" and Julia Evans "What even is a container" (VM-vs-container overview — topic preview, no week-2 prerequisite) |
| 2 | OSTEP 4–6 (processes, limited direct execution); A&D ch. 2 (protection); the monolithic/microkernel/modules overview via a week-16 Liedtke (item 28) read-ahead, with Silberschatz §2.8 as an optional backstop; paper 8; paper 9 §1–3 (Wahbe, SFI — short); paper 11 (Lampson 1973 — covert channels, ~3 pp) |
| 3 | OSTEP 4–5 again + xv6 book ch. 1; Kerrisk ch. 24–27 (process creation/termination/monitoring/exec) alongside Lab 1 (and skim Kerrisk ch. 20–22, 44 — Lab 1's signal/pipe tasks pre-teach week-13 material); A&D ch. 3 (The Programming Interface — the pipes/shared-memory/sockets comparison; for fd-passing, Kerrisk ch. 61 advanced sockets, with the shared-memory chapters Kerrisk ch. 45–54 for depth); paper 12; paper 13 optional; paper 31 optional (Baumann, "A fork() in the road" — a design critique of the fork/exec you build on in Lab 1) |
| 4 | OSTEP 7–8 (scheduling, MLFQ); xv6 book ch. 2 (OS organization) & ch. 4 (traps & system calls) — both for Lab 2; A&D ch. 7 (scheduling theory) |
| 5 | OSTEP 9–10 (lottery, multiprocessor); paper 14; paper 15 §1–3 (Liu & Layland — the RM/EDF bounds); paper 16 optional (Joseph & Pandya 1986 — the response-time-analysis recurrence; supplementary); skim Linux CFS→EEVDF articles (LWN); xv6 book ch. 7 (scheduling — for Lab 3) |
| 6 | OSTEP 25–29 (concurrency intro, locks); xv6 book ch. 6 (locking); A&D §5.7 + §6.1–6.3 (multiprocessor spinlocks, test-and-test-and-set, cache-coherence traffic, MCS locks) and §6.6 (non-blocking synchronization); Preshing, "Memory Barriers Are Like Source Control" (memory reordering); Preshing, "An Introduction to Lock-Free Programming" (lock-free / the ABA problem); Mike Jones, "What Really Happened on Mars" (priority inversion) |
| 7 | OSTEP 13, 15–16 (address spaces, translation, segmentation); OSTEP 14 (memory API) optional — the omitted interlude, assumed C prerequisite; Drysdale's LWN "How programs get run" + "…ELF binaries" and CS:APP ch. 7 (Linking) — static/dynamic linking, ELF, `ld.so`, completing the compile/load/run binding story |
| 8 | OSTEP 17–20 (free space, paging, TLBs, multi-level tables); xv6 book ch. 3 |
| 9 | OSTEP 21–24 (swapping, replacement); paper 17 in full; papers 18, 19: skim §1–2 + conclusions; Chris Down, "In defence of swap" — modern Linux memory pressure (overcommit, reclaim, OOM killer) |
| 10 | OSTEP 36–37, 44 (I/O devices, disks, flash — mass storage and the polling/interrupts/DMA mechanism); the LWN io_uring introduction or Axboe's "Efficient IO with io_uring"; Silberschatz §12.3–12.4 as optional reference for the application-I/O interface (buffering, spooling, block-vs-char); optionally, a guided read of xv6's `kernel/virtio_disk.c` interrupt path — the course's only in-kernel device code; paper 20 optional (Scheduler Activations — the user-level vs kernel-thread trade-off, alongside Lab 5's user-level threads part) |
| 11 | OSTEP 39–41 (files & dirs, FS implementation, FFS); xv6 book ch. 8; paper 21; optional: FreeBSD book (item 5) ch. 7 (VFS/vnode — the multi-filesystem abstraction) and ch. 9 (production FFS contrast with xv6) |
| 12 | OSTEP 30–34 (condition variables, semaphores, common bugs incl. deadlock, event-based concurrency); A&D §6.5 (deadlock — the four conditions, prevention, and §6.5.4 the banker's algorithm, which OSTEP names but never works); skim McKenney's RCU intro (LWN "What is RCU?"); Eli Bendersky, "Basics of Futexes" — how a real sleeping lock is built (underlies Lab 8's uthread mutexes) — this is the week Lab 5's pthreads part lands |
| 13 | Kerrisk ch. 20–22, 44 (signals, pipes) for the Unix case study |
| 14 | OSTEP 42 (crash consistency, journaling); papers 22, 23; LWN's btrfs subvolumes & snapshots article (the source for the copy-on-write-filesystem topic); Dan Luu, "Files are hard" (the application view of crash consistency); optional: FreeBSD book (item 5) ch. 10 (ZFS) for the copy-on-write/snapshots angle |
| 15 | OSTEP appendix on VMMs; A&D §10.2 (VM page tables — shadow paging); papers 24, 25, and 27 (Agesen — hardware CPU virtualization VT-x/AMD-V and nested / two-dimensional paging EPT/NPT); paper 32 (Waldspurger — VM memory reclamation: ballooning, idle-memory tax, content-based page sharing); LWN series on namespaces & cgroups; the man pages namespaces(7), cgroups(7), user_namespaces(7), clone(2) and pivot_root(2) — Lab 7's primary sources; read ahead: paper 26 (MirageOS), paper 33 (Firecracker — the modern microVM), and the ebpf.io "What is eBPF?" explainer (week 16 is paper-dense) |
| 16 | Paper 34 (Heiser seL4 whitepaper — read first) then papers 28, 29 (§1–2 + §4.5), 30; paper 35 optional (Levy — the memory-safe/Rust kernel); revisit week 15's MirageOS paper and eBPF explainer |
| 17 | No new reading — revision (the held-back memory papers y2023p2q3, y2024p2q4, y2025p2q4 + the sealed y2026p2q4 timed mock) |
| 18–19 | No reading — Lab 8 project |
