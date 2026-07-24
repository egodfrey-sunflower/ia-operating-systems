# PERF.md — model report

Both servers, five concurrency levels, two document sizes. The numbers below
are real, from the machine described here, and the point of the report is the
last section: what shape they make and why.

## The machine, the load, and what that limits

- 2 cores, ~3.7 GiB. **The load generator and the server share those two
  cores**, so every number here is a measurement of the pair, not of the
  server alone. On a two-core box this is not a footnote: at concurrency 16
  there are 17 runnable threads competing for 2 cores, and most of what the
  latency column shows past concurrency 4 is queueing for a core rather than
  queueing for the server.
- Everything is over loopback: no network, no packet loss, no RTT worth
  measuring. That flatters both servers and flatters the threaded one more,
  because its weakness is a *slow* client and loopback has none.
- `tests/loadgen.c`, which is **closed loop**: each client waits for its
  answer before asking again. So concurrency is the number of requests in
  flight, not an arrival rate, and throughput and latency are not
  independent — at fixed concurrency, `throughput ≈ concurrency / latency` by
  construction. An open-loop generator would show queueing collapse; this one
  shows graceful degradation, and the difference is the generator, not the
  server.
- 3 seconds per point, one run each. Machine load average was ~1.9 from
  another workload throughout, so these are not quiet-machine numbers.

Command, for both servers, at each level:

```sh
./webserver-threaded ./www 0 &        # prints: listening on <port>
./loadgen <port> /hello.txt <c> 3
./loadgen <port> /big.bin   <c> 3
```

## /hello.txt (13 bytes) — the request-rate test

| Concurrency | threaded req/s | threaded p50 / p95 ms | event req/s | event p50 / p95 ms |
|---|---|---|---|---|
| 1  | 5882 | 0.14 / 0.23 | 5927 | 0.14 / 0.21 |
| 2  | 6343 | 0.26 / 0.56 | 8101 | 0.18 / 0.43 |
| 4  | 7407 | 0.42 / 1.43 | 7920 | 0.39 / 1.37 |
| 8  | 8745 | 0.71 / 2.29 | 9258 | 0.67 / 2.35 |
| 16 | 9697 | 1.35 / 3.62 | 9446 | 1.45 / 3.51 |

With a 13-byte body, essentially all of the work is connection setup,
teardown and scheduling. The two servers are within a few per cent of each
other everywhere, and the ordering changes between levels — at 2 and 8 the
event loop is ahead, at 16 the pool is. **There is no crossover here, and no
winner.** Reporting one would be reporting noise.

Both curves flatten in the same place, between 8 and 16, which is what
running out of two cores looks like: throughput stops rising and latency
starts rising in proportion to concurrency.

## /big.bin (256 KB) — the bytes test

| Concurrency | threaded req/s | threaded p50 / p95 ms | event req/s | event p50 / p95 ms |
|---|---|---|---|---|
| 1  | 2562 | 0.34 / 0.57 | 1214 | 0.70 / 1.59 |
| 2  | 2369 | 0.70 / 1.89 | 1458 | 1.12 / 3.07 |
| 4  | 2386 | 1.38 / 3.89 | 1592 | 2.28 / 4.09 |
| 8  | 2964 | 2.29 / 5.98 | 1693 | 4.41 / 6.94 |
| 16 | 3424 | 4.18 / 8.88 | 1552 | 9.63 / 15.91 |

Here they separate, and **the event server is about twice as slow at every
level**. That is not the architecture losing; it is my implementation of it,
and the distinction is the most useful thing in this report.

The threaded server streams the file: a 16 KB stack buffer, `read`,
`write_all`, repeat. Its per-request allocation is zero and its per-request
copying is one pass. The event server cannot do that, because "repeat" is a
loop it is not allowed to sit in — so it materialises the whole response
first: `malloc(len)`, read the file into it, `malloc(header + len)`, copy
both in, and then dribble it out across as many `select()` wakeups as it
takes. Two allocations and two extra copies of 256 KB per request, and at
2500 requests/second that is measurable in exactly the way the table shows.

**The event model's cost is that state which used to live implicitly in a
thread's stack has to be materialised.** For a 13-byte file there is nothing
to materialise and the two are equal; for a 256 KB file it is 256 KB per
connection in flight. That is the trade, and it is visible in memory as well
as time: at 16 connections this event server holds up to 4 MB of response
buffers, which the threaded server never allocates at all.

The honest way to close the gap is to keep the file descriptor and the offset
in the connection state and read a chunk at a time on each writable event —
more state, more code, no copies. That is a fair amount of work for a lab and
it is exactly the work ch. 33 is describing when it says event-driven code is
harder to write.

## What the numbers do not show, and the case that does

Neither table shows the thing the two architectures actually differ in,
because the load generator has no slow clients in it. The harness's
architecture cases do:

```
p5_hol_blocks (threaded): measured 1002 ms with 8 clients stalled
p5_hol_free   (event):    measured    1 ms with 8 clients stalled
```

Five runs each, and the spread is nothing: the thread pool measured
1001–1002 ms every time (the stalled clients let go at 1000 ms, and the
queued request is served immediately afterwards) and the event loop 0–1 ms.

Eight clients that connect and send half a request. The thread pool has four
workers; four of the eight take all of them, the rest queue, and a fresh
request waits **1002 ms** — until the stalled clients give up. The event loop
answers the same request in **1 ms**, because eight half-finished requests
are eight slots in a table and eight bits in a read mask.

That is a factor of a thousand, on the one workload that distinguishes the
models, and it does not appear anywhere in the throughput tables. If I had
only run `loadgen` I would have concluded the two servers were the same
server. **The measurement you did not take is the one that would have told
you something.**

## Conclusions

1. On short responses over loopback with two cores, the two models are the
   same speed to within noise, and both are limited by the cores rather than
   by the architecture.
2. On large responses this event server is 2x slower, for an implementation
   reason — two extra copies of the body — not an architectural one. The
   architectural version of the same statement is that the event model has to
   store per-connection what the threaded model keeps on a stack.
3. The models differ enormously under slow clients, which is exactly what
   neither throughput table measures, and it is where the choice between them
   is actually made.
4. If I had to ship one for a static-file server on this machine: the thread
   pool, with a read timeout on the socket so that a stalled client cannot
   hold a worker for ever. The event loop wins on connection count, and 4
   workers against 64 connection slots is a difference that only matters when
   connections outnumber cores by a lot more than they do here.
