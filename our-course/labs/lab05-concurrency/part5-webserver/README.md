# Lab 5 Part 5 — A concurrent web server, twice

**Week 15 · 4.0 hours · 23% of Lab 5 · OSTEP ch. 33**

x86-64 Linux, userspace C, two source files of about 150 and 220 lines of
code. You will need `gcc`, `make`, `valgrind` and `python3` (for the fixture
script).

An HTTP server that serves static files, written twice: once as a thread pool
with a bounded work queue, and once as a single-threaded `select()` event
loop. Then `PERF.md`, in which you load-test both and say what the numbers
mean.

This is where the concurrency block cashes out. Everything in Parts 1–4 was a
mechanism; this is a program that does a job, and the two versions of it are
the two answers the field has to the same question. Chapter 33 ships no code
and asks the reader to build the `select()` server from scratch — this part
is that exercise, with the threaded one alongside so that there is something
to compare.

**Both servers must be built.** If week 15 runs short it is the load-testing
sweep that compresses, not the second implementation: the whole point is the
comparison.

## Layout

```
part5-webserver/
  README.md              this handout
  starter/               Makefile, http.[ch] and pcbuffer.[ch] (supplied),
                         wsthread.c and wsevent.c (yours)              <- work here
  www/                   the fixture document root
  tests/run.sh           the autograder
  tests/cases.c          the HTTP client it runs
  tests/loadgen.c        the load generator, for your measurements
  tests/mkwww.sh         builds the document root, including the big files
  tests/helgrind.supp    two suppressions for a helgrind/glibc disagreement
  solutions/             SPOILERS. Both reference servers, model PERF.md, answer key. Later.
```

```sh
cp -r starter tests www ~/lab5/p5
cd ~/lab5/p5/starter
make test
```

## What you hand in

| File | Weight within Part 5 |
|---|---|
| `wsthread.c` — the thread-pool server | 40% |
| `wsevent.c` — the event-driven server | 40% |
| `PERF.md` — the load-test comparison | 20% |

Twenty-one cases. Twenty are machine-checked in full; the twenty-first checks
that `PERF.md` exists, covers both servers at more than one concurrency
level, and contains numbers. Whether the *explanation* is any good is marked
by hand against the rubric in `solutions/README.md`, and that is most of the
marks for the measurement half. **The untouched starter scores `1 passed, 14
failed, 6 skipped`** — the skips are the two valgrind runs and, for each
server, the two long load cases, which are not run against a server that
cannot answer a single request correctly.

## What is supplied, and what is yours

Parsing HTTP is not the lesson. `http.c` is given to you complete: the
request parser, the path resolver, MIME types, the header formatter, the
listening socket. `pcbuffer.c` is Part 3's bounded buffer, supplied here so
that Part 5 does not depend on Part 3 being finished — swap in your own if
you would rather.

What is yours is the concurrency: an accept loop, a pool, a queue, a select
loop, and a state machine.

## The contract

`starter/http.h`, and the two things in it that the harness depends on:

```sh
./webserver-threaded <docroot> <port>
./webserver-event    <docroot> <port>
```

**A port of 0 means "any free port".** That is what the harness always
passes, and your server must then print

```
listening on <port>
```

to stdout, flushed, before it serves anything — `http_announce()` does it.
A harness that hardcoded a port would collide with whatever else on the
machine happened to be using it, and the failure would look like a bug in
your code rather than in the test.

The protocol is HTTP/1.0 with no keep-alive: GET only (anything else is 501),
`/` means `/index.html`, a path containing `..` is 403 **before the disk is
touched**, a path that does not resolve to a readable file is 404, anything
unparseable is 400, every response carries `Content-Length` and
`Content-Type`, and the connection closes afterwards.

Bodies are **bytes, not strings**. One fixture is full of NUL bytes, one is
256 KB — more than one `read()` — and one is 8 MB, which is more than one
`write()` on the event server's non-blocking socket even on loopback.

`WS_POOL_THREADS` is 4 and `WS_QUEUE_CAP` is 16, and those are in the header
rather than in your code because the harness tests a consequence of them.

## The two architectures, and the case that tells them apart

Seven cases run against both servers: the fixtures, a slow-reading client,
the status codes, path traversal, one stalled client, a 200-request concurrent
soak, and 150 connections under a descriptor limit. Both servers must pass all
seven.

Then there is a pair that does not:

