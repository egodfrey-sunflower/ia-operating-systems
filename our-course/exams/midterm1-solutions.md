> # ⚠️ SPOILER — MODEL ANSWERS AND MARK SCHEME ⚠️
> ## Do NOT read this until you have sat Midterm 1 under timed conditions.
> This file contains complete solutions. Reading it first destroys the only
> honest measurement you will get. You have been warned.

---

# Midterm 1 — Solutions and Mark Scheme

General marking guidance: 1 mark ≈ one distinct correct point. On prose parts,
ask "would a supervisor accept this sentence as a made point?" — a vague gesture
at the right idea is half a mark, not a mark. Calculation parts: full method
with an arithmetic slip keeps most marks; a bare correct number with no working
gets at most half.

---

## Question 1 — Protection and system calls

### (a) Dual-mode operation [4 marks]

- The CPU has (at least) two privilege levels: **user mode** (restricted) and
  **kernel/supervisor mode** (unrestricted). A **mode bit** in a status/control
  register records the current mode; the hardware checks it on every
  instruction. **[1]**
- In user mode certain **privileged instructions** trap instead of executing —
  e.g. loading the page-table base register / changing the MMU mapping;
  disabling interrupts; executing HALT; direct access to device I/O
  registers. (Any two concrete examples.) **[2]**
- Transitions: user→kernel only via a controlled entry point — a **trap /
  syscall instruction, an interrupt, or a fault** — which switches the mode bit
  *and* jumps to a kernel-defined handler. Kernel→user via a
  return-from-trap that lowers the mode bit. The key point for the mark: user
  code cannot set the mode bit itself. **[1]**

### (b) Tracing `read(fd, buf, n)` [8 marks]

One mark each for roughly these eight steps (order matters):

1. The libc `read()` wrapper places the **system-call number** and the
   arguments (`fd`, `buf`, `n`) in the agreed registers and executes the
   **trap/`ecall`** instruction. **[1]**
2. The trap switches to kernel mode, saves the user PC and registers (the
   trap frame), and vectors to the kernel's syscall dispatcher, which uses the
   call number to select `sys_read`. **[1]**
3. The kernel **validates**: is `fd` a valid open file descriptor for this
   process? Is the `[buf, buf+n)` range legal, mapped, writable user memory?
   Reject with `-EFAULT`/`-EBADF` otherwise. **[1]**
4. It follows the descriptor to the open-file object / inode and asks the
   **file system**, which determines the disk block(s) needed and calls into the
   **device driver**. **[1]**
5. The driver programs the disk controller (target block, buffer, direction)
   and starts the transfer (typically via DMA). **[1]**
6. The calling process has nothing to do until the data arrives, so the kernel
   **blocks** it (moves it off the CPU, state → *waiting*) and **schedules
   another process**. **[1]**
7. When the controller finishes it raises an **interrupt**; the driver's
   interrupt handler runs, copies/confirms the data into the kernel buffer (or
   completes the DMA), and **wakes** the waiting process (state → *ready*).
   **[1]**
8. When rescheduled, the kernel copies the data into the user's `buf`, puts the
   **return value** (bytes read, or −1) in the return register, and executes
   **return-from-trap** back to the instruction after `read()`, in user mode.
   **[1]**

*Common wrong answer:* claiming the process busy-waits (spins) for the disk —
mark down; the whole point is that it blocks and the CPU is given to someone
else.

### (c) Can protection be software-only? [8 marks]

**(i) Two reasons compilation alone is insufficient [4 marks]** — any two of:
- You cannot trust the binary. A checker only constrains code it actually
  verifies; hand-written assembly, self-modifying code, JITs, or a maliciously
  crafted binary that was never passed through your compiler can do anything.
  Without hardware enforcement there is no way to *require* that only checked
  code runs. **[2]**
- Computed control flow and computed addresses are undecidable in general — you
  cannot statically prove that an arbitrary indirect jump or a pointer computed
  at run time stays in bounds, so a purely static check must either reject
  legal programs or insert run-time checks anyway. **[2]**
