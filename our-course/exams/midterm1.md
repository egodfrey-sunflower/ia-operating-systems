# Midterm 1 — Operating Systems

**Coverage:** weeks 1–10 — all of virtualization (OSTEP ch. 1–23, App. F, and
the assigned cross-readings and papers).
**Time:** 90 minutes. **Closed book.** No calculator — none of the arithmetic
needs one.
**Answer THREE of the four questions.** Each question is worth 20 marks; marks
for each part are shown in brackets. Where a calculation is requested, show
your working: a correct method with an arithmetic slip earns most of the marks;
a bare number earns few. Where a trace depends on a convention, use the one
stated.

---

## Question 1 — Processes, the process API, and limited direct execution

*(a)* During a timer interrupt that leads to a context switch from process A to
process B, registers are saved **twice**, by two different agents, into two
different places. Say what is saved each time, by what, and into what — and
state why the timer interrupt is the piece of this machinery the OS cannot do
without. **[4 marks]**

*(b)* The process API separates process creation into `fork()` and `exec()`.

  (i) Consider this program (error handling omitted; `fork()` succeeds):

```c
int main(void) {
    printf("A\n");
    if (fork() == 0) {
        printf("B\n");
        if (fork() == 0)
            printf("C\n");
    }
    printf("D\n");
    return 0;
}
```

  How many processes run in total, and exactly which lines does each print?
  State, with a one-line justification each, whether the output line `D` can
  ever appear **before** `B`, and whether `C` can ever appear before `B`.
  Assume each `printf` is atomic, and that `stdout` is line-buffered —
  output goes to a terminal, not a file. **[4 marks]**

  (ii) Show how a shell uses `fork()`, `exec()` and `wait()` to run the command
  `wc < in.txt > out.txt`, saying in which process each step happens and how
  the redirections are arranged. Then make the argument — the one Ritchie and
  Thompson make about their own design — for why creation and program-loading
  being *separate* calls is exactly what makes this clean. What would a
  combined `spawn("wc", ...)` call have to grow instead? **[4 marks]**

*(c)* A server core runs at 2 GHz. Each request it serves makes **8** system
calls, and each system call costs **3,000 cycles** of pure kernel-crossing
overhead — trap entry, dispatch, return, and the cache and TLB pollution that
follows — independent of the useful work done inside the kernel.

  (i) At 50,000 requests per second, what fraction of the core is consumed by
  kernel-crossing overhead alone? Show your working, and state what request
  rate this overhead alone would cap the core at if the requests did nothing
  else. **[4 marks]**

  (ii) A colleague waves this away: "CPUs are a thousand times faster than in
  1990 — Ousterhout's worry about operating-system overhead is a museum piece."
  Using Ousterhout's actual thesis about *why* OS performance failed to track
  processor speed, argue where your colleague is wrong — and then state the
  class of workload for which the colleague is, in practice, right. A verdict
  without conditions earns half. **[4 marks]**

---

## Question 2 — Scheduling

Four jobs arrive at a single CPU. No job performs I/O; context-switch overhead
is zero. **Convention:** when two jobs are otherwise tied, the job that arrived
earlier is preferred; under round-robin, a job that arrives at the same instant
another is preempted joins the ready queue **before** the preempted one.

| Job | Arrival | Length |
|-----|--------:|-------:|
| A   | 0       | 8      |
| B   | 2       | 4      |
| C   | 4       | 1      |
| D   | 5       | 2      |

*(a)* Define **turnaround time** and **response time**, and name a policy that
is provably optimal for average turnaround (jobs arriving together) and a
policy designed to optimise response. One sentence on why no single policy
wins both. **[3 marks]**

*(b)* For each of (i) **FIFO**, (ii) **STCF** (preemptive
shortest-time-to-completion-first), and (iii) **round-robin with quantum 2**,
draw the schedule and compute the average turnaround time and average response
time over the four jobs. For STCF, point out each preemption decision as you
make it; for RR, keep the ready queue explicitly. **[9 marks]**

*(c)* Three CPU-bound jobs hold lottery tickets: A 100, B 50, C 250.

  (i) Give each job's expected CPU share, and its expected number of quanta out
  of the next 80. Why are these figures only expectations — and what happens to
  the deviation from them as the jobs run longer? **[2 marks]**

  (ii) Under **stride scheduling** with stride constant 10,000, compute each
  job's stride, then trace the scheduler until all pass values are next equal
  (ties broken alphabetically), listing who runs at each quantum. Confirm the
  resulting counts match the ticket ratios. **[2 marks]**

