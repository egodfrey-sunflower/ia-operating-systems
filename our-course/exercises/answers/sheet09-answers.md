# ⚠️ SPOILER — Examples Sheet 9 model answers ⚠️

> **STOP.** Full worked solutions. Do the sheet closed-book first. The banker's
> safety check (B3) was verified with Python; checks are noted inline.

---

## A. Warm-ups

**A1. False.** A binary semaphore initialised to 1 gives mutual exclusion, so it
*behaves* like a lock in the simple `P … V` pattern — but it is not the same
thing as a mutex. The difference is **ownership**: a mutex is owned by the thread
that locked it and **only that thread may unlock it**, whereas *any* thread may
`V` a semaphore (semaphores are a signalling primitive with no notion of an
owner). That ownership is what lets a mutex support **priority inheritance**,
error-checking (detecting a double-unlock or unlock-by-non-owner), and
recursion; a semaphore, having no owner, supports none of these and can be
posted by a thread that never held it. So "same thing" is false — same effect in
one narrow use, different semantics.

**A2. False.** Condition variables have **no memory**. A `cond_signal` (or
`cond_broadcast`) delivered when no thread is blocked on the CV is simply
**lost** — it does not accumulate a count, and a thread that calls `cond_wait`
*afterwards* blocks as though the signal had never happened. This is exactly the
contrast with a **semaphore**, whose `V` increments a value a later `P` will
observe (see B2(c)). It is *why* the awaited predicate must live in the shared
state and be re-tested under the lock in a `while` loop (B1(b)): correctness must
never depend on a thread having been waiting at the instant of the signal.

**A3. False.** Both involve a thread failing to progress, but they are distinct
failures. **Deadlock** is a *set* of threads each blocked on a resource held by
another member of the set — a circular wait (the Coffman conditions, B3(a)); no
thread in the set can ever proceed. **Starvation** (indefinite postponement) is a
*single* thread perpetually denied a resource while *other* threads keep
acquiring it and making progress — e.g. a low-priority thread under a greedy
priority scheduler, or a writer forever overtaken by a stream of readers. So
deadlock always causes the trapped threads to starve, but starvation routinely
occurs with **no deadlock** (the rest of the system is perfectly live). The fixes
differ, too: deadlock is attacked by denying a Coffman condition; starvation by
adding *fairness* (ageing, FIFO queueing).

---

## B. Concurrency II

**B1. Condition variables — bounded buffer.**

**(a)** Mesa-style pseudocode (two CVs, one mutex):

```
put(x):
    lock(m)
    while count == N:            # WHILE, not if
        cond_wait(notFull, m)
    buf[tail] = x; tail = (tail+1) % N; count++
    cond_signal(notEmpty)
    unlock(m)

get():
    lock(m)
    while count == 0:            # WHILE, not if
        cond_wait(notEmpty, m)
    x = buf[head]; head = (head+1) % N; count--
    cond_signal(notFull)
    unlock(m)
    return x
```

**(b) Why `while`, not `if`.** Under **Mesa** semantics `cond_signal` only marks
a waiter *runnable*; it does not transfer the mutex or run the waiter at once.
Between being signalled and actually re-acquiring `m`, a *third* thread can slip
in and invalidate the condition. Interleaving (buffer initially empty, one
producer Pr, two consumers C1 and C2):

1. C1 calls `get()`, sees `count == 0`, and `cond_wait(notEmpty)` — releases `m`,
   blocks.
2. Pr calls `put(x)`: `count` becomes 1, `cond_signal(notEmpty)` marks C1
   runnable, unlocks `m`.
3. Before C1 gets the CPU, C2 calls `get()`, acquires `m`, sees `count == 1`,
   takes the item: `count` back to 0, unlocks.
4. C1 finally re-acquires `m` and returns from `cond_wait`. If it used an **`if`**
   it now proceeds as though the buffer were non-empty — it reads `buf[head]`
   (**garbage / a stale slot**) and does `count--`, driving `count` to **−1**.

With a **`while`**, C1 re-tests `count == 0` on wake-up, finds it true again, and
goes back to waiting — correct. The rule: *a thread returning from `cond_wait`
must re-check its predicate, because the signal is only a hint that it is worth
looking.*

**(c) Why two condition variables.** A producer blocked on "full" and a consumer
blocked on "empty" are waiting for *different* events. With a single shared
`cond`, a `signal` meant to wake a consumer (item now available) can instead wake
a *producer* (which re-checks "full", finds the buffer still full, and goes back
to sleep) — the consumer that should have run is never woken, and the system can
**deadlock with work available** (a lost-wakeup/liveness failure). You would have
to use `cond_broadcast` (wake everyone) on every operation to be safe with one
CV, which is correct but wastefully stampedes all waiters. Two CVs route each
signal to exactly the class of waiter that can make progress.

