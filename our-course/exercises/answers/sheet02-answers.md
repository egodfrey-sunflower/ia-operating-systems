> # ⚠️ SPOILER — ANSWERS TO EXAMPLES SHEET 2 ⚠️
> **Do not read until you have attempted the sheet closed-book.**
> Model answers / marking notes. IA-cited questions: work them from
> `../../../cambridge-course/examples_sheets/examples_sheet1.pdf`; notes here flag the key points.

---

# Sheet 2 — Answers

## A. Warm-ups

**A1. FALSE.** `fork()` gives the child a *copy* of the address space (logically;
copy-on-write physically). Writes are private — a variable the child writes is
*not* seen by the parent. Sharing writable memory after fork requires an
explicit shared mapping (`mmap(MAP_SHARED)` / shared memory), which is exactly
what the "shared memory" IPC mechanism (Section D) provides.

**A2. FALSE.** They are distinct. An *interrupt* is a hardware event that
transfers control into the kernel; a *context switch* is the kernel replacing one
process's saved state with another's. An interrupt need not cause a switch (the
handler may return to the same process), and a switch can occur without a device
interrupt (e.g. a process blocks in a syscall and the kernel switches). A timer
interrupt is the common *trigger* for a preemptive switch, but they are not the
same event.

**A3. TRUE.** A pipe is an in-kernel buffer shared by related processes on one
host. A socket abstracts a bidirectional endpoint; a *TCP* socket carries data
across machines, and even a Unix-domain socket, while local, is the same API that
generalises to the network. So pipes are same-host-only; sockets need not be.

**A4. FALSE.** The blocked/ready distinction is *correctness*, not just
efficiency. A blocked process is waiting on an event (I/O completion) and is
*not runnable*; scheduling it would busy-spin or, worse, the scheduler would
"run" a process that can make no progress and never yields for the right reason.
The distinction lets the scheduler pick only genuinely-runnable work and lets the
I/O completion (an interrupt) *wake* the process. Ignoring it wastes the CPU and
can deadlock the wakeup logic.

