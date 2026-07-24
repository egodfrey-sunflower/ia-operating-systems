# Lab 5 Part 2 — Locks, from atomics upward

**Week 12 · 3.5 hours · 19% of Lab 5 · OSTEP ch. 28; xv6 rev5 §7.6 (memory barriers)**

x86-64 Linux, userspace C, one source file of about 150 lines. You will need
`gcc`, `make` and `valgrind` — including its development headers, which is the
same package.

Three locks over the same nine-function interface: a spin lock on
test-and-set, a ticket lock on fetch-and-add, and a lock whose waiters sleep
in the kernel instead of spinning. Then measure all three and write down what
the shape of the numbers means.

This is the part students consistently underestimate, and the reason is worth
stating before you start: **an incorrect lock works.** It works on your
machine, on your workload, for hours. A lock built from plain loads and stores
instead of atomics passes most of a test suite. A lock missing its release
barrier passes all of it, on x86-64, today. The difficulty here is not writing
sixty lines of code; it is that nothing tells you when you have got it wrong.
The handout is explicit throughout about which checks can catch which mistakes
and which cannot.

## Layout

```
part2-locks/
  README.md          this handout
  starter/           Makefile, mylock.h (the contract), hbnotate.h, mylock.c  <- work here
  tests/run.sh       the autograder
  tests/bench.c      the measurement driver behind LOCKS.md; `make bench` builds it
  solutions/         SPOILERS. Reference locks, model LOCKS.md, answer key. Later.
```

```sh
cp -r starter tests ~/lab5/p2
cd ~/lab5/p2/starter
make test
```

## What you hand in

| File | Weight within Part 2 |
|---|---|
| `mylock.c` — the three locks | 70% |
| `LOCKS.md` — the measurements and what their shape means | 30% |

Seventeen cases. Sixteen are machine-checked in full. The seventeenth checks
only that `LOCKS.md` mentions all three locks and contains at least twelve lines
with a number in them — it says nothing about whether the report is any good,
and the report is marked against the rubric in `solutions/README.md`, which
you read afterwards. **The untouched starter scores `6 passed, 8 failed, 3
skipped`.** The six it passes are the negative control (which is about the
machine, not about your locks), the three "two locks are two locks" cases (a
lock that does nothing never blocks, so a second lock is always free), the
ticket lock's arrival-order case (six threads staggered 40 ms apart arrive in
order and are served in order with no lock at all), and the sleeping lock's
processor-time case (a lock that does nothing does not spin either). It fails
the spin lock's processor-time case for the same reason it passes the sleeping
lock's: a lock that never makes anyone wait burns nothing while waiting. The
three skips are the helgrind cases, each gated on the plain run it instruments.
Every case that a lock doing nothing *can* fail, it fails.

## The contract

`starter/mylock.h`. Nine functions, three structs, no other API. The harness
never looks inside a lock; it checks five behaviours:

- **a shared counter incremented under the lock reaches exactly *N*×*M***;
- **two threads are never inside the critical section at the same time**;
- **the ticket lock hands over in arrival order** — six threads that arrive
  40 ms apart get the lock back in the order they arrived. A test-and-set lock
  does not do that;
- **the sleeping lock's waiters burn no processor time** — six threads wait
  400 ms for one held lock, and the process's processor time over that window
  is measured. Spinning shows up as CPU seconds. Sleeping does not;
- **the spin lock's waiters do burn it** — the same measurement with the sense
  reversed: four threads wait 200 ms for the spin lock, and a wait that costs
  no processor time means the waiters parked rather than spun.

The last three are what keeps the part honest about having built three
different locks rather than one lock three times: everything else here asks
only for correct mutual exclusion, which all three designs provide equally.
One consequence is worth knowing before you meet it: a spin-then-park hybrid
is a fine lock and a good idea, and it is not the lock Part 2 asks for — the
spin lock spins.

Two locks of the same kind must be two locks: while one is held, the other
must still be free. That is checked, and it rules out keeping the state in a
file-scope variable.

## The memory-ordering rule

There is one rule and it has two halves:

> The atomic that **wins** the lock is `__ATOMIC_ACQUIRE`. Nothing may move
> from after it to before it, or the critical section would begin before the
> lock was held.
>
> The atomic or store that **gives up** the lock is `__ATOMIC_RELEASE`.
> Nothing may move from before it to after it, or the next holder could see a
> half-finished write.

