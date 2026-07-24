> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 14 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. TRUE.** This is ch. 31's stated rule of thumb. A lock is a resource you
can hand out once immediately → init 1. Ordering (parent waits for child) has
nothing to give away until the event happens → init 0. A buffer of `MAX`
empty slots → `empty` starts at `MAX`, `full` at 0. A throttle admitting `k`
threads → init `k`.

**A2. FALSE.** That invariant belongs to Dijkstra's *definition*; it is not
universal in implementations. OSTEP's own Zemaphore — a semaphore built from
a lock and a CV — never lets the value go below zero, and the chapter notes
Linux behaves the same way. The invariant is a useful mental model, not a
portable observable.

**A3. TRUE.** The semaphore's integer *is* state: an early `post` increments
the value, and a later `wait` finds it positive and sails through. This is
exactly the fork/join example — it works whichever of parent and child runs
first. A condition variable, being stateless, loses the early signal, which
is why CVs must be paired with an explicit state variable (week 13, A3).

**A4. TRUE.** The four Coffman conditions are individually necessary, so
defeating any one prevents deadlock — and ch. 32's prevention menu is
organised exactly this way: lock ordering kills circular wait, acquiring all
locks under a master lock kills hold-and-wait, `trylock` back-off
approximates preemption, and lock-free structures remove mutual exclusion.
(Justifications that merely restate the four conditions without the
"individually necessary" point earn little.)

**A5. FALSE.** Lu et al. found the opposite: 74 of 105 bugs (about 70%) were
*non*-deadlock, and 97% of those were atomicity or order violations. The
implication — tools and care should target those two simple patterns first —
is the study's headline (see C1).

**A6. FALSE.** An atomicity violation is about a *region*, not an access. In
the MySQL example, the check `if (thd->proc_info)` and the use
`fputs(thd->proc_info, …)` must sit inside **one** critical section; locking
each access separately still allows the other thread's `NULL` store to land
in the gap between them. The fix is a lock held across check-and-use — which
is why "just lock every access" tools miss these bugs.

**A7. FALSE.** It removes deadlock but introduces **livelock**: two threads
can each grab their first lock, fail the trylock, release, retry, and collide
again indefinitely — running forever without progressing. (Random back-off
makes this unlikely, not impossible.) Progress failure survives the fix; it
just changes species. Also worth noting: backing out is only simple if
nothing else — memory, partial state — was acquired along the way.

**A8. FALSE.** Ch. 32 is blunt: avoidance via scheduling is "only useful in
very limited environments" — you need advance knowledge of every task's
maximum claims, and the conservatism costs concurrency. Kernels instead use
prevention by lock ordering (Linux's mm code documents ten lock-order
groups) plus debugging tools. Banker's-style avoidance lives in embedded
systems with static task sets (B4f).

---

## B. Working the mechanisms

**B1.**
**(a)** Consumer first: `wait(mutex)` (1→0, succeeds), then `wait(full)`
(0→−1, **blocks — holding the mutex**). Producer: `wait(mutex)` (0→−1,
blocks). Now the consumer holds `mutex` and waits for `full`, which only the
producer can post; the producer waits for `mutex`, which only the consumer
can release. At that moment: **mutual exclusion** (the binary mutex),
**hold-and-wait** (consumer holds `mutex` while waiting on `full`),
**no preemption** (nothing can seize the mutex back), **circular wait**
(consumer → `full` → producer → `mutex` → consumer). All four hold; deadlock.

**(b)** The fix removes **hold-and-wait**: with the mutex innermost, no
thread ever sleeps on `empty`/`full` while holding the mutex, and the
critical section it does hold the mutex for contains no waits at all. The
`empty`/`full` waits outside cannot form a cycle because a thread waiting on
them **holds nothing** — a cycle needs every participant to hold something
another participant wants.

**(c)** Not needed. With `MAX = 1`, one producer and one consumer, `empty`
and `full` enforce strict alternation: the consumer cannot enter `get()`
until the producer's `post(full)` — which happens *after* `put()` completes —
and the producer cannot re-enter `put()` until the consumer's `post(empty)`
after `get()` completes. `put()` and `get()` can therefore never overlap,
and there is no race for the mutex to prevent. (The chapter's race needs
multiple producers or consumers racing on `fill`/`use`.)

