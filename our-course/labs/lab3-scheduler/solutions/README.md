# ⚠️ SPOILERS — Lab 3 reference solution ⚠️

```
╔══════════════════════════════════════════════════════════════════════╗
║  STOP. This directory contains the complete reference solution for    ║
║  Lab 3. Do the lab yourself first. Implementing a scheduler is the    ║
║  whole point of the exercise; reading this before you have struggled  ║
║  with it throws that away. You have been warned.                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## What's here

- `solution.patch` — a unified diff (`-p1`) against a tree created by
  `starter/setup.sh`. It touches only these five kernel files: `defs.h`,
  `proc.h`, `proc.c`, `trap.c`, `sysproc.c`.
- `apply.sh <workdir>` — dry-runs then applies `solution.patch` to a starter
  tree. Does not build; run `make SCHED=<policy>` afterwards.
- `files/` — the full post-solution versions of the five changed kernel files,
  for reading without applying the patch.

## Apply and test

```
./starter/setup.sh ~/lab3-sol
solutions/apply.sh ~/lab3-sol
tests/run.sh ~/lab3-sol           # expect all PASS (incl. usertests -q x2)
```

## Design notes

**Compile-time selection.** `scheduler()` in `proc.c` branches on
`SCHED_PRIO` / `SCHED_LOTTERY` / `SCHED_MLFQ`, with the `#else` being the stock
RR loop (unchanged, so `usertests` under `SCHED=RR` is unaffected). A one-line
banner `scheduler: <NAME>` is printed once at first entry so the grader can
confirm which policy booted. Accounting and `getpstat` are policy-independent
and always compiled in.

**Task 0 — instrumentation.** New `struct proc` fields `rtime`, `ctime`,
`stime`, `etime` (plus policy inputs `priority`, `tickets`, `mlfq_level`,
`mlfq_used`, and the RR-fairness stamp `lastrun`) are initialised in
`allocproc()`. `ctime` is stamped there, `stime` the first time the scheduler
dispatches the process, `etime` in `kexit()`. `sched_charge()` (called from the
timer path in `trap.c`, just before `yield()`) bumps `rtime` for the running
process; the bump is lock-free because that CPU is the field's only writer.
`kgetpstat()` fills a `static struct pstat` (too big for the kernel stack) under
each `p->lock` and `copyout`s it.

**Task 1 — static priority.** `SCHEDKEY = priority`. The scheduler does two
passes: find the minimum key among `RUNNABLE` procs, then dispatch the proc
with that key that has waited longest (smallest `lastrun` stamp; a global
`sched_seq` counter is stamped on each dispatch). Using "least-recently-run"
rather than a single shared cursor is what makes RR-among-equals fair even when
a higher-priority process (e.g. one waking every tick in `pause`) is
interleaved — a shared cursor gets reset by that process every tick and starves
one of the equals (this is the subtle bug the handout hint warns about).
`ksetpriority(pid, prio)` rejects an out-of-range priority (no clamping),
returning −1 for a bad value or unknown pid.

**Task 2 — lottery.** `settickets` (`ksettickets`) sets the caller's tickets
(≥ 1) and is inherited across `fork` (copied in `kfork`, alongside priority).
The scheduler sums `RUNNABLE` tickets, draws with a fixed-seed xorshift64 PRNG
(`rand_next`), and walks the table subtracting ticket counts until the winner
is found. Over a long window this gives shares proportional to tickets; with a
fixed seed the sequence is reproducible.

**Task 3 — MLFQ.** `SCHEDKEY = mlfq_level` reuses the same
best-key + least-recently-run selection as PRIO (so RR-within-level is fair).
`NMLFQ = 3`, quanta `{1, 2, 4}` ticks (`mlfq_quantum`). `sched_charge()` bumps
`mlfq_used` each tick and demotes a level once it reaches the quantum (bottom
stays bottom). `sleep()` clears `mlfq_used` (voluntary block ⇒ stay at level,
no penalty), which keeps I/O-bound processes near the top. `mlfq_maybe_boost()`
(called from `scheduler()`, gated to fire ~every `MLFQ_BOOST = 100` ticks)
resets every process to the top level, un-starving anything stuck at the
bottom. New processes are not born with an inherited level — `allocproc` puts
them at the top.

## Representative measured results (CPUS=1)

From the reference solution on this course's setup (numbers are noisy):

- **PRIO** `priotest 40`: `PRIOSTARVE high=39 low=1` (≈97% to the high-priority
  spinner); `PRIOEQUAL a=20 b=20`.
- **LOTTERY** `lotto 150`: `ticks1=110 ticks2=40`, ratio ≈ 2.75 (target 3.0,
  tolerance [2.0, 4.5]).
- **MLFQ** `mlfqtest 60`: `MLFQ-RESPONSE io_level=0 hog_level=2` (I/O stays top,
  CPU hog sinks to bottom, so I/O gets far better response); `MLFQ-BOOST
  bottom_seen=1` with the bottom hog's ticks still climbing across boosts.
- **Regression:** `usertests -q` passes under both `SCHED=RR` and `SCHED=MLFQ`.

## Known limitations

- Proportional/priority *shares* are only meaningful on a single contended CPU;
  the policies are written to be deadlock-free and correct on multiple CPUs
  (`usertests` runs with `CPUS=3`), but fairness is only guaranteed with
  `CPUS=1`, which is what the grader and the demos use.
- `sched_seq` (the dispatch counter) is 32-bit and wraps after ~4 billion
  dispatches, causing at most a one-off transient unfairness; irrelevant for a
  lab.
- `kgetpstat`'s `static` staging buffer is not protected by a lock: two
  processes calling `getpstat` concurrently race on it and can copy out a torn
  snapshot. Benign at `CPUS=1` (the grader's configuration for the scheduling
  demos), but a known limitation.
