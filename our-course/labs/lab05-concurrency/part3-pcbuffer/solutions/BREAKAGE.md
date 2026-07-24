# BREAKAGE.md — model report (Part 3)

> This is the model answer, and it works out both interleavings in full. It is
> a spoiler for the half of Part 3 that the chapter is actually about. Do not
> read it before you have built the broken versions.

Both breakages start from a working buffer: `10 passed, 0 failed`. Each one is
a single edit to a copy.

---

## Breakage 1 — `if` instead of `while`

Both waits changed from `while (...)` to `if (...)`. Nothing else.

### What the harness says

```
  [Part 3: capacity 1 with eight of each, every item exactly once] WRONG ANSWER (case 'p3_cap1')
    --- what the case complained about ---
    77965 of 80000 items never came out (first: 135) and 731 came out more
    than once (first: 1272). Every item put in must come out exactly once.
```

```
== results ==
  PASS   Part 3: one producer, one consumer, every item exactly once
  PASS   Part 3: items come out in the order they went in
  PASS   Part 3: a full buffer blocks the producer
  PASS   Part 3: an empty buffer blocks the consumer
  FAIL   Part 3: four producers and four consumers, every item exactly once
  FAIL   Part 3: capacity 1 with eight of each, every item exactly once
  FAIL   Part 3: every wakeup path is used and none is lost
  FAIL   Part 3: a million items pass through without stalling
  SKIP   Part 3: helgrind finds no race through the buffer

5 passed, 4 failed, 1 skipped
```

**The first four lines matter as much as the failures.** With one producer and
one consumer this bug does not exist. I ran those two cases twenty times each
against the broken buffer and they passed twenty times out of twenty; the two
multi-threaded cases failed twenty times out of twenty. A buffer tested only
with one thread on each side is a buffer this bug is invisible in.

### The interleaving

Capacity 1. Consumers **C1** and **C2**, producer **P**.

| step | thread | state | what happens |
|---|---|---|---|
| 1 | C1 | `count == 0` | takes the mutex, tests `if (count == 0)`, true, waits on `fill` — releasing the mutex |
| 2 | C2 | `count == 0` | same: takes the mutex, tests, waits on `fill` |
| 3 | P | `count == 0` | takes the mutex, stores item 135 in `slots[0]`, `count = 1`, `pthread_cond_signal(&fill)`, unlocks |
| 4 | C1 | — | wakes, reacquires the mutex, **does not re-test** (that is the `if`), takes item 135, `count = 0`, unlocks |
| 5 | C2 | — | wakes — perhaps from the same signal, perhaps because the runtime woke it, perhaps spuriously — reacquires the mutex, **does not re-test**, reads `slots[0]` again with `count == 0`, and returns item 135 a second time. `count` goes to −1 |

Item 135 came out twice; the item the next producer writes into a buffer whose
`count` is now −1 is lost. That is exactly the shape of the harness's
complaint: some values duplicated, far more never delivered.

The mechanism to be clear about is step 5. **The predicate was true when C2 was
signalled and false by the time C2 ran**, and what made it false was a *third
thread* — C1 — getting the mutex first. That is Mesa semantics in one
sentence: a signal says the state may have changed, not that it is still
changed when you get there.

It is worth saying what this is *not*. It is not a spurious wakeup. Spurious
wakeups are real, and they are a second, independent reason for the `while`
loop, but an explanation in terms of them would predict this bug with a single
consumer too — and with a single consumer it does not happen at all.

---

## Breakage 2 — one condition variable instead of two

`empty` deleted; every wait and every signal moved to `fill`.

### What the harness says

```
  [Part 3: capacity 1 with eight of each, every item exactly once] DEADLOCK: no progress in 30s (case 'p3_cap1')
    Not a wrong answer -- no answer. Every thread is
    waiting and nobody is left to signal.
```

```
== results ==
  PASS   Part 3: one producer, one consumer, every item exactly once
  PASS   Part 3: items come out in the order they went in
  PASS   Part 3: a full buffer blocks the producer
  PASS   Part 3: an empty buffer blocks the consumer
  PASS   Part 3: four producers and four consumers, every item exactly once
  FAIL   Part 3: capacity 1 with eight of each, every item exactly once
  FAIL   Part 3: every wakeup path is used and none is lost
  PASS   Part 3: a million items pass through without stalling

8 passed, 2 failed
```

A different failure and a different diagnosis: nothing came out at all. Note
also that this version **passes the four-producer four-consumer case and the
million-item soak**, because those have capacity 8 and 16 — there is enough
slack that producers rarely have to wait, so the wrong-queue signal rarely
matters. Only capacity 1 forces both sides to block, and only then does it
deadlock. Two bugs, two capacities; neither is found by the workload that
finds the other.

### Why it deadlocks

Capacity 1. Producers **P1**, **P2**; consumers **C1**, **C2**.

| step | thread | state | what happens |
|---|---|---|---|
| 1 | P1 | `count == 0` | puts an item, `count = 1`, signals the one variable — nobody is waiting yet — unlocks |
| 2 | P2 | `count == 1` | full, so it waits on the one variable |
| 3 | C1 | `count == 1` | takes the item, `count = 0`, signals the one variable, unlocks |
| 4 | C2 | `count == 0` | empty, so it waits on the one variable |
| 5 | C1 | `count == 0` | comes round again, empty, waits on the one variable |
| 6 | — | | three threads are now waiting on one variable: P2 (wanting room) and C1, C2 (wanting an item) |
| 7 | C1 | | is woken by a later signal, finds `count == 0`, and waits again |

The fatal step is 3 and the fatal detail is that the signal was **consumed by
the wrong thread**. A `pthread_cond_signal` wakes one arbitrary waiter. Step 3
was meant to tell P2 "there is room now"; it went to C2 instead. C2 re-checked
its own predicate, found the buffer still empty, and went back to sleep — and
the signal is gone. P2 is still waiting for a wakeup that will never be sent
again, because the only thread that could send it is a consumer, and every
consumer is waiting for P2.

The signal was not lost in transit. It was delivered, correctly, to a thread
that had no use for it, and the wrong-thread wake is indistinguishable from
no wake at all.

### `broadcast` does not fix the design

Replacing `pthread_cond_signal` with `pthread_cond_broadcast` makes the
deadlock go away, and the suite goes back to `10 passed, 0 failed`. Every
waiter is woken on every state change, so P2 is woken along with everyone
else, and the `while` loops make the extra wakeups harmless.

It is still the wrong design, for two reasons.

**It converts a correctness bug into a cost.** At capacity 1 with sixteen
threads, every put and every get wakes fifteen threads, fourteen of which
immediately re-check a predicate that is false for them and go back to sleep.
That is fifteen context switches and fifteen mutex acquisitions per item
transferred. The two-variable version wakes exactly one thread, and it is
always a thread that can proceed.

**It throws away information the code already had.** A producer that has just
added an item knows that the thread which should run next is a consumer. The
two-variable version writes that down; the broadcast version discards it and
then pays the runtime to rediscover it by trial. Condition variables are named
for the condition they stand for, and the moment two different conditions
share a variable, every wakeup becomes a guess.

The general rule the two breakages amount to together: **`while` makes extra
wakeups safe; separate condition variables make them rare.** You need both,
and the first is not a substitute for the second.