**(d)** Two semaphores, `aArrived = 0`, `bArrived = 0`:

```
A: post(aArrived); wait(bArrived);
B: post(bArrived); wait(aArrived);
```

Post-then-wait. The reversed order (wait-then-post) deadlocks: each waits
for a post the other hasn't made. Credit for noting the semaphore's memory
(A3) is what makes this work regardless of arrival order.

**B2.**
**(a)** R1 acquire-read: `lock` 1→0→1 around `readers` 0→1; first reader, so
`wait(writelock)` 1→0. R2 acquire-read: `readers` 1→2; `writelock`
untouched. W1 acquire-write: `wait(writelock)` 0→−1 — **W1 blocks here, on
`writelock`**, which the *reader group* logically holds via the first
reader's acquisition. R1 release: `readers` 2→1, nothing else. R2 release:
`readers` 1→0 — last reader — `post(writelock)` −1→0, waking W1, which now
holds the write lock.

**(b)** While `readers ≥ 1`, an arriving reader only increments `readers`
and never touches `writelock`. So: R1 in; W1 blocks on `writelock`; R2
arrives and is admitted; R1 leaves; R3 arrives; R2 leaves; … — as long as
reader arrivals overlap, `readers` never reaches 0, `post(writelock)` never
happens, and W1 waits forever. Starvation with every thread making progress
except one.

**(c)** A waiting writer must be able to **bar the door to readers that
arrive after it**: e.g. a turnstile the writer seizes on arrival — arriving
readers must pass through it and so queue behind the writer, while readers
already inside drain out. When `readers` hits 0 the writer proceeds. (Any
design expressing "new readers wait once a writer waits" earns the marks.)