*(d)* An engineer maintaining an MLFQ scheduler proposes deleting the periodic
priority boost: "Rule 4 already charges jobs for their full allotment whether
they block or not, so gaming is solved, and the boost just lets CPU hogs
periodically stall my interactive queue." Evaluate the proposal: say precisely
which problem the boost solves that Rule 4 does not, construct the workload
that the boost-less scheduler mishandles, and state the (real) condition under
which the engineer's configuration would nonetheless be acceptable.
**[4 marks]**

---

## Question 3 — Paging, TLBs, and multi-level tables

A 32-bit machine has 4 KB pages and 4-byte PTEs. It uses a **two-level** page
table: the top 10 bits of a virtual address index the page directory, the next
10 bits index a page of the page table, and PTEs and page-directory entries
hold a 20-bit PFN plus flag bits.

*(a)* (i) Relative to the linear page table it replaces, state the problem the
two-level structure solves and the new cost it introduces. Under what property
of real address spaces does the trade pay? **[2 marks]**

  (ii) Name **four** flag bits found in a typical PTE and give one phrase each
  on what the hardware or OS uses them for. **[2 marks]**

*(b)* The page-directory base register holds physical address `0x10000`. Entry
1 of the page directory is valid with PFN `0x023`; entry 2 of that second-level
page is valid with PFN `0x080`.

  (i) Translate virtual address `0x004025A8`: show the split of the address
  into its three fields (give each field's value), the **physical address of
  each page-table access** the hardware makes, and the final physical address.
  **[5 marks]**

  (ii) A process maps two regions, each aligned to a 4 MB boundary: 8 MB of
  code+heap at the bottom of the address space and a 4 MB stack region at the
  top. How much memory do its page tables occupy in total? Compare with the
  linear table for the same machine, and state the ratio. **[3 marks]**

  (iii) The TLB is probed in 2 ns and memory access takes 60 ns; the TLB hits
  98% of the time; on a miss the full walk is performed with no caching of
  page-table contents. Compute the effective access time of a load. State the
  formula before the numbers. **[2 marks]**

*(c)* The architects propose doubling the page size to 8 KB, keeping everything
else fixed: "the page table halves, TLB reach doubles, and page faults move
twice the data per fault — strictly better." Evaluate: quantify the first two
claims for this machine (a 64-entry TLB), name what the proposal costs and
**where in the workload** that cost lands, and give the workload conditions
under which you would and would not take the trade. **[6 marks]**

---

## Question 4 — Memory under pressure

*(a)* Define **internal** and **external** fragmentation. State which of the
two paging eliminates and which it retains (and where the retained one shows
up). Then one phrase each: why must a free-list allocator **coalesce**, and
what repeated cost does a **slab/segregated-list** cache remove for objects of
a fixed size? **[4 marks]**

*(b)* A process has **3 frames**, initially empty, and references virtual pages:

```
0  1  2  0  3  1  4  2  1  3
```

  (i) Trace **LRU**: for each reference give hit or miss, the eviction if any,
  and the resulting frame contents. State the total miss count. **[3 marks]**

  (ii) Trace the **clock algorithm** with one use bit per page, under exactly
  this convention: pages are loaded with use bit = 1; a hit sets the use bit
  to 1; on a miss the hand examines frames in fill order, starting where it
  last stopped — a use bit of 1 is cleared and the hand advances, a use bit of
  0 selects the victim, and after replacement the hand advances one frame.
  Give the trace and the miss count. **[4 marks]**

  (iii) Compare your two traces: where do the policies part company, and what
  information does LRU hold that the use bit throws away? **[1 mark]**

*(c)* A machine has a 100 ns memory access time and a page-fault service time
of 10 ms when the fault only needs to read the incoming page.

  (i) What is the maximum page-fault rate that keeps the effective access time
  at or below 200 ns? State your EAT formula and any approximation you make.
  **[2 marks]**

  (ii) Measurement then shows that **30%** of faults must first write back a
  dirty victim, costing an additional 10 ms before the read. At the fault rate
  from (i), what is the effective access time now? **[2 marks]**

*(d)* The VAX-11 provided **no reference bit**. Describe the replacement design
VMS shipped instead — the per-process resident-set policy and the global
second-chance lists — and explain how their combination approximates LRU
despite the missing hardware, including which faults complete with no disk I/O
at all. Close with a judgement: name one respect in which this design is
actually *preferable* to a global clock over the whole of memory, and the
condition under which that advantage matters. *(Levy & Lipman is the source;
the mechanism is what earns the marks.)* **[4 marks]**

---

*End of Midterm 1.*