On x86-64 the hardware already gives ordinary loads and stores acquire and
release ordering. So on this machine, what those two constraints mostly bind
is the **compiler** — and that is quite enough to ruin your afternoon. At
`-O2`, gcc looking at a loop that reads a variable which nothing in the loop
writes is entitled to read it once and spin on the register copy. Your lock
then waits for ever for a change that already happened. The graded build is
`-O2` for exactly this reason, and if you write the spin loop with a plain
`while (l->flag)` you will meet it.

The reason to write the barriers correctly even where x86-64 does not need
them is in xv6 §7.6 and it is not "portability" in the abstract: it is that a
lock without them is a lock whose correctness depends on facts about the
processor that are nowhere in the code, and the machine that breaks it is a
machine you will not be sitting at.

Everything you need is a gcc builtin. No `<stdatomic.h>`, no inline assembly:

```c
__atomic_exchange_n(&x, v, order)                 /* old value; sets x = v  */
__atomic_fetch_add(&x, n, order)                  /* old value; adds n      */
__atomic_load_n(&x, order)                        /* a read, ordered        */
__atomic_store_n(&x, v, order)                    /* a write, ordered       */
__atomic_compare_exchange_n(&x, &exp, new, 0, s, f)
```

## helgrind here: what it buys, and what it does not

`valgrind --tool=helgrind` is required for every part of this lab, and Part 2
is the one place where you should be clear about its limits, because
overestimating them is worse than not running it.

helgrind knows what `pthread_mutex_lock` means because it intercepts the call.
It cannot know what *your* lock means: your lock is a loop around an atomic
instruction, and there is nothing to intercept. Run helgrind over a perfectly
correct hand-built spin lock with no annotations and it reports a data race on
every byte the lock protects — thousands of them, all false.

So you annotate. `hbnotate.h` gives you three macros that wrap the standard
valgrind client requests, and the starter says where each one goes. They cost
nothing outside valgrind and they are what real projects with hand-built locks
do.

**And now be honest about what that means.** The annotations are assertions,
not measurements. They tell helgrind that your lock establishes an ordering;
helgrind believes you. A lock built out of plain loads and stores, with the
annotations correctly placed, gets a clean helgrind run — that was checked by
building one. helgrind cannot audit your atomics and this handout does not
claim it can. The counter cases are what catch a lock that does not exclude.

What helgrind finds here, and what nothing else in Part 2 finds, is **shared
state you never put inside a critical section at all**: the variable read just
before the acquire, the flag updated just after the release, the counter one
thread touches without taking the lock. Those survive a thousand green outcome
runs. That is why the run is required.

In Part 3, where the primitives are the pthreads ones, helgrind has no such
blind spot and its verdict is worth much more.

## The three locks

**Spin lock.** Loop until an atomic exchange of 1 into the flag returns 0.
Then improve it: that version writes the cache line on every attempt, and a
write takes the line away from every other core each time. Spin on a plain
atomic *load* until the lock looks free, and only then try the exchange —
test-and-test-and-set. You will see the difference in `make bench`.

**Ticket lock.** Take a number with fetch-and-add; wait until it is called.
Two counters, five lines, and it gives you a property the spin lock does not
have: threads are served in the order they arrived. Decide which of the two
counter accesses needs acquire ordering before you write it.

**Sleeping lock.** A waiter that spins on a lock held for milliseconds burns a
core to no purpose. This one parks in the kernel with `futex`, using the
three-state protocol from OSTEP ch. 28: 0 free, 1 held with nobody waiting, 2
held with someone parked. The two futex calls are given to you — the system
call interface is not the lesson. The third state is: it is what makes an
uncontended release a plain store rather than a system call.

The two things to get right, and they are both about lost wakeups:

- the word must say 2 **before** you park. A releasing holder that sees 1
  concludes nobody is waiting and skips the wake; park while the word still
  says 1 and the wake never comes. `futex_wait` checks the value atomically
  with going to sleep precisely so you can close that window.
- when `futex_wait` returns, **try again**; do not assume the lock is yours.
  It returns for a wake, for a signal, and for no reason at all. This is the
  same rule as Part 3's `while` loop, arriving a week early.

## Measuring: `LOCKS.md`

```sh
make bench          # builds ../tests/bench.c against your locks
./bench             # twelve rows: three locks at 1, 2, 4 and 8 threads
```

`bench` runs each configuration for a fixed wall time and counts how many
acquire/release pairs got through — fixed time and not fixed iterations,
because one of these locks can convoy badly enough that a fixed iteration
count does not finish this afternoon, and a benchmark you have to kill
produces no number at all.

`LOCKS.md` is a short report: the table, and what its shape means. What is
being asked for is an argument about the shape, not a ranking.