**(d) Hoare vs Mesa.** Under **Hoare** ("signal-and-urgent-wait") semantics
`cond_signal` *immediately* transfers the mutex and the CPU to a waiter, so the
signalled thread runs while the condition provably still holds — hence an `if`
would suffice. Under **Mesa** ("signal-and-continue") the signaller keeps
running and the waiter competes for the lock later, so the condition may have
changed — hence `while`. Real systems (pthreads, Java monitors, xv6's
`sleep`/`wakeup`) chose **Mesa** because it is far simpler to implement (no
special scheduling to hand the CPU straight to the waiter, no "urgent" queue) and
composes better; the price — always loop on the predicate — is trivial and is
good defensive practice anyway (it also tolerates spurious wakeups).

**B2. Semaphores — rendezvous and barrier.**

**(a) Rendezvous.** Two semaphores `aArrived = 0`, `bArrived = 0`:

```
Thread A:            Thread B:
  ... a1 ...           ... b1 ...
  V(aArrived)          V(bArrived)
  P(bArrived)          P(aArrived)
  ... a2 ...           ... b2 ...
```

Each thread signals its own arrival, then waits for the other's — so neither
passes until both have reached their first point. **Safe ordering: signal
*before* wait.** If in thread A you swap the two lines to `P(bArrived);
V(aArrived)`, then A blocks on `bArrived` before it has posted `aArrived`; if B
has the *same* swapped order it blocks on `aArrived` first — **both wait for a
signal neither has sent: deadlock.** (Signal-then-wait cannot deadlock; the first
thread to arrive posts and then blocks, and the second unblocks it.)

**(b) Reusable barrier for `n` threads.** Use a counter under a mutex and a
*two-turnstile* structure:

```
count = 0; mutex = 1; turnstile1 = 0; turnstile2 = 1

barrier():
  # phase 1
  P(mutex); count++; if count == n: { P(turnstile2); V(turnstile1) }; V(mutex)
  P(turnstile1); V(turnstile1)          # let all n through, one releasing the next
  # phase 2
  P(mutex); count--; if count == 0: { P(turnstile1); V(turnstile2) }; V(mutex)
  P(turnstile2); V(turnstile2)
```

A **naïve single-semaphore** barrier (last arrival does `n-1` `V`s on one
"go" semaphore) is **not reusable**: a fast thread can race around the loop and
consume a `V` intended for the *next* round, so on the second use some thread is
left permanently blocked (the "turnstile" leaks). The **two-phase** design closes
turnstile 2 before opening turnstile 1 (and vice-versa), so no thread can re-enter
phase 1 until every thread has left phase 2 — the barrier resets cleanly each
round.

**(c) Expressiveness.** A **semaphore** is more natural when you are *counting a
resource* and a post may arrive **before** anyone waits — e.g. a producer/consumer
count of available items, or "N permits" throttling: the semaphore *remembers*
posts (its value), so no wakeup is lost. A **CV+mutex** is more natural when
threads wait on an **arbitrary predicate over shared state** protected by a lock —
e.g. "wait until the queue is non-empty *and* the writer is idle": you can test
any boolean of the protected data under the mutex, which a bare semaphore's single
integer cannot express. **General rule:** reach for a **semaphore** when the
condition is exactly "a count reached zero / a resource is available" and you need
signals to be *remembered*; reach for **CV+mutex** when the wait condition is a
richer predicate over data you already protect with a lock (and you accept that a
signal with no waiter is lost, which is why the predicate is re-checked in a
loop).

**B3. Deadlock — conditions and banker's check.** *(Banker's numbers verified in
Python; results inline.)*

**(a) Four (Coffman) conditions**, all required simultaneously:
1. **Mutual exclusion** — a resource is held in a non-shareable mode.
2. **Hold-and-wait** — a process holds one resource while waiting for another.
3. **No preemption** — a resource is released only voluntarily by its holder.
4. **Circular wait** — a cycle of processes each waiting for the next's resource.

Two prevention techniques (deny a condition):
- Deny **hold-and-wait**: require a process to request *all* its resources at once
  (or release everything before requesting more) — it never holds while waiting.
- Deny **circular wait**: impose a **total order** on resource types and require
  requests in increasing order — a cycle is then impossible.

