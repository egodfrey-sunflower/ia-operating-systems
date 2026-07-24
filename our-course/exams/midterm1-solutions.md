> # ⚠️ SPOILER — MODEL ANSWERS AND MARK SCHEME ⚠️
> ## Do NOT read this until you have sat Midterm 1 under timed conditions.
> Reading it first destroys the only honest measurement you will get.

---

# Midterm 1 — Solutions and Mark Scheme

General guidance: 1 mark ≈ one distinct made point. Prose parts: a vague
gesture at the right idea is half a mark, not a mark. Calculation parts: full
method with an arithmetic slip keeps most marks; a bare correct number earns at
most half. A clearly stated alternative convention, applied consistently, earns
full marks.

---

## Question 1 — Processes, the process API, and limited direct execution

### (a) The two register saves [4 marks]

- **Save 1 — the hardware, at the trap.** When the timer interrupt fires, the
  hardware switches to kernel mode and saves the interrupted process A's user
  registers and PC into A's **kernel stack** (the trap frame) — just enough
  that a return-from-trap could resume A exactly where it was. **[1.5]**
- **Save 2 — the OS, at the switch.** The kernel's scheduler decides to switch:
  the context-switch code saves A's current **kernel** register context (the
  callee-saved registers, stack pointer, return address) into A's process
  structure (PCB), and restores B's saved kernel context, thereby switching
  kernel stacks and eventually returning-from-trap into B. **[1.5]**
- **Why the timer is indispensable:** without it, regaining the CPU depends on
  the process trapping voluntarily; a process that neither makes a system call
  nor faults — a spin loop — holds the CPU forever. The timer converts
  "hopefully" into "within one tick". **[1]**

*Common wrong answer:* describing one save that mixes both — "the OS saves the
registers to the PCB when the interrupt happens". The point of the part is that
the trap-frame save (hardware, kernel stack, every trap) and the context save
(OS, PCB, only on an actual switch) are distinct events with distinct agents.

### (b) fork/exec and the shell [8 marks]

**(i) [4 marks]** The second `fork()` executes only inside the first child
(it is inside the `if (fork() == 0)` branch), so **three processes** run:
the original P, child C1, grandchild C2. **[1]**

Output: P prints `A` then `D`; C1 prints `B` then `D`; C2 prints `C` then `D`.
So one `A`, one `B`, one `C`, and **three `D`s** — six lines. **[1]**

- `D` before `B`: **yes** — P's `D` is concurrent with everything C1 does;
  nothing orders P behind its child (there is no `wait`). **[1]**
- `C` before `B`: **no** — `C` is printed by C2, which is created by C1 *after*
  C1 has printed `B`; program order within C1 plus fork-happens-after make
  `B` precede `C` in every schedule. **[1]**

*Marking note:* the stem's line-buffering assumption is load-bearing. A
candidate who observes that with `stdout` redirected to a file, unflushed
buffer contents are duplicated by each `fork()` — P's buffered `A` reaches
all three processes and C1's buffered `B` reaches C2, giving nine lines
(three `A`s, two `B`s, one `C`, three `D`s) — has answered better than the
model; credit in full.

**(ii) [4 marks]**
1. Shell parses the line, calls **`fork()`**. **[0.5]**
2. **In the child**, before `exec`: rearrange the file descriptors — close
   fd 0 and open `in.txt` (it lands in fd 0, the lowest free slot); close fd 1
   and open/create `out.txt` (lands in fd 1). (Equivalently `open` + `dup2`.)
   **[1]**
3. The child calls **`exec("wc", ...)`**, which replaces the address space with
   `wc` but **preserves the open-file table** — `wc` reads stdin and writes
   stdout as always, never knowing about the redirection. **[1]**
4. The parent shell calls **`wait()`** and prints the next prompt when the
   child exits. **[0.5]**

The Ritchie–Thompson argument: because creation and loading are separate, the
shell gets a moment *inside the new process, before the new program starts*,
in which ordinary code can adjust the environment — redirections, pipes,
closing fds. No cooperation from `wc` and no special-casing in the kernel is
needed. A combined `spawn` must instead grow a parameter for every environment
adjustment anyone will ever need (files to redirect, pipes to wire, directory,
signal state, …) — the interface bloats to enumerate what fork/exec get for
free. **[1]**

