# Lab 3 — xv6 scheduling

**Weeks:** 5–7 · **Budget:** 12–16 hours (over 3 weeks) · **Track:** kernel · **Weight:** 5%

**Syllabus:** IA L4–L5 (scheduling) ·
**Reading:** OSTEP ch. 7 (scheduling), ch. 8 (multi-level feedback queue),
ch. 9 (lottery/proportional-share); Waldspurger & Weihl (1994), "Lottery
Scheduling" (reading-list item 14); the xv6 book, ch. 7 (scheduling). See
`../../reading-list.md`.

In Lab 2 you added system calls; now you replace the heart of the kernel's
CPU-sharing policy. Stock xv6 runs a single round-robin loop over the process
table. You will (0) instrument the scheduler so you can *measure* what it does,
then implement three classic policies selectable at compile time: (1) static
priority, (2) lottery scheduling, and (3) a multi-level feedback queue.

---

## Background

xv6's scheduler is dead simple. Each CPU runs `scheduler()` in
`kernel/proc.c`: it walks `proc[]`, and the first `RUNNABLE` process it finds
is switched to via `swtch()`. A timer interrupt (`kernel/trap.c`,
`which_dev == 2`) calls `yield()` on *every* tick, so every process gets a
one-tick time slice in round-robin order. That is the whole policy.

Everything you need to change lives in three files:

```
kernel/proc.c   scheduler(), allocproc(), kfork(), kexit(), sleep()
kernel/trap.c   the timer-interrupt path that calls yield()
kernel/proc.h   struct proc  (add your per-process bookkeeping fields)
```

plus the syscall plumbing you already know from Lab 2 (`kernel/sysproc.c`,
`kernel/defs.h`).

A clock tick here is about a tenth of a second (`clockintr()` programs the next
timer for `r_time() + 1000000`). `ticks` (in `kernel/trap.c`) counts them; read
it for timestamps. `uptime()` exposes it to user space, and `pause(n)` sleeps
for `n` ticks (the sleep syscall here is named `pause`; the xv6 book calls it `sleep`).

> **Naming note.** Some names differ from the xv6 book: the kernel print
> routine is `printk` (not `printf`), the process primitives are named
> (`kfork`, `kexit`, `kwait`, `kexec`, `kkill`), `sbrk` supports a lazy mode,
> and the user-space sleep call is `pause(n)`. Read the real code — these are
> the names you will see.

### Selecting a scheduler at compile time

The policy is chosen by a make variable that the starter wires up:

```
make SCHED=RR        # classic round robin (default; stock behaviour)
make SCHED=PRIO      # static priority          (Task 1)
make SCHED=LOTTERY   # lottery scheduling        (Task 2)
make SCHED=MLFQ      # multi-level feedback queue (Task 3)
```

This adds `-DSCHED_<policy>` to the kernel `CFLAGS`, which reaches
`kernel/proc.c`. Your `scheduler()` uses `#if defined(SCHED_PRIO)` …
`#elif` … `#else` to select the code path; the `#else` (RR) must keep the
same selection policy as stock — apart from the Task 0 instrumentation,
which touches every policy — so the regression tests keep passing. **Run
`make clean` when you switch `SCHED` values** — make does not track `CFLAGS`
changes, so stale object files would otherwise be linked.

### Measuring: `getpstat` and the tools

You cannot tune what you cannot see. Task 0 adds a `getpstat()` system call
that copies out a `struct pstat` (defined in `kernel/pstat.h`, already in your
tree) with, for every process slot: pid, state, run-time ticks, priority,
tickets, MLFQ level, and creation/first-run/exit timestamps. The starter ships
three user programs that use it (all complete — you do **not** write them):

- **`pstat`** — dumps the `struct pstat` table; run it in one shell while a
  workload runs in another.
- **`schedload [ncpu] [nio] [ticks]`** — a workload generator: forks `ncpu`
  CPU-bound children and `nio` "I/O-bound" children (short compute, then
  `pause(1)`), each of which reports its own run time at exit.
- **`priotest` / `lotto` / `mlfqtest`** — self-checking demos for Tasks 1–3
  that print `... VERDICT=PASS|FAIL` lines. These are exactly what the
  autograder greps.

Until you implement `getpstat`, all of these report that the call is
unimplemented, and the scheduler behaves as plain round robin under every
`SCHED` value.

---

## Setup

