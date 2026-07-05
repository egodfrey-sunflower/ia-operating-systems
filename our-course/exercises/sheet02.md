# Examples Sheet 2 — Processes, Interrupts, and IPC

**Attempt after Week 3.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet02-answers.md` (spoilers — attempt first).

**Reading this sheet leans on** (see `../reading-list.md`): OSTEP ch. 4–6 (the
process, the process API, limited direct execution); the xv6 book ch. 1;
Kerrisk ch. 24–27 (process creation, termination, `wait`, `exec`) and ch. 44 (pipes)
alongside **Lab 1**; A&D ch. 3 (The Programming Interface — the
pipes/shared-memory/sockets comparison Section D leans on), with Kerrisk ch. 61
(advanced sockets) for the file-descriptor-passing detail in D2(d).

IA pointers are into `../../cambridge-course/examples_sheets/examples_sheet1.pdf`.

---

## A. Warm-ups (true/false with justification)

**A1.** "After `fork()`, the parent and child share the same writable memory, so
a variable written by the child is immediately visible to the parent."

**A2.** "A context switch and an interrupt are the same event: the CPU cannot do
one without the other."

**A3.** "Two processes communicating through a pipe must run on the same
machine; two communicating through a socket need not."

**A4.** "Marking a process *blocked* (waiting on I/O) rather than leaving it
*ready* is purely an efficiency optimisation — a correct scheduler could ignore
the distinction and still make progress."

**A5.** "Storing a salted hash of each password means the system never needs to
be trusted with the cleartext password." *(Reading: OSTEP ch. 54,
authentication.)*

---

## B. Process lifecycle and the PCB

**B1. (IA foundations — do by citation.)**
Do **IA Examples Sheet 1, Q4** in `../../cambridge-course/examples_sheets/examples_sheet1.pdf`:
- (a) the process life-cycle diagram — every state and every transition, with
  the reason for each;
- (b) what the OS keeps in the process control block;
- (c) one advantage and one disadvantage of non-preemptive scheduling;
- (d) the steps the OS takes when an interrupt occurs;
- (e) what goes wrong under a very high interrupt load.

For (a), make sure your diagram distinguishes **ready** from **blocked** and
shows why a process cannot go directly from blocked to running. For (e), name
the failure mode and what the fix family looks like.

**B2. The PCB and the context switch.**
- (a) List the register and non-register state the kernel must save and restore
  on a context switch. Which of it lives in the PCB, and which on the kernel
  stack?
- (b) In xv6, `swtch()` saves and restores only the *callee-saved* registers,
  not all of them. Why is saving just those sufficient? (Think about how
  `swtch` is *called*, not interrupted.)
- (c) Distinguish a context switch between two *threads of one process* from one
  between two *processes*. What extra work does the second require, and which
  hardware structure makes it expensive? *(Threads are formalised in week 6 and
  the TLB in week 8 — answer from first principles: what must be saved, and which
  cached state becomes stale.)*

**B3. Interrupts vs traps vs system calls.**
- (a) Define *interrupt*, *trap (exception)*, and *system call*, and classify
  each as synchronous or asynchronous with respect to the running instruction
  stream.
- (b) All three funnel through a single trap vector in xv6. Given that, how does
  the kernel tell them apart once it is in the handler, and why does it need to?
- (c) Why must the very first thing the trap handler does be *not* to touch the
  user's registers before saving them? Relate this to the `TRAPFRAME` you meet
  in Lab 2.

---

## C. Creating and reaping processes

**C1. (IA foundations — do by citation.)**
Do **IA Examples Sheet 1, Q3** in `../../cambridge-course/examples_sheets/examples_sheet1.pdf`:
compare and contrast preemptive and non-preemptive scheduling on *simplicity,
fairness, performance and required hardware support*. Keep your answer tight —
one crisp sentence per (approach × criterion) cell.

**C2. `fork`/`exec`/`wait` and the shell (relate to Lab 1).**
In Lab 1 you build a shell whose spine is `fork` + `execvp` + `waitpid`
(`labs/lab1-shell/README.md`).
- (a) Why is process creation split into two calls (`fork` then `exec`) rather
  than one `spawn(program, args)`? What does the gap between them let the shell
  do — and which Lab 1 features (redirection, pipes) *depend* on that gap?
- (b) What is a *zombie*, what is an *orphan*, and how is each resolved? Which
  Lab 1 task forces you to reap zombies, and what accumulates if you don't?
- (c) `cd` and `exit` must be shell *builtins*, not external programs (Lab 1,
  Task 1 write-up). Explain why, in terms of *which process* a forked `cd` would
  change the directory of.
- (d) Copy-on-write makes `fork` cheap even though it logically copies the whole
  address space. Sketch how, and say what the very common `fork`-then-`exec`
  pattern would waste without it. *(Preview — the full mechanism is week 9 and
  Lab 4; a two-sentence sketch suffices here.)*

---

## D. Interprocess communication

**D1. Mechanisms and their trade-offs.**
Consider three IPC mechanisms: **pipes** (byte streams), **shared memory** (a
shared physical page mapped into both address spaces), and **sockets**
(Unix-domain or TCP).
- (a) For each, state: the unit of transfer, whether it is inherently
  synchronised, whether it crosses a machine boundary, and how many
  user↔kernel copies a single message incurs.
- (b) Shared memory is the fastest for bulk data yet the least used casually.
  Explain the trade-off: what does shared memory make cheap, and what does it
  push back onto the *programmer* that pipes and sockets handle for you?
- (c) A pipe delivers a byte stream with no message boundaries. Give a concrete
  bug this causes when two writers share one pipe, and state the guarantee POSIX
  gives for writes up to `PIPE_BUF` bytes.

**D2. Choosing for a workload.**
For each scenario pick the best-fit mechanism from {pipe, shared memory,
Unix-domain socket, TCP socket} and justify in one sentence:
- (a) A shell wiring `grep | sort | uniq` — three short-lived filters passing a
  text stream (this is literally Lab 1, Task 3).
- (b) A video pipeline passing 4K frames (~25 MB each) between two processes at
  60 fps on one host.
- (c) A web server and a database on different machines.
- (d) A request/response RPC between two daemons on one host that also needs to
  pass an open *file descriptor* from one to the other.

**D3. Rendezvous and blocking.**
- (a) When a reader reads an empty pipe whose write end is still open, it
  *blocks*; when the last write end closes, the same read returns *EOF*.
  Explain how the kernel distinguishes these two cases, and connect it to the
  classic Lab 1 hang: the shell forgets to close a pipe's write end and the
  reader waits forever (Lab 1, Task 3 write-up).
- (b) Contrast this blocking rendezvous with shared memory, where there is *no*
  built-in wakeup. What primitive must you add to shared memory to get the same
  "wait until data is ready" behaviour, and what bug class does that reintroduce
  (a forward pointer to Sheet 5)?

---

## Past paper questions

Per this directory's `README.md`, attempt this under time pressure (~35 min,
closed-book) after finishing this sheet (file in `../../cambridge-course/exam_questions/`):

- **y2019p2q4** — protection mechanisms (how the CPU, memory and I/O are
  protected from user processes), a comparison of the Unix IPC mechanisms, and
  designing with ACLs vs capabilities. The IPC-comparison part is this sheet's
  Section D material; parts (a)/(c) are Sheet-1 revision (protection, the access
  matrix).

For **untimed** drill on this sheet's context-switch and process-state
material, this pre-2016 question fits (in `../../cambridge-course/exam_questions/`):

- **y2012p2q3** — how a process's execution point and address space are saved
  and restored on a context switch, plus a process state-transition diagram
  traced through timer interrupts and system calls — Section B material.

It is drill, not a timed paper: do it now if you have time, or keep it as
untimed warm-up for the **week-5 Midterm 1 dry run** (sat on y2016p2q3 and
y2017p2q3 — see `../exams/README.md`).