*Marking note:* the mark in (ii) is for the *argument*, not the incantation —
"the child runs shell code between fork and exec, so redirection is just
code" is the sentence a supervisor is looking for.

### (c) Syscall overhead, and Ousterhout [8 marks]

**(i) [4 marks]**
- Syscall rate = 50,000 × 8 = 400,000 syscalls/s. **[1]**
- Overhead = 400,000 × 3,000 = 1.2 × 10⁹ cycles/s. **[1]**
- Fraction of the 2 GHz core = 1.2 × 10⁹ / 2 × 10⁹ = **60%**. **[1]**
- Cap: each request costs 8 × 3,000 = 24,000 overhead cycles, so overhead
  alone limits the core to 2 × 10⁹ / 24,000 ≈ **83,000 requests/s**. **[1]**

**(ii) [4 marks]** Ousterhout's thesis (1990): OS operations were not tracking
raw processor speed because they are dominated by things that improve much more
slowly than CPU arithmetic — **memory latency** and the fixed costs of
kernel crossings (trap machinery, and today the cache/TLB pollution each
crossing causes). **[1]** So "CPUs are 1000× faster" misses the point: the
denominator of syscall cost is not CPU arithmetic speed; measured in cycles,
crossings have improved far less, and (i) shows a syscall-heavy server still
burning most of a modern core on overhead. **[1]** The gap CPU-vs-memory has
*widened* since 1990, which strengthens, not dates, the thesis for
crossing-heavy and memory-bound work. **[1]**

Where the colleague is right: **compute-bound workloads** — long CPU bursts,
few crossings, working set in cache — do scale with processor speed; there the
OS is a rounding error. The verdict must carry this condition. **[1]**

*Common wrong answer:* treating Ousterhout as "OSes are badly written". The
paper's claim is architectural (memory and crossing costs), not a code-quality
complaint.

---

## Question 2 — Scheduling

### (a) Definitions [3 marks]

- **Turnaround** = completion − arrival; **response** = first-run − arrival.
  **[1]**
- Optimal average turnaround (simultaneous arrivals): **SJF** (STCF in the
  preemptive/arrival case). Designed for response: **round-robin**. **[1]**
- No single winner: turnaround wants short jobs run *to completion* first;
  response wants every job started *soon* — slicing that helps the second
  stretches completions and hurts the first. **[1]**

### (b) The three schedules [9 marks]

Jobs (arrival, length): A(0,8) B(2,4) C(4,1) D(5,2). Total work 15.

**(i) FIFO [2 marks]**

```
[========A========][===B===][C][=D=]
 0                 8       12  13  15
```

| Job | Finish | TAT | Resp |
|-----|-------:|----:|-----:|
| A   | 8      | 8   | 0    |
| B   | 12     | 10  | 6    |
| C   | 13     | 9   | 8    |
| D   | 15     | 10  | 8    |

Average TAT = (8+10+9+10)/4 = **9.25**; average response = (0+6+8+8)/4 =
**5.5**.

**(ii) STCF [3.5 marks]** Preemption decisions: at t=2, B (4) arrives and beats
A (remaining 6) — **preempt A**. At t=4, C (1) beats B (remaining 2) —
**preempt B**; C runs 4–5. At t=5, D (2) ties B (remaining 2) — tie goes to
the earlier arrival, **B**, which runs 5–7. Then D (2) beats A (6): D runs 7–9.
A finishes 9–15.

```
[A ][B ][C][B ][=D=][=====A=====]
 0  2   4  5   7    9           15
```

| Job | Finish | TAT | Resp |
|-----|-------:|----:|-----:|
| A   | 15     | 15  | 0    |
| B   | 7      | 5   | 0    |
| C   | 5      | 1   | 0    |
| D   | 9      | 4   | 2    |

Average TAT = (15+5+1+4)/4 = **6.25**; average response = (0+0+0+2)/4 =
**0.5**.

**(iii) RR, q = 2 [3.5 marks]** Queue trace (arrivals enqueued before a
simultaneously preempted job): A runs 0–2 (B arrives at 2; A preempted) →
queue [B, A]. B runs 2–4 (C arrives at 4, enqueued before preempted B) →
[A, C, B]. A runs 4–6 (D arrives at 5) → queue at 6: [C, B, D, A]. C runs 6–7,
finishes early. B runs 7–9, finishes at quantum end. D runs 9–11, finishes.
A runs 11–13 and, alone, 13–15.

