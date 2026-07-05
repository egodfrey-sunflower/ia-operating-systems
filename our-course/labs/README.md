# Labs

Eight labs plus a final project. Two tracks interleaved:

- **Userspace track** (Labs 0, 1, 7): systems programming on your own Linux
  machine — the view *above* the system-call line.
- **Kernel track** (Labs 2–6): modifying **xv6-riscv**
  (<https://github.com/mit-pdos/xv6-riscv>) — the view *below* it.
  xv6 is a modern reimplementation of Unix V6; its filesystem is essentially
  the Unix V7 design the IA course examines, so Lab 6 has you improving the
  exact system the Tripos questions ask you to critique.

Setup once, in Lab 0: `qemu-system-riscv64`, `riscv64-unknown-elf-gcc` (or
distro cross-toolchain), `gdb-multiarch`. Everything runs under QEMU — no
hardware needed.

Each lab below lists: goal, tasks, stretch goals, and the concept from the
syllabus it cashes out. The detailed handouts live in the per-lab
directories — `lab0-toolchain/`, `lab1-shell/`, `lab2-syscalls/`,
`lab3-scheduler/`, `lab4-vm/`, `lab5-threads/`, `lab6-fs/`,
`lab7-containers/`, `lab8-project/` — each with `starter/`, an autograder in
`tests/`, and a quarantined reference solution in `solutions/` (spoilers:
attempt first). The exception is `lab8-project/`, which is deliberately
spec-only — an open-ended project has no starter, autograder, or reference
solution. A pristine xv6 tree is vendored at `xv6-riscv/`; never edit
it — each kernel lab's `setup.sh` copies it. Autograders are tiered
(smoke → lab tests → `usertests` regression; iterate with `QUICK=1`) — see
[Conventions & test tiers](#conventions--test-tiers) below.

---

## Lab 0 — Toolchain & Unix warm-up (week 1)

**Goal:** working toolchain; fluency with the Unix userland you're about to
reimplement.

- Install the RISC-V toolchain and QEMU; build and boot stock xv6; attach gdb
  to the kernel and single-step from `_entry` through the first lines of
  `main()` — *watch the machine boot* (IA L1: booting).
- Warm-up C exercises: implement `cat`, `ls -l` (via `stat(2)`), and `grep`
  using raw system calls only (no stdio).
- Use `strace` on your tools and on `bash` to see the syscall layer in action.

**Deliverable:** the three utilities + a short note: every distinct syscall
`strace` showed, and what each was for.

## Lab 1 — Build a shell (weeks 2–3)

**Goal:** own the process abstraction from userspace: `fork`, `exec`, `wait`,
file descriptors, pipes, signals. This is the IA Unix case study, done rather
than described.

- Parse and run simple commands; search `$PATH`.
- I/O redirection (`<`, `>`, `>>`, `2>`), pipelines of arbitrary length
  (`a | b | c`), and `&` background jobs with zombie reaping (`SIGCHLD`).
- Built-ins (`cd`, `exit`) — and be able to say *why* they must be built-ins.
- Signal handling: `Ctrl-C` kills the foreground job, not the shell.

**Stretch:** job control (`fg`/`bg`, process groups, `tcsetpgrp`) — the part
of the Unix case study nobody's notes cover properly.

**Deliverable:** shell passing a provided test script; 1-page design note on
the fd manipulation for `a | b > f`.

## Lab 2 — xv6 system calls (week 4)

**Goal:** cross the user/kernel boundary (IA L2: protection, system calls).

- Trace one existing syscall end-to-end (`ecall` → trampoline → `usertrap` →
  `syscall()` → return) and write it up as an annotated call path.
- Add `trace(mask)`: per-process syscall tracing, printed on return.
- Add `sysinfo()`: report free memory and process count (teaches kernel data
  structure traversal and copyout).
- Add `getppid()` — small, but forces you through the process table.

**Deliverable:** patches + the annotated call-path document.

## Lab 3 — Scheduling (weeks 5–7 — implementation in weeks 5–6, report/wrap-up in week 7)

**Goal:** replace xv6's round-robin scheduler; measure what changes (IA L4–5,
plus week-5 extension material).

- Instrument: add per-process tick counters and a `getpstat` syscall; write a
  userspace workload mixing CPU-bound and I/O-bound processes.
- Implement **static priorities** with `setpriority()`; demonstrate starvation.
- Implement **lottery scheduling** (Waldspurger paper) with `settickets()`;
  show ticket-proportional CPU shares experimentally.
- Implement **MLFQ** with priority boost; show it approximates SJF without
  runtime knowledge, and that the boost fixes the starvation you demonstrated.

**Stretch:** a simple stride scheduler; compare variance against lottery.

**Deliverable:** code + a short lab report with plots (tick share vs tickets;
response time under MLFQ vs RR for the mixed workload).

## Lab 4 — Virtual memory (weeks 7–9 — three weeks; it is two labs' worth of work)

**Goal:** page tables in the concrete (IA L6–8: paging, demand paging, COW).

- **Dissect the stock lazy allocator** (written task): current xv6 already
  demand-pages `sbrk()` via `vmfault()` — trace it and answer precise
  questions on where and why the kernel itself must fault pages in.
- Write `vmprint(pagetable)`: dump a process's page table like a tree —
  forces you to internalize the 3-level Sv39 walk (IA "two-level page table",
  one level deeper).
- **USYSCALL shared page:** a read-only kernel-maintained page mapped into
  every process (the vDSO idea); implement `ugetpid()` with no kernel
  crossing.
- **Copy-on-write fork** (the centrepiece): share frames read-only on fork,
  copy on write fault, with reference counting — and route the fault
  correctly now that scause 15 can mean *either* lazy or COW.
- Measure: fork latency before and after COW for a large process
  (`forkbench`).

**Stretch:** `mmap`/`munmap` with lazy file-backed mappings (the hardest
stretch goal in the course; budget accordingly).

**Deliverable:** patches passing `usertests`, plus the fork latency numbers.

## Lab 5 — Threads & synchronization (weeks 10–12 — the pthreads part deliberately lands in week 12 with Concurrency II)

**Goal:** concurrency from both sides of the kernel line (week 6 & 12
material).

- **User-level threads:** implement a cooperative threading package
  (context save/restore in assembly, `thread_create/yield/exit`) —
  demystifies "context switch" completely (IA L3).
- **Locking:** in xv6, the kernel's memory allocator and buffer cache use
  big locks. Split `kalloc`'s free list per-CPU and rework the buffer cache
  locking (hash buckets); measure contention before/after with lock
  acquisition counters.
- **Classic problems in pthreads:** bounded buffer with condition variables;
  a barrier; readers–writers with writer preference.

**Deliverable:** threading package + xv6 patches + contention measurements.

## Lab 6 — File systems (weeks 13–14 — the crash experiment lands with week 14's crash-consistency theory)

**Goal:** the V7-style filesystem, extended by you (IA L10–12).

- **Large files:** xv6 inodes have 12 direct + 1 singly-indirect block
  pointer. Rebalance the fixed 13-slot address array to 11 direct + 1
  singly-indirect + 1 doubly-indirect; recompute max file size (this is
  IA examples sheet 3 Q5(c) — done for real).
- **Symbolic links:** add `symlink()`; handle recursive resolution with a
  depth limit. Contrast implementation effort with hard links (IA examples
  sheet 3 Q2).
- **Crash consistency:** xv6 has a write-ahead log. Using the provided
  compile-time crashpoint (`CRASH=BEFORE_HEAD|AFTER_HEAD`), kill the kernel
  mid-commit on either side of the log's atomicity point (the header write),
  reboot the *same* disk image, and verify: before → old consistent state;
  after → recovery replays your writes. Plus a written trace of one `write()`
  through `begin_op`/`log_write`/`commit`.

**Stretch:** a block-level LRU cache eviction policy experiment, or extent-
based allocation.

**Deliverable:** patches passing `usertests` + crash-consistency writeup.

## Lab 7 — Containers from scratch (weeks 15–16)

**Goal:** the week-15 material is real: build a mini-Docker in a few hundred
lines of C on Linux. Honest budget: ~8–10 hours — more than a single week's
lab allowance, so either start early or let Task 4 run into week 16.

- `clone()` with `CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS`: a process that
  believes it is PID 1 on its own host.
- `pivot_root` into an extracted Alpine rootfs; mount `/proc`; get a working
  isolated shell.
- cgroups v2: cap the container's memory and CPU; demonstrate the cap with a
  stress workload.
- Unweighted stretch: a network namespace with a veth pair — supplied as a
  cookbook, since networking gets a course of its own.

**Deliverable:** `mycontainer run <rootfs> <cmd>` + demo transcript.

## Lab 8 — Final project (proposal in week 16; main work in weeks 18–19, deadline end of week 19)

Pick one; scope ~25–30 hours:

1. **FUSE filesystem:** a userspace FS with a real on-disk format — e.g. a
   simplified log-structured FS with segment cleaning, or a versioned FS
   that snapshots on every write (IA examples sheet 3 Q6(b), built properly).
2. **xv6 kernel feature:** kernel-level threads (`clone`-style) with a
   userspace pthread-like library on top; or signals for xv6.
3. **Scheduler deep-dive:** implement EEVDF-style virtual-deadline scheduling
   in xv6; compare latency/fairness against your Lab 3 schedulers.
4. **Paravirtual device:** write a virtio block-device driver for xv6 from
   the virtio spec (xv6 ships one to crib from — write yours first, then
   compare).

**Deliverable:** code + a 3–4 page report in workshop-paper style: design,
alternatives considered, evaluation with measurements.

---

## Conventions & test tiers

Every lab (except the spec-only `lab8-project/`) follows the same layout:

```
labN-<slug>/
  README.md    # the student handout
  starter/     # skeleton code the student begins from
  tests/       # autograder; entry point: tests/run.sh <workdir>
  solutions/   # SPOILERS — quarantined reference solution
```

`tests/run.sh` exits 0 on pass, nonzero on fail. For the kernel labs (2–6)
it builds the tree and drives `make qemu` (`-nographic`) through the shared
harness at `common/qemuharness.py` (send shell commands, match expected
output on regexes with generous timeouts, always kill QEMU on exit).

Because `usertests -q` costs several minutes per run, the xv6 autograders
work in **tiers**:

1. **Smoke** (seconds): build + boot + `echo ok`.
2. **Lab tier** (≤ ~1 min): the lab's own targeted test programs
   (trace/cowtest/symlinktest/…). This is the default inner-loop tier;
   graders honour `QUICK=1` to stop here.
3. **Regression tier**: `usertests -q`, run **last** — and, when one
   subsystem is the risk, a named-subtest middle tier first
   (`usertests copyin`, …) to fail fast before paying for the full run.

Iterate with `QUICK=1`; a submission counts only with the full run.
