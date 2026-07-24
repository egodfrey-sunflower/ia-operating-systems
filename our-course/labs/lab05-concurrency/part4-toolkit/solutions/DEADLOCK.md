# DEADLOCK.md — model report

The four necessary conditions, and which one each of my solutions breaks.
All four must hold at once for a deadlock to be possible, so breaking any one
of them is enough — and "enough" is the word to be careful with, because
three of the four are usually not negotiable and the fourth is where the
design lives.

## The four conditions

1. **Mutual exclusion** — a resource is held by one thread at a time. A fork
   cannot be shared; that is what makes it a fork and not a napkin.
2. **Hold and wait** — a thread holding one resource waits for another
   without giving up what it has.
3. **No preemption** — nothing takes a resource back from a thread that holds
   it; it is released voluntarily or not at all.
4. **Circular wait** — there is a cycle in the "waits for" graph: A waits for
   something B holds, B for something C holds, C for something A holds.

## Philosophers: the footman breaks circular wait

My `phil_pickup` waits on a `footman` semaphore initialised to `n - 1` before
touching a fork, and `phil_putdown` posts it after both forks are back.

It does not break mutual exclusion: each fork is still a binary semaphore and
still held by one philosopher at a time. It does not break hold and wait: a
philosopher still takes its left fork and then blocks holding it while it
waits for the right. It does not break no-preemption: nothing ever takes a
fork out of a philosopher's hand.

It breaks **circular wait**, and it does so by making the cycle
unconstructible rather than unlikely. A cycle around this table requires all
*n* philosophers to be holding exactly one fork each — with *n* forks and
each philosopher needing two, any cycle in the waits-for graph must run all
the way round. The footman admits at most *n* − 1 philosophers, so at least
one philosopher is holding nothing at all, and the cycle is broken at that
philosopher whether or not anybody is unlucky. There is no interleaving that
deadlocks; the argument does not mention timing anywhere.

The counting also gives a bound rather than just an absence: with *n* − 1
seated and each needing two forks out of *n*, at least one seated philosopher
has both of its forks free, so somebody can always finish and put two forks
back.

### The other standard answer, and how it differs

Make one philosopher — say the last — pick up its **right** fork first. This
breaks the same condition, circular wait, by a different route: the cycle in
the waits-for graph requires every philosopher to reach for the same
direction, and one philosopher reaching the other way orients the graph so
that no cycle can close. Two philosophers now contend for the same fork
first, which is why it is sometimes described as breaking hold-and-wait; it
does not, it just changes the order.

I chose the footman because the argument for it is a counting argument and
the argument for asymmetry is a graph argument, and the counting one is the
one I can check by reading the code. The cost is throughput: the footman
serialises entry to the table through one semaphore, and at *n* = 5 on two
cores that cost is invisible, but at larger *n* it is a single point every
philosopher must pass.

### What would have been wrong

Taking both forks "atomically" under a single table-wide lock breaks the
deadlock by breaking the problem: it also means only one philosopher eats at
a time, which is the thing dining philosophers exists to avoid. The harness
has a case for that, and it is right to.

## The reader-writer lock: starvation is not deadlock

Asking which condition my reader-writer lock breaks is a trick question, and
the useful answer is that it does not need to break any of them, because the
failure mode of the textbook version is not a deadlock.

The reader-preferring lock in ch. 31 has no cycle in its waits-for graph
under a stream of readers. The writer waits for the room to empty; the
readers wait for nothing at all — they arrive, increment the count, and walk
in. Nobody is waiting on the writer. The system as a whole is making
progress, for ever, and the writer is not part of it. That is **starvation**,
a liveness failure of a different kind: deadlock is "nobody moves",
starvation is "everybody but you moves".

My lock adds a `turnstile` that every reader passes through and a waiting
writer holds shut. The property that buys is bounded waiting: a writer waits
for the readers already inside to drain, and no reader that arrives after it
can extend that wait. It costs read throughput under a write-heavy load,
because readers now queue behind writers instead of streaming past them, and
it is the right trade for a lock in a kernel or a database where a writer
that never runs is an outage.

Where a deadlock *could* appear in this design is in the order the writer
takes its two semaphores: `turnstile` then `room_empty`, always, on every
path. (The release order does not matter, because a post never blocks, so
`rw_release_write` happens to post them in the same order it took them —
turnstile first. It is the *acquire* order that a cycle needs.) A path that
took them the other way
round on one branch would give two writers a lock-ordering cycle — the same
two-lock inversion as ch. 32's, with the same fix, which is a global order on
the locks.

## Where else in this toolkit a deadlock is one line away

- **The rendezvous.** `rv_arrive` posts its own side and then waits for the
  other. Swapping those two lines makes both sides wait for a signal neither
  has sent: hold and wait, circular wait, two threads, no output. It is the
  smallest deadlock in the lab and worth writing once on purpose.
- **The barrier.** The last thread in shuts `gate_b` and opens `gate_a` while
  holding `mutex`. If it did that *after* posting `mutex`, another thread
  could get in and count itself into the round that is already leaving.
  Nested waits on two semaphores in a fixed order is the same discipline as
  the writer's, and it is why `mutex` is taken and released twice in
  `mbarrier_wait` rather than held across the turnstile — holding it across
  the turnstile is a deadlock, because the thread that would open the gate
  needs the mutex to count itself in.