```
[A ][B ][A ][C][B ][D ][A     ]
 0  2   4   6  7   9   11     15
```

| Job | Finish | TAT | Resp |
|-----|-------:|----:|-----:|
| A   | 15     | 15  | 0    |
| B   | 9      | 7   | 0    |
| C   | 7      | 3   | 2    |
| D   | 11     | 6   | 4    |

Average TAT = (15+7+3+6)/4 = **7.75**; average response = (0+0+2+4)/4 =
**1.5**.

*Common errors:* wrongly preempting B with D at t=5 (it is a tie — the stated
convention keeps B); mis-ordering the queue at t=4 (at t=4 the queue holds
[A]; C is enqueued ahead of the preempted B, giving [A, C, B] — so A runs
next, then C); computing B's response as 2−0 rather than from its own
arrival.

### (c) Lottery and stride [4 marks]

**(i) [2 marks]** Total tickets 400. Shares: A 100/400 = **25%**, B **12.5%**,
C **62.5%**; of 80 quanta: A ≈ 20, B ≈ 10, C ≈ 50. **[1]** Only expectations,
because each quantum is an independent random draw; over a window of n quanta
the *proportional* deviation shrinks (relative error ~1/√n), so fairness
improves with run length. **[1]**

**(ii) [2 marks]** Strides: A = 10,000/100 = **100**, B = **200**, C = **40**.
Trace (run min pass; ties alphabetical; pass += stride after running):

| Quantum | Min pass → runs | Passes after (A,B,C) |
|--------:|-----------------|----------------------|
| 1 | all 0 → A (alphabetical) | 100, 0, 0 |
| 2 | 0 (B, C tied) → B | 100, 200, 0 |
| 3 | 0 → C | 100, 200, 40 |
| 4 | 40 → C | 100, 200, 80 |
| 5 | 80 → C | 100, 200, 120 |
| 6 | 100 (A) → A | 200, 200, 120 |
| 7 | 120 → C | 200, 200, 160 |
| 8 | 160 → C | 200, 200, 200 |

All passes next equal at 200, after 8 quanta: **A ran 2, B ran 1, C ran 5** —
exactly 2:1:5 = 100:50:250. **[2 for a correct, consistent trace and the
check; −½ for a tie-break slip that is otherwise consistent]**

### (d) Deleting the MLFQ boost [4 marks]

- What Rule 4 solves is **gaming**: charging the full allotment regardless of
  yields stops a job hovering at high priority by strategic I/O. It does
  nothing about **starvation**: a job legitimately demoted to the bottom queue
  stays there forever if higher queues never drain. **[1]**
- Failure workload: a steady stream of interactive jobs keeps the top queues
  perpetually non-empty; the bottom-queue CPU-bound job receives zero CPU
  indefinitely. The boost bounds its wait by the boost period. **[1]**
- Second, distinct loss: a job that **changes phase** (long compute, then
  becomes interactive) is stuck at the bottom with no route back up; the boost
  is the forgiveness mechanism. **[1]**
- Acceptable when: the machine genuinely has no long-running work it must keep
  alive under sustained interactive load — e.g. a dedicated interactive
  appliance where background work is explicitly best-effort, or a batch-only
  box where the top queues are usually empty. Verdict must name a condition of
  this kind. **[1]**

---

## Question 3 — Paging, TLBs, and multi-level tables

### (a) Bookwork [4 marks]

**(i) [2]** The linear table must be sized for the whole virtual address space
(2²⁰ PTEs = 4 MB here) even when almost none of it is mapped. The two-level
table allocates leaf pages only for regions that exist — space proportional to
what is mapped — at the cost of an **extra memory access per walk** (directory,
then PTE) and extra complexity. It pays because real address spaces are
**sparse**: a few dense regions, vast invalid gaps.

**(ii) [2]** Any four (½ each): **valid** (is the translation usable — else
fault); **protection** (read/write/execute enforcement); **present** (in
memory vs swapped — drives page faults); **dirty** (written since load — must
the evictor write it back); **reference/accessed** (touched recently — food
for clock/LRU approximation); **user/supervisor** (may user mode touch it).

### (b) Translation and sizing [10 marks]

**(i) [5 marks]** `0x004025A8` = binary
`0000 0000 01 | 00 0000 0010 | 0101 1010 1000`:

