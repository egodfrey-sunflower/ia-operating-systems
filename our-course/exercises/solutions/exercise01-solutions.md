> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 1 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> These are model answers and marking notes, not the only correct responses.
> Where a question is open-ended the notes flag the points a supervisor wants to
> see made, rather than prescribing wording.

---

## A. Warm-ups

**A1. FALSE.** An OS adds overhead to almost every individual operation — a
system call is slower than a function call, and address translation costs cycles.
Its purpose is to make the hardware *usable* and *shareable*: abstraction,
protection, and multiplexing. It can improve system-wide *throughput* (by
overlapping one process's I/O with another's computation), and that is worth
credit, but "makes programs run faster" as stated is wrong.

**A2. FALSE.** This is time-sharing, not partitioning. The OS switches a single
physical CPU rapidly among many processes so that each *appears* to have its own.
The whole point is that the number of processes vastly exceeds the number of
cores. A system with one core still virtualizes the CPU for a hundred processes.

**A3. TRUE as stated, but the interesting answer is "not in control, yet still
in charge".** While a user program executes, the OS is genuinely not executing
instructions — it is dormant. But it is not powerless: before handing over, it
armed a timer interrupt that will forcibly return control, and it installed page
tables that constrain every memory access. *Being in control* and *executing* are
different things. This is limited direct execution — week 3.

**A4. TRUE, but sharpen it.** Durability across power loss is the defining
difference. Memory virtualization gives each process a large, private, isolated
address space, but its contents evaporate when power goes. Persistence must
survive not only power loss but crashes *mid-update*, which is why week 21's
crash-consistency material is hard. Credit for noting that persistence is
therefore about *ordering and atomicity*, not just storage.

**A5. FALSE — and this is the most important warm-up on the sheet.** The MMU
performs translation in hardware on every access; the OS only *sets up* the page
tables and handles the exceptional cases (faults). If the OS were consulted on
every load and store, the machine would be unusably slow. This is the
mechanism/policy split that recurs all course: the OS establishes the rules, the
hardware enforces them at speed.

**A6. TRUE.** Both name the creation of an abstraction with no physical
counterpart. OSPP's "illusionist" is the broader term — it covers file systems
and device abstraction too — while OSTEP's "virtualization" is used mainly of the
CPU and memory. Accept either framing; reject an answer claiming they are
unrelated.

**A7. FALSE.** The OS needs privileged instructions (installing page tables,
masking interrupts, setting the timer) that are unavailable in user mode, and it
must already be resident to start anything else. The *language* is genuinely
flexible — C, and increasingly Rust (week 26) — but the implementation must avoid
anything assuming an OS underneath: no garbage collector, no libc allocator, no
threads library. Credit for identifying the runtime constraint rather than a
language constraint.

---

## B. Evidence from the demo programs

**B1.**
**(a)** On a machine with at least four cores, all four run genuinely in
parallel; output interleaves in no fixed order. Most students predict
round-robin alternation and are surprised by long runs from one process.
**(b)** Not identical between runs. The ordering is chosen by the **scheduler**,
influenced by interrupt timing, other system load, and core migration — none of
which the program controls. Non-determinism is the observation that matters.
**(c)** **No — with 4 processes on 8 cores nothing is being virtualized;** there
is a real core each. This is the trap in the question. To actually demonstrate
CPU virtualization you must create contention: run *more* processes than cores
(a few hundred), or confine everything to one core with `taskset -c 0`. Full
credit requires recognising that the original experiment doesn't prove what it
appears to.

**B2.**
**(a)** On any modern Linux the two addresses are **different** — something like
`0x6533a85172a0` and `0x5bf0e55d52a0`. This surprises most readers, because
OSTEP's text shows them identical.
**(b)** Under `setarch -R` both print the **same** address (e.g.
`0x5555555592a0`, identically in both processes). The difference is **ASLR**:
by default Linux randomises where the heap, stack and libraries land in each
process's address space. OSTEP's footnote on this chapter says as much — its
output assumes randomisation is off.
**(c)** The address must be **virtual** — a per-process name that the MMU maps
to different physical memory in each process. Two processes can hold the same
virtual address precisely because it means nothing outside the address space
that contains it.
**(d)** ASLR defends against **memory-corruption exploits**: if an attacker
overflows a buffer, they need to know where to point the corrupted pointer, and
randomising the layout removes that knowledge. It does not undermine the
experiment — quite the opposite. The OS can only randomise each process's layout
independently *because* each process has its own address space to arrange. ASLR
is a consequence of the very virtualization the experiment demonstrates.
**(e)** Without virtualization both would refer to the same physical location:
each would see the other's increments, the counter would advance at roughly
double speed and unpredictably, and in general each program would corrupt the
other's data. This is what early batch systems lived with.

