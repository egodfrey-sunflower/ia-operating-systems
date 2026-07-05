# ⚠️ SPOILERS — Lab 5 reference solution ⚠️

```
╔══════════════════════════════════════════════════════════════════════╗
║  STOP. This directory contains the complete reference solution for    ║
║  Lab 5 (all three parts). Do the lab yourself first. The whole point  ║
║  of threads-and-locks is the struggle with races and deadlocks;       ║
║  reading the answer throws that away. You have been warned.           ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## What's here

- `solution.patch` — a unified diff (`-p1`) against a tree created by
  `starter/setup.sh`, covering the **xv6** parts (A and B). It touches five
  files: `user/uthread.c`, `user/uthread_switch.S`, `kernel/kalloc.c`,
  `kernel/bio.c`, `kernel/buf.h`.
- `apply.sh <workdir>` — dry-runs then applies `solution.patch` to a starter
  tree. Does not build; run `make` (or the autograder) afterwards.
- `files/` — the full post-solution versions of the five xv6 files, for reading
  without applying the patch.
- `hostsync/` — the **Part C** reference solutions (`bbuffer.c`, `barrier.c`,
  `rwlock.c`, `Makefile`). These build with plain `make` on Linux; there is no
  patch for Part C because it is standalone.

## Apply and test

```
./starter/setup.sh ~/lab5-sol
solutions/apply.sh ~/lab5-sol
CPUS=3 tests/run.sh ~/lab5-sol solutions/hostsync   # expect everything PASS
```

## Design notes

### Part A — uthread

`thread_switch` is `kernel/swtch.S` verbatim (save `ra`, `sp`, `s0`–`s11` into
`*old`; load from `*new`; `ret`). `thread_create` plants `ctx.ra = func` and
`ctx.sp = stack + STACK_SIZE`, so the first switch's `ret` lands in `func` on a
fresh stack. `thread_schedule` calls `thread_switch(&prev->ctx,
&next->ctx)`. Only callee-saved state matters because the switch happens at a
function-call boundary, where the caller-saved registers are already dead by the
calling convention — the same reason the kernel's process switch saves so
little. Compared with a process switch, `thread_switch` does **not** change the
page table (`satp`)/flush the TLB, does not switch kernel stacks or trapframes,
and does not touch `p->lock` or the scheduler — which is exactly why it is
cheap.

### Part B.1 — per-CPU kmem (`kalloc.c`)

`struct { spinlock lock; run *freelist; } kmem[NCPU]`. `kfree` pushes onto the
current CPU's list; `kalloc` pops from it. All pages start on the boot CPU's
list (that is where `freerange` runs), so other CPUs steal as they warm up.
When a CPU's list is empty, `ksteal` walks the other CPUs, takes **one** other
lock at a time (never two — no deadlock), unlinks a batch of up to
`STEAL_BATCH` (64) pages, keeps one for the caller and splices the rest onto the
local list. `cpuid()` is read under `push_off()/pop_off()` so a timer interrupt
cannot migrate us mid-operation. Batch stealing keeps cross-CPU acquisitions
rare, so kmem contention collapses (see numbers below).

### Part B.2 — bucketed bcache (`bio.c`, `buf.h`)

`NBUCKET = 13` buckets, each a locked doubly-linked list; `HASH(blockno) =
blockno % NBUCKET`. `struct buf` gains a `lastuse` timestamp, set to `ticks` in
`brelse` when refcnt reaches 0. A hit takes only the block's bucket lock. A miss:

1. release the bucket lock, then scan **all** buckets for the refcnt==0 buffer
   with the smallest `lastuse`, holding **at most one bucket lock at a time**
   (lock bucket j, scan it, release, move on). One lock at a time means there
   is no lock-ordering cycle to build — deadlock is impossible by
   construction — and the short holds keep the scan from serialising other
   CPUs' hits.
2. re-lock the chosen victim's bucket and **re-validate** it (still on that
   bucket's list, still refcnt==0): another CPU may have taken or moved it in
   the gap since the scan. If it changed, rescan; otherwise unlink it and
   release the lock.
3. take the target bucket lock and re-check for the block: if another CPU
   cached it while we were scanning, use theirs and return the victim to the
   bucket as a free buffer (no double-caching); otherwise repurpose the victim.

The invariants that make this safe: a buffer lives on exactly one bucket list,
every list mutation happens under that bucket's lock, and no CPU ever holds two
bucket locks at once — so the cache cannot deadlock, and the claim-time
re-validation plus the insert-time duplicate check close the scan/claim races.

### Part C — host pthreads (`hostsync/`)

- **bbuffer** — mutex + `not_full`/`not_empty` condvars, `while`-predicate
  waits, poison-pill shutdown; the stress test checks item conservation over
  200 000 items.
- **barrier** — generation-counter idiom: the last arriver bumps `generation`,
  resets `count`, and broadcasts; the rest wait `while (generation ==
  my_generation)`. The test asserts every thread is in the same round at each
  crossing.
- **rwlock** — writer preference: readers wait while a writer is active or any
  writer is waiting; writers wait while any reader or writer is active. The test
  shows readers sharing (peak concurrent readers > 1) and a bounded writer wait
  (writers are not starved by the reader stream).

## Sample contention numbers (CPUS=3)

Measured by the built-in tests via `statistics()` (contended acquisitions,
i.e. `#test-and-set`, over each test's workload):

| Lock subsystem | Stock big lock | This solution |
|----------------|---------------:|--------------:|
| kmem (kalloctest test1)   | ~440000–680000 | 0 |
| bcache (bcachetest test0) | ~27000–31000   | 0 |

(Exact stock numbers vary run to run with scheduling; the solution's counts are
0 or near-0 because each CPU/bucket has a private lock and cross-CPU stealing /
cross-bucket eviction is rare.) The tests embed the pass/fail threshold, so
trust their `OK`/`FAILED` verdict rather than a hard-coded number; the point is
the multiple-orders-of-magnitude drop.
