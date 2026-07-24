# Exercise Sheet 23 — Distributed systems: communication and RPC

**Attempt after Week 23.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise23-solutions.md`](solutions/exercise23-solutions.md).

**This sheet leans on:** OSTEP ch. 48 (including its end-to-end Aside);
Birrell & Nelson (1984), *Implementing Remote Procedure Calls*. §B4(c) also
recalls ch. 45's checksums (week 22).

No tooling is needed — the sheet is pen-and-paper throughout. Ch. 48's code
homework (build a UDP client/server and a reliability layer) is Lab 10, not
this sheet.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** Building an RPC system on top of TCP removes the need for the RPC layer
to handle failure.

**A2.** UDP provides no protection against any kind of communication failure.

**A3.** With acknowledgments, timeout/retry and sequence counters, a client can
guarantee that each request is executed exactly once by the server.

**A4.** Receiving an acknowledgment tells the sender that the receiving
*application* has processed its message.

**A5.** Distributed shared memory fell out of use mainly because remote page
fetches are slow.

**A6.** A well-implemented RPC system can make a remote call indistinguishable
from a local one.

**A7.** The end-to-end argument says that lower layers of a system should not
implement reliability machinery.

**A8.** Setting the retry timeout below the network round-trip time improves
responsiveness, because losses are detected sooner.

---

## B. Protocol arithmetic

**B1. Sequence counters, traced.**
A sender S and receiver R use the ch. 48 scheme: both counters start at 1;
each message carries the sender's current counter value; the receiver acks
every message it receives, but delivers a message to the application only if
its ID equals the receiver's counter (then increments it). S sends three
messages, and the network behaves as follows: message 1 arrives, its ack
arrives; message 2 arrives, **its ack is lost**; message 2 is retransmitted;
message 3 is **lost**; message 3 is retransmitted, arrives, its ack arrives.

  (a) Trace the exchange in a two-column timeline: every transmission, each
      side's counter value over time, which acks are sent, and which arrivals
      are delivered to the application versus suppressed.
  (b) The chapter mentions an alternative: give every message a unique ID and
      have the receiver remember every ID it has ever seen. State precisely
      why the sequence counter is preferred, and what assumption about the
      channel the counter scheme depends on that the remember-everything
      scheme does not.
  (c) R now reboots, losing its counter, while S keeps running. Describe a
      concrete resulting misbehaviour, and what this tells you about where
      the scheme's state really lives.

**B2. Timeouts, costed.**
A client and server exchange request/reply pairs. One-way network latency is
**5 ms** each way; server processing time is negligible. Each packet
(request or reply) is independently lost with probability **p = 0.1**.

  (a) A round trip succeeds only if both packets survive. What is the
      probability q of that, and the expected number of attempts 1/q until a
      reply is received?
  (b) The client's timeout is T = 40 ms. Write the expected total latency of
      one successful exchange as a function of q, T and the round-trip time,
      and evaluate it. (Attempts that fail cost a full T before the retry.)
  (c) An engineer sets T = 8 ms to "detect losses faster". What happens on
      *every* exchange, including lossless ones? Quantify the wasted
      transmissions and explain why the receiver nonetheless behaves
      correctly.
  (d) Ch. 48's tip recommends **exponential back-off** when many clients share
      one server. Starting from T₀ = 40 ms and doubling on each failure, how
      long does a client wait in timeouts alone across 5 consecutive failed
      attempts? What server-side condition is this behaviour designed to
      relieve, and why would fixed-interval retry make that condition worse?

**B3. RPC on TCP versus RPC on UDP, counted.**
Consider one RPC: a request message and a reply message, both small.

  (a) Count the packets on the wire when the RPC runs over a reliable
      transport that acknowledges every message (request, its ack, reply, its
      ack), and when it runs over UDP with reliability in the RPC layer,
      where the reply itself acknowledges the request and the client's *next*
      request — or, if none comes, an explicit ack solicited by retransmission —
      covers the reply. State the saving per call.
  (b) A server handles 20,000 RPCs per second. Using your counts from (a),
      how many packets per second does each design cost? If every packet
      costs the server ~10 µs of processing, what fraction of one CPU does
      the difference represent?
  (c) Birrell & Nelson chose the UDP-style design. Give two circumstances —
      one about argument size, one about connection lifetime or count — in
      which building on TCP is the better engineering choice after all.

**B4. The end-to-end file transfer.**
You must copy a file from machine A to machine B so that the bytes on B's disk
provably equal the bytes that were on A's disk.

  (a) Your network transport guarantees in-order, loss-free, corruption-checked
      delivery of every byte handed to it. Give two distinct failure scenarios,
      taken from the chapter's Aside, in which the transfer still silently
      corrupts the file.
  (b) Design the end-to-end check that actually establishes the guarantee.
      Say what is computed, where, and when.
  (c) Given (a), the transport's reliability machinery provides no
      *guarantee* you rely on. Make the argument — the Aside's corollary —
      for keeping it anyway, and be concrete about what it buys during a
      transfer of a 10 GB file over a link that corrupts one packet in 10⁵.

---

## C. Discussion and design critique

**C1.** The systems community extended two abstractions across the network:
the OS abstraction (shared memory, becoming DSM) and the language abstraction
(the procedure call, becoming RPC). One is dead and one is everywhere. Using
ch. 48's account of DSM's failure-handling problem, state the general lesson:
what property must an abstraction have to survive distribution, and why does a
procedure call have it while a load instruction does not?

**C2.** Birrell & Nelson built their own specialised packet protocol for RPC
rather than layering it on an existing reliable bulk-transfer protocol,
optimising for the case of small arguments, small results, and many short
calls. Reconstruct their argument, then state the conditions under which the
opposite choice wins — and name a modern workload that sits on each side of
the line.

**C3.** Two teams propose transports for the same internal service: ~10,000
client machines per server, calls usually under 1 KB, and the service must
give **at-most-once** semantics that survive a server crash and reboot.

- *Design T:* every client keeps a long-lived TCP connection; the RPC layer
  trusts TCP for delivery and ordering, and adds nothing.
- *Design U:* UDP datagrams; the RPC layer does timeout/retry, sequence
  counters per client, and a duplicate cache.

Compare the two designs **under the stated constraints**. Address
specifically: what per-client state each design forces the server to hold and
what 10,000 of them cost; what each design actually guarantees about a call
that was in flight when the server crashed — including whether Design T's
trust in TCP even addresses the question; and which design degrades more
gracefully when the server is overloaded. Say which you would build,
and identify the single measurement, and the result of it, that would flip
your choice.
