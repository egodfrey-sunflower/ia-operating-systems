# Exercise Sheet 11 — Threads, races, and the thread API

**Attempt after Week 11.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise11-solutions.md`](solutions/exercise11-solutions.md).

**This sheet leans on:** OSTEP ch. 26–27; OSPP §4.6–4.8 (§C3 only).

**You will need:** the OSTEP `x86.py` simulator from
`ostep-homework/threads-intro/` (for §B2); a C compiler with `-pthread` and
`valgrind` (for §B4, using the programs from `ostep-homework/threads-api/`).
§B1 and §B3 are pen-and-paper.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** Each thread has its own stack, so a local variable in one thread's
function cannot be read or corrupted by another thread.

**A2.** Switching between two threads of the same process is cheaper than
switching between two processes.

**A3.** On a single-CPU machine, only one thread executes at a time, so
`counter = counter + 1` on a shared counter cannot lose updates there.

**A4.** A program containing a race condition will produce a wrong answer on
every run.

**A5.** `pthread_create`'s start routine takes a `void *` argument and returns
`void *` so that a thread can be handed, and can return, a value of any type.

**A6.** For a program made of several cooperating tasks, threads are always the
better choice over processes.

**A7.** Calling `pthread_create` and then immediately calling `pthread_join` on
the new thread is a reasonable way to offload a computation.

**A8.** A thread calling `pthread_cond_wait` must hold the associated lock at
the call, and it still holds that lock when the call returns.

---

## B. Mechanism and measurement

**B1. The anatomy of a lost update.** *(Pen and paper — this is ch. 26's own
example, so work from its assumptions.)* The critical section
`counter = counter + 1` compiles to three x86 instructions, loaded at address
100: `mov 0x8049a1c,%eax` (5 bytes, at 100), `add $0x1,%eax` (3 bytes, at 105),
`mov %eax,0x8049a1c` (at 108; the next instruction sits at 113). `counter`
starts at 50, and threads T1 and T2 each execute the sequence exactly once.
  (a) OSTEP's Figure 26.7 interrupts T1 after the `add`. Construct a
      **different** interleaving that also yields a final value of 51, stating
      exactly which instruction each thread has completed when each interrupt
      fires.
  (b) List every final value of `counter` this program can produce. Can it
      produce 50? 53? Prove your boundaries, don't just assert them.
  (c) State precisely where a timer interrupt may fall *harmlessly*, and where
      it is *dangerous* — that is, delimit the critical section in terms of the
      three instructions, and say what else must happen during the gap for an
      update to be lost.
  (d) A colleague proposes lengthening the timer interval a-hundred-fold, so
      interrupts almost never land inside the three instructions: "races will
      become vanishingly rare, which is as good as fixed." Give the two distinct
      reasons this is wrong.

**B2. Ad-hoc waiting, observed.** Run
`python3 x86.py -p wait-for-me.s -a ax=1,ax=0 -R ax -M 2000` (thread 0 gets
`%ax = 1`, thread 1 gets `%ax = 0`).
  (a) Read the assembly first. How is the memory word at address 2000 being
      used by the two threads? What is its final value?
  (b) Now swap the inputs: `-a ax=0,ax=1`. What does thread 0 spend its time
      doing, and what governs how long it does so? Try a large interrupt
      interval (e.g. `-i 1000`) and explain what changes.
  (c) This pattern — one thread spinning on a flag another thread will set — is
      exactly what ch. 27 tells you never to write. Give both of the chapter's
      reasons, and name the primitive it says to use instead.

**B3. Reading the API like a lawyer.** *(Pen and paper.)* The following worker
is joined by `main` in the usual way (`pthread_create(&p, NULL, worker, &args)`
with `myarg_t args` local to `main`, then `pthread_join(p, (void **)&res)`):

```c
pthread_mutex_t lock;            /* file scope */
long hits = 0;                   /* file scope */

