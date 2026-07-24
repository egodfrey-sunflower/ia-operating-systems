> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 5 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE — and this is the most important warm-up on the sheet.** Coherence
and synchronization are different guarantees. Coherence ensures that all CPUs
see a consistent view of each individual memory location (via bus snooping,
invalidation or update). It says nothing about a *sequence* of accesses:
ch. 10's `List_Pop` example has two CPUs both read the same `head`, both
advance it, and both free the same node — every individual access was coherent,
and the list is still corrupted. Making the sequence atomic requires a lock (or
similar), which is Part II's subject.

**A2. TRUE.** Cache affinity. Running builds up state in the CPU's caches and
TLBs; rerunning on the same CPU finds some of that state still there and avoids
reloading it. The process runs *correctly* anywhere — coherence guarantees
that — but faster where it is warm. This is why placement is a performance
decision on a multiprocessor.

**A3. FALSE.** Half the claim is right: with one queue, no CPU can idle while
another has a backlog, so load balance is automatic. Scaling is the failure:
every CPU must lock the shared queue to dispatch, and as CPU count grows the
system spends increasing time in lock overhead rather than work. The single
queue also destroys affinity — jobs are handed to whichever CPU is free next
and bounce continuously (see B1c). Credit requires separating the two claims.

**A4. FALSE.** Queues drain unevenly. When one queue empties, its CPU idles
while jobs wait elsewhere (B2b) — and no per-queue policy can fix that, because
the queues are scheduled independently. Migration is the *necessary* companion
of multi-queue scheduling, typically via work stealing.

**A5. FALSE.** Every address a user program can observe is virtual — ch. 13's
slogan. The value is a name in the process's private address space; the OS and
hardware translate it to a physical location the program never sees.

