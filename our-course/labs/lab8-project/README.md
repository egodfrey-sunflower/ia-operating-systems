# Lab 8 — Final Project

**Weeks:** 16, 18–19 · **Budget:** 25–30 hours · **Track:** project · **Weight:** 10%

Proposal in week 16; main work in weeks 18–19 (the project weeks); deadline end
of week 19.

You pick one project, build it, evaluate it with measurements, and write it up
in workshop-paper style. This is the transition from "doing labs" to "doing
systems work": there is no autograder, the spec is deliberately incomplete, and
part of the job is deciding what the right thing to build is.

## Ground rules

- **Proposal first** (½ page, before writing code): what you'll build, the 2–3
  key design decisions you anticipate, and what your evaluation section will
  measure. In a taught course this is where a supervisor says "too big" or
  "too small"; self-taught, sleep on it and cut the scope by a third.
- **Evaluation is not optional.** Every project below names measurements.
  "It works" is a demo, not an evaluation; the deliverable is *numbers plus an
  explanation of why the numbers look like that*.
- **Report:** 3–4 pages: problem, design (with the alternatives you rejected
  and why), implementation notes (what was hard and what you'd redo),
  evaluation, limitations. Model it on the papers from the reading list —
  the LFS paper's structure is the gold standard at miniature scale.

## Option 1 — A FUSE filesystem with a real on-disk format

Build a userspace filesystem via FUSE (libfuse3) backed by a single image
file, with a designed on-disk format — not a passthrough.

FUSE appears nowhere in the course reading, so start from the libfuse
sources: `example/passthrough.c` in the libfuse repository is the canonical
skeleton showing every callback you must implement, and the libfuse API
documentation covers the rest. Use the example as your on-ramp, then design
your own format on top.

Choose one flavour:

- **mini-LFS:** log-structured: all writes append to segments; an inode map;
  a segment cleaner with a simple cost-benefit policy (Rosenblum &
  Ousterhout, reading list #16). This is the deepest option.
- **snapfs:** copy-on-write with O(1) snapshots: `snapshot(name)` freezes the
  current tree (compare with the doomed hardlink scheme from IA examples
  sheet 3 Q6(b) — your design should actually solve what it couldn't).

Must support: create/unlink, read/write, mkdir/readdir, stat, persistence
across unmount/remount, and `fsck.mini` — an offline checker that verifies
your format's invariants.

**Evaluate:** sequential vs random write throughput vs ext4 (loopback) and a
FUSE passthrough baseline (to isolate FUSE overhead); for mini-LFS, cleaning
overhead as the disk fills; for snapfs, space and time cost per snapshot.

## Option 2 — Kernel threads for xv6

Add kernel-level threads to xv6: a `clone(fn, stack, flags)`-style syscall
creating threads that share an address space, plus `join`/`exit`. On top,
a userspace `uthread` library: `thread_create`, mutexes (futex-lite:
sleep/wakeup syscalls or spin-then-yield), condition variables.

Hard parts the report must address: what "share the address space" means for
the page table when one thread grows it via `sbrk` (TLB shootdown-lite);
which proc-table fields become per-thread vs per-process; what happens on
`exec` or `exit` with live siblings; stack allocation and reclamation.

**Evaluate:** thread create/join vs fork/wait latency; a parallel-sum
microbenchmark scaling across QEMU CPUs (1, 2, 4) with your mutex vs a
spinlock.

## Option 3 — EEVDF-style scheduler for xv6

Implement virtual-deadline scheduling in xv6, approximating what Linux ≥ 6.6
ships: per-process weight (nice), virtual runtime, eligibility check, pick
earliest virtual deadline; timer-driven preemption.

Must handle: sleep/wakeup rejoining without gaming vruntime (the "sleeper
fairness" problem — document your choice); weight changes at runtime;
multi-CPU (per-CPU runqueues with a simple steal, or one global queue —
justify).

**Evaluate:** against your Lab 3 schedulers: CPU-share accuracy vs weights
(target ±5%); wakeup-to-run latency for an interactive process under CPU
load (present a distribution, not a mean); fairness over 1s windows
(Jain's fairness index — Jain, Chiu & Hawe, 1984, "A Quantitative Measure of
Fairness and Discrimination for Resource Allocation in Shared Computer
Systems").

## Option 4 — A virtio block driver for xv6, from the spec

Delete `kernel/virtio_disk.c` from your tree (keep it for later comparison),
and rewrite it from the OASIS VirtIO 1.x specification, published at
docs.oasis-open.org (§ split virtqueues + block
device): device discovery/reset/feature negotiation over MMIO, virtqueue
setup, request submission (three-descriptor chains), interrupt-driven
completion, correct memory barriers.

This is the only option where the "customer" is a hostile piece of hardware
(QEMU's device model) and the debugging tool is the spec. Keep a debugging
diary — it becomes the implementation-notes section.

**Evaluate:** correctness under `usertests` + a concurrent I/O stress you
write; throughput with queue depth 1 vs batched submissions; then diff your
design against stock xv6's driver — what did they do that you didn't, and
does it measurably matter?

## Option 5 — File-backed `mmap`/`munmap` for xv6

xv6 has no `mmap`. Add real **file-backed, lazily-populated** memory mappings:
`mmap(addr, len, prot, flags, fd, off)` records a per-process VMA but maps no
pages; the page-fault handler reads the backing file's page on first touch
(demand paging); `munmap` writes back dirty `MAP_SHARED` pages and tears down
the VMA; `fork` and `exit` handle VMAs correctly. Support at least **private
(`MAP_PRIVATE`) and shared (`MAP_SHARED`)** file mappings and **partial**
`munmap`. This is the Lab 4 "stretch" turned into a real implementation — it
reuses your COW refcounting and lazy-allocation machinery.

**Evaluate:** a test suite covering demand fault-in, write-back on `MAP_SHARED`
+ `munmap`, private-vs-shared semantics, partial unmap, and fork inheritance;
then measure fault-in latency and compare a `read()` loop against an `mmap`ed
scan of a large file.

## Choosing

| If you liked most… | Do |
|---|---|
| Lab 6 (file systems) | Option 1 |
| Lab 5 (threads/locks) | Option 2 |
| Lab 3 (scheduling) | Option 3 |
| Lab 2 + gdb spelunking | Option 4 |
| Lab 4 (virtual memory) | Option 5 |

Options are calibrated to be equally hard. 1 and 3 have the most published
material to lean on; 4 has the least scaffolding and the most authentic
misery-to-enlightenment ratio.

## Rubric

| Component | Weight |
|---|---|
| Proposal quality & scope judgement | 10% |
| Design: decisions identified and argued, alternatives considered | 25% |
| Implementation: works, readable, handles the named hard parts | 35% |
| Evaluation: right measurements, honest analysis, limitations owned | 30% |
