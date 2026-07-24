# Lab 5 Part 3 — Reference bounded buffer and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference pcbuffer.c — which is forty lines of code, so      ║
║  there is very little of it to read accidentally — and the        ║
║  rubric for BREAKAGE.md, which is 20% of Part 3, is marked by     ║
║  hand, and is the half of this part the chapter is actually       ║
║  about.                                                           ║
║                                                                   ║
║  BREAKAGE.md in this directory is the model report. It contains   ║
║  both interleavings worked out in full. Reading it before you     ║
║  have built the broken versions removes the exercise entirely.    ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory: 10 passed, 0 failed
```

The reference is 87 lines of `pcbuffer.c` as shipped, of which 43 are code —
`pcb_put` is nine lines and `pcb_get` is eleven. The suite takes about eleven
seconds, most of it the million-item soak and the helgrind run.

**The untouched starter scores `0 passed, 9 failed, 1 skipped` of 10.** The
skip is helgrind, gated on the plain run of the case it instruments.

## The design

There is not much of one, which is the point: a mutex, two condition
variables, a ring of `capacity` slots, and `count`. Every rule in the file is
one of three:

1. **hold the mutex whenever you touch the state** — no path in or out that
   does not;
2. **wait in a `while`** — Mesa semantics, and the whole of chapter 30;
3. **two condition variables** — `fill` for consumers, `empty` for producers,
   and a put signals the one a get waits on.

Plus the house rule from `pcbuffer.h`: **signal before the unlock**, which
POSIX does not require but this lab does, because allowing the other form
would mean giving up the helgrind check.

<details>
<summary><b>Why <code>signal</code> and not <code>broadcast</code></b></summary>

Exactly one consumer can use exactly one new item, so waking the rest only to
have them find the buffer empty again is a thundering herd: *n* threads wake,
*n* − 1 re-check the predicate and go back to sleep, and every one of those
round trips is a context switch.

Broadcast is not *wrong* here. With the `while` loop, extra wakeups are
harmless by construction — that is what the `while` loop is for. It is
wasteful. This is the usual relationship between the two: signal when you can
name how many waiters can make progress, broadcast when you cannot.

The place broadcast becomes necessary rather than merely safe is Part 4's
reader-writer lock, where releasing a write lock can make an arbitrary number
of readers runnable at once.

</details>

<details>
<summary><b>Why the item is copied out before the unlock</b></summary>

`return b->slots[b->head]` after `pthread_mutex_unlock` reads the slot at a
moment when the buffer is unlocked and `count` has already been decremented —
so a producer is entitled to be writing that exact slot. The value returned is
then whatever won the race.

It is a two-line change and it fails the harness's `every item exactly once`
accounting, but only under multi-producer load: with one producer and one
consumer it can run for a very long time without anyone noticing. That
asymmetry — correct-looking under the workload you tested, wrong under the one
you did not — is the recurring theme of the whole lab.

</details>

## What each case is for

Every row was checked by breaking the reference and confirming the named case
fails.

| Case | Isolated by |
|---|---|
| one producer, one consumer, every item exactly once | `pcb_put` that never signals; a buffer that overwrites rather than blocks; a `head`/`tail` that does not wrap |
| items come out in the order they went in | `tail` advancing without wrapping; a `pcb_get` that returns without waiting |
| a full buffer blocks the producer | **a buffer that is not bounded — the only case that catches it.** Everything else eventually consumes, so an unbounded queue passes it |
| an empty buffer blocks the consumer | a `pcb_get` that reads `slots[head]` when `count` is 0 |
| four producers and four consumers, every item exactly once | `if` instead of `while`; the item read after the unlock |
| capacity 1 with eight of each, every item exactly once | `if` instead of `while` — this is where it breaks fastest |
| every wakeup path is used and none is lost | a signal delivered to the wrong condition variable, as a deadlock |
| a million items pass through without stalling | the same, and any wakeup that is lost only rarely |
| helgrind finds no race through the buffer | **state touched outside the mutex, and a signal sent after the unlock — neither is visible to any case above** |
| BREAKAGE.md covers both demonstrations | an absent report, or one that paraphrases the transcripts instead of pasting them |

**Two things worth knowing about the `if` mutation**, because they are the
lesson rather than a detail of the harness:

- it passes `one producer, one consumer` and `items come out in the order they
  went in`. Measured: 20 runs of each against the broken buffer, **0 failures
  out of 20** on both. With one thread on each side there is nobody to steal
  the item between the signal and the wake, so the predicate that was true
  when you were signalled is still true when you run. **A student who tests
  only the single-producer case will never see this bug**, and that is the
  entire argument for the rule;
- with four threads on each side it fails, and reliably: `capacity 1 with
  eight of each` and `four producers and four consumers` each failed **20 runs
  out of 20** against the same binary. The message names a value that came out
  of the buffer having never gone in, which is a consumer reading a slot
  nobody wrote.

Both breakages also show how narrow a workload's reach is. The `if` version
passes the two single-threaded cases and fails the four multi-threaded ones;
the one-condition-variable version passes the multi-threaded cases at capacity
8 and 16 and deadlocks only at capacity 1. Neither is found by the workload
that finds the other.

**Measured detection rates.** On a two-core machine, the correct reference
passed all ten cases on each of 25 consecutive runs. Every mutation in the
table above was compiled `-Wall -Wextra -Werror` and run against the whole
suite; none scored full marks, and every case is isolated by at least one of
them.

## The rubric for BREAKAGE.md (20% of Part 3)

The harness checks that both transcripts are present, by looking for the two
verdict strings the two breakages produce. Everything below is marked by hand.

**Full marks** need all four of:

1. **Both transcripts, pasted rather than described.** `WRONG ANSWER` from the
   `if` version and `DEADLOCK` from the one-variable version, with the
   harness's own message.
2. **A concrete interleaving for the `if` failure**, named threads, in order,
   with what each one sees. The essential three steps are: consumer A and
   consumer B both waiting; producer puts one item and signals; A wakes,
   takes the item, and leaves the buffer empty; B, which was also signalled or
   was already past its `if`, proceeds into an empty buffer. Any account
   without a *second* consumer has not identified the mechanism.
3. **A statement of why one condition variable deadlocks that names the wasted
   signal.** A consumer finishing a get signals the single variable; the
   thread that happens to be waiting on it is another consumer; that consumer
   wakes, finds the buffer still empty, and goes back to sleep. The producer
   that the signal was meant for was never woken and now nobody will signal
   again. The signal was not lost in transit — it was *delivered to the wrong
   thread and consumed*.
4. **The observation about `broadcast`.** Replacing `signal` with `broadcast`
   makes the one-variable version stop deadlocking, because now the producer
   is woken too. Full marks require saying why that does not make the design
   correct: it converts a deadlock into a cost — every wakeup wakes every
   waiter, most of whom immediately re-sleep — and it hides the fact that the
   code no longer distinguishes two different reasons to wait.

**Good but not full marks:** both transcripts, both mechanisms, no
`broadcast` discussion.

**Not enough:** "with `if`, the buffer breaks because the predicate might not
hold." True, and it is the statement in the chapter. What is being asked for
is the interleaving that makes it not hold.

**Actively wrong, and worth marking down for:** describing the `if` failure as
a spurious wakeup. Spurious wakeups are real and are one reason for the
`while` loop, but they are not what fails here: here the wakeup is entirely
genuine and the predicate was made false by a *third thread* between the
signal and the wake. A report that blames spurious wakeups has an explanation
that would also predict failure with one consumer, and one consumer does not
fail.

## What is on your honour

- **The broken versions were derived from your working one.** A bug
  demonstrated in code that never worked demonstrates nothing, and nothing in
  the harness can tell.
- **The `if` version was actually run until it failed.** It does not fail
  every time on every workload — that is the point of the exercise — and a
  transcript is the evidence that it was run rather than predicted.