**(d)** (i) Overhead: every read acquire/release takes the internal `lock`
semaphore — two extra atomic operations and a shared cache line hammered by
all readers, so the "concurrent" readers still serialise on the
bookkeeping (week 12's coherence-cost lesson). (ii) Short critical sections:
if the read section is a few instructions, a plain lock's hold time is
already tiny and the rwlock's extra machinery outweighs any concurrency
won. OSTEP's Hill's-Law aside — simple and dumb often wins — is the theme;
full credit for either concrete mechanism plus the workload condition
(long read sections, high read ratio) under which the rwlock *does* pay.

**B3.**
**(a)** Each philosopher p executes `wait(forks[p])` (left) and then blocks
at `wait(forks[(p+1)%5])`: P0 holds f0 wanting f1, P1 holds f1 wanting f2,
P2 holds f2 wanting f3, P3 holds f3 wanting f4, P4 holds f4 wanting f0.
Mutual exclusion: each fork is a binary semaphore. Hold-and-wait: everyone
holds one fork, waiting for another. No preemption: forks can't be seized.
Circular wait: f0→f1→f2→f3→f4→f0. Deadlock.

**(b)** Number the forks. P0–P3 acquire fork p then p+1: ascending. P4
acquires fork 0 then fork 4: also ascending. So **every philosopher acquires
forks in globally ascending numerical order**. A waiting cycle would require
some philosopher to hold a higher-numbered fork while waiting for a
lower-numbered one — impossible under ascending acquisition. No cycle, no
deadlock. This is ch. 32's total-lock-ordering prevention, discovered a
chapter early.

**(c)** Two. Five forks, two per eater: ⌊5/2⌋ = 2, e.g. P0 (f0,f1) and
P2 (f2,f3). The ordering fix changes only acquisition *order*, not which
fork sets are needed, so the maximum concurrency is unchanged at 2.

**B4.**
**(a)** Allocated column-sums: (4, 4, 5), so
**Available = E − allocated = (2, 1, 2)**.
Need = Max − Allocation:

| | Need (A,B,C) |
|---|---|
| P1 | (2, 1, 3) |
| P2 | (2, 1, 1) |
| P3 | (2, 1, 2) |
| P4 | (2, 2, 2) |

**(b) Safe.** From Available (2,1,2): P1 needs C = 3 — no; P4 needs B = 2 —
no; **P2** (2,1,1) fits, and so does **P3** (2,1,2) — either may go first.
Taking P2: it runs to completion and returns its allocation:
Available = (2,1,2) + (2,1,1) = **(4,2,3)**. Now P1 (2,1,3) fits:
→ (4,2,3) + (1,1,2) = **(5,3,5)**. P3 fits: → **(6,3,7)**. P4 fits:
→ **(6,5,7)** = E. ✓ Safe sequence **⟨P2, P1, P3, P4⟩** (⟨P3, …⟩ variants
equally correct).

**(c) Grant.** Checks: request (1,0,1) ≤ Need_P2 (2,1,1) ✓; ≤ Available
(2,1,2) ✓. Pretend-grant: Available (1,1,1); P2's Allocation (3,1,2), Need
(1,1,0). Safety: P2's new Need (1,1,0) ≤ (1,1,1) ✓ — P2 completes →
Available (1,1,1) + (3,1,2) = (4,2,3); then P1 → (5,3,5); P3 → (6,3,7);
P4 → (6,5,7). Safe, so **granted**.

**(d) Deny.** Checks: (1,0,1) ≤ Need_P4 (2,2,2) ✓; ≤ Available (2,1,2) ✓.
Pretend-grant: Available (1,1,1); P4's Allocation (1,2,1), Need (1,2,1).
Safety scan against (1,1,1): P1 (2,1,3) — no; P2 (2,1,1) — A short; P3
(2,1,2) — A short; P4 (1,2,1) — B short. **No process can complete**: the
state is unsafe, so the request is **refused** and P4 must wait, even though
resources are physically available.

**(e)** After the pretend-grant in (d) nothing is deadlocked — every process
is runnable and may never ask for its maximum. **Unsafe** means: *there
exists* a future in which every process demands its full declared Max and no
completion order can satisfy them — deadlock becomes possible, not actual.
**Deadlocked** means the cycle exists now. The Banker refuses to enter
states where the *worst legal future* jams; it insures against declared
maxima all being exercised at the worst moment, at the price of refusing
requests that would usually have been fine. That conservatism is the
concurrency cost named in ch. 32.

**(f)** (i) Every process's **maximum claim, declared in advance**; (ii) a
**closed, known population** of processes and resources (the algorithm has
no answer for tasks that appear unannounced). Both hold in **embedded /
hard-real-time systems** with static task sets — precisely where OSTEP says
avoidance survives — and neither holds in a general-purpose OS.

---

## C. Discussion and design critique

**C1.**
**(a)** Build the atomicity/order-violation detector, not the deadlock
detector. 70% of the bugs are non-deadlock, and 97% of *those* fall into
just two shallow, mechanically recognisable patterns: a check and use of the
same shared location not covered by one lock (atomicity), and an access that
assumes another thread's prior write with no enforcing synchronization
(order). Narrow patterns + majority share = the highest bugs-found-per-
engineering-hour. Deadlock may be *ignored* by the tool not because it is
unimportant but because it is already served: prevention by lock ordering is
a discipline, not a detector, and cycles are loud when they happen. (Credit
for the sharper point: the taxonomy's value is exactly that it licenses
tools to be narrow.)

**(b)** The fixes add a CV wait for "A has happened". Skip the **state
variable** (week-13 rule 4) and the fix inherits the lost-wakeup bug: if A's
`signal` fires before B reaches `wait`, the signal evaporates — stateless
CVs — and B sleeps forever. The ch. 32 ordering fix is careful to set
`mtInit = 1` under the lock and have B check `while (mtInit == 0)`: state,
lock, `while` — all three rules, or the cure is a new disease.

**(c)** All four subjects are large *application* codebases, mostly
event-driven or request-parallel, written to portable thread APIs. A kernel
differs in ways that plausibly shift the mix: pervasive fine-grained locking
with documented lock orders (fewer naive deadlocks, or more?), interrupt
context (a bug class the study cannot contain at all), and heavy use of
per-CPU data. To check: mine the kernel's own fixed-bug history — commits
tagged as races/deadlocks — and classify a sample with the same taxonomy;
compare proportions. (Any concrete structural difference plus a
measurement-over-assertion answer earns the marks.)

**C2.** A strong answer separates the claims.

**Are declared maxima realistic?** No — and ch. 32 explains why:
**encapsulation**. A kernel call path's lock set depends on what its callees
lock (the Java `Vector.AddAll()` lesson: the locks are invisible at the call
site). VM calls into the FS which calls back into VM; the honest "maximum
set of locks" for a syscall approaches "most of the kernel", and safe
over-approximation is exactly what makes the Banker maximally conservative.
The declarations would rot faster than the ordering docs they replace.

**Hot-path cost.** A modern uncontended lock acquire is a single atomic
instruction — the futex fast path never enters the kernel (week 12). The
proposal replaces that with a safety check over P processes × R locks,
executed under a lock on the *central allocator's own state* — a global
serialisation point touched by every acquire on every core. That is an
Amdahl bottleneck installed at the hottest site in the kernel; the cure
costs orders of magnitude more than the disease.

**Conservatism.** With over-approximated claims, the safe-state test
refuses vast numbers of harmless acquisitions (B4d in miniature,
everywhere): concurrency collapses toward serial execution — the same
lesson as ch. 32's static-scheduling example, where deadlock-freedom was
bought with idle processors.

**Auditability.** The lock *order* is not just a deadlock widget; it is
documentation of design intent that reviewers check locally ("this function
may not call that one while holding X"). Replacing a locally checkable
discipline with a global runtime oracle makes every "why was this acquire
refused?" a whole-system question. The proposal deletes the very artifact
that made reasoning modular.

**Where it earns its keep.** Closed systems with static task sets and
genuinely known claims — embedded and hard-real-time — where the check runs
over a handful of tasks and refusal-conservatism is a price willingly paid
for a no-deadlock guarantee. That is where avoidance already lives.

**Recommendation.** Reject for a general-purpose kernel. Keep prevention by
lock ordering (address-order for peer locks; documented hierarchy
otherwise) and add *detection tooling* in debug builds — building the
acquisition graph and flagging cycles — which finds ordering violations
without taxing the hot path. Flip conditions: a small, static, certifiable
system; a lock population small enough that claims are exact; or a domain
where a hard no-deadlock guarantee is a requirement rather than a
preference.

*Marking note: the four load-bearing points are encapsulation vs
declarations, the centralised hot-path cost, conservatism, and the loss of
local auditability. An answer that only says "Banker's is slow" earns
little.*

**C3.** **Against semaphores-only:** (i) the value conflates two meanings —
mutual exclusion and event counting — so intent disappears from the code;
a reader cannot tell a lock from an ordering constraint without finding the
`init`. (ii) There is no broadcast: waking all waiters requires knowing how
many there are — the covering-condition pattern (week 13, B4) has no
natural encoding. (iii) The waiter cannot *re-check a predicate*: a
semaphore hands you permission, not truth, so complex conditions
("`bytesLeft ≥ size`") must be dismantled into counting form, which is
exactly where the classic puzzles get hard. (iv) The empirical point ch. 31
cites: building CVs from semaphores misled even Birrell-class experts —
if the primitive's own experts trip on it, average users will. **For:** one
small orthogonal primitive; posts are never lost (no lost-wakeup class);
locks, ordering, throttling and barriers all fall out of one mechanism;
minimal API surface to implement and teach. **Ship:** locks + condition
variables as the primary API — they match what code means to say and carry
30 years of idiom (`while`-loop waiting) that survives Mesa semantics and
spurious wakeups — with semaphores alongside for counting and throttling.
What flips it: an audience of a few expert systems programmers on a tiny
embedded runtime, where API minimality beats expressiveness and the
discipline can be enforced by review. *Marking note: the question asks for
a judgement; either shipping decision earns full marks if the user
condition attached to it is doing real work.*
