# Lab 5 Part 4 — A synchronisation toolkit

**Week 14 · 3.5 hours · 19% of Lab 5 · OSTEP ch. 31–32**

x86-64 Linux, userspace C, one source file of about 150 lines. You will need
`gcc`, `make` and `valgrind`.

A counting semaphore, built on a mutex and a condition variable. Then four
classic structures built on the semaphore and on nothing else: a reusable
barrier, a rendezvous, a reader-writer lock that does not starve writers, and
a table of philosophers that cannot deadlock. Then `DEADLOCK.md`, which says
which of the four necessary conditions each solution breaks.

The semaphore is twenty lines and you have already written most of it in Part
3. The rest of the time goes on the four structures, and the reason they are
worth the time is that they are all the same primitive used four ways — which
is chapter 31's whole argument, and it does not land by being read.

**Sheet 14's `threads-sema/` and `threads-bugs/` code homework lands here.**
There is nothing separate to hand in for it.

## Layout

```
part4-toolkit/
  README.md              this handout
  starter/               Makefile, toolkit.h (the contract), toolkit.c   <- work here
  tests/run.sh           the autograder
  tests/helgrind.supp    two suppressions for a helgrind/glibc disagreement
  solutions/             SPOILERS. Reference toolkit, model DEADLOCK.md, answer key. Later.
```

```sh
cp -r starter tests ~/lab5/p4
cd ~/lab5/p4/starter
make test
```

## What you hand in

| File | Weight within Part 4 |
|---|---|
| `toolkit.c` — the semaphore and the four structures | 80% |
| `DEADLOCK.md` — which condition each solution breaks | 20% |

Nineteen cases. Eighteen are machine-checked in full. The nineteenth checks
that `DEADLOCK.md` exists, names the four conditions and covers both
structures it is asked about; the argument in it is marked against the rubric
in `solutions/README.md`. **The untouched starter scores `3 passed, 13
failed, 3 skipped`** — the three skips are the helgrind runs, which do not
instrument a case that is already failing, and two of the three passes are
cases that only a *wrong* implementation fails, which the answer key explains.

## The contract

`starter/toolkit.h`. Fifteen functions, five structs, no other API. The
structs are spelled out because the harness has to be able to declare one of
each; it reads no field of any of them.

**The semaphore is built on pthreads. Everything else is built on the
semaphore.** No `pthread_mutex_lock`, no `pthread_cond_wait`, no atomic below
the semaphore layer — the barrier, the rendezvous, the reader-writer lock and
the philosophers see one primitive and use it four ways. No test can check
this and it is the single most important thing in the part: it is what makes
the four structures *patterns* rather than four pieces of cleverness. It is
on your honour, and the answer key says so too.

**Why pthreads rather than your Part 2 locks.** Two reasons. The parts of
this lab are independent, so being stuck on Part 2 must not stop you doing
Part 4. And helgrind understands the pthreads primitives completely and
cannot audit a lock you built yourself out of atomics — Part 2 explains that
in detail. Building the semaphore on pthreads is what buys this part a
helgrind verdict worth having.

The rules from Part 3 carry over unchanged, because `msem_wait` is a
condition-variable wait like any other: **wait in a `while`**, and **signal
from inside the critical section, before the unlock**.

### The barrier has to be reusable

`mbarrier_wait` is called by *n* threads; none returns until all *n* have
called it; and then the barrier is ready for the next round, and the round
after that. A barrier that works once and jams on the second round is the
classic bug in chapter 31, and it is invisible to any test that uses the
barrier once. Get one round working, then run two hundred and think about
what the fastest thread is doing while the slowest is still leaving.

### The reader-writer lock: writer starvation is the exercise, not a bug

Chapter 31's reader-writer lock lets any reader in whenever another reader is
already inside. Under a steady stream of readers the count never reaches
zero, and a writer waiting for an empty room waits for ever. **OSTEP is right
that this is a property of that design rather than a defect in it** — it is a
deliberate choice to favour readers, and it is stated as such.

**This lab's contract asks for the other design.** A writer that is waiting
must not be overtaken by readers that arrive after it. The harness has a case
for it: a background reader holds the lock, a relay of readers behind it
hands over so that the room is provably never empty, and a writer arrives in
the middle. Under the textbook lock the writer never gets in and the case
says so; under the contract's lock it gets in as soon as the readers already
inside have drained.

If you build the textbook one you will fail one case and the failure message
will tell you exactly this. It is worth writing that version first anyway,
for two minutes, to watch it happen — and then writing down in `DEADLOCK.md`
why a starved writer is not a deadlocked one.

### Where `broadcast` would have been necessary

Part 3 said that the place `pthread_cond_broadcast` stops being merely safe
and becomes necessary is this part's reader-writer lock, and it is worth
seeing why — even though the lock you are about to write does not call it.

Build the reader-writer lock the other way, on a mutex and a condition
variable rather than on semaphores. A writer releasing the lock can make an
*arbitrary number* of readers runnable at once: every reader blocked on
"no writer holds this" now has a true predicate. `signal` wakes one of them,
and the others go on waiting for a wakeup that will never come, because the
thing that would have sent it has already finished. That is the rule from
chapter 30 stated exactly: **signal when you can name how many waiters can
make progress; broadcast when you cannot.**

In the semaphore version the same fact is still there, wearing a different
hat. `turnstile` is waited on and immediately posted by each reader that
passes — one thread waking the next, *n* times — which is how a semaphore
expresses "let everybody through" without a broadcast primitive at all. It is
the same wakeup pattern, hand-rolled, and noticing that is most of what
chapter 31 is for.