- Other valid points: an errant DMA or a wild pointer needs *run-time*
  interception the compiler cannot provide; a bug in the trusted compiler/loader
  compromises the whole system with no defence in depth. (Award for any two
  well-argued reasons.)

**(ii) Designing software-only confinement [4 marks]**

This is a *derivation* part: credit **any coherent scheme** that (1) inserts
run-time checks or transformations on memory accesses and control transfers,
(2) closes the "jump past the check" loophole, and (3) correctly identifies
the cost and where it falls. The model answer below is essentially Wahbe et
al.'s software fault isolation (1993; assigned week-2 reading) — but neither
the term nor the paper is required, and a different-but-sound design (full
bounds checks with traps, or even interpretation/JIT with checks, with its
higher cost acknowledged) earns the same marks.

- **Confining loads/stores [1]:** allocate the sandbox at an alignment-chosen
  base (all its addresses share a fixed top-bit pattern), and have the
  rewriter insert, before every load/store through a *computed* address, a
  short sequence that **forces the address into the region** — AND with a mask
  then OR in the region tag (or an explicit bounds-check that traps).
  Statically verifiable accesses need no inserted code.
- **Confining jumps, unskippably [1]:** indirect jumps/calls get the same
  masking so they can only land inside the sandbox's code area — and the
  checks must be impossible to bypass, e.g. force all control transfers to
  land on fixed-alignment instruction boundaries so a jump cannot land
  *between* a check and the access it guards, or keep the sanitised address in
  a **dedicated reserved register** that the untrusted code is verified (at
  load time) never to write by any other path. A scheme with checks but no
  answer to "why can't the module jump over them?" loses this mark.
- **Cost and where it falls [1]:** a few extra ALU instructions on **every
  unverifiable load, store, and indirect branch** — overhead paid continuously
  *inside* the module's execution (typically a few to tens of percent), unlike
  MMU protection, which is free in the running code and costs only at boundary
  crossings. The verifier/rewriter also joins the trusted computing base.
- **When it beats a separate process [1]:** when **boundary crossings
  dominate** — many small, frequent calls between host and module (kernel
  extensions, packet filters, device-driver plugins), where the process-based
  design pays a trap/mode-switch and its TLB/cache fallout per call but a
  sandboxed call is just a function call. (Also acceptable: hardware with no
  MMU at all.)

---

## Question 2 — Processes and context switching

### (a) State-transition diagram [4 marks]

```
                 admitted            dispatch
   [new] ---------------> [ready] -------------> [running] ---------> [terminated]
                            ^   ^                  |   |     exit
              I/O or event  |   |  timer/pre-empt  |   |
              completes     |   +------------------+   | I/O or wait
                            |         (pre-empted)     v
                            +----------------------[waiting/blocked]
```

- **new → ready:** admitted by the long-term scheduler. **running → ready:**
  pre-empted (timer/quantum expiry or higher-priority arrival). **ready →
  running:** dispatched by the scheduler. **running → waiting:** issues a
  blocking request (I/O, wait, lock). **waiting → ready:** the event completes.
  **running → terminated:** exits. **[4]** — award 1 per correct
  state/transition group; full marks need all five states and the
  pre-emption (running→ready) edge, which is what makes it a *pre-emptive*
  system.

### (b) PCB contents [3 marks]

Half a mark each, six items (need six for full marks):
- **PID / identity** — to name and refer to the process.
- **Process state** — ready/running/waiting, for the scheduler.
- **Saved CPU registers + program counter** — to resume after a switch.
- **Memory-management info** — page-table base / address-space descriptor.
- **Open-file table / descriptors** — the process's I/O state.
- **Scheduling info** — priority, tickets, accumulated run time.
- (Also acceptable: parent pointer, exit status, pending signals, accounting.)

### (c) Process vs user-level thread switch [8 marks]