- **`a stalled client blocks the pool`** runs against `webserver-threaded`
  only, and it requires head-of-line blocking to **happen**. Eight clients
  connect and send half a request each. Four of them take every worker in the
  pool; a fresh request then waits until one comes free. That is the honest
  behaviour of a bounded pool of blocking workers, and if it does not happen,
  this binary is not one.

- **`a stalled client blocks nobody`** runs against `webserver-event` only,
  and requires the opposite. Eight half-finished requests are eight slots in
  a table and eight bits in a read mask, and they cost the other connections
  nothing.

Plus a structural check on each: `/proc/<pid>/status` is read while the
server is under load, and the threaded one must report more than one thread
and the event one must report exactly one.

Between them, these four make it impossible to hand in one server twice, in
either direction. That is deliberate. Two servers that behave identically
under every test are one server with two names, and Part 5's argument
disappears.

Measured on a two-core machine, five runs each: the reference thread pool
answers that fresh request in **1001–1002 ms** — it waits for the stalled
clients to give up — and the reference event loop in **0–1 ms**. That is the
factor of a thousand the two architectures differ by, and no throughput
measurement in `PERF.md` shows it.

## What else the harness does that is worth knowing

- **The document root is the harness's**, built by `tests/mkwww.sh` into a
  temporary directory — including `big.bin` (256 KB) and `huge.bin` (8 MB),
  which are generated rather than committed. A file called `secret.txt` is
  placed *outside* it, and the traversal case goes looking for it. Run
  `mkwww.sh` yourself to get the same set for your own testing.
- **One fixture is 8 MB, and only one case fetches it.** A 256 KB response
  goes into a socket send buffer whole on a machine like this one, so it does
  not make `write()` return short — measured, not assumed. `huge.bin` is
  larger than the kernel's maximum send buffer, and the client that asks for
  it uses a 4 KB receive buffer and waits 300 ms before reading. That is what
  makes the write go short, and it is the only case in the suite that catches
  an **event** server which ignores what `write()` returned.
- **That case cannot say anything about your threaded server, and this is
  worth knowing rather than glossing.** The threaded server writes to a
  *blocking* socket, and a blocking write on Linux does not come back short:
  it waits until the last byte has been copied into the kernel. Measured on
  this machine — a single blocking `write()` of all 8 388 608 bytes, to a
  client reading 4 KB at a time after a 300 ms pause, returned 8 388 608. So
  no fixture, at any size, can make `write_all`'s loop matter here.
  **Write the loop anyway.** POSIX permits a short blocking write, a signal
  arriving mid-write can produce one, and the same function on a non-blocking
  socket returns short constantly. It is on your honour, and the answer key
  says so too.
- **The servers run under a descriptor limit of 64.** The 150-connection case
  is there to catch a descriptor leaked per connection: a server that leaks
  one works perfectly for the first fifty and then fails everything, which is
  exactly how this bug behaves in the wild and exactly why it is worth a
  case.
- **Two different valgrind tools, one per server, on purpose.** helgrind over
  the threaded server, where several workers share a queue and possibly a
  buffer. **memcheck** over the event server, because it has one thread and
  helgrind would have nothing to say — what it does have is a state machine
  full of buffer arithmetic, which is what memcheck is for.
- **Every case has a timeout**, and a server that accepts a connection and
  never answers it fails that way rather than hanging the suite.

## The measurements: what `PERF.md` has to contain

`tests/loadgen.c` builds as `loadgen`:

```sh
./loadgen <port> <path> <concurrency> <seconds>
```

It is a **closed-loop** generator: each client waits for its answer before
asking again, so concurrency is the number of requests in flight rather than
an arrival rate. Say so in your report — it is why throughput and latency are
not independent in your numbers.

Required:

1. **Both servers**, at **at least four concurrency levels** (1, 2, 4, 8, 16
   is a good sweep on two cores), with throughput *and* a latency
   distribution — a mean alone hides everything interesting.
2. **At least two document sizes.** The 13-byte file measures request rate;
   the 256 KB file measures bytes, and the two do not have the same shape.
   (The 8 MB one is for the partial-write case, not for the sweep.)
3. **The machine**: how many cores, what else was running, and the fact that
   the load generator is competing with the server for those cores.
4. **An explanation of the shape.** Where does each curve flatten, and why?
   Is there a crossover? **If there is not, say there is not** — a crossover
   invented to make the report tidier is worse than none.