From this directory, create a fresh working tree (a copy of the vendored xv6
with the lab's starter files overlaid):

```
./starter/setup.sh ~/lab3
cd ~/lab3
make qemu                       # boots xv6 (RR); quit with Ctrl-a x
make clean && make qemu SCHED=MLFQ   # boot a different policy
```

`setup.sh` refuses to overwrite an existing directory. The starter tree
**builds and boots under all four `SCHED` values**; the new system calls are
stubbed (they return −1) and the scheduler is stock RR until you implement the
tasks.

Run the autograder at any time:

```
tests/run.sh ~/lab3           # full run (all policies + usertests -q)
QUICK=1 tests/run.sh ~/lab3   # skip the long usertests regressions
```

The autograder builds the tree once per policy (with `make clean` between),
boots each under QEMU, and greps the `VERDICT=` lines. Scheduling is noisy, so
it retries a borderline check once before failing.

One boot-banner contract: your kernel must print a one-line banner
`scheduler: <NAME>` (e.g. `scheduler: LOTTERY`) when the scheduler first
runs, naming the policy it was compiled with. Whenever the autograder sees
that banner it cross-checks the name against the `SCHED=` value it just
built — a mismatch (usually stale objects from a missing `make clean`) is an
automatic FAIL. The starter prints no banner, so the graders skip the
cross-check until you add it; add it along with the Task 0 instrumentation.

---

## Tasks

### Task 0 — Instrumentation (15%)

Add per-process accounting and expose it.

1. Add fields to `struct proc` (`kernel/proc.h`): total ticks scheduled
   (`rtime`), creation time (`ctime`), first-scheduled time (`stime`, −1 until
   first run), completion time (`etime`), plus the policy inputs later tasks
   need (`priority`, `tickets`, and an MLFQ `level`). Initialise them in
   `allocproc()`; set `ctime` from `ticks` there, `stime` the first time the
   scheduler dispatches the process, and `etime` in `kexit()`.
2. Charge run time: on each timer tick, before the scheduler yields, add one to
   the running process's `rtime`. (A small helper called from the
   `which_dev == 2` path in both `usertrap()` and `kerneltrap()` is the clean
   spot.)
3. Add `int getpstat(struct pstat *)`: fill a `struct pstat`
   (`kernel/pstat.h`) — one entry per `proc[]` slot — and `copyout` it. Return
   0 on success, −1 if the copyout fails.

The provided `pstat` tool should now print a sensible table, and `schedload`
should report each child's run time. This works under every `SCHED` value.

### Task 1 — Static priority (25%)

Add `int setpriority(int pid, int prio)` (0 = highest … 9 = lowest, default 5)
and, under `-DSCHED_PRIO`, a scheduler that always runs a highest-priority
`RUNNABLE` process, round-robin among equals. A `prio` outside `[0, 9]` is
rejected, not clamped: return −1 for a bad priority or an unknown pid.

**Deliverable — demonstrate starvation.** On one CPU, a high-priority spinner
must starve a low-priority one. The provided `priotest` shows this
(`PRIOSTARVE`: the high-priority process should get essentially all the ticks)
and also checks round-robin fairness among equal priorities (`PRIOEQUAL`).

*Hint on fairness among equals:* a single shared next-index cursor is not
enough — run `priotest` and work out why it keeps resetting.

### Task 2 — Lottery scheduling (30%)

Read Waldspurger & Weihl (1994). Add `int settickets(int n)` (sets the
*calling* process's ticket count; a request below 1 is clamped up to 1 and
still returns 0; **inherited across `fork`**) and, under `-DSCHED_LOTTERY`, a scheduler that holds a lottery among
`RUNNABLE` processes each time it picks: sum their tickets, draw a random
number in `[0, total)`, and run the process holding the winning ticket. You
need a PRNG in the kernel — a small xorshift with a fixed seed is fine (seed
deterministically so runs are reproducible).

**Deliverable — proportional share.** With two identical spinners holding 30
and 10 tickets on one CPU, the tick-share ratio should converge near 3:1 over a
long-enough window. The provided `lotto` measures it (`LOTTERY`, PASS iff the
ratio is in [2.0, 4.5] — lotteries are noisy, so use a long window).

### Task 3 — Multi-level feedback queue (30%)

Read OSTEP ch. 8. Under `-DSCHED_MLFQ`, implement three queues with
round-robin time slices that double per level (e.g. 1 / 2 / 4 ticks):

- A new process enters the **top** queue.
- Charge the running process a tick each timer interrupt; when it has used a
  **full quantum** at its level, **demote** it one level (bottom stays bottom).
- A process that **blocks voluntarily** (e.g. an I/O `pause`) before using its
  quantum **stays** at its level (clear its partial-quantum counter so it is
  not penalised) — this keeps interactive/I/O-bound work near the top.