void *worker(void *arg) {
    myarg_t *args = (myarg_t *) arg;
    pthread_mutex_lock(&lock);
    hits = hits + args->weight;
    pthread_mutex_unlock(&lock);
    myret_t r;
    r.x = hits;
    r.y = args->id;
    return (void *) &r;
}
```

  (a) This code contains **three** distinct defects that ch. 27 warns about by
      name. Identify each, say what actually goes wrong at runtime, and fix it.
  (b) `main` passes `&args` — the address of one of its own stack variables —
      into `pthread_create`. The worker returns the address of one of *its*
      stack variables. One of these is a defect and one is not. Explain the
      difference.
  (c) Why does `pthread_join` take a `void **` for its second argument rather
      than a `void *`?
  (d) `pthread_mutex_trylock` and `pthread_mutex_timedlock` exist, yet ch. 27
      says both "should generally be avoided". Give one situation where
      `trylock` is genuinely the right tool, and say what property of plain
      `lock` it is sacrificing to get there.

**B4. What a race detector can and cannot see.** Build the ch. 27 homework
programs and run them under `valgrind --tool=helgrind`.
  (a) Predict, before running, what helgrind will report for `main-race.c`.
      Then run it. Does the report identify the racing source lines? What else
      does it tell you?
  (b) Add a lock around **one** of the two accesses to the shared variable
      (not both), rebuild, and re-run. Is the program now correct? What does
      helgrind say, and why is that the right verdict?
  (c) `main-deadlock-global.c` acquires two locks in inconsistent order — the
      shape of a deadlock — yet the code also wraps every such acquisition in
      one additional global lock, so the deadlock can never actually occur.
      Run helgrind on it. What does it report, and what does the answer tell
      you about how a dynamic analysis tool decides to raise an alarm?
  (d) Compare `main-signal.c` (flag) with `main-signal-cv.c` (condition
      variable): run both under helgrind, then state what the CV version buys.
      Is it correctness, performance, or both? Be precise about what is wrong
      with the flag version even on runs where it produces the right output.

---

## C. Discussion and design critique

**C1. Diagnose a failure.** A production statistics daemon runs eight worker
threads. Its authors report three symptoms:

1. Under load, the nightly total is reproducibly 0.1–0.5% *below* the known
   ingest count; rebuilt with a single worker, the totals are exact.
2. When they attach a debugger and single-step the workers, the totals are
   always exact — the bug refuses to appear.
3. Roughly one shutdown in fifty, the final per-worker summary line prints
   garbage numbers, or the daemon crashes outright.

The relevant code:

```c
long processed = 0;                       /* shared, unprotected */

void *worker(void *arg) {
    summary_t s = { 0 };                  /* per-worker summary */
    while (get_request()) {
        processed = processed + 1;
        s.handled++;
    }
    return (void *) &s;
}

/* in main, at shutdown: */
for (int i = 0; i < 8; i++) {
    summary_t *sp;
    pthread_join(w[i], (void **) &sp);
    print_summary(sp);
}
```

  (a) There are two distinct defects. For each: identify it, and explain the
      **mechanism** — at the instruction level for one, at the storage-lifetime
      level for the other — by which it produces its symptom.
  (b) Explain why the debugger makes symptom 1 vanish, and why symptom 3 is
      intermittent rather than reliable.
  (c) Give the fix for each defect. For the counter, say what you would use
      this week (given ch. 27's toolbox) and what property it restores.
  (d) For each defect, name a tool from this week's reading that would have
      caught it before deployment, and say what the tool would have reported.

**C2.** A teammate asserts: "Threads exist to exploit multiple cores. On our
single-core embedded board, a multi-threaded design is pointless — one thread
is all the parallelism the hardware can deliver." Assess this claim using
ch. 26's two motivations for threads. Under what workload conditions is your
teammate actually right, and what single property of the application would you
check first to decide?

**C3.** A thread library can be implemented entirely in user space: the kernel
sees one process, and the library multiplexes several user-level threads over
it, switching between them with an ordinary function call's worth of work
(OSPP §4.6–4.8's design space). Using what you know about mode transitions (week 3)
and ch. 26's I/O-overlap motivation, give the strongest advantage of this
design and its most structural weakness. What piece of information does the
kernel have that the library cannot see, and what does the library have that
the kernel cannot see?