(Also acceptable: deny *no-preemption* by allowing the OS to revoke and roll
back; deny *mutual exclusion* by making resources shareable/spooled where
possible.)

**(b) Banker's algorithm.** Totals `(A,B,C) = (13, 11, 9)`.

**(i) Available and Need.** Sum of Allocation = `(9, 8, 7)`, so
**Available = (13,11,9) − (9,8,7) = (4, 3, 2)**. `Need = Max − Allocation`:

| Process | Allocation | Max | Need |
|:-------:|:----------:|:---:|:----:|
| P0 | 3 2 2 | 6 2 5 | **3 0 3** |
| P1 | 3 3 2 | 3 4 4 | **0 1 2** |
| P2 | 3 3 2 | 3 4 3 | **0 1 1** |
| P3 | 0 0 1 | 2 2 3 | **2 2 2** |

The state is **safe**. Safe sequence **⟨P1, P0, P2, P3⟩** (verified):
- Work `(4,3,2)`: P1 Need `(0,1,2) ≤ (4,3,2)` → run, free `(3,3,2)` → Work `(7,6,4)`.
- P0 Need `(3,0,3) ≤ (7,6,4)` → run, free `(3,2,2)` → Work `(10,8,6)`.
- P2 Need `(0,1,1) ≤ (10,8,6)` → run, free `(3,3,2)` → Work `(13,11,8)`.
- P3 Need `(2,2,2) ≤ (13,11,8)` → run, free `(0,0,1)` → Work `(13,11,9)`. All finish.

**(ii) The two requests.**

*Request X — P3 asks `(2,2,2)`.* Check `(2,2,2) ≤ Need[P3] = (2,2,2)` ✓ and
`(2,2,2) ≤ Available = (4,3,2)` ✓. Tentatively grant: Allocation[P3] → `(2,2,3)`,
Available → `(2,1,0)`, Need[P3] → `(0,0,0)`. Safety search: P3 Need `(0,0,0) ≤
(2,1,0)` → run, frees `(2,2,3)` → Work `(4,3,3)`; then P0 `(3,0,3) ≤ (4,3,3)` →
`(7,5,5)`; P1 `(0,1,2)` → `(10,8,7)`; P2 `(0,1,1)` → `(13,11,9)`. **SAFE — grant
it.** Safe sequence **⟨P3, P0, P1, P2⟩**.

*Request Y — P0 asks `(1,0,2)`.* Check `(1,0,2) ≤ Need[P0] = (3,0,3)` ✓ and
`(1,0,2) ≤ Available = (4,3,2)` ✓. Tentatively grant: Allocation[P0] → `(4,2,4)`,
Available → `(3,3,0)`, Need[P0] → `(2,0,1)`. Now check each process against
Available `(3,3,0)`:
- P0 Need `(2,0,1)`: C-component 1 > 0 → **blocked**.
- P1 Need `(0,1,2)`: C 2 > 0 → **blocked**.
- P2 Need `(0,1,1)`: C 1 > 0 → **blocked**.
- P3 Need `(2,2,2)`: C 2 > 0 → **blocked**.

Every remaining Need requires at least one more **C**, but Available C has fallen
to **0** and no process can release any until it finishes — so no process can
proceed. **No safe sequence exists → UNSAFE — deny (P0 must wait).**

