# Lab 5 Part 4 — Reference toolkit and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference toolkit.c — a semaphore and four structures        ║
║  built on it, about 150 lines of code — and the rubric for        ║
║  DEADLOCK.md, which is 20% of Part 4 and is marked by hand.       ║
║                                                                   ║
║  DEADLOCK.md in this directory is the model report. It contains   ║
║  the whole argument, including the one about the reader-writer    ║
║  lock that most students get half of. Reading it before you have  ║
║  written yours removes the exercise.                              ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory: 19 passed, 0 failed
```

The reference is 289 lines of `toolkit.c` as shipped, of which 146 are code.
The suite takes ten to fifteen seconds on a loaded two-core box, of which the
nineteen cases are about 2.4 s in total (the longest is the starved-writer
case at 0.6 s) and the rest is the three helgrind runs.

**The untouched starter scores `4 passed, 12 failed, 3 skipped` of 19 most
runs, varying between 3 and 5 passes** (the variation is `p4_phil`, below). The
three skips are the helgrind runs, each gated on the plain run of the case it
instruments. The reliably-passing cases are worth being explicit about, because
they are the ones a *broken* implementation can pass:

- *an unsynchronised counter really does lose updates* — the negative
  control, which does not touch the student's code at all;
- *readers hold the read lock together* — a `rw_acquire_read` that does
  nothing lets four readers in, and this case only catches the opposite
  mistake (a reader-writer lock that excludes readers from each other);
- *a waiting writer is not overtaken for ever* — likewise: a lock that does
  nothing never starves anybody.

Those three are isolated by mutations that are the realistic wrong answers (a
reader-preferring lock, a lock that is really a mutex), which is the standard
for a case in this course; none is a test that cannot fail.

The fourth, variable, pass is **`p4_phil`**: the empty philosopher stubs never
pick up a fork, so the "two non-adjacent philosophers eat at once" check is a
coin-flip on scheduling — it passes on the runs where the do-nothing threads
happen to interleave favourably and fails otherwise. That is why the starter's
pass count wanders between 3 and 5; a real submission passes `p4_phil`
deterministically.

## The design

<details>
<summary><b>The semaphore, and why it is the only place pthreads appears</b></summary>

`msem_wait` is Part 3's `pcb_get` with the buffer taken out: a mutex over one
integer, a `while` around the wait, a signal from inside the critical
section. Twenty lines.

Everything else in the file uses semaphores only. That constraint is doing
real pedagogical work: it forces the barrier's counter to be guarded by a
*binary semaphore*, the barrier's gates to be *turnstiles*, and the
reader-writer lock's room to be *a semaphore held collectively*, rather than
letting each structure reach for whichever pthreads primitive is closest to
hand. Chapter 31's claim is that one primitive expresses all of them, and the
only way to believe it is to be made to do it.

It also keeps helgrind useful, which Part 2 could not: helgrind models
pthreads mutexes and condition variables exactly, so a race anywhere in the
toolkit surfaces through the semaphore layer it is built on.

</details>

<details>
<summary><b>The barrier, and why one turnstile is not enough</b></summary>

Two turnstiles. `gate_a` starts closed, `gate_b` starts open. The *n*th
thread to arrive shuts `gate_b` and opens `gate_a`; every thread then passes
through `gate_a` (wait, then post — the idiom for "let everybody through");
the last thread out shuts `gate_a` and opens `gate_b`; everybody passes
`gate_b`.

With one turnstile, the last thread in opens the gate and leaves it open. A
fast thread can go round its own loop and arrive back at the barrier while
the slowest thread is still leaving the previous round — and walk straight
through a gate that was opened for a round it has already finished. The
barrier is now permanently one round out of step, or wedged, depending on
timing.

The mutant is real and the harness catches it: see `m3` in the table below.
It is caught by the two-hundred-round case and by nothing else, which is the
entire argument for that case existing.

</details>

<details>
<summary><b>The reader-writer lock, and the three lines that make it fair</b></summary>

`room_empty` is a lock over the whole data structure. Readers take it
collectively — first one in takes it, last one out releases it, with `mutex`
making the counting safe. A writer takes it alone. That is chapter 31's
reader-preferring lock, and it starves writers.

`turnstile` is the fix, and it is three lines: every reader passes through it
without holding it, and a writer *takes and keeps* it. A waiting writer's
first act is therefore to shut the door behind the readers already in the
room; they drain, the room empties, the writer works, and only then does the
door open. Readers that arrive after the writer queue behind it.

The cost is read throughput under a write-heavy load, and the handout says
so. The property bought is bounded waiting: a writer waits for the readers
already inside and no more.

</details>

<details>
<summary><b>The philosophers, and what "cannot deadlock" has to mean</b></summary>

A binary semaphore per fork plus a footman semaphore initialised to *n* − 1.

The argument has to be a counting argument, not a probabilistic one. A cycle
in the waits-for graph around this table requires all *n* philosophers to
hold exactly one fork; the footman admits *n* − 1, so at least one
philosopher holds nothing and the cycle cannot close. There is no
interleaving that deadlocks, and the argument never mentions timing.

`DEADLOCK.md` in this directory works this through, including the comparison
with the asymmetric solution and the observation that both break the same
condition — circular wait — by different routes.

</details>

<details>
<summary><b>Where <code>broadcast</code> would have been necessary — Part 3's forward reference, paid off</b></summary>

Part 3's answer key says the place broadcast becomes necessary rather than
merely safe is Part 4's reader-writer lock. The reference lock does not call
`pthread_cond_broadcast`, and that is not the promise going unpaid — it is
the point.

Built on a mutex and a condition variable, releasing a write lock makes an
arbitrary number of readers runnable at once, and `signal` would wake one and
strand the rest: broadcast is required, and it is required precisely because
the releaser cannot name how many waiters can now proceed.

Built on semaphores, that same "wake everybody" is `msem_wait(&turnstile);
msem_post(&turnstile);` — each reader through the turnstile releases it for
the next, *n* wakeups in a chain rather than one broadcast. The barrier's two
gates are the same idiom. A semaphore has no broadcast, so the pattern has to
be written out, and writing it out is what makes it visible.

The handout says this in its contract section, so a student meets it before
building the lock rather than afterwards.

</details>

## What each case is for

Every row was checked by breaking the reference and confirming the named case
fails. The mutant column names the mutation from the table further down.

| Case | Isolated by |
|---|---|
| an unsynchronised counter really does lose updates | nothing in the student's code — it is the control, and it fails if the machine cannot lose an update |
| a semaphore at 1 leaves the counter at exactly N x M | `if` instead of `while` (m1); a post that does not signal (m9) |
| a semaphore at 3 admits three and no more | the same `if` (m1, 10 runs out of 10 once the window inside the room was made big enough to see — see below); a semaphore that ignores its initial value, caught by the *lower* bound in the same case |
| a wait on a zero semaphore blocks | a `msem_wait` with no wait in it (the starter); a post that never signals (m9) |
| a thread waiting on a semaphore burns no processor time | **a `msem_wait` that spins on the value (m10) — the only case that catches it** |
| nobody leaves the barrier until everybody has arrived | a barrier that does not block |
| the barrier works two hundred rounds running | one turnstile instead of two (m3) |
| the barrier works at 1, 2, 3 and 32 threads | **a barrier that ignores n (m14), one wrong at exactly two (m15), or one that jams at n = 1 — none of which any eight-thread case can see**; also the one-turnstile barrier (m3) |
| neither side leaves the rendezvous alone | wait-before-post (m7), as a DEADLOCK |
| readers hold the read lock together | a reader-writer lock that is really a mutex |
| a writer excludes readers and other writers | a first reader that does not take the room (m8) |
| a waiting writer is not overtaken for ever | **the textbook reader-preferring lock (m4) — the only case that catches it**; and a writer that takes its two semaphores in the wrong order (m13) |
| five philosophers eat without deadlocking | a footman that admits n rather than n − 1 (m11), as a DEADLOCK; a pickup that takes one fork |
| non-neighbouring philosophers eat at the same time | **one lock around the whole table (m6) — the only case that catches it**; and it is where the naive left-then-right deadlock (m5) actually shows up |
| the table works at two philosophers and at seven | **a footman that ignores n (m16) — a DEADLOCK at exactly two, invisible to every five-philosopher case**; fork clashes and starvation at a size other than five |
| helgrind finds no race in the semaphore | **a post sent after the unlock (m2) — invisible to every outcome case**; state touched outside the mutex |
| helgrind finds no race in the barrier | **an `arrived++` outside its guard (m12) — the only case that catches it** |
| helgrind finds no race in the reader-writer lock | a post sent after the unlock (m2); the reader count touched outside `mutex` |
| DEADLOCK.md names the conditions each solution breaks | an absent report, or one that never names the four conditions |

## Measured: every mutation, against the whole suite

Sixteen mutants, each compiled by the harness's own `-Wall -Wextra -Werror`
line and run against all nineteen cases on a two-core machine. **None scored
full marks, and every case that can be isolated is isolated.** The last three
(m14–m16) are why `barrier_sizes` and `phil_sizes` exist: each ignores the
size argument the header makes a contract, and before those two cases each
scored a clean 17 out of 17 — a barrier built only ever at eight and a table
built only ever at five cannot see a structure that is wrong at any other
size, including the barrier that is wrong at exactly two.

| # | Mutation | Result | Cases that failed |
|---|---|---|---|
| m1 | `msem_wait` waits with `if`, not `while` | ~12 P, 5 F, 2 S | sem_mutex, sem_room, rw_excl, phil_parallel, phil_sizes, and one helgrind run — the exact set shifts run to run |
| m2 | `msem_post` signals after the unlock | 16 P, 3 F | **all three helgrind runs, and nothing else** |
| m3 | barrier with one turnstile | 16 P, 2 F, 1 S | **barrier_rounds and barrier_sizes** (helgrind-barrier skips, gated on rounds) |
| m4 | textbook reader-preferring rwlock | 18 P, 1 F | **rw_nostarve alone** |
| m5 | philosophers take left then right, no footman | 17 P, 2 F | **phil_parallel and phil_sizes, as DEADLOCKs** (phil itself catches it ~1 run in 3) |
| m6 | philosophers under one table-wide lock | 18 P, 1 F | **phil_parallel alone** |
| m7 | `rv_arrive` waits before it posts | 18 P, 1 F | **rendezvous alone, as a DEADLOCK** |
| m8 | first reader does not take `room_empty` | 17 P, 1 F, 1 S | **rw_excl alone** (helgrind-rwlock skips, gated on rw_excl) |
| m9 | `msem_post` never signals | 3 P, 13 F, 3 S | everything that waits, as deadlocks |
| m10 | `msem_wait` spins on the value | 15 P, 1 F, 3 S | **sem_parks alone** (the helgrind runs skip: instrumenting a spin costs minutes) |
| m11 | footman admits n, not n − 1 | 17 P, 2 F | phil_parallel, phil_sizes (both DEADLOCK) |
| m12 | `arrived++` outside the binary semaphore | 18 P, 1 F | **helgrind-barrier alone** |
| m13 | writer takes `room_empty` before `turnstile` | 18 P, 1 F | **rw_nostarve alone** |
| m14 | `mbarrier_init` hardcodes n = 8, argument ignored | 18 P, 1 F | **barrier_sizes alone** (DEADLOCK at n = 1) |
| m15 | barrier correct except at exactly n = 2 | 18 P, 1 F | **barrier_sizes alone** (wrong-count at n = 2) |
| m16 | `phil_init` hardcodes the footman at 4 | 18 P, 1 F | **phil_sizes alone** (DEADLOCK at n = 2) |

Two things in that table are worth reading twice.

**m12 is the case for helgrind.** An unguarded `arrived++` in the barrier
passes every outcome case in the suite — two hundred rounds, eight threads,
no wrong phase — and helgrind fails it in about a second. That is the same
result Part 3 got with a signal moved outside the mutex, and it is why the
plan calls helgrind the single most valuable check in the lab.

**m2 is caught by helgrind and by nothing else.** A `msem_post` that signals
after releasing the mutex is legal POSIX and works perfectly: every outcome
case in the suite passes it. The helgrind runs fail it — usually all three,
though the semaphore run is the least reliable of them and has been seen to
pass on its own while the barrier and rwlock runs failed, so a single suite
may show two failures rather than three. Either way the mutation is caught.
That is the whole argument for requiring the signal inside the critical
section, and it is the same result Part 3 got.

**m5 is caught by the *parallel* case and not by the meals case.** Five
philosophers doing four thousand back-to-back meals with no pause do not
deadlock on this machine, because the deadlock needs all five to be holding
one fork at the same moment and the window between the two acquisitions is a
few nanoseconds. Give each of them a one-millisecond meal and they
desynchronise and the circle closes immediately. **A student who tests only
the fast loop concludes that left-then-right works** — which is the same
lesson as Part 3's `if`-with-one-producer, in a different structure.

## Measured: the numbers in the handout

- **Reference, whole suite, twenty consecutive runs on a two-core machine:
  20/20 at 19 passed, 0 failed.** Suite wall time 7–18 s, depending on what
  else the machine is doing. Repeated with four
  unrelated busy loops saturating both cores: 10/10.
- **Starter, unmodified: modally 4 passed, 12 failed, 3 skipped** (3–5 passing;
  `p4_phil` is the coin-flip, see above).
- **The negative control's parameters are measured, not chosen.** At four
  threads and 50 000 increments each the unsynchronised counter came out
  exactly right on 11 of 20 runs — a control that only sometimes controls. At
  eight threads and 200 000 it lost an update on 20 runs out of 20, which is
  why those are the numbers in `cases.c`. The `critical_bump` read-pause-write
  is Part 2's, for the same reason.
- **The starved-writer case is a construction, not a race.** Reference: 10/10
  runs pass. Reader-preferring mutant: 10/10 runs caught. It got there by the
  relay handing over *before* releasing, with a 50 ms limit on how long a
  reader will wait for its successor — three orders of magnitude away from a
  thread wakeup, so neither direction is a coin flip. An earlier version
  without the limit caught the mutant only two runs in three.
- **The philosophers' deadlock**, mutant m5, was caught on 10 of 10 runs by
  the parallel case and 0 of 10 by the meals case.
- **The counting-semaphore case needed a bigger window than it looked like it
  needed.** `a semaphore at 3 admits three and no more` detects an
  over-admitting semaphore by counting how many threads are inside at once —
  and on a TWO-CORE machine only two threads are running at any instant, so
  with a one-microsecond critical section eight threads never overlap even
  when nothing is stopping them. Measured: with the original `dawdle(200)` the
  untouched starter (whose `msem_wait` does nothing at all) **passed** this
  case, and mutant m1 was caught only intermittently. At `dawdle(20000)` —
  tens of microseconds — the starter fails it 10 runs out of 10 and m1 is
  caught 10 out of 10, and the reference still passes in 0.1 s. The general
  form of this is worth remembering when writing any occupancy test: *on n
  cores, only n threads can be caught in the room unless the room takes
  longer to cross than a scheduling quantum's worth of luck.*

## The rubric for DEADLOCK.md (20% of Part 4)

The harness checks that the report exists, names the four conditions, and
mentions both the philosophers and the reader-writer lock. Everything below
is marked by hand.

**Full marks** need all four of:

1. **The four conditions, named and stated**: mutual exclusion, hold and
   wait, no preemption, circular wait, and the fact that all four are
   necessary — so breaking any one is enough.
2. **Which one the philosophers' solution breaks, with an argument that does
   not mention timing.** For the footman: a cycle needs all *n* philosophers
   holding one fork each, at most *n* − 1 are seated, so one philosopher
   holds nothing and the cycle cannot close. For the asymmetric solution: one
   philosopher reaching the other way orients the graph so no cycle can
   close. Either is full marks; "it makes deadlock very unlikely" is not,
   because it is not what the exercise asked.
3. **An explicit statement that the other three conditions are NOT broken.**
   A fork is still exclusive, a philosopher still holds one while waiting for
   the other, and nothing is ever taken back. Students who skip this usually
   end up claiming the footman breaks hold-and-wait, which it does not.
4. **Starvation is not deadlock.** The reader-writer question is the one that
   separates a good report from a complete one: under the textbook lock there
   is *no cycle* in the waits-for graph, nobody is waiting on the writer, and
   the system as a whole makes progress for ever. Deadlock is "nobody moves";
   starvation is "everybody but you moves". A report that answers "the
   reader-writer lock breaks circular wait" has not noticed that there was
   nothing to break.

**Good but not full marks:** conditions named, philosophers argued
correctly, reader-writer lock treated as a deadlock question.

**Not enough:** "my solution uses a footman semaphore so it cannot
deadlock." That is the mechanism, not the argument.

**Actively wrong, and worth marking down for:** claiming the footman breaks
hold-and-wait; claiming the asymmetric solution breaks mutual exclusion;
claiming the reader-writer lock deadlocks.

Worth credit where it appears: the observation that `rw_acquire_write` takes
`turnstile` and then `room_empty` on every path, and that a path taking them
in the other order would give two writers the classic two-lock inversion.
Mutant m13 is exactly that, and the harness catches it.

## What is on your honour

- **The four structures are built on the semaphore and on nothing else.** No
  test can check it. A barrier built on a pthreads mutex and condition
  variable directly passes every case in the suite, and has not done the
  exercise.
- **The reader-writer lock's cost was thought about, not just its
  correctness.** The harness checks that writers are not starved; nothing
  checks that readers still run concurrently in the common case, beyond the
  four-reader case, and nothing checks the write-heavy behaviour at all.
- **`msem_destroy` and the other destroy functions actually release what
  they hold.** The suite does not run under memcheck's leak check, so a
  toolkit that never destroys anything scores full marks.
- **The philosophers' argument was constructed rather than observed.** The
  suite proves that this table on this machine did not deadlock in four
  thousand meals; only the argument in `DEADLOCK.md` proves it cannot.