**(i) What a process switch does that a ULT switch does not [5 marks]** — three
or more of, each with a reason:
- **Switch the address space** — reload the page-table base register (e.g.
  `satp`/CR3). ULTs of one process share the address space, so nothing changes.
  **[1 + 1 for the flush point below]**
- **Flush / invalidate the TLB** (or rely on ASIDs) because the virtual→physical
  mapping just changed; the ensuing TLB misses are a real cost. Not needed for
  ULTs — same mappings. **[1]**
- **Cross the user/kernel boundary** — a process switch happens *in the kernel*
  via a trap and return-from-trap, saving the full trap frame and touching
  kernel scheduler state. A ULT switch is a plain function call entirely in user
  mode: it saves only the callee-saved registers + `ra`/`sp` (the caller-saved
  ones are already dead at the call), and never enters the kernel. **[1 + 1]**
- Also creditable: switching kernel stacks, updating `current`/`myproc`,
  re-arming per-process kernel state.

Full 5 marks: at least three distinct items, each with the "why it's
unnecessary for ULTs" reason. The register-set observation (callee-saved only,
because the switch is a function call) is the one most students miss — reward
it.

**(ii) Overhead calculation [3 marks]**
- Process: 50 000 × 6 µs = 300 000 µs = 0.30 s of switching per second =
  **30%** of CPU time. **[1.5]**
- ULT: 50 000 × 0.1 µs = 5 000 µs = 0.005 s per second = **0.5%**. **[1.5]**

### (d) What user-level threads cannot do [5 marks]

Two things, each with the I/O-bound and multi-core consequence:
- **A blocking system call blocks the whole process.** The kernel sees one
  schedulable entity; if one ULT makes a blocking `read`, every ULT in the
  process stalls until it returns. For an **I/O-bound** program this is
  crippling — you lose exactly the overlap threads were meant to provide (unless
  you resort to non-blocking I/O + an event loop). **[2]** — (mention of
  scheduler activations / async I/O as the fix is a bonus, not required.)
- **ULTs cannot run in parallel on multiple cores.** The kernel schedules the
  one process on one core at a time, so N user threads still get one core's worth
  of CPU. On a **multi-core** machine a CPU-bound multithreaded program gets no
  speed-up from ULTs, whereas kernel threads can be placed on different cores.
  **[2]**
- The remaining **[1]** for a clear framing of the underlying reason: the kernel
  is unaware of user-level threads, so it can neither reschedule around a block
  nor spread them across cores.

---

## Question 3 — Scheduling

### (a) Definitions [3 marks]

- **Turnaround time** = completion time − arrival time (total time in the
  system). **[1]**
- **Waiting time** = turnaround − CPU burst (time spent ready but not running).
  **[1]**
- **Response time** = first-dispatch time − arrival time (time until the process
  first runs). An interactive scheduler should minimise **response time**,
  because perceived interactivity depends on how quickly a job first reacts, not
  on when it finally finishes. **[1]**

### (b) The three schedules [8 marks]

Table for reference (arrival, burst): P1(0,9) P2(1,5) P3(3,2) P4(7,3).

**(i) FCFS** — run in arrival order. **[2 marks: chart 1, metrics 1]**

```
    [========P1========][===P2====][P3][=P4=]
     0                  9          14  16   19
```

| Proc | Finish | TAT | Wait | Resp |
|------|-------:|----:|-----:|-----:|
| P1   | 9      | 9   | 0    | 0    |
| P2   | 14     | 13  | 8    | 8    |
| P3   | 16     | 13  | 11   | 11   |
| P4   | 19     | 12  | 9    | 9    |

Average TAT = (9+13+13+12)/4 = **11.75**; average Wait = (0+8+11+9)/4 =
**7.00**; average Resp = (0+8+11+9)/4 = **7.00** (under FCFS a process first
runs the moment it reaches the front, so response = waiting here). The long P1
at the head of the queue is a small convoy effect in action.

**(ii) SRTF** (pre-emptive; at each instant run the least remaining time).
**[2.5 marks: chart 1.5, metrics 1]**

