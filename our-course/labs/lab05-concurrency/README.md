# Lab 5 — Concurrency

**Weeks 11–15 · 18 hours · OSTEP ch. 26–33 · xv6 rev5 §7.6 (memory barriers)**

Userspace C on Linux, from scratch. No kernel work, no xv6, no QEMU, no root.
You will need `bash`, `gcc`, `make`, and `valgrind` — the last of these is not
optional here, and the parts that need it say why.

Built in the order the reading arrives. A user-level thread package with its
own context switch and scheduler. A lock package built up from hardware
atomics to a lock that sleeps. A bounded buffer with condition variables. A
synchronisation toolkit — semaphores and the classic structures — on top of
those. And a web server that serves real HTTP, written twice: once over a
thread pool running its work queue on that bounded buffer, and once as a
single-threaded event loop. You cannot build a correct lock before you have
atomics and memory barriers, so the parts unlock in chapter order.

## Layout

```
lab05-concurrency/
  README.md            this handout: what to hand in, and how it is checked
  part1-uthread/       threads: a context switch, a stack, a ready queue
  part2-locks/         locks: test-and-set, ticket, and one that sleeps
  part3-pcbuffer/      condition variables and a bounded buffer
  part4-toolkit/       semaphores and the classic synchronisation structures
  part5-webserver/     a concurrent HTTP server, threaded and event-driven
```

Start with the part whose week you are in:

| Part | Handout | Weeks |
|---|---|---|
| 1 — a user-level thread package | [`part1-uthread/README.md`](part1-uthread/README.md) | 11–12 |
| 2 — locks, from atomics upward | [`part2-locks/README.md`](part2-locks/README.md) | 12 |
| 3 — condition variables and a bounded buffer | [`part3-pcbuffer/README.md`](part3-pcbuffer/README.md) | 13 |
| 4 — the synchronisation toolkit | [`part4-toolkit/README.md`](part4-toolkit/README.md) | 14 |
| 5 — a concurrent web server | [`part5-webserver/README.md`](part5-webserver/README.md) | 15 |

Every part is a self-contained directory with its own handout, `starter/`,
`tests/` and `solutions/`, and its own `README.md` with the detail. **They are
separate on purpose**: five weeks is long enough that being stuck on Part 1
must not stop you starting Part 2. **No part depends on the code in any
other**: Part 4's semaphore is built on pthreads rather than on your Part 2
locks, and Part 5 ships a copy of the Part 3 bounded buffer for its work
queue — swap in your own if you would rather, but you do not have to. This
file is the only thing you need to have read before opening any of them: it
says what the whole lab hands in, how the checking works, and why a green run
here means less than a green run elsewhere. The detail — the contract, the
starter, the hints — is in the part.

Inside a part, copy the working directories somewhere of your own:

```sh
cp -r part1-uthread/starter part1-uthread/tests part1-uthread/fixtures ~/lab5/p1
cd ~/lab5/p1/starter
make test
```

`solutions/` is deliberately left behind. Copy `tests/` along with `starter/`,
and the data directory where a part has one — `fixtures/` in Part 1, `www/` in
Part 5: the Makefile runs `../tests/run.sh`, and that path only resolves if the
layout comes with you. Each part's handout gives the exact line.

## What you hand in

| File | Part | Hours | Week(s) | Weight |
|---|---|---|---|---|
| `uthread.c`, `swtch.S` — threads that run, yield and exit | 1 | 3.5 | 11–12 | 19% |
| `mylock.c` — three locks; `LOCKS.md` — what they cost | 2 | 3.5 | 12 | 19% |
| `pcbuffer.c` — the bounded buffer; `BREAKAGE.md` — the two demonstrations | 3 | 3.5 | 13 | 20% |
| `toolkit.c` — semaphores, barrier, rendezvous, reader-writer lock, philosophers; `DEADLOCK.md` — which condition each breaks | 4 | 3.5 | 14 | 19% |
| `wsthread.c` and `wsevent.c` — the server twice; `PERF.md` — the load-test comparison | 5 | 4.0 | 15 | 23% |

## How it is checked, and the caveat that governs everything

**Concurrency tests are probabilistic, and a passing run is weak evidence.**
That is not a disclaimer bolted on to the end; it is the fact the whole
checking scheme is designed around, and it is why the harness looks different
here from the other labs.

- **Parts 2, 3 and 4 auto-grade on outcome.** After *N* threads each increment
  a counter *M* times, the value must be exactly *N×M*; after *N* items go
  into a buffer, exactly those *N* items must come out; a semaphore
  initialised to three admits three. High thread counts, many
  repetitions, and a hard timeout — and **a timeout is reported as a deadlock,
  not as a wrong answer**, because "everyone is asleep" and "two threads got in
  each other's way" are different bugs with different causes and the results
  line has to say which.

- **Part 5 auto-grades on bytes and on time.** Fetch the fixtures, diff them
  byte for byte, check the status codes, soak it with concurrent clients —
  and measure what happens to one client while others are stalled, which is
  the only test that can tell the two server architectures apart. Its
  performance half is a rubric, not a test: the harness checks that `PERF.md`
  covers both servers at several concurrency levels and says nothing about
  whether the explanation is any good.

- **Parts 2 and 4 open with a negative control.** The same workload with no
  lock at all must produce the wrong answer. If it does not — if the machine is
  so loaded, or so short of cores, that two unsynchronised threads cannot lose
  an update — then the cases after it are not evidence of anything, and the
  harness says so in as many words.

- **A clean `valgrind --tool=helgrind` run is required everywhere there is
  more than one thread.** It is the check that finds what the outcome tests
  miss: shared state touched outside a critical section, a condition variable
  used with the wrong mutex, a wait on a mutex the thread does not hold. Each
  part's handout says exactly what helgrind can and cannot see there — in Part
  2 it cannot audit your atomics and the handout explains what it does
  instead, and in Part 5 the event server has one thread, so the harness runs
  **memcheck** over that one rather than pretending a race detector has
  something to say about it.

- **Part 1 is the exception, and it is a pleasant one.** Cooperative threads
  yielding in a fixed pattern produce exactly one legal output, so six of Part
  1's cases are byte-for-byte diffs against a stored transcript. It is the one
  part of a concurrency lab that can be tested like ordinary code, and it is
  tested like ordinary code.

- **Run the suites more than once.** A single green run of a concurrency test
  suite tells you less than a single green run of any other suite in this
  course. Measured on a two-core machine, every part's cases pass a correct
  implementation on every one of twenty-five consecutive runs for Parts 1–3
  and twenty for Parts 4 and 5; that is the number you should expect, and one
  failure in twenty is a result worth investigating rather than re-running
  away.

**What is marked by hand and by no test at all:** `LOCKS.md`'s argument in
Part 2, `BREAKAGE.md`'s in Part 3, `DEADLOCK.md`'s in Part 4 and `PERF.md`'s
in Part 5. The harness checks that the measurements and the transcripts are
there; it says nothing about whether the reasoning is any good. Each is a fifth of its
part, and a full green run is reachable having written none of them well. A
green run is not a finished lab.

## Stretch goals

- Add preemption to Part 1 with a timer signal, and find out which of your own
  functions are not async-signal-safe.
- Implement an MCS queue lock and compare its scaling with your ticket lock.
  The `bench` driver in Part 2 will measure it unchanged if you give it the
  same three-function shape.
- Make Part 5's event server stream large files instead of buffering them, and
  find the change in the numbers. The reference does not, and pays for it.