### Philosophers: philosopher *w* eats with forks *w* and *w*+1

That numbering is part of the contract, because the harness shadows every
fork and checks that no fork is ever in two hands. Two things are required:
nobody deadlocks, and **at least two philosophers eat at the same time**. The
second is there because a single lock around the whole table satisfies the
first perfectly and answers a different question.

## What the harness checks and how

- **It opens with a negative control.** The same counter workload with no
  semaphore at all must get the wrong answer. If it does not — if this
  machine cannot lose an update between two unsynchronised threads — then the
  cases after it are not evidence of anything, and the harness says so.
- **Outcome, exactly.** *N* threads, *M* increments each, exactly *N*×*M*, and
  a counting semaphore initialised to 3 must let three in and never four.
- **How a thread waits, not just whether it does.** Four threads waiting on a
  semaphore at zero for 300 ms must cost the process almost no processor
  time. A `msem_wait` that loops on the value burns a core per waiter, and
  this is the case that says so.
- **Timeout as a distinct verdict.** A case that has to be killed is reported
  as `DEADLOCK`, not as a wrong answer, because "everybody is asleep" and
  "two threads got in each other's way" are different bugs. Philosophers in a
  circle, a rendezvous that waits before it posts, and a barrier gate nobody
  opens all show up here.
- **Writer starvation is neither.** The starved-writer case cannot hang —
  the readers keep making progress for ever, which is the definition of
  starvation — so it measures instead, and reports how long the writer waited
  and how many readers went past it.
- **helgrind, three times**: over the semaphore, over the barrier and over
  the reader-writer lock. It sees what the outcome cases cannot: a signal
  sent after the unlock, and a field touched outside the semaphore that
  guards it. An `arrived++` without its guard passes every outcome case in
  the suite and helgrind fails it in about a second.

`tests/helgrind.supp` suppresses two message kinds that glibc 2.34 and later
provoke from inside `pthread_cond_wait` itself. Both suppressions require
glibc's internal frame to be on the stack, so the mistake they could
plausibly hide — signalling without the mutex — is still reported.

## Running the tests

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory
```

The graded build is `run.sh`'s own `gcc` line, not your Makefile. The suite
takes ten to fifteen seconds, of which the nineteen cases themselves are
about two: the rest is the three helgrind runs.

**Run it more than once.** Measured on a two-core machine, a correct toolkit
passed all nineteen cases on each of twenty consecutive runs.

## If you get stuck

1. **`DEADLOCK: no progress in 30s` on the philosophers** — five hands each
   holding one fork. That is the circular wait, and breaking it is the
   exercise; `DEADLOCK.md` is where you say which condition you broke.
2. **`DEADLOCK` on the rendezvous** — `rv_arrive` waits for the other side
   before announcing its own arrival. Post first, then wait.
3. **`DEADLOCK` on a barrier round** — a turnstile that is waited on and
   never posted again on the way through, or the mutex held across the
   turnstile. Everybody is waiting for the gate to be opened by a thread that
   is waiting for the mutex you are holding.
4. **`a thread came out of round N and found another thread already in round
   M`** — the reusable-barrier bug. One turnstile is not enough.
5. **`a semaphore at 1 ... N updates were lost`** — `msem_wait` proceeded on a
   value it read before it was woken. `while`, not `if`.
6. **`a semaphore initialised to 3 let 5 threads inside at once`** — the same
   fault, seen from the other end: the value went below zero.
7. **`no two of them were ever inside at the same time`** — `msem_init` is
   ignoring its argument, or `msem_wait` blocks while the value is positive.
8. **`msem_wait returned from a semaphore whose value was 0`** — there is no
   wait in it at all.
9. **`the process burned N ms of processor time`** — the wait is a spin. Wait
   on the condition variable.
10. **`a writer waited 3000 ms ... and never got it`** — you built the
    textbook reader-preferring lock. See the contract section above; this is
    the one case in the lab that fails a design that a textbook prints.
11. **`a reader ... saw the two guarded values disagree`** — a reader is
    getting in while a writer holds the lock.
12. **`fork N was in two philosophers' hands at once`** — `phil_pickup`
    returns holding one fork, or the fork numbering does not match the
    contract.
13. **`no two of them were ever eating at the same time`** — one lock around
    the whole table. Deadlock-free, and not a solution.
14. **helgrind: `associated lock is not held by calling thread`, with
    `pthread_cond_signal` and your `msem_post` on the stack** — the signal is
    after the unlock. Move it back inside.
15. **helgrind: a race on `arrived` or `readers`** — a counter touched
    outside the binary semaphore that guards it.
16. **`could not build the case driver against your library`** — `toolkit.h`
    has been changed. It is the contract; put it back.

## Stretch goals

- Write the **light-switch** out of the reader-writer lock as its own little
  abstraction — "the first one in locks the room, the last one out unlocks
  it" — and use it in both directions to build a writer-preferring lock and a
  reader-preferring one from the same two pieces. Downey's *The Little Book
  of Semaphores* names this pattern and it is worth having a name for.
- Add `msem_trywait`, and then find a use for it that is not a spin loop.
- Solve the philosophers with the asymmetric solution (one philosopher picks
  up its right fork first) as well as the one you handed in, and put both in
  `DEADLOCK.md` with an argument about which condition each breaks. They are
  not the same argument.
- Build the barrier a second way, on one semaphore and a counter, and
  convince yourself it is or is not reusable.