*Python verification:* base state safe (⟨P1,P0,P2,P3⟩); request X safe
(⟨P3,P0,P1,P2⟩); request Y unsafe (grant leaves Available `(3,3,0)` with every
Need's C-component positive, deadlocking). Totals `(13,11,9)` and Available
`(4,3,2)` are deliberately different from the final exam's banker instance.

**B4. RCU.**

**(a) Writer publishes by pointer swap.** To update a node the writer
**copies** it, **modifies the copy**, then does a **single atomic pointer store**
that swings the parent's pointer from the old node to the new one (with a memory
barrier so the new node's contents are visible before the pointer). Because a
reader dereferences that pointer in one load, it sees either the old pointer
(→ wholly-old node) or the new pointer (→ wholly-new node) — **never a
half-updated node**, since the reader never observes the writer's private copy
mid-edit.

**(b) Grace period.** After the swap, some readers may *still hold a reference*
to the old node (they loaded the pointer just before the swap). A **grace
period** is an interval guaranteed to be long enough that **every reader that
existed at the moment of the swap has finished its read-side critical section**.
Its completion guarantees no reader can still be looking at the old node, so the
writer may **now free it**. It cannot be freed the instant the pointer is swapped
because a pre-existing reader could still be traversing it — freeing then would be
a use-after-free.

**(c) Cheap reads, and the trade.** Read-side cost is essentially **zero**: no
lock, no atomic RMW, no cache-line contention — just marking entry/exit of a
read-side critical section (often a no-op, or merely disabling preemption). RCU
buys this by pushing *all* the cost onto the **writer** and onto **reclamation**:
writers must copy, order the publish, and then **wait out a grace period** (high
write latency), and the scheme only works where readers can tolerate seeing
*slightly stale* data during the grace period. **Right tool:** read-mostly,
rarely-written shared structures where staleness is acceptable — e.g. routing
tables, the Linux dentry cache, module/notifier lists. **Wrong tool:**
write-heavy or read-modify-write workloads, or where readers must see the very
latest value synchronously (the grace-period latency and copy cost dominate, and
a plain lock or seqlock is better).

**B5. Monitors.**

**(a) What a monitor is.** A **monitor** is an abstract data type that bundles
three things: (1) some **shared state**, (2) a **mutex** guarding it, and (3) one
or more **condition variables** on which threads wait for predicates over that
state. Its defining invariant is **mutual exclusion over the monitor**: *at most
one thread is active inside the monitor at any instant.* Every public operation
implicitly acquires the monitor lock on entry and releases it on exit; a thread
parked in `cond_wait` has released the lock and does *not* count as active. It is
a **language-level** construct because that mutual exclusion is *automatic and
encapsulated* — the compiler/runtime wraps each method in lock/unlock and the
protected state is reachable *only* through the monitor's operations. A bare
**semaphore** is an unstructured primitive by comparison: nothing forces a caller
to `P` before touching the data or to `V` afterwards, so correctness rests
entirely on programmer discipline. (Real monitors: Java `synchronized` methods
with `wait`/`notify`; the original Mesa monitors.)

**(b) The pthreads idiom.** pthreads gives you the *ingredients*, not the
keyword, so you build a monitor **by convention**: place a `pthread_mutex_t m`
and the needed `pthread_cond_t`s inside the object's struct next to the shared
state, and make **every** public operation
- call `pthread_mutex_lock(&m)` as its first act and `pthread_mutex_unlock(&m)`
  on every return path;
- touch the shared state *only* while holding `m`;
- wait for a predicate with `while (!predicate) pthread_cond_wait(&cv, &m);` —
  `cond_wait` atomically releases `m`, blocks, and re-acquires `m` before
  returning;
- signal a state change with `pthread_cond_signal`/`broadcast` before unlocking.

pthreads condition variables give you **Mesa** ("signal-and-continue") semantics
(B1(d)): the signaller keeps running and the woken waiter merely becomes
runnable, competing for `m` later. That forces **every wait to sit in a `while`
loop re-testing the predicate** — never a one-shot `if` — because the predicate
may have been falsified again before the waiter reacquires the lock (the loop
also absorbs spurious wakeups).

**(c) The counter as a monitor.**

```
monitor Counter:
    int   counter = 0
    mutex m
    cond  isZero               # signalled when counter reaches 0

    inc():
        lock(m)
        counter = counter + 1
        unlock(m)

    dec():
        lock(m)
        counter = counter - 1
        if counter == 0:
            cond_broadcast(isZero)      # wake any waiters
        unlock(m)

    get():
        lock(m)
        v = counter
        unlock(m)
        return v

    wait_until_zero():
        lock(m)
        while counter != 0:             # WHILE, not if (Mesa)
            cond_wait(isZero, m)         # releases m, blocks, re-acquires m
        unlock(m)
```

Every operation now runs under `m`, so `inc`/`dec`/`get` are mutually exclusive —
no lost updates on the read-modify-write of `counter`, no torn read in `get`.
`wait_until_zero` no longer busy-waits: it **blocks** on the condition variable
and is woken only when `dec` drives the count to zero. The `while` re-tests the
predicate under the lock, so an `inc` slipping in between the signal and the
waiter's wake-up cannot let it return with a non-zero count. (`cond_broadcast` is
used because several threads may be waiting; a single expected waiter could use
`cond_signal`.)

**B6. Futexes — how a real sleeping lock is built.**

**(a) Fast path vs slow path.** The futex is a single word of user-space memory
(say `0` = free, `1` = held).

- *Uncontended.* `lock()` is an atomic **compare-and-swap**: `CAS(word, 0→1)`. If
  the word was `0` it is now `1` and `lock()` returns — the whole operation is one
  atomic instruction in user space, **no syscall**. `unlock()` is symmetric: an
  atomic store (or `CAS(1→0)`) of `0`, again no syscall. Nothing enters the kernel
  because there was nothing to wait for.