**A6. FALSE.** The layout is convention, not hardware requirement — ch. 13 says
so explicitly ("this placement ... is just a convention; you could arrange the
address space in a different way"). The convention is a good one (see B4a), and
it stops working once a process has many threads, each needing its own stack.

**A7. FALSE.** The scheme is rejected on *performance*, not security: saving
and restoring whole memory images through the disk is brutally slow (B3 puts it
at tens of seconds per switch). Security points the other way — it is the fix,
leaving processes *resident*, that creates the protection problem, because now
several programs sit in physical memory together.

**A8. FALSE.** Peeking at other queues costs synchronization and cache traffic;
peek too often and you have reimplemented the single queue's contention, which
multi-queue scheduling existed to avoid. Peek too rarely and imbalances persist.
The chapter is honest that the right interval is a "black art" — the point is
that a tension exists, not that a formula resolves it.

---

## B. Mechanism and measurement

**B1.**
**(a) 30 ticks.** The working set (200) never fits the cache (100), so the
cache never warms; 30 units of progress at 1 unit/tick.
**(b) 20 ticks.** Ten warmup ticks at rate 1 complete 10 units; the remaining
20 units run warm at 2 units/tick, costing 10 ticks. In general, for run time
`R` > warmup `W`:  `T = W + (R − W)/2`.
**(c)** (i) Each job switches CPU every 5 ticks, so it never accumulates the 10
*consecutive* ticks on one CPU that warming requires. Both jobs run cold
throughout: **100 ticks each**. (ii) Pinned, each job warms at tick 10 (10
units done) and finishes the remaining 90 units in 45 warm ticks: **55 ticks
each**. Migration alone — no queueing, no contention, both jobs ran
continuously in both cases — nearly doubles the running time.
> **If you ran the simulator, you saw 255, not 300 — and that is worth understanding.**
> `python3 multi.py -n 1 -L a:100:100,b:100:100,c:100:100 -M 100 -q 5 -w 10 -r 2 -c`
> reports `Finished time 255`, and its per-CPU line reports `warm 17.65` — so the
> cache *did* go warm, despite no job ever holding the CPU for 10 consecutive
> ticks. The reason: `multi.py` warms a cache after 10 **cumulative** ticks of a
> job's residency on that CPU, whereas the model stated on the sheet requires 10
> **consecutive** ticks. Under the sheet's model the answer below (300) is right;
> under the simulator's, 255 is right.
>
> Neither is wrong — they are different models of the same phenomenon, and the gap
> between them is the interesting part. Ask yourself which better describes a real
> cache: does a job's working set survive in L2 while two other jobs run on the
> same core for five ticks each? Real caches lose lines to whoever runs next, so
> the sheet's stricter "consecutive" rule is closer to hardware; the simulator's is
> a simplification that makes warmth easier to reach.

**(d)** (i) With a 5-tick slice no job ever runs 10 consecutive ticks (and each
job's arrival evicts the previous occupant of the one cache anyway): everything
is cold. 300 units of total work through one CPU at 1 unit/tick: **makespan
300**. (ii) One job per CPU: 55 ticks each (as in c-ii), **makespan 55**.
Speedup = 300/55 ≈ **5.5× on 3× the CPUs**. (iii) No law is violated. The folk
bound "speedup ≤ N" assumes each processor runs the work at the same rate in
both configurations. Here the aggregate cache tripled, and — decisively — each
job's working set now *fits undisturbed* in a private cache, so per-CPU
execution rate doubled. The extra speedup is bought by cache capacity, not
conjured. (iv) With 50-unit caches nothing ever fits, so nothing ever warms:
(i) is still 300; (ii) becomes 100 ticks per job, makespan **100**; speedup
exactly **3.0** — linear. The super-linearity in (ii) came entirely from cache
fit, which is the chapter's point about affinity being a real resource.

**B2.**
**(a)** CPU0 serves only A: **A gets 6** of its 6 slices. CPU1 alternates B and
D: **3 each**. A receives exactly **twice** B's share, despite identical demand.
**(b)** Q0 is empty, so CPU0 idles — **utilization 50%** — while B and D still
share one CPU. The pathology: queues are scheduled independently, so no
mechanism connects CPU0's idleness to CPU1's backlog.
**(c)** Move B (say) to Q0: now each queue holds one job, each job owns a full
CPU, and the assignment is balanced permanently — one migration suffices. In
(a), any static assignment of three jobs to two queues puts two jobs in one
queue; a single migration only changes *which* two share ({A,B} vs {D} gives
shares ½, ½, 1 again). No single move can equalize three jobs over two queues.
**(d)** Fairness target: 2 CPUs ÷ 3 jobs = **2/3 CPU each**. Statically, a
job's share is 1/(jobs in its queue), and with queue sizes {1, 2} the shares
are {1, ½, ½} — never 2/3. Now rotate: every period `T`, migrate so that a
different job is the solo one. Over 3T, each job spends T solo (share 1) and 2T
paired (share ½): average = (1·T + ½·2T) / 3T = **2/3** ✓ — continuous
migration achieves what no static placement can.
**(e)** Every rotation moves jobs onto CPUs where their caches are cold, so
each period reopens B1's warmup cost — fairness is being bought with affinity.

**B3.**
**(a)** 4096 MB ÷ 200 MB/s ≈ 20.5 s to save, the same to load: **≈ 41 s per
switch**.
**(b)** Not remotely. A 100 ms response with 10 users needs the machine to
switch attention on the order of every few tens of milliseconds; the switch
alone costs ~41 s — **roughly 400× too slow**, before any useful work is done.
(Accept any answer in the hundreds; the point is orders of magnitude, not the
third digit.)
**(c)** Residency means several programs occupy physical memory at once, so
the OS must ensure no process can read or write another's memory (or the
OS's own). That obligation is **protection** — the third of ch. 13's goals
(with transparency and efficiency), delivering isolation between processes.

**B4.**
**(a)** Each region grows until it meets the other (or exhausts the space
between): the failure is running out of room *inside the address space*. With
the two growing regions at opposite ends, the free region between them is
shared: neither has a fixed private cap, and failure is postponed until their
*combined* growth fills the gap — the most flexible arrangement for two regions
whose sizes are unknown in advance.
**(b)** The address must be **virtual**: a per-process name that the OS and
hardware map to different physical memory in each process — exactly the
conclusion week 1's `mem.c` experiment forced, now with the abstraction named.
**(c)** Typically dozens of mappings. Missing from the three-region picture,
any two of: the **shared libraries** (libc and friends — each contributing its
own code and data mappings), the program binary split into **separate text /
read-only / writable data mappings**, **anonymous regions** created by the
allocator via `mmap`, and kernel-provided pages such as the **vdso**. The
lesson: ch. 13's diagram is a deliberate simplification; a real address space
is a collection of many segments, most of which are library code and data the
program never explicitly asked for. (The chapter admits as much — "there are
other things in there too".)

---

## C. Discussion and design critique

*All three are diagnosis questions. Full credit requires naming the mechanism —
not just describing the symptom back — plus a concrete fix and the conditions
under which the fix fails. A correct fix with no failure conditions earns
little more than half.*

**C1.** **Mechanism: multi-queue load imbalance.** Per-core queues are
scheduled independently and jobs are never moved, so when the workload drains
unevenly, a queue that happens to hold the long tail keeps its core busy while
empty queues leave their cores idle — precisely ch. 10's idle-CPU pathology.
The variable latency has the same root: a late job's fate depends on which
queue it landed in, not on system-wide load.

**Fix: migration, via work stealing.** A queue that is empty (or notably
shorter) periodically peeks at another queue and, if the target is notably
fuller, steals one or more jobs. At the end of a run this moves the tail's jobs
onto idle cores within one peek interval.

**When the fix hurts, and the controlling parameter.** The peek interval.
Too frequent, and every core is regularly taking locks on other cores' queues —
reintroducing the synchronization overhead and cache traffic that per-core
queues existed to avoid, and it worsens as core count grows. Too rare, and
imbalance windows stay long (the twenty-minute symptom shrinks but doesn't
vanish). Additionally, every stolen job restarts cold (B1), so stealing
near-finished or cache-heavy jobs can cost more than briefly idling. The
threshold has no closed form — the chapter calls it a black art — so the answer
is monitoring and tuning, not a constant.

**C2.** **The two mechanisms:** (i) **contention on the single run-queue
lock** — every dispatch on every CPU serializes on it, so as CPU count rises, a
growing fraction of time is lock overhead rather than request work; this caps
throughput scaling well below 4×. (ii) **Affinity loss** — dispatching each
worker to "whichever CPU is free" bounces it between CPUs, so it repeatedly
restarts with cold caches and TLBs.

**Which one the per-request CPU rise identifies:** affinity loss. Lock
contention wastes time *between* requests; it would depress throughput but not
inflate the cycles charged to a request's own execution. Executing the same
instructions with cold caches means more misses and therefore more CPU time
per request — the tell that the workers are migrating. (The measured 1.6× is
both mechanisms compounding.)

**Fix:** per-CPU queues with jobs staying put — MQMS — plus periodic
stealing for balance; or, minimally, an affinity mechanism layered on the
existing queue so each worker prefers its previous CPU.

**When that is the wrong trade:** *Workload property* — if request costs are
highly skewed or bursty, per-CPU queues drift into imbalance and the stealing
machinery (with C1's tuning burden) is doing all the work; or if the workers'
working sets are far larger than a CPU cache, affinity buys almost nothing, so
you pay MQMS's complexity for no gain. *Machine property* — at low CPU counts
(2–4) the single lock may be nowhere near saturation, and a single queue's
automatic balance is worth more than the small contention cost; the 1.6×
should be *measured* as contention vs. affinity before rebuilding the
scheduler. Credit for proposing that measurement.

**C3.** **The misreading: printed addresses are virtual.** The pointer is a
name in the process's private virtual address space — a large, sparse illusion
the OS provides — not a physical location. `0x7f8a12345678` says the allocation
sits high in the *virtual* layout (in the region where, on a typical 64-bit
Linux layout, the stack and mmap'd regions live); the OS maps the handful of
regions actually in use onto whatever physical frames exist. A 64 MB board
comfortably hosts processes whose virtual addresses run into the terabytes.
What the value tells you: where the object lives in the address-space layout.
What it cannot tell you: anything about the amount, or addresses, of physical
RAM.

**The condition under which the colleague would be right:** a system with no
memory virtualization — no MMU, program running directly on physical memory,
as on many small embedded systems (and as in ch. 13's early machines). There,
addresses in the program *are* physical addresses, and a pointer beyond the end
of RAM really would indicate a fault. The inference's validity turns entirely
on whether an OS/MMU sits underneath — which is, in one line, the whole content
of "every address you see is virtual."

*Marking note: the strongest C3 answers connect the two halves — the same
printed number is evidence of a broken system on one class of machine and
evidence of a working illusion on the other, and knowing which machine you are
on is the diagnosis.*