Reasoning highlights — there are **two pre-emptions**: P1 runs 0–1; at t=1 P2
arrives with burst 5 < P1's remaining 8, **pre-empting P1**. P2 runs 1–3; at
t=3 P3 arrives with burst 2 < P2's remaining 3, **pre-empting P2**. P3 runs
3–5 and finishes. P2 resumes 5–8 (at t=7 P4 arrives with burst 3, but P2's
remaining 1 is smaller, so **no** pre-emption) and finishes. Then P4 (3) beats
P1 (remaining 8): P4 runs 8–11; P1 finally runs 11–19.

```
    [P1][P2 ][P3 ][P2   ][=P4=][=======P1=======]
     0  1    3    5      8     11                19
```

| Proc | Finish | TAT | Wait | Resp |
|------|-------:|----:|-----:|-----:|
| P1   | 19     | 19  | 10   | 0    |
| P2   | 8      | 7   | 2    | 0    |
| P3   | 5      | 2   | 0    | 0    |
| P4   | 11     | 4   | 1    | 1    |

Average TAT = (19+7+2+4)/4 = **8.00**; average Wait = (10+2+0+1)/4 = **3.25**;
average Resp = (0+0+0+1)/4 = **0.25**.

**(iii) RR, q = 2** (new arrivals enqueued before a simultaneously-pre-empted
process; on this table no arrival coincides with a quantum expiry, so the
convention is never actually exercised — but it must be stated).
**[2.5 marks: chart 1.5, metrics 1]**

Queue trace: P1 runs 0–2 (P2 arrives at 1); at t=2 queue [P2, P1]. P2 runs 2–4
(P3 arrives at 3) → [P1, P3, P2]. P1 runs 4–6 (rem 5) → [P3, P2, P1]. P3 runs
6–8 and finishes (P4 arrives at 7) → [P2, P1, P4]. P2 runs 8–10 (rem 1) →
[P1, P4, P2]. P1 runs 10–12 (rem 3) → [P4, P2, P1]. P4 runs 12–14 (rem 1) →
[P2, P1, P4]. P2 runs 14–15, done → [P1, P4]. P1 runs 15–17 (rem 1) →
[P4, P1]. P4 runs 17–18, done → [P1]. P1 runs 18–19, done.

```
    [P1][P2][P1][P3][P2][P1][P4][P2][P1][P4][P1]
     0  2   4   6   8   10  12  14  15  17 18  19
```

| Proc | Finish | TAT | Wait | Resp |
|------|-------:|----:|-----:|-----:|
| P1   | 19     | 19  | 10   | 0    |
| P2   | 15     | 14  | 9    | 1    |
| P3   | 8      | 5   | 3    | 3    |
| P4   | 18     | 11  | 8    | 5    |

Average TAT = (19+14+5+11)/4 = **12.25**; average Wait = (10+9+3+8)/4 =
**7.50**; average Resp = (0+1+3+5)/4 = **2.25**.

**Comment [1 mark]:** SRTF gives by far the
lowest average turnaround (8.00 vs FCFS 11.75 vs RR 12.25). It is provably
optimal for average turnaround/waiting because always running the shortest
remaining job minimises the total time work sits unfinished; the cost is
possible starvation of long jobs (here P1, the longest, finishes last with
TAT 19) and the need to know/estimate burst lengths. RR time-slices for good
response (avg 2.25 vs FCFS's 7.00), but the slicing stretches every job's
completion — its average turnaround is even worse than FCFS's here.

*Common errors:* missing the second SRTF pre-emption (P3 pre-empting P2 at
t=3), or wrongly pre-empting P2 with P4 at t=7 (P2's remaining 1 < P4's 3);
mis-ordering the RR queue after P3's slice ends at t=8 (P2 is at the head, not
P1); a different but clearly-stated, internally-consistent RR convention can
still earn full marks; computing waiting time as turnaround without
subtracting the burst.

### (c) Lottery in expectation [3 marks]

Total tickets = 6 + 3 + 1 = 10.

