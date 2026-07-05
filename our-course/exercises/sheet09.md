# Examples Sheet 9 — Concurrency II

**Attempt after Week 12.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet09-answers.md` (spoilers — attempt first).

*Extension material. Covers **concurrency II** — condition
variables, semaphores, monitors, deadlock (prevention, avoidance, and the
banker's algorithm), and read-copy-update (RCU).*

Reading: OSTEP ch. 30–34 (condition variables, semaphores, common concurrency
bugs incl. deadlock, event-based concurrency) and A&D §6.5 (deadlock — the four conditions, prevention, and
§6.5.4 the banker's algorithm, which OSTEP names but never works); McKenney's
*"What is RCU?"* intro (all week-12 reading). This sheet pairs with **Lab 5 (threads & locks,
continued into pthreads)** — several questions relate to it.

---

## A. Warm-ups (true / false, with a one-line justification)

**A1.** A semaphore initialised to 1 is the same thing as a mutex.

**A2.** A `cond_signal` delivered when no thread is waiting on the condition
variable is remembered, and wakes the next thread to call `cond_wait`.

**A3.** Deadlock and starvation are two names for the same situation: a thread
that never makes progress.

---

## B. Concurrency II: condition variables, semaphores, deadlock, RCU

*This is the material Sheet 5 (concurrency I) told you would be assessed here.*

**B1. Condition variables — a bounded buffer.**
A **bounded buffer** of capacity `N` is shared by several producer and several
consumer threads, under a single mutex `m` and two condition variables `notFull`
and `notEmpty`. Assume **Mesa** semantics: `cond_wait(cv, m)` atomically releases
`m`, blocks on `cv`, and re-acquires `m` before returning; `cond_signal` merely
marks a waiter runnable (it does *not* hand over the mutex or run the waiter
immediately).

- (a) Write pseudocode for `put(item)` and `get()` from scratch — the lock, the
  wait, the buffer update, and the signal. (Do *not* copy a fragment from
  anywhere; build it yourself.)
- (b) Your `put`/`get` must re-test the buffer condition in a **`while`** loop,
  not a one-shot `if`. Explain, with a concrete three-thread interleaving, why an
  `if` is a bug under Mesa semantics — i.e. why a thread that returns from
  `cond_wait` cannot assume the condition it waited for still holds.
- (c) Why do you need **two** condition variables here rather than one? What goes
  wrong (correctness or liveness) if producers and consumers wait on a single
  shared `cond`?
- (d) Under **Hoare** semantics the `while` could in principle be an `if`.
  State the one-sentence difference between Hoare and Mesa signalling that makes
  this so, and say why real systems (pthreads, Java, xv6's `sleep`/`wakeup`)
  nonetheless chose Mesa.

**B2. Semaphores — building a rendezvous and a barrier.**
A counting semaphore supports `P`/`wait` (decrement, block while the value is
negative) and `V`/`signal` (increment, wake one waiter).

- (a) **Two-thread rendezvous.** Thread A must not pass point `a2` until thread B
  has reached `b1`, and symmetrically B must not pass `b2` until A has reached
  `a1`. Using two semaphores initialised to 0, write the four lines. Show that
  swapping a `wait` and a `signal` in one thread can **deadlock**, and say which
  ordering is safe.
- (b) **Barrier for `n` threads.** Implement a reusable barrier: each thread
  calls `barrier()` and none returns until all `n` have arrived. Use a counter
  protected by a mutex plus one or more semaphores. State why a *naïve*
  single-semaphore barrier fails to be reusable (the "turnstile" problem), and
  repair your design so the barrier is safe to use again for a second round.
- (c) **Expressiveness.** A semaphore bundles a *counter* and a *waiting queue*
  into one primitive; a condition variable has **no memory** (a signal with no
  waiter is lost). Give one problem that is more naturally expressed with a
  semaphore than with a CV+mutex, and one that is more naturally expressed with a
  CV+mutex than with a semaphore. What is the general rule for which to reach for?

**B3. Deadlock — conditions and a banker's safety check.**

- (a) State the **four** conditions that must *all* hold for deadlock to be
  possible (Coffman conditions). For **two** of them, give a concrete technique
  that prevents deadlock by denying that condition.
- (b) A system has three resource types **A, B, C** with total instances
  **(A, B, C) = (13, 11, 9)**. Four processes hold and may still request:

  | Process | Allocation (A B C) | Maximum (A B C) |
  |:-------:|:------------------:|:---------------:|
  | P0      | 3 2 2              | 6 2 5           |
  | P1      | 3 3 2              | 3 4 4           |
  | P2      | 3 3 2              | 3 4 3           |
  | P3      | 0 0 1              | 2 2 3           |

  (i) Compute the **Available** vector and each process's **Need**, and show the
  current state is **safe** by exhibiting a safe sequence.

  (ii) Two requests arrive; consider each **independently, from the state
  above**. Request **X**: P3 asks for **(2, 2, 2)**. Request **Y**: P0 asks for
  **(1, 0, 2)**. Using the banker's algorithm, decide for each whether it can be
  granted. Exactly one is safe: grant it and give the safe sequence; for the
  other, show that granting it leaves no safe sequence.

**B4. Read-copy-update (RCU).**
- (a) RCU lets readers traverse a shared structure with **no locks and no atomic
  writes on the read side**. Sketch what a *writer* does to publish an update to
  a linked node (copy → modify the copy → single pointer swap) and why an
  in-flight reader always sees either the wholly-old or the wholly-new version,
  never a torn one.
- (b) What is a **grace period**, and what does its completion guarantee that lets
  the writer finally free the old node? Why can the old node *not* be freed the
  instant the pointer is swapped?
- (c) Why do readers "pay nothing" — what exactly is the read-side cost, and what
  does RCU **trade away** to buy that? Name one workload where RCU is the right
  tool and one where it is the wrong one.

**B5. Monitors.**
- (a) What *is* a monitor? Describe the three things it bundles — a mutex, one or
  more condition variables, and the shared state they protect — and the invariant
  it enforces about how many threads may be active *inside* it at once. Why is a
  monitor a *language-level* construct in a way a bare semaphore is not?
- (b) pthreads has no `monitor` keyword. Describe the idiom by which you build a
  monitor out of `pthread_mutex_t` and `pthread_cond_t`: what every public
  operation must do on entry and exit, and where `pthread_cond_wait` fits. Which
  signalling discipline (recall B1(d)) does the idiom hand you, and what does that
  force on every wait?
- (c) The counter below has a racy API — no locking, and `wait_until_zero()`
  busy-waits on an unsynchronised read. Rewrite it as a **monitor** in
  pseudocode: make every operation mutually exclusive, and give
  `wait_until_zero()` a correct *blocking* implementation using a condition
  variable.

  ```
  counter = 0                        # shared, no locking

  inc():             counter = counter + 1
  dec():             counter = counter - 1
  get():             return counter
  wait_until_zero(): while counter != 0: /* busy-wait */ ;
  ```

**B6. Futexes — how a real sleeping lock is built.**
*(reading: Bendersky, "Basics of Futexes"; optional depth: Drepper, "Futexes Are
Tricky".)*
When you call `pthread_mutex_lock` on a **free** lock it returns in a handful of
instructions and **never enters the kernel**; only when the lock is *contended*
must the caller block. The mechanism underneath is the **futex** ("fast userspace
mutex"): one word of user-space memory plus two kernel calls — `FUTEX_WAIT`
(sleep until this word changes) and `FUTEX_WAKE` (wake threads sleeping on this
word). The whole design rule is *kernel involvement only on contention*.

- (a) **Fast path vs slow path.** Describe what `lock()` and `unlock()` do on the
  **uncontended** path (no other thread holds or wants the lock) and why that path
  issues **no system call** — name the atomic instruction that carries it. Then
  describe the **contended** path: which futex call a would-be locker uses to go to
  sleep, and which one the owner issues as it releases the lock.
- (b) **The lost-wakeup race.** A naïve `lock()` might be: *read the futex word; if
  it reads "locked", call `FUTEX_WAIT` to sleep.* Give a two-thread, two-CPU
  interleaving in which this races with an `unlock()` so the sleeper **misses its
  wakeup and blocks forever**. Then explain the fix: `FUTEX_WAIT(addr, expected)`
  takes the value the caller *expected*, and the kernel **re-checks the word
  against `expected` under the kernel's own internal lock** before it actually
  sleeps. Why does that close the race? Relate it to the condition-variable rule in
  B1(b) and the "check-then-sleep" hazard the course flags for shared memory.
- (c) **Keeping `unlock` off the kernel too.** With a two-valued word (`0` = free,
  `1` = held) an `unlock()` cannot tell whether anyone is waiting, so to be safe it
  would have to call `FUTEX_WAKE` on **every** release — a syscall on the common
  path. Show how a **three-state** word (`0` free / `1` held, no waiters / `2` held
  with waiters) lets the uncontended `unlock()` skip the syscall entirely, and say
  who writes the `2`.
- (d) **The bridge.** A week-6 **spinlock** never enters the kernel but a contended
  waiter burns CPU spinning; the week-12 **sleeping** primitives (semaphores, CVs,
  monitors) block a waiter cleanly but pay a syscall on *every* acquire. In a
  sentence or two each, say where the futex sits between these two and why it gets
  "the best of both" for the common case. (Lab 8 Option 2 builds its "futex-lite"
  uthread mutexes on exactly this: an atomic word for the fast path, a wait queue
  for the slow path.)

---

## Past paper questions

No past-paper set is allocated to this sheet. Concurrency II — condition
variables, semaphores, monitors, deadlock and RCU — is **extension material**
with no Part IA Tripos equivalent, so this directory's `README.md` allocates no timed
paper here. Use Section B itself as the drill, and the banker's-algorithm part
(B3) as revision for the deadlock material that recurs in the final.
