> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 15 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** The first half is true and is the model's chief selling
point: one handler runs at a time, so no interleaving and no locks. But it
does **not** survive multicore — to use more than one CPU the server must
run handlers in parallel, and ch. 33 concedes that the usual problems
(critical sections) and the usual solutions (locks) then return. The
lock-freedom was a property of having one CPU, not of events.

**A2. FALSE.** There are no other threads to run: the event loop *is* the
server. A blocking `open()` or `read()` in one handler halts service to
every connection until it returns. Hence the model's iron rule — no blocking
calls, ever — and the need for asynchronous I/O.

**A3. TRUE.** A page fault in a handler blocks the process — implicit
blocking the programmer never wrote. Ch. 33 flags exactly this: events
integrate poorly with paging, and a faulting handler stalls the whole
server however disciplined the explicit I/O is.

**A4. FALSE.** Interrupts pay a fixed overhead (switch away, run the
handler, switch back). For a fast device that overhead exceeds the wait —
the first poll finds the device done — so polling wins; and under a flood of
arrivals interrupts can **livelock** the machine (B3d). Slow devices favour
interrupts; unknown or bimodal devices favour the two-phase hybrid.

**A5. FALSE.** The point of DMA is to free the CPU during the transfer; the
DMA controller **raises an interrupt** on completion. Having the CPU
periodically poll the status would reintroduce the busy work DMA exists to
eliminate (though polling remains an *option*, it is not the mechanism the
design intends).

**A6. FALSE.** Memory-mapped I/O changes how the *OS* addresses device
registers — loads and stores instead of special `in`/`out` instructions —
not who may do it. The register addresses are mapped only in the kernel's
portion of the address space, and device access remains privileged; a user
program that could drive the disk directly would bypass every protection
check in the system.

**A7. TRUE.** Double buffering's gain is **overlap**, not smoothing: while
the consumer drains one buffer the producer fills the other, so equal rates
`r = p` cut elapsed time nearly in half — `N(r+p)` becomes about
`N·max(r,p) = N·r` (B1d). Smoothing bursts is a separate benefit that needs
unequal instantaneous rates.

**A8. FALSE.** Buffering absorbs *variance*, never sustained rate excess.
If the producer's long-run average exceeds the consumer's rate, backlog
grows without bound and any finite buffer eventually overflows; the buffer
only chooses *when* data is lost. Survivability requires long-run average
strictly below the consumption rate, with the buffer sized to the worst
burst (B2e).

---

## B. The arithmetic of I/O

**B1.**
**(a)** Strictly serial: 20 × (3 + 2) = **100 ms**.

**(b)** Pipeline: read block 1 (3 ms); thereafter each 3 ms read of block
*i*+1 fully hides the 2 ms processing of block *i*; finally process block 20
(2 ms). Total = 3 + 19×3 + 2 = **62 ms**; steady state **3 ms per block** —
`max(r, p)`, the read being the bottleneck. Assumes: a **DMA** engine (the
transfer proceeds while the CPU computes — with programmed I/O the CPU would
be doing the copying and nothing overlaps), an **interrupt** on completion
(so the CPU need not poll), and **two buffers**' worth of memory.

**(c)** Now `max(r, p) = 4`: total = 3 + 19×4 + 4 = **83 ms**. The CPU is
the bottleneck; the disk is idle 1 ms in 4. Double buffering has bought the
hiding of all reads but the first — 140 ms serial becomes 83 ms — but it
cannot make the elapsed time drop below the dominant stage. Pipelines run at
the speed of their slowest stage.

**(d)** Single: `N(r + p)`. Double: `r + (N−1)·max(r,p) + p` ≈
`N·max(r,p) + min(r,p)`. Overlap converts "sum of stages" into "max of
stages".

**B2.**
**(a)** Fill rate = 5 − 1 = **4 MB/s**; over 2 s the backlog peaks at
**8 MB**.

**(b)** One 512 KB buffer fills in 512 KB ÷ 4 MB/s = **0.125 s** — data is
lost from 125 ms into a 2 s burst. Two buffers (1 MB) last **0.25 s**. Both
lose the great majority of the burst: the shortfall is 8 MB of capacity,
and doubling 0.5 MB to 1 MB is not even close.

**(c)** 8 MB ÷ 512 KB = **16 buffers**. (Accept 17 with the argument that
the unit being drained is only partially reusable at burst onset — the
fencepost is not the point; the 8 MB sizing logic is.)