**(i) [2 marks]** Expected CPU share = tickets/total:
A = 6/10 = **60%**, B = 3/10 = **30%**, C = 1/10 = **10%**. Over 100 quanta the
expected counts are A ≈ 60, B ≈ 30, C ≈ 10. **[2]**

**(ii) [1 mark]** Each lottery C wins with probability p = 1/10 independently,
so the number of lotteries held up to and including C's first win is geometric
with mean **1/p = 10** lotteries **[0.5]**. (If a student instead counts the
lotteries C *loses* before its first win — the failures-before-success
convention, mean (1 − p)/p = **9** — accept it for full credit provided the
convention is stated; the question asks for the count *including* the winning
lottery, so an unexplained 9 is not credited.) The weakness exposed: lottery's shares are only correct *in
expectation* — over short intervals the variance is large and a process can be
unlucky for a long stretch — whereas **stride scheduling** achieves the same
proportions deterministically (per-process "pass" advanced by a stride ∝
1/tickets; always run the smallest pass), with bounded short-term error.
**[0.5]**

### (d) MLFQ behaviour [2 marks]

- **H (never blocks)** uses a full quantum at each level, so it is demoted
  Q0 → Q1 → Q2 and settles in **Q2** (until a 100-tick boost briefly lifts
  it). **L** runs 1 tick and blocks *before* consuming its 2-tick Q0 quantum,
  so by the voluntary-block rule it stays in — and settles in — **Q0**. **[1]**
- Consequence: whenever L's I/O completes it is runnable in Q0, strictly above
  H, so it is dispatched **almost immediately** — excellent response — while H
  runs in the gaps L leaves. MLFQ thus approximates SJF without knowing burst
  lengths: interactive work floats, CPU-bound work sinks. **[1]**

*Common error:* asserting L is demoted because it "used a tick at Q0" — the rule
demotes only on using a *full* quantum; a voluntary block before that keeps the
process where it is.

### (e) Real-time: RM bound and EDF [4 marks]

**(i) Utilisation vs the Liu–Layland bound [2 marks]**

U = 2/5 + 2/8 + 2/10 = 0.40 + 0.25 + 0.20 = **0.85**. **[1]**
0.85 > 0.780, so the set **fails** the Liu–Layland sufficient test. What that
tells you: RM is **not proven** schedulable. What it does *not* tell you: that
RM will miss a deadline — the bound is **sufficient, not necessary**, so the
set may still be RM-schedulable; only an exact test (e.g. response-time
analysis) can decide. **[1]** — the mark is for correctly stating the
one-sidedness; "fails the test ⇒ unschedulable" is the classic error and
loses it.

*For reference (not required):* response-time analysis with RM priorities
(T1 > T2 > T3 by period) gives fixed points R₁ = 2 ≤ 5, R₂ = 2+2 = 4 ≤ 8,
R₃ = 2+4+2 = 8 ≤ 10 — all deadlines met, so this set **is** RM-schedulable
despite failing the bound. Reward a candidate who demonstrates this.

**(ii) EDF [2 marks]**

EDF (dynamic priority: always run the released job with the earliest absolute
deadline) is optimal for this model: it meets **all deadlines whenever
U ≤ 1** — and here U = 0.85 ≤ 1, so EDF is **guaranteed** to schedule the set.
**[1]** Why the bound is 1: EDF re-orders work at run time so the processor is
never idle while any job is urgent-and-ready, so feasibility reduces to simply
not demanding more than 100% of the CPU. RM's priorities are **fixed** by
period; a low-rate task can be forced to wait through worst-case phasings of
higher-rate tasks even when spare capacity exists elsewhere in the timeline,
which is why static priorities cannot promise more than ≈ 0.693–0.78 (→ ln 2
as n → ∞) in general. **[1]**

---

## Question 4 — Concurrency

### (a) Definitions [4 marks]

- **Race condition:** the result depends on the relative timing/interleaving of
  operations by concurrent threads on shared state; different schedules give
  different (some wrong) outcomes. **[1]**
