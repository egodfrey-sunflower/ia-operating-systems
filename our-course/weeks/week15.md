# Week 15 — Events, and the turn to persistence: I/O devices and buffering

> **Part II ends, Part III begins.** Week 15 of 27 — ch. 33–34 close
> Concurrency; ch. 35–36 open Persistence.

## What you'll learn

This is a hinge week: one last concurrency chapter, then the book's third
piece begins.

Chapter 33 presents the road not taken: **event-based concurrency**. Instead
of threads, one loop waits for events (`select()`/`poll()`), and each handler
runs to completion. On a single CPU this dissolves the whole of weeks 11–14:
no interleaving, no locks, and scheduling becomes an application decision.
The price arrives in instalments. A handler that blocks stalls the *entire*
server, so every I/O must become **asynchronous**, and the state a thread
would have kept on its stack must be packaged by hand into **continuations**
— Adya et al. call it manual stack management, and the chapter is honest that
this is events' deep cost. Multicore brings the locks back; page faults block
implicitly no matter how careful you are. Ousterhout's talk — this week's
short paper — is the classic statement of the pro-event case: threads are too
hard for most purposes, events are the tool for GUIs and many servers, and
you should pay for threads only when you need true CPU parallelism.

Chapter 36 then starts Persistence at the bottom: how an OS talks to a
device at all. A canonical device is a few registers — status, command, data
— behind a bus hierarchy that is tiered because fast buses must be short.
The chapter builds the interaction up mechanism by mechanism: **polling**,
then **interrupts** (worth it only for slow devices — for fast ones the
context switches cost more than the wait, and under packet floods interrupts
**livelock** a system, so real systems use hybrids and coalescing), then
**DMA** to get the CPU out of the data-copying business, then port-mapped vs
**memory-mapped I/O**, and finally the **device driver** — the abstraction
that lets a file system not care what disk it sits on, and, at ~70% of Linux
kernel code, the OS's largest and buggiest province.

The Silberschatz cross-reading is **required this week, not optional** — the
single place this course teaches **I/O buffering theory**: why kernels buffer
at all (speed mismatch, transfer-size mismatch, copy semantics), single vs
**double buffering** and the overlap it buys, and **spooling** as the treatment
for dedicated devices like printers. (The circular-buffer sizing you drill in
§B2 — against a bursty producer — is `y2020p2q4`'s own analysis, not
Silberschatz's, which stops at single/double buffering.) OSTEP ch. 36
teaches none of it, and Cambridge examines it directly: `y2020p2q4` rests
largely on this material and becomes attemptable from this week.

**Key ideas:** event loop · `select()`/`poll()` · no blocking calls ·
asynchronous I/O · continuations and stack ripping · events × multicore =
locks again · polling vs interrupts vs hybrid · interrupt livelock ·
coalescing · DMA · memory-mapped I/O · device drivers · single/double/
circular buffering · sizing a buffer against a burst · spooling.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 33** | Event-based Concurrency (Advanced) | 12 | 1.7 h |
| 2 | **OSTEP ch. 34** | Summary Dialogue on Concurrency | 2 | 0.2 h |
| 3 | **OSTEP ch. 35** | A Dialogue on Persistence | 4 | 0.3 h |
| 4 | **OSTEP ch. 36** | I/O Devices | 14 | 2.0 h |
| 5 | **Silberschatz §12.4.2 + §12.4.4** (10th ed.) | Buffering; spooling — **required**, the course's only source for I/O buffering theory (skip §12.4.3 caching, unassigned) | ~4 | 0.6 h |

**Paper (required, short):** Ousterhout (1996), *Why Threads Are A Bad Idea
(for most purposes)*, USENIX invited talk. OSTEP calls it "a great talk". It
is slides, not prose — twenty minutes to read, longer to argue with, which
is what sheet §C asks you to do.

> **Why Silberschatz here, uniquely.** This course prefers OSPP wherever both
> books would serve; buffering theory is the one topic nothing else on the
> reading list covers, so Silberschatz survives in exactly this slot. Cite it
> by section — buffering is §12.4.2 and spooling §12.4.4, inside §12.4
> *Kernel I/O Subsystem* (10th-edition numbering).

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise15.md`](../exercises/exercise15.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise15-solutions.md`](../exercises/solutions/exercise15-solutions.md). Mostly original material — see the note below |
| **Lab** | [`../labs/lab05-concurrency/`](../labs/lab05-concurrency/) **ends** · [`../labs/lab06-io-storage/`](../labs/lab06-io-storage/) **starts** (weeks 15–17). Budget 6.0 h across the pair (4.0 h finishing lab 5, 2.0 h opening lab 6). Lab 6 opens with interface-level work — the I/O model — deliberately scoped ahead of ch. 37–38's disk and RAID internals, which arrive next week |
| **Past paper (timed)** | `y2012p2q3` — 35 min timed, then self-mark (~1 h). Context switching, address-space preservation, and a process state-transition diagram traced through four scenarios. Spaced retrieval of weeks 3–9 material |
| **Untimed drill** | This week unlocks `y2014p2q4` (blocking/non-blocking I/O, double-buffering overlap arithmetic, polling vs interrupts, DMA) and **`y2020p2q4`** (buffer sizing against a bursty producer — rests largely on Silberschatz §12.4.2/§12.4.4; its part (f) is context-switch state). Both are now fully attemptable; drill them when the sheet's §B feels shaky. `y2006p1q8` also unlocks, but only partially — its bus-timing and profiling parts are outside any assigned reading. `y2023p2q3` also becomes usable this week but is not to be spent: **held back for revision** — `y2023p2q3`, `y2024p2q4`, `y2025p2q4` are deliberately unspent until week 27 |