**(d)** After the burst: producer 0.5 MB/s, consumer 1 MB/s → net drain
0.5 MB/s → 8 MB ÷ 0.5 MB/s = **16 s**. So bursts must be at least 16 s
apart, else the next burst starts with a non-empty ring and overflows
before its end. A buffer sized for one burst implicitly assumes a recovery
interval; full credit requires stating that assumption.

**(e)** Nothing, in the long run. With average arrival = consumption
exactly, the backlog performs a random walk with no restoring drift: each
burst adds 8 MB and the quiet periods no longer remove it. Any finite
buffer eventually overflows; the principle — **buffering absorbs bursts
around an average the consumer can sustain; it cannot repair a sustained
rate mismatch** — is the sentence the marker is looking for.

**B3.**
**(a)** Polling costs the CPU the whole wait: `D`. Interrupts cost the fixed
overhead: `C`. **Poll when `D < C`; interrupt when `D > C`.**
(i) 5 µs < 20 µs → poll — an interrupt would take the operation from 5 µs to
25 µs, **five times** the cost (or 4× the added overhead). (ii) 10 ms ≫ 20 µs → interrupt; polling would burn five hundred
interrupts' worth of CPU per I/O.

**(b)** **Poll for up to `C`; if the device is still busy, arm the interrupt
and sleep.** If `D ≤ C` you behave optimally (cost `D`). If `D > C` you pay
`C` of polling plus `C` of interrupt = `2C`, where the oracle pays `C`.
Never worse than twice optimal: **2-competitive** — the identical argument,
with identical constants, to week 12's spin-for-`C`-then-sleep two-phase
lock. Same problem (wait cost unknown), same answer.

**(c)** Uncoalesced: 50,000 × 4 µs = 200 ms of CPU per second = **20%**.

Coalesced, the trap is to assume batches of 8. **They never reach 8.** Packets
arrive every 1/50,000 s = **20 µs**, so eight completions would take 160 µs —
but the 100 µs timer fires first. The **timer**, not the batch count, is the
binding constraint, and each interrupt therefore carries about 100 µs ÷ 20 µs =
**5 packets**:

    10,000 interrupts/s × 4 µs = 40 ms/s = **4%**

(Accept ~3.3% for 6 packets per batch if you count the packet arriving exactly
at the 100 µs boundary; the boundary convention is not the point. **2.5% is
wrong** — it assumes a batch size the arrival rate cannot supply.)

The price is **latency**: a packet may wait up to 100 µs before its interrupt is
delivered, plus burstier per-interrupt work. The general trade is overhead
against responsiveness, tuned by the coalescing window.

*Marking note: the useful lesson is that a coalescing device has two limits and
you must work out which one binds. Students who reach for the batch count
without checking the arrival rate against the timer get 2.5% — plausible,
tidy, and wrong. Ask what would have to change for 8-packet batches to become
reachable: the answer is a packet rate above 80,000/s.*

**(d)** **(Receive) livelock**: the CPU services interrupts continuously and
no process — including the one that would actually consume the packets —
ever runs. Remedy, counter-intuitively: **turn interrupts off and poll**
under load, so the OS controls when device servicing happens and can give
the application CPU time; interrupts return when load drops (Mogul &
Ramakrishnan, cited by ch. 36).