- **Critical section:** a region of code that accesses shared state and must not
  be executed by more than one thread at a time. **[1]**
- Four conditions for correct mutual exclusion (any phrasing): **(1) mutual
  exclusion** — at most one thread in the critical section; **(2) progress** —
  if the section is free, one of the contending threads must be allowed in (no
  needless blocking); **(3) bounded waiting / no starvation** — a thread waits
  only a bounded number of turns; **(4) no assumptions about speed or number of
  CPUs**. **[2]** (half a mark each for the three beyond mutual exclusion).

### (b) The race and its fix [8 marks]

**(i) Interleaving [3 marks]** — `balance` starts at 100, both withdraw 80:

```
 T1: b1 = balance (100)
 T2: b2 = balance (100)      <- both read before either writes
 T1: 100 >= 80  -> b1 = 20
 T2: 100 >= 80  -> b2 = 20
 T1: balance = 20
 T2: balance = 20
```

Both tests saw 100, both succeeded, and the account paid out 160 from a balance
of 100. Final `balance` = **20** (a lost update — the second write clobbers the
first; the "impossible" outcome is two successful 80-withdrawals from 100).
**[3]** — 2 for a correct interleaving showing both reads before both writes, 1
for the correct final value and the observation that both withdrawals succeeded.

**(ii) Spinlock from test-and-set [5 marks]**

```c
volatile int lock = 0;

void spin_lock(volatile int *lock) {
    while (test_and_set(lock) != 0)   // spin while it was already held
        ;                             // (returns old value; 0 means we got it)
}
void spin_unlock(volatile int *lock) {
    *lock = 0;
}

void withdraw(int amount) {
    spin_lock(&lock);
    int b = balance;
    if (b >= amount) {
        b = b - amount;
        balance = b;
    }
    spin_unlock(&lock);
}
```

Marks: correct `test_and_set` loop semantics — spin while the returned *old*
value is 1, proceed when it is 0 **[2]**; `spin_unlock` simply stores 0 **[1]**;
lock acquired **before the read** and released **after the write-back**, so the
whole read-test-modify-write is one critical section **[2]**. Deduct if the lock
wraps only the write (the race is in the read-test-write as a whole), or if
`test_and_set` is used non-atomically.

### (c) When a spinlock is wrong [8 marks]

**(i) Long/blocking critical sections and single-core [4 marks]**
- A spinlock **busy-waits**, burning CPU while it waits. If the holder runs for
  a long time or (worse) **blocks** inside the critical section, every waiter
  spins uselessly for that whole time — pure waste and possibly deadlock (a
  spinlock holder must never sleep). Use a **blocking mutex / sleep-lock**: a
  waiter is put to sleep and the CPU is given to other work, then woken on
  release. **[2]**
- On a **single core** a spinlock is especially bad: the waiter spins but the
  *only* CPU is exactly what the lock-holder needs to make progress and release
  the lock. The spinner just wastes its whole quantum; nothing can change until
  it is pre-empted. Here you should **yield/block** (a mutex that deschedules the
  waiter) so the holder can run. Spinlocks only make sense when the holder is
  running *concurrently on another CPU* and the wait is expected to be very
  short. **[2]**

**(ii) Lab 5 allocator contention [4 marks]**
- The single lock is *correct*, but it **serialises** every `kalloc`/`kfree`
  across all CPUs: while one CPU holds it, the others spin (test-and-set
  repeatedly) instead of doing useful work, and the cache line holding the lock
  ping-pongs between cores. The cost of contention is this lost parallelism plus
  the coherence traffic — throughput stops scaling with cores. **[2]**
- The fix (as in Lab 5) is to **split the data structure so unrelated operations
  take different locks**: give each CPU its own free list and lock, so the common
  case takes an uncontended local lock; only when a CPU's list is empty does it
  **steal** a batch from another CPU's list (briefly taking that lock). Same
  semantics — you can still find any free page — but contention collapses.
  **[2]** (Analogous answer for the bucketed buffer cache is equally valid.)

---

*End of Midterm 1 solutions.*
