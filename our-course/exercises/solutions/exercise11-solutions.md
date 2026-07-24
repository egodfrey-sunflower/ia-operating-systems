> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 11 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** All the stacks live in the *same address space* — that is the
defining property of threads. A thread's stack is private by convention only:
nothing stops another thread that holds a pointer into it from reading or
scribbling on it (ch. 27's guideline says other threads cannot *easily* access
it, which is not the same as cannot). This is also why returning a pointer to
stack data across threads is such a productive source of bugs (B3, C1).

**A2. TRUE.** Both switches save one register set to a control block and
restore another, but a thread switch within a process keeps the address space:
there is no page-table switch. (Week 8 adds the deeper consequence: no loss of
address-translation state either.) The saving is real, which is one reason
servers use threads rather than processes for many similar tasks.

**A3. FALSE.** Lost updates need interleaving, not simultaneity.
`counter = counter + 1` is load, add, store; a timer interrupt after the load
lets the other thread run its entire update in the gap, and the first thread's
store then overwrites it. Ch. 26's 50→51-not-52 trace is explicitly on a single
processor.

**A4. FALSE.** A racy program is **indeterminate**: the output depends on where
interrupts land, so it varies from run to run — and is very often correct. That
is precisely what makes races hard: absence of failure in testing is not
absence of the bug (see B4, C1b).

**A5. TRUE.** A `void *` argument lets any pointer type be smuggled in and
cast back inside the thread; a `void *` return does the same on the way out. A
single scalar can even ride in the pointer value itself, as ch. 27's
`long long int` example shows. The cost is that the compiler can no longer
type-check the interface — discipline replaces checking.

**A6. FALSE.** Ch. 26 is explicit: threads are the natural choice when tasks
*share data*; processes are the **sounder** choice for logically separate tasks
with little sharing, because the shared address space that makes threads
convenient also removes all isolation between them.

**A7. FALSE.** Create-then-immediately-join is, as ch. 27 puts it, "a pretty
strange way" to do something with an easier name: a **procedure call**. No
concurrency is gained — the caller sleeps for exactly the duration of the work
it could have done itself.

**A8. TRUE — and both halves matter.** The caller must hold the lock at the
call; `pthread_cond_wait` *releases* it while the thread sleeps (otherwise the
signaller could never acquire it to make the condition true), and *re-acquires*
it before returning. So from the caller's point of view the lock is
continuously held, even though it was given up in the middle.

---

## B. Mechanism and measurement

**B1.**
**(a)** Any schedule that splits one thread's load from its store around the
other thread's complete update works. One that differs from Figure 26.7:
- T1 executes the `mov` at 100 only (eax₁ = 50, counter = 50) — **interrupt**.
- T2 executes all three: 100 (eax₂ = 50), 105 (eax₂ = 51), 108 (counter = 51)
  — **interrupt**.
- T1 resumes at 105: `add` (eax₁ = 51), then 108: `mov` — counter = **51**.

The book's trace interrupts T1 after the `add` (PC = 108); this one interrupts
after the load (PC = 105). Full credit needs the PC/register bookkeeping, not
just "T2 runs in between".

**(b)** The possible finals are **{51, 52}**.
- *52*: any serial order — the second thread's load sees 51 and stores 52.
- *51*: any overlapped order — both loads see 50, both stores write 51 (part a).
- *Not 50*: every store writes some thread's `eax` after the `add`, i.e. a
  loaded value plus one. The smallest loadable value is 50, so every store
  writes ≥ 51, and the final value is the last store.
- *Not 53*: a store of 53 would require a load that observed 52, and 52 can
  only exist after both stores have already happened — there is no third
  increment left to store it.

**(c)** The critical section spans **from the load at 100 up to and including
the store at 108**. An interrupt is harmless if it falls *before* a thread's
load or *after* its store — the thread's update is then either entirely after
or entirely before whatever the other thread does. It is dangerous only when
it falls **between one thread's load and that thread's store**, *and* the
other thread completes a store to the same address inside the gap. Both
conditions are needed: an interrupt inside the window during which the other
thread merely computes (or touches other data) loses nothing.

**(d)** Two separate reasons:
1. **Rare is not correct.** The program remains indeterminate; the race is
   still reachable and will fire eventually — at a frequency you no longer
   control and can no longer reproduce, which makes the bug *harder* to
   diagnose, not gone.
2. **The timer is not the only source of interleaving.** On a multiprocessor
   the two threads run genuinely simultaneously, and the loads and stores
   interleave with no interrupt anywhere. The fix must make the sequence
   atomic (mutual exclusion), not make preemption rare.

