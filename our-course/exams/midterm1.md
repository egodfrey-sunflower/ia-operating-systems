# Midterm 1 — Operating Systems

**Coverage:** weeks 1–6 (OS structure, protection, processes, scheduling,
concurrency I).
**Time:** 90 minutes. **Closed book.**
**Answer THREE of the four questions.** Each question is worth 20 marks.
Marks for each part are shown in brackets. Where a calculation is requested,
show your working: a correct method with an arithmetic slip earns most of the
marks; a bare number earns few.

---

## Question 1 — Protection and system calls

A modern operating system relies on hardware **dual-mode operation** to protect
itself and other processes from a buggy or malicious program.

*(a)* Describe dual-mode operation. Your answer should explain what
distinguishes user mode from kernel (supervisor) mode, give **two** examples of
operations the hardware forbids in user mode, and state how the CPU knows which
mode it is in. **[4 marks]**

*(b)* A user program calls the C library function `read(fd, buf, n)` to read
`n` bytes from an open file into `buf`. Trace, in order, the sequence of events
from the moment the program invokes `read()` until control returns to the
instruction after the call. Your trace should mention: the transition into the
kernel and how the arguments and the call number are conveyed; how the kernel
validates the request; the role of the device driver and of the disk
controller's interrupt; what the calling process does while the data is being
fetched; and how execution and the return value are delivered back to user
mode. **[8 marks]**

*(c)* A colleague argues that hardware protection is unnecessary: "a sufficiently
clever compiler and a trusted loader could guarantee that no program ever
executes a privileged instruction or touches memory it does not own, so we could
run everything in a single address space in kernel mode and save the cost of the
trap on every system call."

  (i) Give **two** reasons why protection cannot, in general, be achieved by
  compilation and static checking alone on conventional hardware. **[4 marks]**

  (ii) Suppose you nevertheless *must* run an untrusted native-code module in
  the kernel's single address space, with **no** hardware protection available
  for it. You may transform the module's machine code with a compiler or
  binary rewriter before it runs, and you may reject code you cannot make
  safe. **Propose** a scheme that confines the module's loads, stores, and
  jumps to a designated "sandbox" region of memory. Say what your tool inserts
  or checks, how you prevent the module simply jumping *past* your inserted
  checks, and what run-time cost your scheme imposes and where that cost
  falls. Finally, give **one** situation in which this software-only
  confinement would genuinely be preferable to putting the module in its own
  process. Marks are for a coherent, defensible design, not for naming any
  published system. **[4 marks]**

---

## Question 2 — Processes and context switching

*(a)* Draw the process state-transition diagram for a pre-emptively scheduled
operating system. Label every state and every transition, and for each
transition name the event that causes it. **[4 marks]**

*(b)* List **six** distinct items of information the kernel must keep in a
process control block (PCB), and for each say in one phrase why it is needed.
**[3 marks]**

*(c)* A *process* context switch and a *user-level thread* context switch have
very different costs.

  (i) State precisely what must be saved, restored, or changed on a switch
  between two **processes** that is *not* required on a switch between two
  **user-level threads of the same process**. Name at least three such items and
  explain why each is unnecessary for the user-level thread switch. **[5 marks]**

  (ii) On a particular machine a full process context switch costs 6 µs
  (including the indirect cost of the cache and TLB misses that follow it),
  whereas a user-level thread switch costs 0.1 µs. A server performs 50 000
  context switches per second. Calculate the fraction of CPU time consumed by
  switching under each scheme. **[3 marks]**

*(d)* Given your answer to (c), a user-level thread library looks strictly
better. State **two** important things a pure user-level threading package
*cannot* do that kernel threads (or processes) can, and explain the consequence
of each for a program that is (i) I/O-bound and (ii) running on a multi-core
machine. **[5 marks]**

---

## Question 3 — Scheduling

Four processes arrive at a single CPU with the arrival times and CPU-burst
lengths below (all times in abstract units). No process performs I/O in parts
(a)–(c). Assume context-switch overhead is zero, and that when two processes are
otherwise tied the lower-numbered process is preferred.

| Process | Arrival time | CPU burst |
|---------|-------------:|----------:|
| P1      | 0            | 9         |
| P2      | 1            | 5         |
| P3      | 3            | 2         |
| P4      | 7            | 3         |

*(a)* Define **turnaround time**, **waiting time**, and **response time** for a
process, and state which one a scheduler for an interactive system should try
hardest to minimise, and why. **[3 marks]**