**A5. FALSE.** The stored salted hash protects the password database *at rest* —
if the file leaks, an attacker cannot read the passwords out of it, and the
per-user **salt** defeats precomputed rainbow tables and hides the fact that two
users chose the same password. But at **login time** the user transmits the
cleartext password and the system must hash it (with that user's salt) to compare
against the stored value — so the system *is* trusted with the cleartext at the
moment of authentication (and must not log or leak it). The hash removes the need
to *store* cleartext, not the need to *handle* it. (OSTEP ch. 54.)

## B. Process lifecycle and the PCB

**B1 (IA Sheet 1 Q4).** Marking notes:
- (a) States: **new → ready → running → {blocked/waiting} → ready → …
  → terminated**. Transitions: ready→running (dispatch/schedule);
  running→ready (preempt: timer/quantum expiry or higher-priority arrival);
  running→blocked (issues I/O or waits on an event); blocked→ready (event/I/O
  completes — note: goes to *ready*, not straight to *running*, because the CPU
  may be busy); running→terminated (exit). There is *no* blocked→running edge:
  a woken process must be re-scheduled.
- (b) PCB holds: pid, process state, saved registers/PC/PSW, memory-management
  info (page-table base / segment info), scheduling info (priority, accounting),
  open-file table / fd table, pending signals, parent/children links, credentials.
- (c) Non-preemptive: *advantage* — simple, low overhead, no locking of kernel
  data against preemption, good throughput for batch; *disadvantage* — a
  long/looping job monopolises the CPU, terrible response time and no fairness
  for interactive users.
- (d) On an interrupt: finish current instruction; save minimal state / switch to
  kernel stack; consult the interrupt vector; save the interrupted context; run
  the handler (acknowledge device, do the work / wake a blocked process);
  possibly mark a process ready and set the "reschedule" flag; restore context
  and return (to the same or a newly-scheduled process).
- (e) Very high interrupt load ⇒ the CPU spends all its time in handlers and
  never returns to useful (user) work — *receive livelock*. Mitigations:
  interrupt coalescing, switching to polling under load (NAPI), rate-limiting.

**B2. PCB and context switch.**
- (a) Save/restore: general-purpose registers, PC, stack pointer, processor
  status/flags, and the *memory map* (page-table base register). The register
  file is saved on the *kernel stack* (in the trap frame) on entry; the pointer
  to that saved context, plus page-table base and scheduling/accounting state,
  lives in the **PCB**.
- (b) `swtch()` is a normal function *call* from `scheduler()`/`yield()`, so by
  the C calling convention the *caller* has already saved caller-saved registers;
  only *callee-saved* registers (plus `ra`, `sp`) must be preserved across the
  call. The rest are either dead or already spilled. (Interrupt entry, by
  contrast, must save *everything*, because it interrupts arbitrary code.)
- (c) Thread-to-thread within one process: save/restore registers + stack only;
  the address space (page table) is unchanged, so the **TLB stays warm**.
  Process-to-process additionally switches the page-table base register, which
  (absent tagged TLBs / ASIDs) **flushes the TLB**, causing a burst of
  page-walk misses afterwards — the expensive part.

**B3. Interrupts vs traps vs syscalls.**
- (a) *Interrupt*: asynchronous, caused by an external device, unrelated to the
  current instruction. *Trap/exception*: synchronous, caused *by* the current
  instruction (page fault, divide-by-zero, illegal op). *System call*: a
  synchronous, *deliberate* trap the program executes to request service. Async:
  interrupts. Sync: traps and syscalls.
- (b) They share one vector, so the handler reads a *cause* register
  (`scause`/`mcause` on RISC-V) to distinguish device interrupt vs exception vs
  environment-call, and dispatches accordingly. It must, because the responses
  differ (retry a faulting instruction vs advance past an `ecall` vs service a
  device).
- (c) The user's registers *are the user's live state*; if the handler clobbers
  them before saving, that state is lost and the process can't resume correctly.
  So `uservec` first spills all user registers into the `TRAPFRAME` (Lab 2)
  before using any register for kernel work.

## C. Creating and reaping processes

**C1 (IA Sheet 1 Q3).** One-sentence-per-cell table:
- *Simplicity*: non-preemptive is simpler (no timer, fewer race windows in the
  kernel); preemptive needs a timer and careful concurrency control.
- *Fairness*: preemptive is far fairer (bounds any one job's CPU hold);
  non-preemptive lets a job hog the CPU.
- *Performance*: non-preemptive can give higher throughput (less switching) but
  poor interactive response; preemptive gives good response at some
  context-switch cost.
- *Hardware*: preemptive **requires a timer interrupt**; non-preemptive needs
  none (cooperative yield suffices).

**C2. fork/exec/wait and the shell.**
- (a) Splitting into `fork` then `exec` opens a *window in the child*, running
  the parent's code with the child's identity, in which the shell rearranges file
  descriptors *before* the new program starts. Redirection (`dup2` onto 0/1/2)
  and pipes (wire pipe ends to fds) both live entirely in that window (Lab 1
  Tasks 2–3). A single `spawn()` would have to encode every such setup as
  parameters; `fork`+`exec` makes it ordinary code.
- (b) A *zombie* is a terminated child whose exit status has not yet been
  collected — it still occupies a PCB slot until the parent `wait`s. An *orphan*
  is a child whose parent exited first; it is re-parented to `init` (pid 1),
  which reaps it. Lab 1 Task 4 forces you to reap background children (via
  `SIGCHLD` or `waitpid(WNOHANG)`); un-reaped zombies accumulate and eventually
  exhaust the process table.
- (c) `cd`/`exit` must be builtins because a child is a *separate process*: a
  forked `cd` would `chdir` in the child, which then exits — the shell's own
  working directory is untouched. `exit` in a child would kill only the child.
  They must run *in the shell process itself* to have any effect.
- (d) Copy-on-write: `fork` shares all pages read-only and marks them COW; a page
  is physically copied only when one side first *writes* it. So `fork` copies page
  *tables*, not page *contents*. Without COW, `fork`-then-`exec` would eagerly
  duplicate the whole address space only for `exec` to throw it away
  microseconds later — pure waste.

## D. IPC

**D1. Mechanisms.**
- (a) Table:
  | | unit | inherently synchronised? | crosses machine? | copies per msg |
  |---|---|---|---|---|
  | pipe | byte stream | yes (blocks on empty/full) | no | 2 (writer→kernel, kernel→reader) |
  | shared memory | raw bytes in shared page | **no** | no | **0** (direct loads/stores) |
  | socket (UNIX/TCP) | byte stream (or datagram) | yes | TCP: yes | 2 (often more) |
- (b) Shared memory makes *bulk transfer* cheap (zero copies, no syscall per
  message once mapped). In return it pushes *synchronisation and framing* back
  onto the programmer: there is no built-in "data ready" signal and no message
  boundaries, so you must add locks / condition variables / semaphores and define
  your own protocol — reintroducing the race-condition risk (Sheet 5) that pipes
  and sockets hide inside the kernel.
- (c) A byte stream has no message boundaries, so two writers' bytes can
  *interleave* mid-message, corrupting both. POSIX guarantees writes of **≤
  `PIPE_BUF` bytes are atomic** (not interleaved), so keeping each message ≤
  `PIPE_BUF` and one write per message avoids the interleave; larger writes may
  be split and interleaved.

**D2. Choosing.**
- (a) **Pipe** — exactly the shell filter case; a kernel-buffered byte stream
  between related short-lived processes, with automatic flow control and EOF.
- (b) **Shared memory** — 25 MB × 60 fps is 1.5 GB/s; copying through the kernel
  twice would be prohibitive, so map the frame buffers shared and copy zero
  times (add a semaphore/ring for hand-off).
- (c) **TCP socket** — different machines ⇒ you need the network; sockets are the
  only option here.
- (d) **Unix-domain socket** — RPC request/response on one host, *and* it is the
  mechanism that can pass an open fd between processes (`SCM_RIGHTS` ancillary
  data); pipes/shared memory cannot transfer a file descriptor.

**D3. Rendezvous.**
- (a) The kernel tracks the pipe's *reference count of write ends*. `read` on an
  empty pipe blocks **while ≥ 1 write end is open** (more data may come); it
  returns **0 (EOF) only when the last write end closes**. The Lab 1 hang: the
  shell (or a sibling child) leaves a copy of the write end open, so the write-end
  count never reaches zero, so the reader's `read` never sees EOF and blocks
  forever. Fix: every process closes *both* ends it isn't using after `dup2`.
- (b) Shared memory has *no* kernel bookkeeping of readers/writers and *no*
  wakeup: a reader sees whatever is in memory and cannot "block until ready" for
  free. You must add a synchronisation primitive (semaphore, condition variable,
  or a spin/futex) to signal "data available" — which reintroduces the classic
  concurrency bugs (missed wakeups, races on the shared flag) that are the subject
  of Sheet 5.

## Past paper notes
y2012p2q3 covers the context-switch mechanics and the process state machine —
the Section B material. Answers follow from B1–B3 above.