**B2.**
**(a)** The word at address 2000 is a **flag** used for ad-hoc signalling: the
thread given `%ax = 1` stores 1 to it (the signaller); the thread given
`%ax = 0` sits in a load–test–jump loop reading it until it sees a non-zero
value (the waiter). Final value: **1**. With the signaller scheduled first the
waiter's loop exits almost immediately.
**(b)** With the inputs swapped, thread 0 is the *waiter* and runs first — so
it spins: load 2000, test, jump back, over and over, making no progress. It
spins until the scheduler's interrupt lets thread 1 run and store the flag; the
**interrupt interval governs exactly how long the CPU is burned**. With
`-i 1000` thread 0 wastes on the order of a thousand instructions before
thread 1 gets its first chance. Every spin iteration is work the machine did
that no one wanted.
**(c)** Ch. 27's two reasons: (1) **performance** — spinning wastes CPU for
potentially a long time, precisely what (b) showed; (2) **it is error-prone** —
the chapter cites the study [X+10] finding roughly *half* of such ad-hoc
synchronisations buggy. The primitive to use instead is the **condition
variable** (with its associated lock): the waiter sleeps instead of spinning,
and the handshake has defined semantics.

**B3.**
**(a)** The three defects, all named by ch. 27:
1. **`lock` is never initialised.** A mutex must be set up with
   `PTHREAD_MUTEX_INITIALIZER` or `pthread_mutex_init()` (and torn down with
   `pthread_mutex_destroy()`); an uninitialised lock has no guaranteed starting
   state, giving code that "sometimes works and sometimes fails in very strange
   ways". Fix: `pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;` or an init
   call checked for success.
2. **Return codes are unchecked.** `pthread_mutex_lock` (like `create`/`join`)
   can fail; ignored, the failure is silent and the "critical section" may
   admit several threads at once. Minimal fix: wrapper functions that
   `assert(rc == 0)`; real programs handle the error.
3. **`return (void *) &r` returns a pointer into the worker's stack.** `r` is
   deallocated the moment `worker` returns, so `main` receives a dangling
   pointer. Fix: allocate the result on the **heap** (`malloc`) and let the
   joiner free it — or have `main` pass in a pointer to a result slot it owns.

**(b)** The difference is **lifetime, not location**. `&args` in `main` is safe
*in this program* because `main` is blocked in `pthread_join` until the worker
finishes — the frame containing `args` provably outlives every use. The
worker's `&r` is the opposite: every use of that pointer happens *after* the
frame has died, because `pthread_join` returning is exactly the event that
makes the return value visible. (Marking note: answers that say "stack addresses
must never be passed to threads" have over-generalised; the chapter's guideline
is "probably doing something wrong", and this pair is why the qualifier is
there.)

**(c)** Because `pthread_join` **writes** the thread's return value into the
caller's variable. To change the caller's `void *` it needs that variable's
address — hence `void **`. Passing a `void *` would hand join only a copy.

**(d)** `trylock` is right where blocking could be fatal or self-defeating —
the standard case (which ch. 27 itself points toward) is **deadlock
avoidance**: holding lock A and wanting lock B, try B, and if it is taken,
release A and retry, rather than sleeping while holding A. What is sacrificed
is the blocking call's simple contract — *when it returns, you hold the lock* —
so every call site grows a failure path, and a careless retry loop
reintroduces exactly the spinning that B2 condemned.

**B4.**
*(Exact helgrind wording varies by version; the content below is what to look
for.)*
**(a)** Prediction: a **possible data race** on the shared variable, because
two threads access it with at least one write and no common lock. Helgrind's
report does identify the two racing source lines (with a stack trace for each
conflicting access) and names the threads involved — enough to walk straight
to the bug. It also reports the address and size of the racing location.
**(b)** **The program is still broken.** A race is a *pair* of unsynchronised
conflicting accesses; locking one side leaves the other free to interleave, so
the lost-update schedule survives. Helgrind still reports the race — the right
verdict, because mutual exclusion only works if **every** access to the shared
datum takes the lock. (Locking both sides silences the report.)
**(c)** Helgrind still raises a **lock-order alarm**: it observes lock A taken
before B in one thread and B before A in another, which is the deadlock
*pattern* — even though the enclosing global lock makes the deadly interleaving
unreachable. Lesson: a dynamic tool raises alarms on **suspicious structure it
observed**, not on a proof that a bad execution is reachable. Its alarms can be
false positives (this one), and its silences are bounded by the schedules it
happened to see. Both directions need interpretation — the tool is an evidence
generator, not a judge.
**(d)** The CV version buys **both**. *Correctness*: `main-signal.c`
synchronises through a bare flag with no lock — helgrind reports the race on
`done`, and the pattern is the ad-hoc synchronisation that [X+10] found buggy
in about half its uses; the CV version has defined semantics and a clean
helgrind run. *Performance*: the flag parent **spins**, burning CPU for the
whole duration of the child's work even on runs that print the right answer;
the CV parent sleeps and is woken once. "Wrong even when right" is the point:
wasted cycles and a latent race are defects the correct-looking output hides.

