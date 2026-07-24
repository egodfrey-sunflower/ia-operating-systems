# Lab 5 Part 2 — Reference locks and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference mylock.c, the account of what each case can and    ║
║  cannot catch, and the rubric for LOCKS.md — which is 30% of      ║
║  Part 2, is marked by hand, and has no single right answer.       ║
║  Reading the rubric before you have your own numbers turns the    ║
║  one open-ended piece of this part into a fill-in form.           ║
║                                                                   ║
║  LOCKS.md in this directory is the model report. Same warning,    ║
║  twice as strongly: it contains the conclusions.                  ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory: 17 passed, 0 failed
make bench      # the twelve rows behind LOCKS.md
```

The reference is 165 lines of `mylock.c` as shipped, of which 89 are code and
the rest comment. The suite takes about thirteen seconds, most of it the three
helgrind runs.

**The untouched starter scores `6 passed, 8 failed, 3 skipped` of 17.** The
six it passes are worth understanding, because they are the map of what each
case can see:

- **the negative control** — it is about the machine, not about your locks,
  and a machine that can lose an update does so whether or not you wrote a
  lock;
- **the three "two locks are two locks" cases** — a lock that does nothing
  never blocks, so a second lock is always available;
- **the ticket lock's arrival-order case** — six threads staggered 40 ms apart
  arrive in order and are served in order even with no lock at all;
- **the sleeping lock's processor-time case** — a lock that does nothing does
  not spin either. The spin lock's processor-time case is the same observation
  read the other way, and the starter fails it: a lock nobody waits for burns
  nothing while waiting.

All three helgrind cases SKIP rather than fail, each gated on the plain run it
instruments.

Every case that a lock doing nothing *can* fail, it fails.

## The three locks in one paragraph each

**Spin lock.** Test-and-test-and-set. The outer loop attempts
`__atomic_exchange_n(&l->flag, 1, __ATOMIC_ACQUIRE)`; the inner loop spins on
`__atomic_load_n(..., __ATOMIC_RELAXED)` until the lock looks free. The inner
loop is not an optimisation detail: the exchange writes, and a write takes the
cache line exclusively from every other core on every attempt, so the naive
version turns *n* waiters into *n* coherence transactions per iteration. The
relaxed load is safe because the exchange that follows carries the acquire.

**Ticket lock.** `fetch_add` on `ticket` to take a number, relaxed — nothing
is being protected yet, the number is just a number. Spin on
`__atomic_load_n(&l->serving, __ATOMIC_ACQUIRE)` — this one is the acquire,
because seeing your number is what makes the lock yours. Release is
`fetch_add(&l->serving, 1, __ATOMIC_RELEASE)`; only the holder writes
`serving`, so a plain release store would also do.

**Sleeping lock.** OSTEP ch. 28's three-state futex mutex, unchanged: a
compare-and-swap 0→1 on the fast path; on the slow path, force the word to 2
before parking and re-take it as 2 rather than 1 when the park returns.
Release is `fetch_sub`, and the only branch that makes a system call is the
one where the old value was not 1.

<details>
<summary><b>Why the slow path claims the lock as 2, not 1</b></summary>

Because the thread coming out of `futex_wait` cannot know whether anyone else
is still parked. Taking the lock as 1 says "nobody is waiting", and if that is
wrong the next release skips the wake and the remaining sleeper never wakes at
all. Taking it as 2 says "somebody might be", which is always safe: the cost
of being wrong is one unnecessary `FUTEX_WAKE` on release, and the cost of the
other error is a thread that sleeps for ever.

The general shape — when in doubt, over-report the presence of waiters — is
worth carrying into Part 4.

</details>

<details>
<summary><b>Why the word must say 2 before the park, and not after</b></summary>

The releasing holder decides whether to make a system call by looking at the
word. If it sees 1, it concludes nobody is waiting and returns without a wake.
So a thread that parks while the word still says 1 has left no evidence of
itself, and nobody will ever wake it.

`futex_wait(addr, expected)` closes the remaining window: it re-checks
`*addr == expected` *atomically with going to sleep*, so a release that lands
between your last read and your call to `futex_wait` cannot be missed — the
value will have changed and the call returns immediately. That atomic
recheck is the entire reason the futex interface has an `expected` argument,
and it is the same idea as `pthread_cond_wait` atomically releasing the mutex
in Part 3.

</details>

## What each case is for — and what nothing here can catch

Every row was checked by breaking the reference and confirming the named case
fails. The last section is the honest part.

| Case | Isolated by |
|---|---|
| an unsynchronised counter really does lose updates | nothing in `mylock.c` — it is a check on the machine |
| the spin lock leaves the counter at exactly N × M | a spin lock built from plain loads and stores; a spin loop reading `l->flag` without an atomic load |
| the ticket lock leaves the counter at exactly N × M | a ticket lock whose `fetch_add` is `l->ticket++` |
| the sleeping lock leaves the counter at exactly N × M | a non-atomic compare-and-swap; a release that never wakes; a park on state 1 |
| no two threads are inside the *X* lock at once | the same faults, caught as overlap rather than as arithmetic |
| two *X* locks are two locks | the lock word kept in a file-scope variable |
| the ticket lock serves threads in arrival order | **a ticket lock that is really a test-and-set lock — the only case that catches it** |
| the sleeping lock's waiters burn no processor time | **a sleeping lock that is really a spin lock — the only case that catches it** |
| the spin lock's waiters spin | **a spin lock that is really the sleeping lock — the only case that catches it. Measured: a `spinlock_*` that is a second copy of the futex mutex scores 16 of 17 with this case as the single failure, and would score 17 of 17 without it** |
| helgrind is clean around the *X* lock | the annotations missing or in the wrong place; a lock word kept in a file-scope variable, which makes the annotations name the wrong object |
| LOCKS.md reports all three locks under contention | an absent or empty report |

**Measured detection rates**, because "it fails the case" is not the same
claim as "it fails the case reliably". On a two-core machine, over 10 runs of
each case:

- a lock that does nothing at all: caught by the spin counter case 10 times in
  10, and by the exclusion case 10 times in 10;
- a spin lock built from plain loads and stores rather than atomics: caught by
  the spin counter case 10 times in 10, by the exclusion case 5 times in 10;
- the correct reference: 0 failures in 10 runs of every individual case, and
  `17 passed, 0 failed` on 25 consecutive whole-suite runs;
- the two processor-time cases, which are the only ones here that measure
  rather than count and are therefore the ones to distrust: over the measured
  window the sleeping lock's waiters use 0.000 s of processor time and the spin
  lock's use 134–150% of one core, against thresholds of 50% and 25%. The spin
  figure falls to 62% with four unrelated busy loops fighting for the same two
  cores, which is still more than twice the threshold. Each case caught its
  mutant on 12 runs out of 12 and passed the reference on 25 out of 25.

The counter case is the strong one and the exclusion case is the corroborating
one, which is why both exist.

### What no case in this part catches

**Memory ordering, on this hardware.** Replacing every `__ATOMIC_ACQUIRE` and
`__ATOMIC_RELEASE` in the reference with `__ATOMIC_RELAXED` scores
`17 passed, 0 failed`. On x86-64, ordinary loads have acquire semantics and
ordinary stores have release semantics in hardware, so the only thing the
annotations constrain is the compiler — and gcc happens not to reorder across
these particular atomics even when told it may.

This is not a gap that can be closed by trying harder on this machine, and the
handout does not pretend otherwise. It is why the memory-ordering rule is
taught as a rule with a reason rather than as something a test will hold you
to. The one ordering mistake that *is* caught is the practical one: dropping
the atomic load from the spin loop, so that gcc at `-O2` is free to hoist the
read, which fails the spin lock's counter and exclusion cases.

**Lock quality.** Fairness beyond the ticket lock's arrival-order case, and
scalability, are measurement questions. They belong in `LOCKS.md` and are
marked by the rubric below, not by the harness.

## The rubric for LOCKS.md (30% of Part 2)

There is no single right answer. What is being marked is whether the report
treats its own numbers honestly.

**Full marks** need all four of:

1. **The table is there and was actually run** — twelve rows, three locks at
   four thread counts, with the machine's core count stated. The core count is
   not decoration: it is the independent variable the whole table turns on.
2. **The noise is quantified before any claim is made.** Several runs per
   point, a stated spread, and no conclusion resting on a difference smaller
   than the spread. A report that treats a 20% difference in ns/acq as a
   finding has over-read its data.
3. **The ticket lock's collapse is explained by the mechanism, not by
   "contention".** The distinguishing observation is that it is *bimodal*:
   fast when no waiter is descheduled, catastrophic when one is, with almost
   nothing in between. Any explanation in terms of gradual overhead is wrong,
   and the numbers say so.
4. **The sleeping lock's result is connected to what is scarce.** It wins
   above two threads on a two-core machine not because system calls are cheap
   but because a parked thread is not competing for a core. A report that
   concludes "sleeping locks are faster" has learned the wrong lesson; one
   that concludes "sleeping locks stop wasting the resource that ran out" has
   learned the right one.

**Good but not full marks:** the table, the noise, and a correct description
of the shape without a mechanism for the ticket lock's cliff.

**Not enough:** a ranking. "Sleep is fastest, then spin, then ticket" is true
of four of the twelve rows and false of the other eight, and says nothing
about why.

**Actively wrong, and worth marking down for:** any claim built on a single
run; any claim about the ticket lock's collapse that does not mention thread
count exceeding core count; treating the one-thread row as evidence about the
locks, when it is mostly evidence about function call overhead.

## What is on your honour

- **Three genuinely different locks.** One case per lock checks the property
  that makes it that lock — arrival order, parking, spinning — and between
  them they rule out submitting one implementation three times under three
  names. They are still not proof that each lock is *the* lock asked for: a
  ticket lock whose acquire spins on `serving` but whose release stores an
  arbitrary larger number passes the arrival-order case, and a spin lock that
  is really a ticket lock passes the spinning case. What is closed is the cheap
  version of the trick, where two of the three names share one body.
- **A spin-then-park hybrid is excluded by fiat.** The spinning case requires
  the spin lock's waiters to burn processor time, so an adaptive lock — which
  is a better lock than either — fails it. That is a deliberate cost: the part
  is about three named designs and what each one does with a waiting thread,
  and a case that accepted the hybrid could not tell the spin lock from the
  sleeping one at all. The handout says so where it states the contract.
- **Barriers where the rule says, not where a test says.** See above: on this
  hardware nothing checks them. The reason to write them correctly is that a
  lock whose correctness depends on unstated facts about the processor is not
  a lock you can move.
- **The annotations describe what your lock does.** They are assertions to
  helgrind, and helgrind believes them. Putting `MYLOCK_HG_RELEASING` after
  the release rather than before it, or annotating a lock you never take,
  produces a clean run and means nothing.
