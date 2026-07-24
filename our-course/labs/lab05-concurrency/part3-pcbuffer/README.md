# Lab 5 Part 3 — Condition variables and a bounded buffer

**Week 13 · 3.5 hours · 20% of Lab 5 · OSTEP ch. 30**

x86-64 Linux, userspace C, one source file of about sixty lines. You will need
`gcc`, `make` and `valgrind`.

A producer/consumer bounded buffer on a mutex and two condition variables.
Then you break it twice, on purpose, and write down what happened.

Sixty lines is not a mistake in the estimate. Most of the 3.5 hours is the
second half: building the two broken versions, getting them to misbehave
reproducibly, and explaining the mechanism. The correct version is short
because condition variables are a good abstraction; the broken versions are
where the chapter's argument lands.

You are using the pthreads mutex and condition variable here, not your Part 2
locks. The subject is the *pattern* — wait, predicate, signal — and helgrind
understands the pthreads primitives completely, which makes its verdict in
this part worth far more than in the last one.

## Layout

```
part3-pcbuffer/
  README.md              this handout
  starter/               Makefile, pcbuffer.h (the contract), pcbuffer.c  <- work here
  tests/run.sh           the autograder
  tests/helgrind.supp    two suppressions for a helgrind/glibc disagreement
  solutions/             SPOILERS. Reference buffer, model BREAKAGE.md, answer key. Later.
```

```sh
cp -r starter tests ~/lab5/p3
cd ~/lab5/p3/starter
make test
```

## What you hand in

| File | Weight within Part 3 |
|---|---|
| `pcbuffer.c` — the bounded buffer | 80% |
| `BREAKAGE.md` — the two deliberate breakages, with transcripts | 20% |

Ten cases. Nine are machine-checked in full. The tenth checks that
`BREAKAGE.md` covers both demonstrations and contains captured output rather
than a description of it; the reasoning in it is marked against the rubric in
`solutions/README.md`. **The untouched starter scores `0 passed, 9 failed, 1
skipped`** — the skip is helgrind, which does not run while the case it
instruments is failing.

## The contract

`starter/pcbuffer.h`. Four functions. The struct is spelled out because the
harness has to be able to declare one; the harness reads none of its fields.

**Two condition variables, not one.** `fill` is where consumers wait for
something to consume. `empty` is where producers wait for room. A put signals
`fill`; a get signals `empty`.

**Wait in a `while`, never an `if`.** This is Mesa semantics, which is what
POSIX gives you and what chapter 30 spends the chapter on. `pthread_cond_wait`
returns when you have been signalled, when you have been broadcast to, when a
signal arrived, and sometimes for no reason at all. None of those means the
predicate is true. A signal is a **hint that the state might have changed**,
not a handoff of the lock together with a promise about it: between the
signaller releasing the mutex and you reacquiring it, any number of other
threads can run and undo the thing you were told about.

**Signal while you still hold the mutex.** POSIX permits releasing it first,
and there are arguments for that; this lab does not allow it. It is the form
chapter 30 uses, it removes a class of "who wakes whom" reasoning while you
are learning the rest, and helgrind reports the other form — so allowing it
would mean giving up the helgrind check, which is the most valuable check in
this part.

**Copy the item out before you unlock.** Returning `b->slots[b->head]` after
the mutex is released reads a slot a producer may already have overwritten.

## What the harness checks and how

It accounts for **every item individually** rather than adding them up: each
value goes in once and must come out exactly once. A checksum can come out
right with a duplicate and a loss cancelling, and those are precisely the two
errors a broken buffer makes together.

Two failure modes, kept apart on the results line because they come from
different mistakes:

- **`WRONG ANSWER`** — an item came out twice, or never, or a slot was read
  before it was written. That is the shape of a wait guarded by `if`.
- **`DEADLOCK`** — nothing came out at all and the case had to be killed.
  That is the shape of a signal delivered to the wrong queue. No assertion in
  the driver can catch it, because the process never reaches one; the timeout
  catches it.

Cases you should expect to have to think about:

- **a full buffer blocks the producer.** With capacity 4 and nobody
  consuming, a producer completes exactly four puts and then blocks. A buffer
  that grows, or that overwrites the oldest slot, passes everything else,
  because everything else eventually consumes.
- **an empty buffer blocks the consumer.** A `pcb_get` that returns a stale
  slot rather than waiting is caught here and, if you are unlucky, nowhere
  else.
- **capacity 1 with eight producers and eight consumers.** Every put fills the
  buffer and every get empties it, so every consumer that wakes is racing
  every other consumer for the single item. This is where `if` breaks fastest.
- **a million items** through a buffer of 16, four threads each way, which is
  what "does not stall" means.