- Directory index = top 10 bits = **1**; page-table index = next 10 bits =
  **2**; offset = **0x5A8**. **[1.5]**
- Access 1 — page-directory entry: 0x10000 + 1 × 4 = **0x10004** → PFN 0x023,
  so the leaf page lives at 0x023 × 0x1000 = 0x23000. **[1.5]**
- Access 2 — PTE: 0x23000 + 2 × 4 = **0x23008** → PFN 0x080. **[1]**
- Physical address = 0x080 × 0x1000 + 0x5A8 = **0x805A8** (a third access
  fetches the data itself). **[1]**

**(ii) [3 marks]** One leaf page of PTEs maps 2¹⁰ × 4 KB = **4 MB**. 8 MB of
code+heap → 2 leaf pages; 4 MB stack → 1 leaf page; plus the directory itself:
4 pages × 4 KB = **16 KB**. **[2]** Linear table: 2²⁰ × 4 B = **4 MB** — a
ratio of **256×**. **[1]**

**(iii) [2 marks]** EAT = TLB + h·(mem) + (1−h)·(walk + mem) =
2 + 0.98 × 60 + 0.02 × (2 × 60 + 60) = 2 + 58.8 + 3.6 = **64.4 ns**. **[2 —
formula 1, evaluation 1]** (Assumption: walk accesses are full memory accesses,
TLB probe paid on every access.)

### (c) 8 KB pages [6 marks]

- **"The table halves" — two defensible readings; either earns the mark.**
  Against a *linear* table: 2³²/2¹³ = 2¹⁹ entries × 4 B = **2 MB** (from
  4 MB) — halved as claimed. The stronger answer quantifies it for the
  **two-level table this machine actually uses**, with (b)(ii)'s process: at
  8 KB pages a table page holds 8 KB / 4 B = 2,048 PTEs, the split becomes
  8 (directory) + 11 (table) + 13 (offset), and one leaf now maps
  2,048 × 8 KB = 16 MB — so the 8 MB code+heap and the 4 MB stack take one
  leaf each, plus a 256-entry directory (1 KB): 2 × 8 KB + 1 KB = **17 KB,
  up from 16 KB** (24 KB if the directory is padded to a full page). For a
  sparse process the claim is *false* — the leaves got bigger while the
  process never needed many — and saying so, with the computation, is the
  best answer and earns full credit. **[1]**
- **Reach doubles — true:** 64 × 8 KB = **512 KB** (from 256 KB), so fewer TLB
  misses for a given working set. **[1]**
- **The cost: internal fragmentation.** Every mapped region now wastes on
  average half of 8 KB rather than half of 4 KB in its last page — and the
  waste lands on **processes with many small regions** (many small mappings,
  many small processes): their resident footprint inflates, which is memory
  *and* cache-of-pages pressure. Fault transfers also move 8 KB whether or not
  the second half is wanted, wasting disk bandwidth when locality is poor.
  **[2]**
- **Verdict with conditions:** take the trade for machines running few, large,
  dense working sets (databases, scientific compute — the workloads for which
  large/huge pages exist); refuse it where the workload is many small
  processes or sparse fine-grained mappings, where the fragmentation bill
  exceeds the table-and-reach win. **[2]**

*Marking note:* "strictly better" is the flag — full marks require locating
who pays (internal fragmentation, wasted transfer), not merely agreeing with
the two true claims.

---

## Question 4 — Memory under pressure

### (a) Fragmentation and allocators [4 marks]

- **External:** free memory shredded into pieces too small to satisfy a
  request although the total would suffice — a disease of variable-size
  allocation. **[1]** **Internal:** waste *inside* an allocated unit larger
  than the request. **[1]**
- Paging eliminates **external** fragmentation (any free frame serves any
  page) and retains **internal** — the unused tail of a region's last page.
  **[1]**