---

## C. Discussion and design critique

**C1.**
**(a)** Defect 1: **`processed = processed + 1` is an unprotected read-modify-
write on shared data.** It compiles to load, add, store; when two workers'
windows overlap, one increment overwrites the other (the B1 mechanism). Each
lost update subtracts exactly one from the total — hence a total that is
*slightly low*, with the shortfall scaling with contention, i.e. with load.
One worker ⇒ no interleaving ⇒ exact totals, which is why the single-worker
rebuild "fixes" it without fixing anything.
Defect 2: **`return (void *) &s` returns the address of a stack variable.**
The summary lives in the worker's frame; the frame dies when the worker
returns; `pthread_join` then hands `main` a pointer into dead stack, and
`print_summary` reads whatever is there now.

**(b)** The debugger hides symptom 1 because single-stepping **serialises the
schedule**: each thread's load–add–store completes as a unit before attention
moves on, so the dangerous window is never split — the classic heisenbug, and
a direct corollary of A4 (the race's *appearance* depends on timing that the
debugger destroys). Symptom 3 is intermittent because dead stack memory
usually still *contains* the old bytes: garbage appears only if that region
was reused (or unmapped) between the worker's return and `main`'s read, which
depends on what ran in between — ch. 27's "you'll probably (but not
necessarily!) be surprised" is exactly this.

**(c)** Counter: with this week's toolbox, a properly initialised
`pthread_mutex_t` around the increment — restoring **mutual exclusion**, which
makes the three-instruction sequence atomic with respect to other workers.
(Also creditable: keep the count per-worker in `s.handled` — already there! —
and have `main` sum after joining; that *removes* the sharing instead of
arbitrating it, and needs no lock. Either earns full marks with the property
named.) Summary: heap-allocate the `summary_t` in the worker and free it in
`main` after printing, or pass each worker a pointer to a slot in a
`main`-owned array.

**(d)** Defect 1: **helgrind** — it reports a possible data race on
`processed`, citing both racing lines (B4a). Defect 2: **the compiler** —
`gcc -Wall` warns about returning the address of a local variable; ch. 27
notes gcc "will likely complain", and ch. 26's tools aside is blunt that you
should be using these warnings. Marking note: full credit for (a) requires the
instruction-level mechanism for the counter and the lifetime argument for the
summary; "there's a race and a dangling pointer" without mechanism earns
little.

**C2.** The claim covers one of ch. 26's two motivations and misses the other.
**Parallelism** is genuinely dead on one core: there is no second CPU to
exploit, and threading a CPU-bound computation adds context-switch and
synchronisation cost for zero speed-up. But the **I/O-overlap** motivation
survives intact: while one thread is blocked on a device — network, flash,
sensor — another can compute, so a threaded design keeps the single core busy
where a sequential one would idle. Embedded workloads are, if anything,
unusually I/O-bound.
The teammate is right precisely when the workload is **CPU-bound with no
blocking waits** — then one thread is optimal and additional ones are pure
overhead and risk. The single property to check first: **does the application
ever block on I/O while having other useful work it could do?** If yes,
threads (or some other overlap mechanism) pay on any number of cores; if no,
the teammate wins.
*Marking note: the shape wanted is "right for parallelism, wrong for overlap,
and here is the workload test" — a verdict with the condition attached.
Answers may also note that overlap can instead be had with event-driven I/O;
fine, but it must be named as an alternative mechanism, not a refutation.*

**C3.** **Strongest advantage:** a user-level thread switch is just saving and
restoring a few registers in the library — no trap, no mode transition, no
kernel involvement at all. Against week 3's cost model for entering the kernel,
that makes creation and switching orders of magnitude cheaper, and it lets the
application schedule its threads with a policy the kernel could never be told.
**Most structural weakness:** the kernel schedules what it can see — **one
process**. When any user-level thread makes a blocking system call, the kernel
blocks that whole process: every other user thread stalls, defeating exactly
the I/O-overlap motivation that justifies threads (and one process can never
use a second CPU, defeating the other one).
**The information asymmetry:** the kernel knows the facts that matter for
blocking and hardware — that a system call will sleep, when the I/O completes,
how many CPUs exist — but sees a single schedulable entity. The library knows
the facts that matter for policy — how many threads exist, which are runnable,
what they are doing — but cannot see blocking coming or harvest more than one
CPU. Each side holds precisely the information the other's decisions need,
which is why the design space OSPP §4.8 maps (kernel threads, M:N hybrids,
upcall-style schemes) is about *moving information across that boundary*, not
about either extreme winning outright.
*Marking note: full credit requires both directions of the asymmetry stated as
information, not just "blocking syscalls are a problem".*