**helgrind is the check that earns its runtime here.** Unlike Part 2 there is
nothing it has to take your word for: it intercepts every mutex and condition
variable call and models them exactly. It sees state touched without the
mutex, a condition variable used with two different mutexes, a wait on a mutex
the thread does not hold, and a signal sent after the unlock. None of those is
visible to the outcome cases.

`tests/helgrind.supp` suppresses two message kinds that glibc 2.34 and later
provoke from inside `pthread_cond_wait` itself — helgrind models an older
implementation of it. Both suppressions require glibc's internal frame to be
on the stack, so the mistake they could plausibly hide is still reported; that
was checked by building a buffer that signals outside the mutex and confirming
helgrind still fails it.

## The two breakages

This is the half of Part 3 that no test can grade, and the half the chapter is
about. **Get the correct version passing first** — a bug demonstrated in code
that was not working to begin with demonstrates nothing.

Then, for each of the two:

```sh
cp -r ~/lab5/p3/starter ~/lab5/p3/broken-if     # your working copy, not the skeleton
# edit ~/lab5/p3/broken-if/pcbuffer.c: while -> if, both waits
~/lab5/p3/tests/run.sh ~/lab5/p3/broken-if      # capture what it says
```

**Breakage 1: `if` instead of `while`.** Show the wakeup that fires against a
false predicate. Expect a `WRONG ANSWER` in the item-accounting form — the
message reports items that never came out of the buffer and items that came out
more than once, because a spurious wakeup lets a consumer take from an empty
slot and corrupts the running tally. Explain the interleaving: three threads,
in order, with what each one sees.

**Breakage 2: one condition variable instead of two.** Collapse `fill` and
`empty` into a single variable and show the consumer-wakes-consumer deadlock.
Expect a `DEADLOCK`. Explain why the buffer ends up with every thread asleep,
and — this is the part worth getting right — why `pthread_cond_broadcast`
makes the symptom go away without making the design correct.

`BREAKAGE.md` holds both: the transcripts, pasted rather than paraphrased, and
the mechanism in each case. The harness checks that both transcripts are there
by looking for the two verdict strings; it cannot check the reasoning, which
is where the marks are.

A third thing worth trying, though it is not required: make the `if` version
pass. One producer, one consumer, capacity 4, and it will run for ever without
a complaint. That is the lab in one observation — the rule exists for the
interleaving you did not test.

## Running the tests

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory
```

The graded build is `run.sh`'s own `gcc` line, not your Makefile. The suite
takes about eleven seconds, most of it the soak and the helgrind run.

**Run it more than once.** Measured on a two-core machine, a correct buffer
passed all ten cases on each of twenty-five consecutive runs.

## If you get stuck

1. **`DEADLOCK: no progress in 30s`** — everyone is asleep. In order: one
   condition variable doing both jobs; `pcb_put` signalling the variable
   producers wait on (or `pcb_get` the consumers'); a path out of `pcb_put` or
   `pcb_get` that does not unlock the mutex.
2. **`a value came out of the buffer that was never put in`** — a slot read
   before it was written. This is the `if`-instead-of-`while` signature, and
   if you have not broken it deliberately yet, check the waits.
3. **`N of M items never came out ... and K came out more than once`** — the
   same fault, or `head`/`tail`/`count` updated inconsistently.
4. **`items must come out in the order they went in`** — `head` and `tail`
   both advancing on the same operation, or one of them not wrapping.
5. **`a producer with no consumer completed N puts`** — `pcb_put` is not
   waiting while `count == capacity`.
6. **`pcb_get returned from an empty buffer`** — `pcb_get` is not waiting
   while `count == 0`.
7. **`killed by signal 11`** — indexing past `capacity` because `head` or
   `tail` was not wrapped, or reading `slots[head]` when `count` is 0.
8. **helgrind: `associated lock is not held by calling thread`, with
   `pthread_cond_signal` and your `pcb_put` on the stack** — the signal is
   after the unlock. Move it back inside.
9. **helgrind: a race on `count`, `head` or `tail`** — some path touches the
   state without the mutex. The usual one is a "fast path" that checks
   `count` before locking.
10. **`BREAKAGE.md` ... `neither string is in BREAKAGE.md`** — the two
    breakages fail in different ways and the report has to show both
    verdicts. Paste the transcripts.
11. **`could not build the case driver against your library`** —
    `pcbuffer.h` has been changed. It is the contract; put it back.

## Stretch goals

- Replace the two condition variables with one, plus `pthread_cond_broadcast`.
  It passes the suite. Write down what it costs at capacity 1 with sixteen
  threads, using `bench`-style timing of your own, and decide whether you
  would ship it.
- Make `pcb_get` take a timeout. The interesting part is not
  `pthread_cond_timedwait` but what the caller can safely conclude when it
  returns `ETIMEDOUT`.