- **Periodically boost**: every ~100 ticks, move every process back to the top
  queue. This is what fixes the Task-1-style starvation.
- Schedule: run a process from the highest non-empty queue, round-robin within
  a level.

**Deliverable — response and un-starving.** The provided `mlfqtest` checks that
an I/O-bound process sits in a strictly higher queue than a CPU hog
(`MLFQ-RESPONSE` — better response), and that a hog which has sunk to the
bottom queue keeps accruing ticks across boosts rather than being frozen out
(`MLFQ-BOOST`). Also try the mixed workload with `schedload` and eyeball the
I/O-bound children's run times vs the CPU hogs' under MLFQ vs RR.

---

## Hints

- **Where the tick is charged.** xv6 already yields on every timer tick. For RR
  and PRIO and LOTTERY that is all you need — the policy only changes *which*
  runnable process the scheduler picks. For MLFQ the per-tick charge also
  drives demotion; the time slice governs *when a process is demoted*, while
  the scheduler still re-picks the best queue each tick.
- **Locking.** Follow stock `scheduler()`'s discipline: hold `p->lock` only
  while inspecting/dispatching one process, never two at once. The per-tick
  counters (`rtime`, quantum-used) have a single writer — the CPU running that
  process — so bumping them lock-free from the timer path is fine. Your
  periodic boost can lock one `p->lock` at a time, exactly like `wakeup()`.
- **`getpstat` staging.** A `struct pstat` is a few kilobytes — too big for the
  kernel stack. Think about where the kernel-side copy can live before you
  `copyout` it.
- **Reading `ticks`.** Reading the global `ticks` word for a timestamp needs no
  lock.
- **Determinism.** Pin scheduling experiments to one CPU (`make qemu CPUS=1`,
  which the autograder uses); shares are only meaningful when a single core is
  contended. Seed your lottery PRNG to a constant.
- **Keep RR stock.** Put the new policies behind `#if defined(SCHED_…)` and
  leave the `#else` branch's selection logic identical to the original loop
  (the Task 0 instrumentation aside), or `usertests -q` (which runs under
  `SCHED=RR`) may regress.
- **Iterate cheaply.** The full `usertests -q` takes several minutes per
  `SCHED` value — don't run it in your edit-compile loop. Iterate with
  `QUICK=1 tests/run.sh …` (skips the regression tier) or by booting and
  running `priotest`/`lotto`/`mlfqtest` by hand, and probe one risk area fast
  with a *named* subtest from the xv6 shell: `usertests preempt`,
  `usertests forkfork`, `usertests reparent`, `usertests exitwait`. Pay for
  the full regression once, at the end.

---

## Deliverables

1. Your modified xv6 tree (the changed kernel files), or a patch of them,
   building and booting under all four `SCHED` values.
2. A short **lab report**: a table (or plot) of measured tick share vs tickets
   for the lottery scheduler, and of response times / run-time shares for the
   mixed `schedload` workload under MLFQ vs RR. Generate the numbers with
   `pstat`/`schedload`; plotting is optional and happens on the host
   (matplotlib, gnuplot, …), not inside xv6. Comment on how your measurements
   match (or don't) the theory in the reading.
3. `tests/run.sh` passes on your tree.

---

## Rubric (100%)

| Task | Weight | What is graded |
|------|-------:|----------------|
| 0 — instrumentation | 15% | `getpstat` copies out correct per-proc stats; `rtime`/`ctime`/`stime`/`etime` tracked; `pstat`/`schedload` work under all policies. |
| 1 — static priority | 25% | Highest-priority `RUNNABLE` runs; RR among equals; demonstrated starvation. (`PRIOSTARVE`, `PRIOEQUAL`) |
| 2 — lottery | 30% | `settickets` inherited across fork; per-tick lottery; 30:10 share converges to ~3:1. (`LOTTERY`) |
| 3 — MLFQ | 30% | 3 queues, doubling quanta, demote on full quantum, stay on voluntary block, periodic boost that un-starves; I/O beats CPU hog on response. (`MLFQ-RESPONSE`, `MLFQ-BOOST`) |
| Regression | gate | `usertests -q` must still pass under `SCHED=RR` and `SCHED=MLFQ`. A change that breaks the kernel forfeits the coding marks. |

The autograder prints a PASS/FAIL table and exits non-zero on any failure.
`QUICK=1` skips `usertests -q` while you iterate; the final check runs it.
Because scheduling is stochastic, the grader retries a borderline verdict once;
if your policy is correct but a run is unlucky, just re-run.