- *Contended.* The `CAS` fails because the word is already `1`. The would-be locker
  then calls **`FUTEX_WAIT(word, expected)`** to block in the kernel until the word
  changes. When the owner finally releases, it issues **`FUTEX_WAKE(word, 1)`** to
  wake one sleeper, which re-tries the `CAS`.

So the kernel is touched **only on contention** — exactly the design rule.

**(b) The lost-wakeup race.** Take the naïve `lock()` = "read word; if it reads
`1`, `FUTEX_WAIT`." Let T2 hold the lock and T1 want it, on separate CPUs:

1. T1 reads `word == 1` (locked) and *decides* to sleep — but has not yet made the
   syscall.
2. Before T1's syscall lands, T2 runs `unlock()`: it stores `word = 0` and calls
   `FUTEX_WAKE`. No thread is queued on the futex yet, so the wake finds **no
   waiter** and does nothing.
3. T1 now calls `FUTEX_WAIT(word, …)` and goes to sleep — on a lock that is
   actually **free**, with no future wake coming. T1 blocks indefinitely. That is a
   **lost wakeup**.

The gap between "checked the value" and "went to sleep" is the whole problem. The
fix is that `FUTEX_WAIT` carries the value T1 *expected* the word to still hold
(here `1`): the kernel takes its internal per-futex (hash-bucket) lock, compares
`*word` against `expected`, and **only sleeps if they still match** — otherwise it
returns `EAGAIN` immediately without enqueuing. `FUTEX_WAKE` takes the *same*
internal lock, so the compare and the wake cannot interleave. Replaying step 3:
the kernel holds its lock, reads `word == 0 ≠ expected 1`, returns `EAGAIN`; T1
does **not** sleep, loops back, and re-`CAS`es the now-free lock. The atomic
"re-check the value then decide to block, under a lock the waker must also take"
is what closes the window.

This is the *same shape* as B1(b): testing a predicate ("word looks locked" /
"buffer looks empty") and then blocking is unsafe if the state can change in the
gap — the check and the decision-to-sleep must be atomic with respect to the
wakeup. A condition variable achieves it by re-testing the predicate under the
**mutex** in a `while` loop; a futex achieves it by having the **kernel** re-test
the value under its **own** lock. It is precisely the "check-then-sleep" hazard the
course flags for shared-memory signalling, solved once in the kernel for everyone.

**(c) Keeping `unlock` off the kernel too.** With only `0`/`1`, `unlock()` has no
way to know whether a waiter exists, so a *safe* `unlock()` would have to call
`FUTEX_WAKE` on **every** release — a syscall on the uncontended path, wrecking the
whole point. The standard cure is a **three-state** word:

- `0` = free, `1` = held with **no** waiters, `2` = held **with** waiters.

A locker that fails its `CAS` and is about to sleep first sets the word to **`2`**
(so it records "there is now a waiter") and then `FUTEX_WAIT`s expecting `2`.
`unlock()` atomically takes the word back to `0` (an exchange, or Drepper's
decrement) and inspects the old value: if it was `1` (held, no waiters) it simply
returns — **no syscall**; only if it was `2` does it call `FUTEX_WAKE`. Hence an uncontended release makes no syscall,
and the kernel is entered on unlock *only* when a waiter genuinely needs waking —
"kernel involvement only on contention" now holds for the release side as well.
(This three-state dance is the "tricky" part Drepper's note is about.)

**(d) The bridge.**

- A **week-6 spinlock** never enters the kernel, but a contended waiter **burns
  CPU** spinning — fine when the lock is held for a few instructions, ruinous when
  the wait is milliseconds.
- A **pure kernel sleeping lock** (a plain semaphore/CV/monitor) blocks a waiter
  cleanly, but pays a **syscall on every acquire and release**, even when the lock
  is free — wasteful precisely because most acquisitions are uncontended.
- The **futex sits between them**: the fast path is a user-space atomic (as cheap
  as a spinlock, no kernel), and it *falls through* to a kernel **sleep** — block,
  don't spin — only when actually contended. So the common case costs one `CAS`,
  and the rare contended case sleeps in the kernel instead of burning a core. That
  "atomic in user space, syscall only on contention" is exactly what **Lab 8
  Option 2**'s futex-lite uthread mutexes implement: an atomic word for the fast
  path, a wait queue for the slow path.

---

*Python verification summary:* base banker state safe (⟨P1,P0,P2,P3⟩); request X
safe (⟨P3,P0,P1,P2⟩); request Y unsafe.
