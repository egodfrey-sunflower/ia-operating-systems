# Lab 5 Part 5 — Reference servers and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  Both reference servers — the thread pool and the event loop —    ║
║  and the rubric for PERF.md, which is 20% of Part 5 and is        ║
║  marked by hand.                                                  ║
║                                                                   ║
║  PERF.md in this directory is the model report, with real         ║
║  numbers from a two-core machine and, more importantly, the       ║
║  argument about which of its differences are architectural and    ║
║  which are its own implementation's fault. Reading it before you  ║
║  have taken your own measurements removes the exercise.           ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory: 21 passed, 0 failed
```

`wsthread.c` is 217 lines as shipped, of which 144 are code; `wsevent.c` is
302, of which 217 are. The suite takes about thirteen seconds, most of it the
two 200-request soaks, the two 150-connection runs and the two valgrind runs.

**The untouched starter scores `1 passed, 14 failed, 6 skipped` of 21.** The
single pass is `exactly one thread is doing the serving` for the event
server: the skeleton has not created a thread, and it has not served anything
either. It is isolated by mutation n10, which makes the event server spawn
threads.

The six skips are the two valgrind runs and, per server, the 200-request soak
and the 150-connection run. Those two are gated on the plain fetch case for a
reason worth stating: against a server that answers nothing, 350 more
requests measure nothing and take ten minutes of socket timeouts to do it.
The gate is the same one the valgrind runs use, and the skip message says
what to fix first.

## The two designs, and what each one is for

<details>
<summary><b>The thread pool: state on the stack</b></summary>

An accept loop, `pcb_put` into a bounded queue of descriptors, four workers
doing `pcb_get` / `serve` / `close`. `serve()` is straight-line blocking code:
read until the request is complete, resolve, open, write the header, write the
file. It reads like a description of the protocol, because the thread's
program counter and stack are holding the connection's state for free.

Two properties come out of the bounded queue rather than out of the threads:
when it fills, the accept loop blocks, which stops descriptors being accepted
faster than they can be served and leaves the arrivals in the kernel's listen
backlog. That is back-pressure, and it is why the pool is a queue rather than
`pthread_create` per connection.

The cost is the case the harness insists on: four workers is four connections
being served, and a client that stops talking mid-request holds a worker
until it gives up.

</details>

<details>
<summary><b>The event loop: state in a table</b></summary>

One thread, `select()`, and `struct conn` per connection holding what the
worker's stack was holding: the descriptor, which half of the conversation it
is in, how much of the request has arrived, the response and how much of it
has gone.

Every place the threaded server blocks is a place this one returns to the
loop, and that is the whole trade. The read is one `read()` and a return, not
a loop; the write is one `write()`, `sent += n`, and a return.

There is no locking anywhere in the file and none is needed. That is the
other half of what the model buys, and it is why the harness runs *memcheck*
over this server rather than helgrind — helgrind would have nothing to say
and saying it would be dishonest.

</details>

<details>
<summary><b>Why the reference event server buffers the whole response, and what it costs</b></summary>

`set_file` reads the file into memory and then copies header and body into
one buffer, which the write side dribbles out. It is the simplest correct
thing: the write side has exactly one number to track.

It costs two allocations and two copies of the body per request, and on the
256 KB fixture that is about a factor of two against the threaded server —
which streams from a 16 KB stack buffer and allocates nothing. `PERF.md`
measures it and says plainly that this is an implementation cost and not an
architectural one; the architectural version of the statement is that the
event model has to materialise per connection what the threaded model keeps
implicitly on a stack.

Streaming — keep the file descriptor and an offset in `struct conn`, read a
chunk per writable event — is the handout's first stretch goal. It is also
what a real one does.

</details>

## What each case is for

Every row was checked by breaking a reference server and confirming the named
case fails; the mutant column names the mutation in the table below.

| Case | Isolated by |
|---|---|
| every fixture comes back byte for byte | a body written with a string function (n5), which stops at the first NUL; a wrong `Content-Length` (n11); any server that answers nothing |
| a slow-reading client still gets every byte | **an EVENT server that ignores what `write()` returned (n4) — the only case that catches it, and it took an 8 MB fixture and a deliberately slow client to build one that does.** It cannot catch the same mistake in the threaded server (n4b): that socket is blocking, and Linux blocks rather than returning short |
| 404, 400 and 501 are what they should be | a server that closes the connection instead of answering; a missing status branch |
| a path with .. does not escape the root | `http_resolve`'s return value ignored (n8) |
| a stalled client does not delay another | a server that serves inline in the accept loop (n2) — the minimum meaning of "concurrent"; and an event loop that reads until the request is complete (n3) |
| 200 concurrent requests all come back right | a task lost between the accept loop and the workers (n7); shared state that should be per-connection |
| 150 connections under a limit of 64 descriptors | **a descriptor leaked per connection (n1, n6) — the first case to catch it, and the one whose message names it.** The soak fails too, a little later and less clearly |
| more than one thread is doing the serving | the threaded server submitted as a copy of the event loop (n9) |
| a stalled client blocks the pool | the same (n9), and any server that is not a bounded pool of blocking workers |
| exactly one thread is doing the serving | **the event server spawning threads (n10) — the only case that catches it** |
| a stalled client blocks nobody | a read loop in the event server that waits for the whole request (n3), and any event server that has stopped being able to accept (n6, n6b) |
| helgrind finds no race between the workers | shared state touched outside the queue's mutex |
| memcheck finds no bad memory in the state machine | buffer arithmetic in the connection table |
| PERF.md reports both servers at several concurrencies | an absent or one-sided report |

## Measured: every mutation, against the whole suite

Fourteen mutants, each compiled by the harness's own `-Wall -Wextra -Werror`
lines and run against all twenty-one cases on a two-core machine. **One of
them scores full marks and always will (n4b), one scored full marks until a
case was added for it (n4), and both are in the table.**

| # | Mutation | Result | Cases that failed |
|---|---|---|---|
| n1 | threaded: `send_file` never closes the file it opened | 18 P, 3 F | fdsoak, soak, hol_blocks — the realistic leak: the socket is closed, so responses are correct until the process runs out of descriptors |
| n1b | threaded: the accepted socket is never closed at all | 13 P, 5 F, 3 S | fetch, status, traversal, stalled, hol_blocks — a client cannot tell "no EOF" from "no answer", so everything fails |
| n2 | threaded: the accept loop serves inline, no pool | 20 P, 1 F | **stalled alone.** Note it still passes hol_blocks: a serial server blocks behind stalled clients too |
| n3 | event: a read loop that waits for the whole request | 17 P, 4 F | **hol_free**, stalled, soak, fdsoak — the event loop's cardinal sin |
| n4 | event: `write()`'s return value ignored (`sent = outlen`) | 20 P, 1 F | **drip alone** |
| n4b | threaded: `write_all` reduced to one unchecked `write()` | **21 P, 0 F** | **none, and none is possible** — see below |
| n5 | threaded: body written with a string function | 16 P, 2 F, 3 S | fetch, drip — `bytes.bin` is full of NULs |
| n6 | event: `set_file` never closes the file it opened | 18 P, 3 F | fdsoak, soak, hol_free |
| n6b | event: `conn_close` never closes the socket | 13 P, 5 F, 3 S | fetch, status, traversal, stalled, hol_free |
| n7 | threaded: one connection in fifty taken off the queue and dropped | 18 P, 2 F, 1 S | **soak**, fdsoak — a task lost under load |
| n8 | threaded: `http_resolve`'s return value ignored | 20 P, 1 F | **traversal alone** — and it served the file outside the root |
| n9 | `webserver-threaded` is a copy of the event loop | 19 P, 2 F | **thread count, hol_blocks** — the "submit one server twice" case |
| n10 | `webserver-event` spawns a thread per connection | 20 P, 1 F | **thread count alone** — the same trick in the other direction |
| n11 | threaded: `Content-Length` one byte short | 17 P, 1 F, 3 S | fetch |

Three of those are worth reading twice.

**n4 is the reason the 8 MB fixture exists.** With the fixture set at 256 KB,
the mutation that ignores `write()`'s return value and closes anyway scored
**21 passed, 0 failed** — a genuine test-that-cannot-fail, found by mutating
rather than by reading. The reason is measurable: on this kernel a 256 KB
`write()` into a socket whose send buffer has autotuned to a few megabytes is
accepted whole even when the receiver's buffer is 4 KB and the receiver is
asleep. It took an 8 MB body (`net.ipv4.tcp_wmem`'s maximum here is 4 MB), a
4 KB client receive buffer and a 300 ms pause to make the first `write()`
return 1.8 MB of 8.4 MB. With that case in place the mutation is caught, by
that case and nothing else.

**n4b is the limit of that fix, and it is a declared hole rather than a
patched one.** The same mutation applied to the *threaded* server —
`write_all` reduced to a single unchecked `write()` — scores **21 passed, 0
failed**, and no fixture at any size will change that. The threaded server's
accepted socket is blocking, and a blocking `write()` on Linux does not come
back short: it waits until the last byte is in the kernel. Measured directly,
outside the harness: one blocking `write()` of all 8 388 608 bytes, to a
client with a 4 KB receive buffer that waits 300 ms and then reads 4 KB at a
time, returned 8 388 608. Capping the send buffer would change it, but
`SO_SNDBUF` is set on the server's own socket and the harness is outside that
process; making the socket non-blocking would change it too, and would delete
the architectural contrast the whole part is built on. So the threaded
`write_all` loop is on the honour list below, and the handout, the starter
comment and the case's own failure message all say which server the case
covers.

**n2 shows what the head-of-line case does not prove.** A server with no pool
at all, serving inline in the accept loop, still "blocks behind stalled
clients" — so `hol_blocks` is not evidence that a thread pool exists. That is
what the thread-count case is for, and the two together are what n9 fails.

**n7's lost task is caught as a timeout, not as a wrong answer.** The client
gets no response at all, the case reports how many, and the message names the
two places a descriptor goes missing between the accept loop and the workers.


## The two architecture cases, and why they are not unfair

The pair `a stalled client blocks the pool` / `a stalled client blocks
nobody` is the only place in this course where a case *requires* a program to
be slow. It is worth being explicit about why.

Part 5's deliverable is not "a working web server". It is "the same web
server under two concurrency architectures, measured against each other". If
both binaries can be the same program, the comparison in `PERF.md` compares
nothing, and the part's entire argument — ch. 33's argument — evaporates. The
cheapest way to submit half the work is to write one server and copy it, and
these two cases are what make that impossible in both directions.

They are also not arbitrary: head-of-line blocking behind a slow client is
*the* difference between the models, the one that motivated event-driven
servers historically, and the one that no throughput measurement on loopback
will ever show. Measured on the references, with 8 stalled clients:

```
threaded:  1001-1002 ms   (5 runs)
event:         0-1 ms     (5 runs)
```

A factor of about a thousand, with no overlap between the two distributions,
on the workload that matters — and invisible in every other case in the
suite and in every number in `PERF.md`'s throughput tables.

## Measured: the numbers in the handout

- **Reference, whole suite, twenty consecutive runs on a two-core machine:
  20/20 at 21 passed, 0 failed.** Suite wall time 13–15 s. (The first attempt
  at this measurement came back 13/20 — see the next section; that was the
  harness's fault and it is fixed.)
- **Starter, unmodified: 1 passed, 14 failed, 6 skipped**, reproducibly.
- **The architecture pair**, five runs each: thread pool 1001–1002 ms, event
  loop 0–1 ms, threshold 400 ms. No overlap, and three orders of magnitude of
  margin on the event side.
- **Connection churn**: about 1 100 sockets per suite run, which leaves the
  machine with ~5 700 in TIME_WAIT after twenty back-to-back runs and no
  failures. Before the fix it was ~14 000 per run and the kernel's TIME_WAIT
  table was at its 16 384-entry cap.

## Two harness defects found by measuring rather than by reading

Both are worth recording, because both are the kind of thing that looks fine
in review and is only visible in numbers.

**A case that could not fail.** With the fixture set topping out at 256 KB,
the mutation that ignores `write()`'s return value scored 21 out of 21. On
this kernel a 256 KB `write()` is accepted whole into a socket send buffer
that has autotuned into the megabytes, even when the receiver's buffer is
4 KB and the receiver is asleep — so nothing in the suite ever made a write
go short. The fix is the 8 MB fixture and the slow-reading client, and with
it the mutation is caught in the **event** server by that case and nothing
else. It is caught in the threaded server by nothing at all, and cannot be:
that socket is blocking (n4b above). Half a fix, and the half that does not
work is declared on the honour list rather than papered over — the failure
mode here is not the missing case, it is claiming coverage the artefact does
not have. The lesson is the one this course keeps relearning: *a test
verifies a property only if some plausible wrong implementation produces
different output*, and the only way to know is to build the wrong
implementation.

**A test that failed good code, and it was the harness's own fault.** The
first flake measurement — twenty consecutive runs of the whole suite against
the reference — came back **7 failed of 20**, with failures scattered across
`every fixture comes back byte for byte`, the descriptor soak, and both
architecture cases. None of them was the servers. The suite was opening about
**14 000 connections per run**, almost all of them from one 4-thread run of
`loadgen` per server used only to sample `/proc/<pid>/status`, and after a few
runs the machine had tens of thousands of sockets in TIME_WAIT and the
harness's own `connect()` began failing. Three changes: sample the thread
count during a case that is running anyway rather than under a load generator
(14 000 connections per run → about 1 100), shrink the two long load cases
(320 → 200 requests, 300 → 150 connections), and retry a failed `connect()`
eight times before giving up — and then say *the harness could not open its
stalled connections* rather than blaming the server. Re-measured: 20 of 20.

## The rubric for PERF.md (20% of Part 5)

The harness checks that the report exists, mentions both servers, uses the
words the measurement is about, and contains numbers at several concurrency
levels. Everything below is marked by hand.

**Full marks** need all five of:

1. **Both servers, four or more concurrency levels, two document sizes**, with
   throughput and a latency distribution rather than a mean alone.
2. **The machine stated**, including that the load generator competes with
   the server for the same two cores, and that loopback has no network in it.
   A report that presents these numbers as properties of the servers alone
   has over-claimed.
3. **The shape explained.** Where the curves flatten and why; whether there
   is a crossover. **Saying there is no crossover, when there is not, is full
   marks**; inventing one is not.
4. **The slow-client behaviour**, which the throughput sweep does not show.
   Full marks require noticing that the load generator cannot measure the
   difference the two architectures are actually about.
5. **Architecture separated from implementation.** The reference event server
   is 2x slower on the large file because of two extra copies, not because of
   `select()`. A report that says "event-driven servers are slower for large
   files" has drawn an architectural conclusion from an implementation
   detail.

**Good but not full marks:** both servers measured properly, shape
explained, no discussion of slow clients.

**Not enough:** a table with no prose. The numbers are the easy half.

**Actively wrong, and worth marking down for:** concluding that one
architecture is faster from differences inside the run-to-run noise on a
loaded two-core box; reporting a mean latency only and calling the tail
"noise" without measuring it; presenting closed-loop throughput as if it were
a service rate.

## What is on your honour

- **Both servers are yours.** The harness makes it hard to submit one twice,
  but a student who writes the event loop and then wraps it in a pool of
  threads that each run their own loop can satisfy both structural cases with
  one design. Nothing checks the intent.
- **The measurements were taken on a quiet machine, or the report says they
  were not.** Nothing can tell.
- **The event server has no lock in it.** It does not need one, and adding
  one because it "feels safer" means the model has not landed. The harness
  cannot see that, and memcheck will not object.
- **The threaded server's `write_all` really loops.** No case can reach it:
  mutation n4b above scores full marks and is not fixable from outside the
  server's process. The loop is still required — POSIX permits a short
  blocking write, a signal arriving mid-write produces one, and the same
  function on `wsevent.c`'s non-blocking socket returns short constantly.
- **The response buffer is per connection.** With one thread and one
  connection at a time in testing, a single global buffer passes everything
  except the soak — and sometimes passes the soak too.