*(b)* For **each** of the following policies, draw a Gantt chart of the schedule
and compute the average turnaround time, the average waiting time, and the
average response time over the four processes:

  (i) First-Come-First-Served (FCFS);

  (ii) Shortest-Remaining-Time-First (SRTF, i.e. pre-emptive SJF);

  (iii) Round-Robin with a time quantum of 2 units (RR, q = 2). When a running
  process is pre-empted at the same instant that another process arrives, place
  the newly-arrived process on the ready queue **before** the pre-empted one.

Comment briefly on which policy is best for turnaround time and why SRTF
achieves it. **[8 marks]**

*(c)* Instead of the table above, suppose three permanently CPU-bound processes
A, B, C are scheduled by a **lottery scheduler** holding a fresh lottery before
every quantum. A holds 6 tickets, B holds 3, and C holds 1.

  (i) State the expected fraction of the CPU each process receives, and hence
  the expected number of quanta each runs out of the next 100. **[2 marks]**

  (ii) State the expected number of lotteries held **up to and including** the
  one C first wins, and the weakness of lottery scheduling this exposes compared
  with a deterministic proportional-share scheduler such as stride scheduling.
  **[1 mark]**

*(d)* A multi-level feedback queue (MLFQ) has three levels Q0 (highest), Q1, Q2
with round-robin time quanta of **2, 4, and 8** ticks respectively. The rules
are: a new process starts in Q0; a process that uses a **full** quantum at its
level is demoted one level (Q2 is the bottom); a process that **blocks
voluntarily before** using its quantum stays at its level (its partial-quantum
count is reset); a periodic boost returns every process to Q0 every 100 ticks.

Process **H** is CPU-bound and never blocks. Process **L** is interactive: each
time it is scheduled it runs for exactly 1 tick and then blocks for I/O.
Starting with both in Q0 on one CPU, state which queue each process settles in
and explain briefly why L consistently gets excellent response time even though
H never yields. **[2 marks]**

*(e)* Three periodic **real-time** tasks run on one CPU. Task *i* has
worst-case compute time *Cᵢ* and period *Tᵢ*, and each job's deadline is the
end of its period:

| Task | Cᵢ | Tᵢ |
|------|---:|---:|
| T1   | 2  | 5  |
| T2   | 2  | 8  |
| T3   | 2  | 10 |

  (i) Compute the total utilisation *U* = Σ *Cᵢ*/*Tᵢ* and test it against the
  Liu–Layland bound for **rate-monotonic** (RM) scheduling of three tasks,
  *U* ≤ 3(2^(1/3) − 1) ≈ 0.780. State precisely what the outcome of this test
  does — and does **not** — tell you about whether RM will meet all deadlines
  for this set. **[2 marks]**

  (ii) For the same task set, state what **EDF** (earliest-deadline-first)
  guarantees, and explain why EDF's utilisation bound is exactly 1 while the
  static-priority RM bound is lower. **[2 marks]**

---

## Question 4 — Concurrency

Two threads share a bank account. `balance` is a global integer initialised to
100. Each thread runs the following C fragment once, concurrently, on a
multiprocessor with no locking:

```c
void withdraw(int amount) {
    int b = balance;      // (1) read
    if (b >= amount) {    // (2) test
        b = b - amount;   // (3) compute
        balance = b;      // (4) write back
    }
}
```

Thread T1 calls `withdraw(80)`; thread T2 calls `withdraw(80)`.

*(a)* Define **race condition** and **critical section**, and state the four
conditions a correct mutual-exclusion mechanism must satisfy (mutual exclusion
plus three others). **[4 marks]**

*(b)* (i) Give a specific interleaving of the numbered statements above that
produces an **outcome** — the pair of returned results together with the final
`balance` — that should have been impossible: **both** withdrawals succeed.
State the final balance your interleaving produces. **[3 marks]**

  (ii) The hardware provides an atomic `test_and_set(lock)` that sets `*lock` to
  1 and returns its *previous* value. Using it, write a `spin_lock()` and
  `spin_unlock()`, and show exactly where you would place the calls in
  `withdraw()` to remove the race. **[5 marks]**

*(c)* Spinlocks are not always the right tool.

  (i) Explain why a spinlock is a poor choice for guarding a critical section
  that may block or run for a long time, **and** why it is a poor choice on a
  **single**-core machine in particular. State what synchronisation primitive
  you would use instead in each case and how it avoids the problem. **[4 marks]**

  (ii) In Lab 5 (weeks 10–12) you will meet the stock xv6 physical-memory
  allocator, which guards one global free list with a **single** spinlock; on a
  multi-core machine this lock becomes heavily contended. The lock is correct —
  so what exactly is the cost of the contention, and what design change removes
  most of it without changing the lock's semantics? **[4 marks]**

---

*End of Midterm 1.*