*Marking note: (a) is deliberately a trap — the observation contradicts the
textbook, and the resolution is ASLR, not a mistake by the reader. A student who
runs it once, sees different addresses and concludes the chapter is wrong has
learned less than one who reaches for `setarch`.*

**B3.**
**(a)** At `N = 1000` the result is frequently the correct `2000` — the race
window is small and the threads may not overlap at all. At `N = 100000` the
result is almost always **less than** `2N`. Larger `N` means more increments and
therefore more opportunities for two threads to be inside the unprotected
read-modify-write at once. Credit for noting that a test passing at small `N`
proves nothing.
**(b)** `counter++` compiles to load, add, store. To lose exactly one update:
both threads load the same value `v`; both compute `v+1`; both store `v+1`. One
increment vanishes. Every other iteration runs without overlap. Result: `2N - 1`.
**(c)** Two distinct things: **mutual exclusion** (a lock, so only one thread is
in the critical section) and, underneath it, **hardware support for atomicity**
(test-and-set, compare-and-swap, or load-linked/store-conditional) — because you
cannot build a correct lock from ordinary loads and stores at acceptable cost.
Accept "a way to wait without spinning" as the second item; that is week 13.

**B4.**
**(a)** `write()` typically only copies data into the OS page cache and returns;
the data is not yet on stable storage. `fsync()` blocks until the file system has
pushed it to the device (and, if implemented honestly, until the device reports
it durable).
**(b)** Performance. Making every `write()` durable turns each into a device
round-trip — orders of magnitude slower, and it destroys the OS's ability to
batch, reorder, and coalesce writes. The interface instead exposes the choice, so
applications pay for durability only where they need it. Credit for observing
this pushes a hard correctness burden onto application authors — the subject of
Pillai et al., assigned in week 21.

---

## C. Discussion and design critique

**C1.** Marking note: the classification matters less than the justification.
Several are genuinely dual-role and the best answers say so.
- **(a) Referee.** Arbitrating one contended resource among competitors.
- **(b) Illusionist** primarily — a contiguous growing heap over discontiguous
  physical pages. Also **referee**, since physical memory is contended.
- **(c) Illusionist.** A named hierarchy over a flat block array.
- **(d) Glue** primarily — a common communication service. Also **referee**:
  the pipe buffer must be synchronised and flow-controlled.
- **(e) Referee.** Enforcing limits, the least glamorous refereeing there is.
- **(f) Glue** — one interface across unlike devices. Arguably **illusionist**
  too, since the printer is made to look like a file.

**C2.** Any well-evidenced pair. Strong examples:
- **Protection vs performance:** KPTI, the Meltdown mitigation, unmaps kernel
  pages during user execution. Correct, and it makes every system call pay extra
  page-table switches — single-digit to ~30% regressions on syscall-heavy
  workloads. (OSTEP ch. 23, week 10.)
- **Abstraction vs performance:** the file abstraction hides physical layout, so
  an application cannot exploit knowledge of sequential placement. Databases
  routinely bypass it with `O_DIRECT` and their own buffer pools — paying in
  complexity to reclaim performance the abstraction cost them.
- **Reliability vs performance:** journaling writes metadata twice. Ordered mode
  exists precisely as a compromise. (Week 21.)
Credit requires a *measurable* cost, not merely "there is a trade-off".

**C3.** A good answer separates three categories.

**Genuinely disappear** under the stated assumptions: multi-user protection,
logins, permissions; scheduling *between competing users*; and — if memory is
statically allocated and there is one address space — much of virtual memory's
isolation purpose.

**Quietly reinvented inside the application:** device drivers (something must
still talk to the hardware); interrupt handling and its concurrency hazards;
dynamic memory management if anything is allocated at runtime; a boot and
initialisation sequence; I/O buffering; and some notion of scheduling the moment
there is more than one concurrent activity — even an interrupt handler plus a
main loop is concurrency. The engineer is not deleting the OS, they are
**writing one, without the benefit of it having been debugged by others**.

**What you'd need to know first:** Is there genuinely only one concurrent
activity? How many interrupt sources? Is memory allocation static? Are there
real-time deadlines? Will the device be field-updated? Is there a network
interface — which drags in buffering, timers and untrusted input at once? Any
safety or certification requirement?

**Recommendation.** For a genuinely single-activity, static-memory, few-interrupt
device, running on bare metal is defensible and routine in embedded practice; the
principled version of this argument is the **unikernel** (week 26), which keeps a
library OS but discards the protection boundary. Otherwise the proposal trades a
well-tested kernel for a bespoke, worse one. Recommend a small RTOS or minimal
kernel unless the constraints really are that tight — and note that "we get every
cycle" is the claim most likely to prove false, since a hand-rolled driver rarely
beats a mature one.

*Marking note: this question rehearses the Cambridge house style — a proposal
that is not simply wrong, requiring a judgement with conditions attached. Answers
that only say "bad idea" earn little; answers that specify what would change the
verdict earn most.*
