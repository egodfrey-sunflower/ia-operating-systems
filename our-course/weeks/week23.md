# Week 23 — Distributed systems: communication and RPC

> **Part IV: Distribution.** Week 23 of 27.

## What you'll learn

This week the course leaves the single machine. Chapter 46 is the two-page
dialogue that closes Part III; chapter 47 is the two-page dialogue that opens
Part IV; the real content is chapter 48, and its one big idea is stated in the
first section and never retracted: **communication is fundamentally
unreliable**. Packets are corrupted, links die, and — most fundamentally — even
a perfect network drops packets, because a router with full buffers has no
choice. Everything else in the chapter is machinery for living with that fact.

The chapter builds upward from raw messages. UDP gives you an unreliable
datagram with a checksum and nothing more. To get reliability you add
**acknowledgments** and **timeout/retry** — and immediately discover that a
lost *ack* makes the receiver see the same message twice, so you add **sequence
counters** to suppress duplicates and recover **exactly-once** delivery (in the
absence of total failure; at-most-once when machines actually die). That
ack/timeout/sequence trio is TCP in miniature, and it is also precisely the
protocol you will build with your own hands in Lab 10.

The abstraction question then follows: what should programmers *see*?
Extending OS abstractions across machines — **distributed shared memory**,
where remote page faults fetch pages over the network — failed, chiefly
because a machine crash tears a hole in the middle of your address space.
The abstraction that won is the programming-language one: **remote procedure
call**, split into a **stub generator** (marshalling arguments into messages,
so you don't hand-write that code badly) and a **run-time library** (naming,
transport choice, fragmentation, byte-order via XDR, and the timeout/retry
machinery again — note the argument for building RPC on *unreliable* UDP
rather than paying TCP's redundant acks). Birrell & Nelson's 1984 paper is
where all of this comes from; OSTEP calls it "the foundational RPC system upon
which all others build". Read it after the chapter and you will recognise every
piece.

At 21pp this ties week 1 for the lightest OSTEP reading, and is the lightest by
total assigned pages — deliberately, because
Lab 10 starts this week at its heaviest (5.0 h): chapter 48's homework *is*
"build a UDP client/server, then your own reliability layer", and the lab is
exactly that.

**Key ideas:** unreliable networks as the ground truth · checksums ·
ack + timeout/retry · sequence counters and duplicate suppression ·
exactly-once vs at-most-once · why DSM failed · RPC: stubs, marshalling,
run-time · RPC over UDP vs TCP · the end-to-end argument.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 46** | Summary Dialogue on Persistence | 2 | 0.2 h |
| 2 | **OSTEP ch. 47** | A Dialogue on Distribution | 2 | 0.2 h |
| 3 | **OSTEP ch. 48** | Distributed Systems | 17 | 2.6 h |
| 4 | **TLPI ch. 56** | Sockets: Introduction | — | ~1.0 h *(reference — consult while writing Lab 10's socket code, don't read cover to cover)* |

**Paper (required):** ★ Birrell & Nelson (1984), *Implementing Remote
Procedure Calls*, ACM TOCS. The foundational RPC system. Read for: the
argument that RPC should feel like a local call, the stub/run-time split, the
call-identifier machinery that gives at-most-once semantics, and their choice
to build a specialised packet protocol rather than reuse a bulk-transfer one.
Sheet 23 §C2 works directly from that last choice.

**Not this week:** Saltzer, Reed & Clark's *End-to-End Arguments in System
Design* is cited prominently by ch. 48 (and summarised in its Aside, which you
should read carefully — the sheet uses it). The full paper is optional depth;
the Aside carries what you need.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise23.md`](../exercises/exercise23.md) — budget 3 h. Work closed-book first, then self-mark against [`../exercises/solutions/exercise23-solutions.md`](../exercises/solutions/exercise23-solutions.md) |
| **Lab** | [`../labs/lab10-distributed/`](../labs/lab10-distributed/) — **starts this week**, ends week 24. Budget 5.0 h this week: UDP client/server, then the timeout/retry/sequence reliability layer. This is ch. 48's code homework, grown into a lab |
| **Timed past paper** | `y2021p2q3` — 35 min closed book, then self-mark (~1 h total). **Almost entirely calculation, across both halves of the course**: paging arithmetic (frame/offset bit counts, page-table size); TLB sizing; page-table-levels-for-a-target-EAT; then inode maximum file size and path-resolution disk-access cost, where caching absorbs a shared path prefix. Marks (1+2+4+3) + (3+4+3) = 20 |
| **Untimed drill** | The six protection questions unlocked at week 20 are spread across weeks 21–24. Take whichever you have not yet attempted as untimed drills — but leave `y2015p2q3` (week 24's timed paper) and `y2025p2q3` (week 22's timed paper) for their sittings. `y2019p2q4` pairs especially well with this week — its closing part designs a capability API for file protection, and capabilities-as-unforgeable-tokens is about to matter again in week 25's distributed security |

### Why the timed paper is not about distribution

Cambridge IA examines distributed systems only glancingly, so weeks 23–26
carry timed papers that span both halves of the course. `y2021p2q3` is the
clearest example: its first half is paging arithmetic and TLB sizing from week 9,
its second half is inode sizing and path-resolution cost from week 18. Sitting it
five weeks before the exam rehearses the two calculation families that carry the
most marks, in one paper.

## Week load

```
OSTEP ch. 46-48     21pp ÷ 7  =  3.0 h
TLPI ch. 56 (reference)       =  1.0 h
Birrell & Nelson [M]          =  1.5 h
Exercise sheet 23             =  3.0 h
Timed paper (y2021p2q3)       =  1.0 h
Lab 10 (start)                =  5.0 h
                                ------
                                14.5 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

The light reading is what pays for the lab's heaviest week. If you are behind,
trim lab polish, not the sheet — §B is the only place the protocol arithmetic
gets drilled.

## Notes for the curious

- **Ch. 48's homework is code, and it ships.** Unlike the security chapters,
  ch. 48 supplies a genuine homework: the `client.c`/`server.c`/`udp.c`
  skeletons printed in the chapter, plus a graded sequence of exercises
  (library → timeout/retry → fragmentation → sliding-window performance).
  Lab 10 follows that sequence, which is why the sheet's §B stays pen-and-paper.
- OSTEP covers Part IV in three teaching chapters where a networking course
  would take a term. The dialogue is upfront about this: the focus here is
  *failure*, and distributed *file systems* — next week — are the worked
  example. TCP internals, congestion control and routing belong to a
  networking course; Van Jacobson's 1988 congestion paper is the canonical
  entry if you want it.
- The end-to-end argument (the chapter's Aside) is one of the few ideas in
  this course that transfers to *everything* — storage checksums (ch. 45),
  reliable file transfer, even why fsync exists. The corollary is as
  important as the argument: lower-level reliability is never *sufficient*,
  but it is often a worthwhile *performance* optimisation.
- Modern RPC is everywhere and mostly invisible: Sun RPC (which carries NFS —
  next week), gRPC, Thrift. When you meet protobufs in industry you are
  looking at a stub generator; ch. 48 §48.5 is why it exists.