## Week load

```
OSTEP ch. 33–36          32pp ÷ 7  =  4.6 h
Silberschatz §12.4.2/.4   4pp ÷ 7  =  0.6 h
Ousterhout [S]                     =  0.75 h
Exercise sheet 15                  =  3.0 h
Timed paper (y2012p2q3)            =  1.0 h
Lab 5 ends · Lab 6 starts          =  6.0 h
                                     ------
                                     16.0 h   — over the 12–14 h band (labs are not
                                              trimmed to fit)
```

No slack. If pressed, the two dialogues (ch. 34–35) can be skimmed in
minutes — but do not skip the Silberschatz sections; nothing else will ever
cover them, and one whole past-paper question stands on them.

## Notes for the curious

- **Ch. 33 has a "Homework (Code)" section but ships nothing** — no
  `threads-events` directory exists in either upstream repo (the text asks
  you to build a `select()`-based server from scratch; lab 5's web server
  is this course's version of that exercise). **Ch. 36 has no homework at
  all.** Sheet 15 is therefore mostly original — its §B drills the
  buffering and overlap arithmetic that Cambridge sets and no upstream
  homework rehearses.
- The Flash web server (Pai et al., cited in ch. 33) is the canonical
  hybrid: events for network traffic, a small thread pool to absorb disk
  I/O the event loop must not wait for. Most production "event-driven"
  servers are secretly this.
- Ch. 36's livelock discussion (Mogul & Ramakrishnan) is the rare case
  where *polling* is the sophisticated choice: under overload, taking
  fewer interrupts is how the system keeps doing useful work.
- The two-phase "poll briefly, then use interrupts" hybrid is the same
  competitive-analysis shape as week 12's spin-then-sleep lock — the sheet
  makes the connection explicit.
- xv6's IDE driver (reproduced in ch. 36) is a complete, readable driver in
  under a page of C — worth ten minutes with the chapter open. The FreeBSD
  book's ch. 8 is the deep reference on how a production kernel organises
  the same job, and returns as next week's reference reading.