**B4.**
**(a)** 10,000 × 16 KB = **160 MB** of stack for mostly idle connections.
**(b)** 10,000 × 256 B = **2.56 MB** — about 60× less.
**(c)** Three potential blocking points rip the handler into **four**
pieces. At each seam the code must package by hand what the thread version
kept on its stack for free: which connection this is, the file/socket
descriptors in play, buffer pointers and counts, and *where in the protocol
we are* — then index it (ch. 33's example: a hash keyed by fd) so the
completion event can find it. That is manual stack management, and it is
Adya et al.'s "stack ripping".
**(d)** (a) squanders **memory** — stacks pinned for idle connections is
the thread model's per-connection tax. (c) reveals the event model's tax is
paid in **programmer effort and code structure**: control state that
threads carry implicitly must be maintained, and maintained *correctly*,
by hand.

**B5.**
**(a)** A driver buffer smooths rate mismatches, but two processes writing
concurrently still have their output **interleaved** in the stream — page 3
of one report between pages 2 and 4 of another — and a minutes-long job
still makes every other client wait on the device's timescale. Buffering
solves neither exclusive use nor decoupled completion.

**(b)** Each job's output is staged **in full** to secondary storage — one
spool file per job. A daemon owns the printer exclusively, dequeues
*complete* jobs, and prints them one at a time. Submitters return as soon
as their file is written; the queue is inspectable, reorderable, and
survives a jam or reboot because it lives on disk. (§12.4.4's model.)

**(c)** Spool wins because: (i) **submitters never wait at device speed**
— a lock-holder prints for minutes while everyone queues; the spool
decouples submission from the device entirely; (ii) **jobs become
persistent, schedulable objects** — an operator can reorder, cancel, or
reprint after a crash, none of which a lock provides; (iii) relatedly, a
client that dies mid-job cannot wedge the device — with a lock it can.
The lock is preferable when the interaction is genuinely **two-way and
real-time** — a device session with mid-operation feedback (think of an
operator-attended device) — where output cannot be staged in advance
because later output depends on responses to earlier output.

---

## C. Discussion and design critique

**C1.** Model shape (a table is fine):

- **Memory.** Threads: ~160 MB of stacks at 10k connections (B4) on a
  256 MB box — well over half the machine gone before any data is cached,
  directly shrinking the cache whose hits the workload depends on. Events:
  ~2.5 MB. On this constraint alone events lead.
- **Fast path (cache hit).** Events: the handler runs to completion in
  microseconds, no context switch, no lock — ideal. Threads: each request
  costs a wakeup and switch, and shared cache structures need locks even
  on one core (preemption), taxing exactly the path that carries ~98% of
  requests.
- **Miss path (disk).** Threads: trivially natural — the thread blocks,
  others run. Events: needs asynchronous I/O, plus a continuation per
  outstanding read; if the platform's AIO is weak, this path is where the
  design strains (ch. 33's admission).
- **Failure modes.** Threads: races, deadlock — the weeks 11–14 tax, paid
  in correctness. Events: stack ripping, and *accidental* blocking — one
  synchronous library call or page fault stalls all 10,000 connections.

**Verdict:** on a single core with tight memory and a cache-hit-dominated
workload, the event loop — the constraint neutralises events' multicore
weakness and the workload plays to its fast path.
**Flip 1 — the box grows cores:** the event loop must become several, with
locking between them; threads now buy true parallelism, and the choice
reopens. **Flip 2 — the workload goes disk-heavy (or per-request CPU
grows):** the miss path dominates, AIO plumbing and continuation churn
swamp the fast-path elegance, and thread-per-connection's natural blocking
wins. **Flash** runs events on the fast path and hands disk I/O to a small
thread pool — it is engineered precisely to dodge Flip 2 while keeping the
event loop's cheap fast path; it also contains the accidental-blocking
hazard for file operations by design.

*Marking note: the verdict earns little without the two flips; the flips
are the question.*

**C2.**
**(a)** Steelman: the empirical record says ordinary programmers cannot
reliably write threaded code — Lu et al. found 105 concurrency bugs *in
mature, expert-maintained systems* (MySQL, Apache, Mozilla, OpenOffice),
70% of them atomicity/order violations: exactly the subtle,
non-deterministic, hard-to-reproduce failures Ousterhout warned about.
Weeks 11–14 of this course are, in effect, a catalogue of ways experts got
locks and CVs wrong (missed wakeups, mis-directed signals, two-line
deadlocks). Events eliminate the whole class at a stroke on one CPU: one
handler at a time, no interleaving, no locks, scheduling under the
application's control. For reactive programs — GUIs, many servers — whose
work items are short and I/O-bound, that is the right default, and threads
should be reserved for the rare case that needs genuine multiprocessor
parallelism.

**(b)** Against, using ch. 33's own admissions: **(i) multicore became
universal** — the "one CPU" premise died, and with it the no-locks
guarantee: parallel event handlers need the same synchronization threads
do, so the tax returns without the natural programming model. **(ii)
asynchronous I/O never integrated cleanly** — disk AIO arrived late,
never unified with network readiness APIs, and implicit blocking (page
faults) breaks the model regardless; meanwhile stack ripping remains a
permanent, compounding cost — ch. 33 notes that when a callee changes from
non-blocking to blocking, its callers must be re-ripped. So the events
camp pays its complexity tax forever, and no longer collects the
simplicity dividend that justified it.

**(c)** A defensible modern rule: *"Use an event loop for I/O-bound
coordination within one scheduling domain; use threads for parallelism and
for anything that must block — and expect production systems to be
hybrids of the two."* Conditions attached: the event side assumes real
async I/O support and a workload of short handlers; the thread side
assumes the discipline of weeks 11–14 (lock ordering, `while`-loop waits)
is actually applied. Ch. 33's closing line — both models persist because
neither dominates — is the honest summary, and answers that pick one
absolute winner have missed the point.
