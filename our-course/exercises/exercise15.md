# Exercise Sheet 15 — Events, I/O devices, and buffering

**Attempt after Week 15.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise15-solutions.md`](solutions/exercise15-solutions.md).

**This sheet leans on:** OSTEP ch. 33 and 36; Silberschatz §12.4.2 (buffering)
and §12.4.4 (spooling), 10th ed.; Ousterhout (1996), *Why Threads Are A Bad
Idea*. §B3(b) assumes week 12's spin-vs-sleep analysis.

**You will need:** pen, paper, and a calculator. No code, no simulator —
ch. 33 ships no homework code and ch. 36 ships none at all; every question
here is original, and §B is deliberately arithmetic-heavy because that is
what Cambridge sets on this material.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns
nothing.*

**A1.** A single-threaded event-based server needs no locks, and this
advantage survives the move to a multicore machine.

**A2.** In an event-based server, a handler that makes a blocking system
call delays only the client whose request it is handling.

**A3.** An event-based server that is careful to issue only asynchronous
I/O can still be blocked by the virtual-memory system.

**A4.** Interrupts are always more efficient than polling, because polling
burns CPU while waiting.

**A5.** With DMA, the CPU no longer moves the data, so the OS discovers the
transfer is finished by periodically reading the device's status register.

**A6.** Memory-mapped I/O means user programs can drive devices directly
with ordinary loads and stores.

**A7.** Double buffering can raise throughput even when producer and
consumer have identical average rates.

**A8.** Provided its buffer is large enough, a circular buffer lets a
consumer keep up with a producer whose *average* rate exceeds the consumer's
rate.

---

## B. The arithmetic of I/O

**B1. Overlap, or: what double buffering buys.**
A program processes a 20-block file. Reading one block into memory takes
3 ms; processing one block takes 2 ms of pure CPU.
  (a) With a single buffer — read a block, process it, read the next — what
      is the total elapsed time?
  (b) With two buffers and DMA, the next block can be read while the current
      one is processed. Derive the steady-state time per block and the total
      elapsed time. State exactly which hardware capabilities your answer
      assumes.
  (c) Redo (b) with processing taking 4 ms per block. What now bounds the
      elapsed time, and what has double buffering bought?
  (d) State the general rule for the elapsed time of an N-block job with
      per-block read time `r` and process time `p`, single- vs
      double-buffered.

**B2. Sizing against a burst.** *(The `y2020p2q4` pattern — Silberschatz
§12.4.2 material.)*
A capture device delivers a data stream to an application that consumes at a
steady 1 MB/s. The stream usually runs at 0.5 MB/s, but bursts at 5 MB/s for
2 s at a time. Kernel buffers come in 512 KB units.
  (a) At what rate does buffered data accumulate during a burst, and how
      much has accumulated by the burst's end?
  (b) Show that single buffering with one 512 KB buffer, and double
      buffering with two, both lose data — and compute how early in the
      burst each starts losing it.
  (c) How many 512 KB buffers must a circular buffer chain to survive one
      full burst without loss?
  (d) After the burst ends, how long until the backlog is fully drained —
      and therefore what is the minimum quiet gap between bursts for your
      answer in (c) to keep working?
  (e) Suppose the stream's long-run average rate rose to exactly 1 MB/s,
      bursts included. What does *any* finite buffer do for you now? State
      the general principle.

**B3. Polling, interrupts, and the hybrid.**
Let a device's operation take `D` µs. Handling an interrupt costs `C` µs of
CPU (context switch away, handler, switch back); a polling check is
effectively free but occupies the CPU for the whole wait.
  (a) In CPU time lost per operation, when should the OS poll and when
      should it use interrupts? Apply your rule to: (i) a modern NVMe-class
      device with `D = 5` µs, (ii) a disk with `D = 10` ms, both with
      `C = 20` µs.
  (b) The device's `D` is unknown and variable. Propose a two-phase hybrid
      and bound its worst-case cost relative to an oracle that knows `D`.
      (You proved the same bound in week 12 — for what?)
  (c) A NIC delivers 50,000 packets/s, each raising an interrupt with
      `C = 4` µs. What fraction of the CPU disappears into interrupt
      handling? The NIC can instead coalesce interrupts, batching up to 8
      completions or 100 µs, whichever comes first. Recompute the CPU
      fraction at full batching and state what has been paid for it.
  (d) Under a packet flood the machine does nothing but service interrupts.
      Name this condition and the counter-intuitive remedy ch. 36
      recommends.

**B4. The price of each concurrency model.**
A server must hold 10,000 concurrent connections, mostly idle.
  (a) Thread-per-connection, with an 8 KB kernel stack and 8 KB of user
      stack per thread: how much memory do the stacks alone pin?
  (b) Event-based: each idle connection needs a continuation record of
      roughly 256 bytes. Total?
  (c) A handler's logic performs three I/O operations, each of which may
      block. Into how many pieces must the handler be ripped in the pure
      event model, and what exactly must be packaged into the continuation
      at each seam?
  (d) One line each: which resource does (a) squander, and which programmer
      cost does (c) reveal?

**B5. Spooling is not buffering.**
A department shares one printer. Jobs arrive from many machines, take
minutes to print, and must come out uninterleaved.
  (a) Why is a buffer in the printer driver not sufficient? Say precisely
      which problem remains.
  (b) Describe the spool design (§12.4.4): what is stored, where, and what
      the daemon does.
  (c) A lock on the printer — acquire, print, release — would also
      serialise jobs. Give two concrete reasons the spool is the better
      design for this device, and one situation where the lock would be
      preferable.

---

## C. Discussion and design critique

**C1. Threads or events, under a stated constraint.**
You are building the connection-handling core of a network file server that
will run on a **single-core** appliance with 256 MB of RAM. The workload:
thousands of mostly idle connections; most requests are served from an
in-memory cache in microseconds; a few per cent miss and need a disk read
(milliseconds).

Compare the two designs from this week's reading — (i) thread-per-connection
with blocking I/O, and (ii) a single event loop with asynchronous I/O — *for
this machine and workload*. Address: memory footprint at 10,000 connections;
what each design does with the cache-hit fast path; what each does when a
request misses and the disk is involved; and which failure modes each is
exposed to (locks and races vs stack ripping and accidental blocking).
Deliver a verdict — and then state, precisely, the two changes to the
constraint or workload that would each flip it. Finish by explaining what
the Flash server's hybrid (events + a small thread pool for disk I/O) buys,
and which of your two flips it is designed to dodge.

**C2. Arguing with Ousterhout.**
Ousterhout's talk claims threads are too hard for most programmers and most
purposes — locks, deadlock, and non-determinism are the everyday tax — and
that events are the right default, with threads reserved for true CPU
parallelism.

  (a) Steelman him: using only material from weeks 11–14 (the bug taxonomy
      of Lu et al. is admissible evidence), make his case as strongly as
      the evidence allows.
  (b) Now make the strongest case against, using ch. 33's own admissions.
      Two developments since 1996 do most of the damage — identify them.
  (c) Conclude with the version of his claim you would defend today: a
      one-sentence rule for choosing between threads and events, with its
      conditions attached.