5. **The slow-client behaviour**, which none of your throughput numbers will
   show. The harness measures it; you can too. A report that concludes the
   two servers are equivalent because their throughput curves match has
   missed the one workload where they are not.

A good report explains a number it did not expect. An excellent one
distinguishes what its measurements say about *the architecture* from what
they say about *its own implementation* of it — those are different claims,
and the reference `PERF.md` gets a factor of two from the second kind.

## Running the tests

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread, zero warnings
make test       # ../tests/run.sh on this directory
```

The graded build is `run.sh`'s own `gcc` lines, not your Makefile — but your
Makefile must build **both** servers, and the harness checks that separately.
The suite takes about thirteen seconds.

**Run it more than once**, and if a case ever fails with *the harness could
not open its stalled connections*, that is this machine out of sockets after
many runs in a row rather than a verdict about your server: wait a minute and
run it again. Measured on a two-core machine, both correct servers passed all
twenty-one cases on each of twenty consecutive runs.

## If you get stuck

1. **`no server: it never printed its port`** — `http_announce(bound)` after
   `http_listen` succeeds, before serving. Stdout, flushed.
2. **`a client that read slowly got N bytes of huge.bin's 8388608`** — a
   `write()` whose return value was ignored. It took what fitted in the
   socket buffer and told you so; the rest is your problem. This case only
   ever fires against the **event** server, where the rest goes out on the
   next writable event; the threaded server's blocking socket never returns
   short, so its `write_all` loop is on your honour.
3. **`GET /bytes.bin ... they first differ at offset 0`, and the body is
   short** — the body went out through a string function and stopped at the
   first NUL. Bodies are bytes; you know the length.
4. **`a stalled client does not delay another` fails** — the server is
   serving one connection at a time. In the threaded server, the accept loop
   must hand off rather than serve; in the event server, no loop may wait for
   one connection's data.
5. **`a stalled client blocks nobody` fails on the event server** — there is
   a read loop in it that keeps going until the request is complete. That is
   the threaded server's code. Return to `select()` on HTTP_INCOMPLETE.
6. **`a stalled client blocks the pool` fails on the threaded server** — a
   fresh request was answered while every worker should have been stuck. If
   this server is a copy of the event loop, or spawns a thread per
   connection, that is what this case is for.
7. **`150 connections under a limit of 64 descriptors`, failing partway
   through** — a descriptor leak. Every accepted descriptor is closed exactly
   once, on every path out, including the error paths and the path where the
   client hung up first.
8. **`200 concurrent requests ... N got no response at all`** — a connection
   accepted and never answered: put into the queue with nothing taking it
   out, or taken out and dropped.
9. **`came back wrong (one status was ...)`** under load only — shared state
   that should be per-connection. A single global request buffer will do
   this, and so will one global `struct conn`.
10. **`GET /../secret.txt SERVED A FILE FROM OUTSIDE THE DOCUMENT ROOT`** —
    `http_resolve`'s return value is not being checked.
11. **`exactly one thread is doing the serving` fails** — the event server
    has created a thread. It is one thread and a `select()` loop; that is the
    claim being compared.
12. **helgrind reports a race between the workers** — something global that
    should be on the worker's stack or in the connection's state.
13. **memcheck reports an invalid read or write** — the state machine's
    arithmetic. Usually `sizeof buf - reqlen` when `reqlen` has already
    reached `sizeof buf`.
14. **The suite hangs and then reports `TIMED OUT`** — see 8. A server that
    stops answering is indistinguishable from a crashed one, from outside.

## Stretch goals

- **Stream the file in the event server** rather than reading it into memory:
  keep the file descriptor and an offset in the connection state and read a
  chunk on each writable event. The reference does not, and pays about a
  factor of two on the 256 KB fixture for the two extra copies — which is in
  `PERF.md` and is a fair criticism of it.
- **Add a read timeout to the thread pool** so that a stalled client cannot
  hold a worker for ever, and then rerun the head-of-line case and watch the
  number change. This is what a real thread-pool server does about the
  problem, and it does not remove it, it bounds it.
- **`epoll` instead of `select`**, and measure the difference at 64
  connections and at 1000. The interesting part is not the speed but the
  interface: `select` rebuilds its state on every call and `epoll` does not,
  which is the whole of why one scales and the other does not.
- **HTTP/1.1 keep-alive.** One connection, several requests, and now the
  connection state has to survive the end of a response. In the threaded
  server that is a loop; in the event server it is another state.
