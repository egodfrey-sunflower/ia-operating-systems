# LOCKS.md — model report (Part 2)

> This is the model answer, and it is a spoiler for the one open-ended piece
> of Part 2. Do not read it before you have your own numbers.

## The measurements

`./bench 300` — three locks at 1, 2, 4 and 8 threads, 300 ms per point — on a
**two-core** x86-64 Linux machine with other work running on it. Three runs;
the second is shown, and the run-to-run spread is discussed rather than
hidden.

| lock | threads | acquires | acq/s | ns/acq |
|---|---|---|---|---|
| spin | 1 | 15 511 397 | 50 994 238 | 19.6 |
| spin | 2 | 6 014 191 | 20 030 534 | 49.9 |
| spin | 4 | 3 638 919 | 12 080 114 | 82.8 |
| spin | 8 | 1 783 097 | 5 828 640 | 171.6 |
| ticket | 1 | 12 777 333 | 42 374 195 | 23.6 |
| ticket | 2 | 2 179 567 | 7 241 067 | 138.1 |
| ticket | 4 | 39 692 | 130 139 | 7 684.1 |
| ticket | 8 | 62 712 | 201 633 | 4 959.5 |
| sleep | 1 | 12 467 068 | 41 509 880 | 24.1 |
| sleep | 2 | 10 250 750 | 33 345 516 | 30.0 |
| sleep | 4 | 11 239 447 | 37 178 512 | 26.9 |
| sleep | 8 | 7 441 673 | 24 730 779 | 40.4 |

## What the numbers are worth

Across the three runs, the spread on the contended spin and sleep points is
about a factor of two — 82.8, 105.0 and 96.1 ns for the spin lock at four
threads; 26.9, 36.9 and 34.7 for the sleeping lock at the same point. So
nothing below rests on a percentage.

The ticket lock above two threads is different in kind. Its four-thread number
across the three runs was **1 847, 7 684 and 14 558 ns** — a factor of eight
between runs of the same binary on the same machine, and at eight threads the
ordering between the two thread counts even reverses. That is not noise to be
averaged away; it is the finding, and the shape of it is the point: the run
either convoys or it does not.

The one-thread row is the most trustworthy number in the table and the least
interesting. It is one atomic read-modify-write plus call overhead, and all
three locks land within 30% of each other because all three do exactly the
same thing when nobody is waiting.

## The shape

**Two cores. That is the fact the whole table is about.**

**The spin lock degrades smoothly.** One thread to eight costs about 9×.
The cache line ping-pongs and a thread that loses the exchange burns processor
time a waiting thread could have used, but nothing structurally bad happens:
it degrades because it wastes work, and the waste grows roughly in proportion
to the number of wasters.

**The ticket lock falls off a cliff between two and four threads.** It is
already about 3× *slower* than the spin lock at two threads — fairness is not
free even when it is working — and then it drops by a further factor of fifty
to a hundred.

The mechanism is not contention. A ticket lock hands the lock to a *named*
thread, and with four runnable threads on two cores the named thread is
usually not on a processor. Every other waiter then spins for a whole
scheduler timeslice, achieving nothing, until the kernel gets round to running
the one thread that could make progress. The lock is correct throughout. What
is catastrophic is the interaction between a fair handover policy and an
oversubscribed machine — and the bimodality is the signature of exactly that:
a run in which nobody is descheduled at the wrong moment is fast, and one in
which somebody is, is not.

Stated generally: **a fair spin lock is a bad idea whenever there can be more
runnable threads than cores**, and in userspace there always can be. That is
the argument OSTEP ch. 28 makes for a lock that sleeps, and it is a great deal
more convincing measured than read.

**The sleeping lock barely degrades at all** — 24 ns to 40 ns across an
eightfold increase in threads, and from two threads upward it is the best
number in the table. Two things are going on. Uncontended, it is a
compare-and-swap and nothing else: no system call, which is exactly what the
third value in the state word buys. Contended, a waiter leaves the run queue,
so it stops competing for a core with the thread that is holding the lock. The
apparently expensive design — involve the kernel — is the one that scales,
because what is scarce here is cores, not instructions.

## What I would take away

Ranking the three by speed is the wrong exercise; at one thread they are the
same lock. The useful statement is about what each one spends when something
is short:

- the spin lock spends **cores** to save **latency**. A good trade exactly
  when the critical section is shorter than a context switch and a core is
  free;
- the ticket lock spends cores to buy **fairness**, and on an oversubscribed
  machine that trade is not merely bad but catastrophic — precisely *because*
  the fairness forces it to wait for one specific thread rather than any
  thread;
- the sleeping lock spends **latency**, up to two system calls on the
  contended path, to save cores. Above one thread on this machine that pays
  for itself several times over.

The thing I did not expect: the ticket lock is already 3× slower than the spin
lock at two threads on two cores, where in principle nothing is descheduled.
The best explanation I have is that a strict handover order forces the line
carrying `serving` through a coherence round trip on every release, while the
test-and-set lock lets whichever core already owns the line take the lock
again. I have not measured that, so it stays a hypothesis and not a
conclusion.