- Coalescing: without merging adjacent free chunks, splitting is a one-way
  ratchet — the list degenerates until no large request can ever succeed.
  Slab/segregated caches: for a fixed-size object they remove the repeated
  search-and-split of a general allocator and (Bonwick's point) the repeated
  **re-initialisation** — freed objects come back still constructed. **[1]**

### (b) LRU vs clock [8 marks]

Reference string: 0 1 2 0 3 1 4 2 1 3, three frames.

**(i) LRU [3 marks]** (state shown most-recent first)

| Ref | Result | Evict | State (MRU→LRU) |
|----:|--------|-------|------------------|
| 0 | miss | — | 0 |
| 1 | miss | — | 1 0 |
| 2 | miss | — | 2 1 0 |
| 0 | hit  | — | 0 2 1 |
| 3 | miss | 1 | 3 0 2 |
| 1 | miss | 2 | 1 3 0 |
| 4 | miss | 0 | 4 1 3 |
| 2 | miss | 3 | 2 4 1 |
| 1 | hit  | — | 1 2 4 |
| 3 | miss | 4 | 3 1 2 |

**8 misses** (2 hits). **[3 — trace 2, count 1]**

**(ii) Clock [4 marks]** Frames F1–F3 in fill order; superscript = use bit.

| Ref | Result | Hand action | Frames after |
|----:|--------|-------------|--------------|
| 0 | miss | fill | 0¹ – – |
| 1 | miss | fill | 0¹ 1¹ – |
| 2 | miss | fill | 0¹ 1¹ 2¹ |
| 0 | hit | use←1 | 0¹ 1¹ 2¹ |
| 3 | miss | clear 0,1,2; evict **0**; hand→F2 | 3¹ 1⁰ 2⁰ |
| 1 | hit | use←1 | 3¹ 1¹ 2⁰ |
| 4 | miss | clear 1; evict **2**; hand→F1 | 3¹ 1⁰ 4¹ |
| 2 | miss | clear 3; evict **1**; hand→F3 | 3⁰ 2¹ 4¹ |
| 1 | miss | clear 4; evict **3**; hand→F2 | 1¹ 2¹ 4⁰ |
| 3 | miss | clear 2; evict **4**; hand→F1 | 1¹ 2⁰ 3¹ |

**8 misses** (2 hits: references 4 and 6). **[4 — trace 3, count 1]**

**(iii) [1 mark]** They diverge first at the miss on 3: with **every use bit
set, clock degrades to FIFO** and evicts 0 (oldest), where LRU evicts 1
(least recent). Clock therefore keeps 1 and hits at reference 6 where LRU
misses — and pays it back at reference 9. LRU holds a **total recency
ordering**; the use bit remembers only "touched since the hand last passed".
(Here the totals happen to tie at 8 — the approximation is good, not free.)

*Common error:* forgetting that the hand does **not** advance on a hit, or
restarting the sweep at F1 on every miss instead of where the hand stopped.

### (c) Fault-rate algebra [4 marks]

**(i) [2]** EAT = (1−p)·100 ns + p·(10 ms) ≈ 100 + p × 10⁷ ns. Requiring
≤ 200 ns: p × 10⁷ ≤ 100 → **p ≤ 10⁻⁵** — one fault per 100,000 accesses.
(The −100p term is negligible; stating the approximation earns the formula
mark.)

**(ii) [2]** Mean service = 0.7 × 10 ms + 0.3 × 20 ms = **13 ms**. EAT =
100 + 10⁻⁵ × 1.3 × 10⁷ = 100 + 130 = **230 ns**. **[working 1, result 1]**

### (d) VMS without a reference bit [4 marks]

- Each process has a **resident-set limit**; within it, replacement is plain
  **FIFO** — no reference information needed. **[1]**
- Pages evicted from a resident set are not discarded: they move, still in
  memory, onto global **second-chance lists** — a clean list and a dirty list
  (dirty pages written back lazily, in batches). A fault on a listed page is a
  **soft fault**: the page is re-attached from the list with **no disk I/O**.
  Only pages that age off the lists are truly lost. **[1]**
- Why this approximates LRU: FIFO's mistakes — evicting a hot page — are
  cheaply undone, because a hot page faults back from the list before it ages
  out. The lists give FIFO exactly the "was it referenced again soon?" signal
  the missing hardware bit would have supplied, at the cost of a soft fault
  rather than a bit test. **[1]**
- Judgement: the per-process limit **isolates** processes — a memory hog can
  only churn its own resident set, while a global clock lets the hog evict
  everyone's pages. That matters precisely on multiprogrammed machines with
  untrusted or bursty workloads; on a single-user machine with one dominant
  working set the global policy wastes less. (Either well-argued advantage —
  isolation, or batched dirty writes — earns the mark, with its condition.)
  **[1]**

---

*End of Midterm 1 solutions.*
