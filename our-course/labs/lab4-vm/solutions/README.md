# ⚠️ SPOILERS — Lab 4 reference solution ⚠️

```
╔══════════════════════════════════════════════════════════════════════╗
║  STOP. This directory contains the complete reference solution for    ║
║  Lab 4 (virtual memory). Do the lab yourself first. Copy-on-write is   ║
║  the hardest xv6 lab to get right; reading the answer before you have  ║
║  fought usertests throws away the entire point. You have been warned.  ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## What's here

- `solution.patch` — a unified diff (`diff -ruN`, `-p1`) against a tree
  created by `starter/setup.sh`. It touches eleven files:
  `kernel/{memlayout.h, riscv.h, proc.h, kalloc.c, vm.c, trap.c, proc.c,
  sysproc.c, syscall.c, defs.h}` and `user/ulib.c`.
- `apply.sh <workdir>` — dry-runs then applies `solution.patch` to a starter
  tree. Does not build; run `make` afterwards.
- `files/` — the full post-solution versions of the changed files, for
  reading without applying the patch.

## Apply and test

```
./starter/setup.sh ~/lab4-sol
solutions/apply.sh ~/lab4-sol
tests/run.sh ~/lab4-sol           # expect all PASS, incl. usertests -q
```

## Design notes

- **`vmprint`** (`kernel/vm.c`) — a small recursive walker prints each valid
  PTE with `" .."` repeated once per level; it descends when
  `(pte & (PTE_R|PTE_W|PTE_X)) == 0` (a non-leaf, as in `freewalk`).
  `sys_vmprintme` (`sysproc.c`) calls `vmprint(myproc()->pagetable)`. The top
  index is 255, not 511, because `MAXVA` is `1<<38` — one bit short of the full
  Sv39 range, so the trampoline region lands at PTE index 255 at level 2.

- **USYSCALL** (`memlayout.h`, `proc.c`, `ulib.c`) — `struct usyscall` (guarded
  by `#ifndef __ASSEMBLER__`, since `memlayout.h` reaches `trampoline.S`) sits
  at `TRAPFRAME - 3*PGSIZE` = `0x3fffffb000`, **not** MIT's canonical
  `TRAPFRAME - PGSIZE` (see the post-review note below).
  `allocproc` `kalloc`s the page and writes the pid;
  `proc_pagetable` maps it `PTE_R | PTE_U`; `freeproc` frees it and
  `proc_freepagetable` unmaps it — mirroring the trapframe exactly. `ugetpid()`
  in `ulib.c` just reads `((struct usyscall*)USYSCALL)->pid`.

- **Reference-counted frames** (`kalloc.c`) — a `kref.count[]` array sized
  `(PHYSTOP - KERNBASE)/PGSIZE`, indexed by `(pa - KERNBASE)/PGSIZE`, under its
  own `kref.lock`. `kalloc` sets a new frame's count to 1; `krefinc` bumps it;
  `kfree` decrements and only enqueues the frame when the count hits 0.
  `freerange` seeds each page's count to 1 before the boot-time `kfree`.
  `krefcount` exposes the count to the COW fault handler.

- **Copy-on-write** (`riscv.h`, `vm.c`, `trap.c`) — `PTE_COW = 1<<8` (an RSW
  bit). `uvmcopy` drops `PTE_W` and sets `PTE_COW` in the parent PTE, maps the
  child to the same frame with the same flags, and `krefinc`s. `iscowfault`
  recognises a fault on a valid, user, `PTE_COW` page; `cowfault` breaks it —
  if it is the sole owner (`krefcount == 1`) it just re-enables `PTE_W`,
  otherwise it `kalloc`s a private copy, remaps, and `kfree`s its share of the
  old frame. `usertrap` checks `iscowfault` **before** falling through to the
  lazy-allocation `vmfault`, resolving the scause-15 ambiguity. `copyout`
  dispatches to `cowfault` when its target PTE is `PTE_COW`, because the kernel
  writes through the page table by hand with no hardware fault to trigger the
  break.

## Post-review note: the USYSCALL address bug

The first cut of this solution used MIT's canonical `USYSCALL = TRAPFRAME -
PGSIZE` (`0x3fffffd000`). Every lab-tier test passed; the deferred full
regression then failed exactly one test — `lazy_copy`, with `write
succeeded`. Diagnosis: `lazy_copy` (`user/usertests.c`, the `bad[]` array
~line 2686) asserts that `read()`/`write()` **fail** for six addresses,
`0x3fffffc000` … `0x8000000000` — i.e. `MAXVA - 4*PGSIZE` upward. A
user-readable USYSCALL page inside that range makes `copyin` succeed, so
`write()` returns ≥ 0 and the assertion trips. Fix: move the page one page
below the asserted range, to `TRAPFRAME - 3*PGSIZE` (`0x3fffffb000`); its
level-0 vmprint index is therefore **507**, not 509. Moral (now also a
teaching point in the handout): choosing a fixed VA means auditing everything
else that asserts invariants about that region — and only an end-to-end
regression catches the collision.

## Verification (as recorded by the course designer)

- Starter tree: builds and boots; `run.sh` reports `vmprint`, `ugetpid`, and
  `cowtest` as FAIL (unimplemented), `forkbench` PASS (eager fork works). No
  hangs.
- Solution tree: all lab-tier tests PASS (vmprint, ugetpid, cowtest,
  forkbench) and, after the USYSCALL address fix, one full `usertests -q`
  run PASSES on a freshly regenerated `fs.img` (stale images produce
  spurious `iref` failures).
- `forkbench` (100 fork+wait of a 4 MB parent): eager fork vs. copy-on-write
  tick counts are reported in the top-level run logs — COW is dramatically
  cheaper because a fork-then-exit child never copies the parent's pages.