**Know what the numbers are worth before you write about them.** On a two-core
machine this measures the lock, the scheduler and whatever else is running,
and only one of those is the subject. Run each configuration several times.
Measured here, run-to-run spread on the contended points is around a factor of
two, and on the ticket lock above two threads it is far larger than that,
because a run either convoys or does not. **Treat anything under a factor of
two as no result.** What is stable, and what the report should be about, is
which lock degrades as threads are added, which does not, and why.

One observation is worth expecting rather than discovering by accident: a
**fair** spin lock is catastrophic when there are more runnable threads than
cores. The thread whose turn it is may be off the processor, and every other
waiter burns a full scheduler timeslice finding that out. That is not a defect
in your ticket lock; it is the argument for the sleeping lock, and OSTEP ch. 28
makes it in those terms. For the same reason the harness gives the two
spinning locks one thread per core in the cases that hammer them for millions
of acquisitions, and reserves the high thread counts for the sleeping lock — a
case that punished you for a true property of ticket locks would be a bad case.
The two cases that do run more threads than cores on a spinning lock ask each
thread for a single acquisition, so the whole convoy is a handful of handovers
and it ends.

## Running the tests

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory
```

The graded build is `run.sh`'s own `gcc` line, not your Makefile. The whole
suite takes about thirteen seconds, most of it in the three helgrind runs.

**Run it more than once.** Every case here is probabilistic. Measured on a
two-core machine, a correct set of locks passed all seventeen cases on each of
twenty-five consecutive runs — that is the standard to hold yourself to, and
one failure in twenty-five is worth investigating rather than re-running away.

The suite opens with a **negative control**: the same workload with no lock at
all, which must produce the wrong answer. If it does not, the machine cannot
lose an update in this loop today — usually because only one core is available
to the process — and everything after it is theatre. The harness says so.

## If you get stuck

1. **`DEADLOCK: no progress in 20s`** — not a wrong answer, no answer. A
   release path that does not free the lock; a spin loop the compiler hoisted
   the load out of (see the memory-ordering rule); a sleeping lock that parked
   a waiter without leaving a mark for the releaser, so no wake was sent.
2. **`the counter is at N, not M — K updates were lost`** — two threads were
   inside the critical section together. Check that the acquire is one atomic
   operation, not a read followed by a write.
3. **`a thread found another thread's id in the critical section`** — the same
   fault, caught directly rather than through the counter.
4. **`a second, separate lock of the same kind could not be acquired`** — the
   lock's state is in a file-scope variable rather than in `*l`.
5. **`the threads arrived ... and were served in the order ...`** — the ticket
   lock is not a ticket lock. Take a number with fetch-and-add on `ticket`,
   wait for `serving` to reach it, and advance `serving` on release.
6. **`they are spinning, not sleeping`** — the sleeping lock never reaches
   `futex_wait`. A short spin before parking is fine; a loop that never parks
   is not.
7. **`this one is going to sleep instead`** — the opposite complaint about the
   other lock: `spinlock_acquire` blocks in the kernel. A test-and-set lock
   loops on the flag, and the case measures that it does. If the two locks in
   your `mylock.c` share an implementation, this is the case that says so.
8. **`helgrind found a data race`** — first check the annotations are where
   `hbnotate.h` says: `MYLOCK_HG_ACQUIRED` where acquire is about to return,
   `MYLOCK_HG_RELEASING` immediately *before* the store that frees the lock,
   `MYLOCK_HG_INIT` in init. Announcing a release you have not performed
   describes a lock you did not build. If they are right, helgrind is telling
   you about something genuinely outside a critical section.
9. **`helgrind ... SKIPPED: <valgrind/helgrind.h> is not installed`** — the
   annotations compiled to nothing, so the run was not worth making. Install
   valgrind; the header ships with it.
10. **`an unsynchronised counter really does lose updates` FAILED** — nothing
   is wrong with your code. The machine could not produce a lost update in
   twenty attempts, so the outcome cases cannot distinguish a working lock
   from a missing one right now. Close something, or find a machine with two
   cores free, and run again.
11. **`build produced warnings`** — the graded build is `-Werror` and is not
    your Makefile's. `(void)l;` placeholders left over from the starter will
    do this once the real code is there.
12. **`LOCKS.md has only N line(s) with a number in them`** — `make bench`
    produces twelve rows and the report is expected to contain them.

## Stretch goal

Implement an MCS queue lock — each waiter spins on a flag in its own cache
line, and the holder hands the lock to a named successor. `bench.c` will
measure it unchanged if you give it the same three-function shape. The
interesting question is not whether it is faster at two threads (it is not)
but what happens to the shape of the curve.
