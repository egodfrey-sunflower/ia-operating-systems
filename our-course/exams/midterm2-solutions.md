> # ⚠️ SPOILER — MODEL ANSWERS AND MARK SCHEME ⚠️
> ## Do NOT read this until you have sat Midterm 2 under timed conditions.
> Reading it first destroys the only honest measurement you will get.

---

# Midterm 2 — Solutions and Mark Scheme

General guidance: 1 mark ≈ one distinct made point. Prose parts: a vague
gesture at the right idea is half a mark, not a mark. Calculation parts: full
method with an arithmetic slip keeps most marks; a bare correct number earns
at most half. Derivations asked for "with a one-line reason" earn nothing for
the number alone.

---

## Question 1 — Concurrency

### (a) Lu's taxonomy [4 marks]

- **Atomicity violation:** a code region intended to execute as a unit —
  typically a check of shared state followed by a use of it — is interleaved
  by another thread between the two. Sketch: T1 tests `if (p != NULL)`;
  T2 sets `p = NULL`; T1 dereferences `p`. Each access may even be locked
  individually; the *pair* is not. **[1.5]**
- **Order violation:** the code assumes A happens before B but nothing
  enforces it. Sketch: T1 spawns T2, which uses `state`; T1 initialises
  `state` *after* the spawn, and T2 occasionally runs first. **[1.5]**
- Together these covered **97% of the study's 74 non-deadlock bugs**. **[0.5]**
  Implication: a tool that detects just these two patterns — check/use pairs
  broken up, and unenforced orderings — addresses nearly all non-deadlock
  concurrency bugs; exotic bug classes can be ignored at first order. **[0.5]**

### (b) The broken bounded buffer [8 marks]

**(i) [2 marks]** (1) The waits use **`if`, not `while`** — a woken thread
never rechecks its condition. (2) **One condition variable serves two
different conditions** ("buffer not full" and "buffer not empty"), so a signal
can wake the wrong kind of thread. *(Both needed for 2; one earns 1.)*

**(ii) [3 marks]** One producer P, consumers C1 and C2, buffer empty:

1. C1: lock; `count == 0` → `wait` (releases the lock, sleeps). **[½]**
2. P: lock; buffer not full → `put()`, `count = 1`; `signal` — C1 is moved to
   ready **but holds nothing and runs later**; P unlocks. **[1]**
3. C2: acquires the lock first; `count == 1`, so its `if` falls through;
   `get()` → `count = 0`; signals; unlocks. **[1]**
4. C1 finally runs: it reacquires the lock and resumes *after* its `wait` —
   the `if` was already passed, so it calls `get()` with `count == 0`.
   Underflow. **[½]**

The staleness moment: between step 2 (told "there is an item") and step 4
(acting on it) the world changed; with `if`, C1 never looks again.

**(iii) [3 marks]** Replace both `if`s with **`while`** loops around the wait
**[1]**; split the CV into two — producers wait on `empty` and signal `fill`,
consumers wait on `fill` and signal `empty` (a single CV with `broadcast` is
also correct, at a wakeup cost) **[1]**. The rule: under **Mesa semantics** a
signal is a *hint* — it moves a waiter to ready but transfers neither the lock
nor the truth of the condition; by the time the waiter runs, anything may have
happened, so waits must re-test in a loop. (Under Hoare semantics the `if`
would be sound; no mainstream system implements it.) **[1]**

### (c) Semaphores [4 marks]

`empty` initialised to **MAX** (free slots), `full` to **0** (items),
`mutex` to **1**. **[1]**

- `put`: `wait(empty); lock(mutex); add; unlock(mutex); post(full);`
- `get`: `wait(full); lock(mutex); remove; unlock(mutex); post(empty);` **[1]**

Mutex-first deadlock, buffer empty: consumer takes `mutex`, then waits on
`full` — sleeping **while holding the mutex**. Producer runs: `wait(empty)`
succeeds, then blocks on `mutex`. Now the consumer waits for a `post(full)`
only the producer can perform, and the producer waits for a mutex only the
consumer holds — a cycle; deadlock. **[2 — interleaving 1, the cycle stated 1]**

*Marking note:* the point of the ordering is that `empty`/`full` waits can
sleep for unbounded time and so must never be inside the mutual-exclusion
region.

### (d) Priority inversion [4 marks]

**(i) [2 marks]** **Priority inversion.** The scheduler is correct by its own
lights: H is *blocked* (not runnable), and M is the highest-priority runnable
thread, so running M is what fixed priorities demand. What it cannot see is
that **H's progress depends on L** — lock ownership is not scheduler state, so
the dependency H→L is invisible, and M starves L, which transitively starves
H.

**(ii) [2 marks]** Any two, mechanism + drawback (1 each):

- **Priority inheritance:** while L holds a lock that H is blocked on, L runs
  at H's priority; on release it reverts. Drawback: implementation complexity
  (inheritance must chase chains of nested locks) and extra work on the
  lock's slow path.
- **Priority ceiling** (the Mesa/Lampson–Redell form: each monitor carries the
  priority of its highest-priority user, and any holder runs at that ceiling):
  simple and pre-emptive of the problem. Drawback: needs the ceiling known in
  advance, and elevates L even when no high-priority thread is waiting —
  unnecessary priority distortion.
- Also creditable: never share locks across priority levels (restructure);
  disable preemption during critical sections (drawback: global latency hit);
  make the shared structure lock-free (drawback: difficulty and limited
  applicability).

---

## Question 2 — I/O and RAID

### (a) Bookwork [4 marks]

**(i) [2]** **Seek** (move the arm), **rotational delay** (wait for the sector
to come around), **transfer** (read it as it passes). For small random
requests, **positioning — seek + rotation — dominates**; transfer is
negligible.

**(ii) [2]** **Interrupts** when the device is slow relative to a context
switch (a disk's milliseconds): the CPU does useful work while waiting.
**Polling** when the device completes faster than (or comparably to) the
cost of taking an interrupt and switching — spinning briefly is cheaper —
or when interrupt rates are so high that the system would livelock in
handlers; a hybrid (interrupt, then poll in batches) is the practical
compromise.

### (b) The disk model [6 marks]

**(i) [1]** One rotation = 60,000 ms / 12,000 = **5 ms**; average rotational
delay = half a rotation = **2.5 ms**.

**(ii) [3]** T = seek + rotation + transfer = 3.5 ms + 2.5 ms +
(4 KB / 100 MB/s = 0.04 ms) ≈ **6.04 ms**. **[2]**
Throughput = 4 KB / 6.04 ms ≈ **0.68 MB/s**. Positioning dominates: 6 ms of
the 6.04 — transfer is under 1%. **[1]**

**(iii) [2]** T = 3.5 + 2.5 + (100 MB / 100 MB/s = 1,000 ms) = 1,006 ms →
throughput ≈ **99.4 MB/s** — essentially full bandwidth. Ratio ≈ 99.4 / 0.68 ≈
**150×, i.e. of order 100×**. **[1]** Justifies any design that turns small
scattered I/O into large sequential I/O — FFS's rule of placing large files in
big contiguous chunks so that positioning cost is amortized to a small
fraction of transfer time (equally acceptable: write buffering to batch
updates). **[1]**

### (c) RAID arithmetic [6 marks]

With N = 6 and per-disk random 4 KB bandwidth R (≈ 0.68 MB/s from (b)):

| | Capacity | Random read | Random write | Reason |
|---|---|---|---|---|
| RAID-0 | **6** disks | **6R** | **6R** | no redundancy; requests spread over all spindles **[1]** |
| RAID-1 (3 mirrored pairs) | **3** | **6R** | **3R** | a read is served by either copy, so all 6 disks serve reads; a logical write is two physical writes, one per side of a pair → N/2 **[1½]** |
| RAID-5 | **5** | **6R** | **1.5R** | parity rotates, so all 6 disks hold data and serve reads; a small write = 4 I/Os (read old data + old parity, write new data + new parity), spread evenly → N/4 **[1½]** |

**RAID-4's wall [2]:** with one dedicated parity disk, *every* small write —
whichever data disk it lands on — must read and write **that one parity
disk**: two I/Os there per logical write. The parity disk can do R worth of
I/Os, so the array completes **R/2** logical writes per second no matter how
many data disks are added — added spindles add data bandwidth to a bottleneck
that isn't the data. RAID-5 changes exactly one thing: parity blocks rotate
across all disks, so the two parity I/Os land on a different disk per stripe
and the load spreads — giving N/4 · R, which *does* grow with N.

*Common wrong answer:* RAID-1 random read as 3R ("half the disks") — reads
need only one copy, and independent random reads can be steered to either side
of each mirror, so all six spindles contribute.

### (d) Choosing a configuration [4 marks]

- **Configuration P (six-disk RAID-5):** capacity 5 × 4 TB = **20 TB**;
  random read **6R**; random write 6R/4 = **1.5R** — straight from the (c)
  table. **[1]**
- **Configuration Q (two RAID-1 pairs + 2 spares):** the array proper is
  N = 4, so capacity 2 × 4 TB = **8 TB** — it meets the floor exactly;
  random read **4R** (either side of each pair serves reads, so all four
  spindles help); random write 4R/2 = **2R**. The spares add no bandwidth;
  they shorten the exposure window after a failure. **[1]** Both
  configurations clear the 8 TB floor, so capacity does not decide.
- **The deciding quantity:** the fraction of write traffic that is **small
  and random** rather than large/sequential (equivalently, the full-stripe
  write fraction). **[1]**
- **Verdict on each side:** if small random writes dominate, **Q** wins —
  2R vs 1.5R is a third more write throughput, with no parity
  read-modify-write in the latency path. If writes arrive as large
  sequential runs, **P** wins — full-stripe writes compute parity from data
  in hand and sidestep the small-write penalty entirely, and P carries 2.5×
  the usable capacity as headroom. **[1]** *Also creditable at the verdict
  mark:* Q's write advantage is bought with a read cost — 6R falls to 4R —
  so Q is right only where small random writes genuinely **dominate** the
  mix, not merely where they are present; a read-heavy workload with some
  random writes still favours P.

---

## Question 3 — File systems

### (a) Bookwork [4 marks]

**(i) [2]** Inode stores (any four, ½ each): file size; type; ownership;
permissions; timestamps; link count; the block pointers (direct/indirect).
It does **not** store the file's **name** — names live in directory entries,
which map name → inode number; a file with two hard links has two names and
one inode.

**(ii) [2]** A **hard link** is another directory entry naming the *same
inode* — after creation the two names are indistinguishable; the file dies
when the link count reaches zero. A **symbolic link** is a separate small
file whose content is a *pathname*. Hard links cannot cross file systems (an
inode number is only meaningful within one) and generally cannot name
directories (cycle risk); symlinks can do both but can **dangle** — the
target's removal leaves them pointing at nothing.

### (b) Inode arithmetic [10 marks]

Pointers per 4 KB block = 4096 / 8 = **512**.

**(i) [4 marks]**
1. Direct: 12 × 4 KB = **48 KiB**. **[½]**
2. + single indirect: + 512 × 4 KB = 2 MiB → **2 MiB + 48 KiB**
   (= 2,146,304 B). **[1]**
3. + double indirect: + 512² × 4 KB = 512² blocks = 262,144 × 4 KB = **1 GiB**
   → 1 GiB + 2 MiB + 48 KiB. **[1]**
4. + triple indirect: + 512³ × 4 KB = **512 GiB** → maximum ≈ **513 GiB**
   (512 GiB + 1 GiB + 2 MiB + 48 KiB). **[1½]**

**(ii) [3 marks]** (1 each)
1. Offset 10,000 → block ⌊10,000/4096⌋ = **2** → a direct pointer → **1 read**
   (the data block).
2. Offset 5,000,000 → block 1220. Direct covers blocks 0–11, single covers
   12–523, double covers 524–262,667; 1220 is in the double range → double
   indirect block, then the second-level indirect block, then data =
   **3 reads**.
3. Offset 2³¹ → block 2³¹/2¹² = 2¹⁹ = 524,288 > 262,667 → triple chain:
   triple indirect, double, single, data = **4 reads**.

**(iii) [1 mark]** Every direct byte costs 1 read; every single-indirect byte
costs 2. So the largest all-bytes-in-≤2-reads file is direct + single
indirect fully used: **48 KiB + 2 MiB = 2,146,304 bytes**.

**(iv) [2 marks]** `/usr/share/dict/words`, cold: root inode, root data,
`usr` inode, `usr` data, `share` inode, `share` data, `dict` inode, `dict`
data, `words` inode = **9 reads** to open (an inode read and a data read per
directory level, then the file's inode — `2(d+1) + 1` with `d = 3`). **[1]**
Two data blocks per directory: each of the four directory levels (root plus
`usr`, `share`, `dict`) now costs an inode read plus, in the worst case,
**both** data blocks — 3 reads per level — and the final inode read is
unchanged: 4 × 3 + 1 = **13 reads**. Generally **3(d+1) + 1**, and with `b`
blocks per directory, `(b+1)(d+1) + 1`: directory growth multiplies the
per-level *data* cost but never the inode cost, so the count scales with
depth × directory size while the `+1`s stay fixed. **[1]**

### (c) FFS and the small-file question [6 marks]

**(i) [3 marks]** The old layout's two seek taxes: (1) **inode ↔ data** —
inodes clustered at one end of the disk, data sprawled across it, so every
open-then-read pays a long seek between metadata and content **[1]**; (2)
**file ↔ related file** — files in the same directory (accessed together)
scattered arbitrarily, plus data fragmenting over time **[1]**. Cylinder
groups put a file's inode, its data, and its directory neighbours in the same
small region. The rule is broken deliberately for **large files**: left in one
group, a big file would swamp it and destroy locality for everything else, so
FFS spreads large files in **chunks** across groups — affordable because a
large-enough chunk amortizes each inter-group seek down to a small fraction of
its transfer time. **[1]**

**(ii) [3 marks]** The proposal has it backwards: small files are where
cylinder-group placement earns the most. Reading a 4 KB file whole is almost
pure positioning cost — (b) put transfer at under 1% of a small request's
service time — so the seeks the layout removes (inode ↔ data, file ↔
directory neighbours) are essentially *all* of this workload's cost. What
this workload leaves idle is the **large-file exception** — an amortization
device — not the locality machinery. **[1]** First-free allocation looks
fine on an empty disk; the bill arrives as the disk fills and churns.
Related files — one mailbox's messages, delivered over months into whatever
holes deletion left — end up scattered, inode allocation drifts away from
data, and a mailbox sweep becomes a random walk of long seeks: exactly the
aging-driven decay to a few percent of bandwidth that FFS was built to fix.
**[1]** Verdict: keep the placement logic. The flat layout is fine while
the file system is mostly empty or short-lived (nothing has fragmented),
when files are read singly with no inter-file locality to exploit, or when
a cache in front absorbs nearly all reads — conditions required for the
mark. **[1]**

---

## Question 4 — Security

### (a) Goals, policy, mechanism [4 marks]

- **Confidentiality** — only authorised parties can read; **integrity** —
  data/system state changes only as authorised; **availability** — the
  service remains usable by those entitled to it. **[1½]**
- **Policy** = *what* is allowed ("students may not read staff files");
  **mechanism** = *how* the system enforces it (permission bits checked at
  `open()`, page-table protection, login). The OS ships mechanisms and lets
  installations express policies over them; conflating the two bakes one
  policy into the system. Example pair: policy "each user's home directory is
  private"; mechanism: `rwx` mode bits + UID checks in the kernel. **[2½ —
  distinction 1½, examples 1]**

### (b) Matrix, projections, Unix bits [8 marks]

**(i) [3 marks]** Matrix (r/w/x; running `deploy` is written x):

| | rel | tape | deploy |
|---|---|---|---|
| **dana** | rw | w | x |
| **omar** | — | rw | x |
| **mon** | r | — | — |

**ACLs (columns):** rel: {dana: rw, mon: r} · tape: {dana: w, omar: rw} ·
deploy: {dana: x, omar: x}.
**C-lists (rows):** dana: {rel: rw, tape: w, deploy: x} · omar: {tape: rw,
deploy: x} · mon: {rel: r}.
**[matrix 1, each projection 1]**

**(ii) [2 marks]** (1) Immediate revocation is cheap under **ACLs**: one
list, attached to `deploy`, delete one entry. Under capabilities the right
lives with the *subject* (possibly copied, cached, delegated) — the system
must find or invalidate every outstanding capability. **[1]** (2) The audit
is cheap under **capability lists**: read omar's row. Under ACLs it requires
scanning *every object's* list. Structural because each representation
stores the matrix sliced along one axis only — whichever axis you slice by,
queries along the other axis require a full scan. **[½]** The setuid caveat:
omar's row omits `rel`, yet running `deploy` swaps in *dana's* identity —
and dana holds `rel: r` and `tape: w`, so omar can cause `rel`'s contents to
land on a tape he may read, despite holding no right to `rel` himself. A
matrix row records direct rights only; an honest audit must also chase
programs that switch identity on the subject's behalf. **[½]**

**(iii) [3 marks]**
1. **Expressible [1]** — by the owner-slot-as-denial trick: make **mon the
   owner** of `scratch` with owner bits `---`, and give "other" `-w-` (group
   unused, e.g. also `-w-`). Because the owner class is checked first and is
   final, mon matches it and gets nothing; every other user falls through to
   "other" and may write. The 9-bit scheme *can* express "everyone except
   one named user" — by burning the owner slot on the excluded party.

   *Marking note:* a candidate who notes that **mon can restore its own
   bits** — an owner may always `chmod` its own file, and mon is an
   unattended daemon, precisely the principal one worries about — and
   concludes the policy is *expressible* in the bits but not *enforceable*
   against mon, has answered better than the model; credit in full.
2. **Expressible [2]** — and the point is *why*. Under a rights-are-unions
   reading it looks impossible: probation members are also "users on the
   system", so any bits that let everyone write would seem to reach them
   too. But class matching is **first-match-final**, so the group slot can
   *subtract* rights: owner dana `rw-`, group `probation` with `r--`, other
   `rw-` — mode `0646`. A probation member matches the group class and
   stops at `r--`, never seeing other's `w`; every non-member falls through
   to `rw-`. **[construction 1]** This is the same order-of-check property
   that (1) exploited in the owner slot, one slot down — the group field
   used as a *demotion* class, not a grant. And unlike (1)'s trick it *is*
   enforceable: probation members do not own `worklog`, so they cannot
   `chmod` their way back in. **[why first-match-final makes it work, 1]**
   *Marks for the mechanism argument; the bare mode string earns ½ at
   most. A candidate who answers "impossible" via the union reading has
   missed exactly the semantics the stem states.*

### (c) Iterated hashing against a deadline [4 marks]

*(The stem supplies the salted baseline: N × D = 10¹¹ hashes, 100 s at
10⁹ hashes/s. What is examined is the deadline reasoning built on it.)*

**(i) [1]** With `k` iterations per evaluation the full attack takes
100·k seconds. The deadline is 30 days = 30 × 86,400 ≈ 2.6 × 10⁶ s, so the
attack misses it once 100·k > 2.6 × 10⁶, i.e. **k > ~26,000 — any round
figure of order 3 × 10⁴ earns the mark** (working required).

**(ii) [2]** The single-user attack is **N times cheaper** — D·k hashes
instead of N·D·k. At k = 26,000 it takes 5 × 10⁷ × 26,000 / 10⁹ =
0.05 × 26,000 ≈ **1,300 s ≈ 22 minutes**: it beats the 30-day deadline by a
factor of ~2,000. **[1]** To push even one targeted account past the
deadline, 0.05·k′ > 2.6 × 10⁶ ⟹ **k′ > ~5.2 × 10⁷** — that is, **k′ = N·k**:
whatever `k` defends the whole file, defending each *individual* account to
the same deadline costs a further factor of exactly N, because the attacker
no longer pays the per-user multiplier that salting imposed. A rotation
policy calibrated to the file-wide attack quietly leaves every single
account ~N times less protected than it looks. **[1]**

**(iii) [1]** A legitimate login pays `k` **once**: at k = 26,000 that is
26,000 / 10⁹ ≈ **26 µs** — imperceptible; at k′ = 5.2 × 10⁷ it is
**52 ms** — noticeable but tolerable, and this is the real ceiling: `k` is
bounded by login latency the site will accept, not by the attacker. The
property embodied: a *tunable, deliberately high cost per evaluation* — the
attacker's 10⁹/s collapses to 10⁹/k. The asymmetry that makes the scheme
work: the defender pays k once per login; the attacker pays it D times per
targeted user, N·D times for the file. (Rate-limiting the login prompt is
irrelevant here — the file is stolen; the attack is offline.)

### (d) The print daemon [4 marks]

Three behaviours; principle + attack + fix (any reasonable minimal fix):

1. **Runs as root → least privilege.** Any bug in the daemon is a
   root compromise; and as a **confused deputy** it can be pointed at files
   its caller couldn't read (`submit /etc/shadow`). Fix: run as a dedicated
   unprivileged user; have users' processes copy the file into the spool
   under *their own* credentials (or pass an open descriptor), so the daemon
   never wields root authority on users' behalf. **[1½]**
2. **Unparsable policy ⇒ allow all → fail-safe defaults.** Corrupting one
   config file (or a bad deploy) silently disables all quota/permission
   enforcement — the failure mode is the attacker's friend. Fix: on parse
   failure, **deny** (queue jobs, alert an operator); errors must fail
   closed. **[1½]**
3. **Check at submission, act hours later → complete mediation** (a
   time-of-check-to-time-of-use gap). A user authorised at submit time may be
   deauthorised — or the submitted path may be re-pointed at a different
   file — before printing; the stale decision is honoured. Fix: re-validate
   at the time of use (or bind the check to the immutable spooled copy rather
   than to a re-resolvable name). **[1]**

---

*End of Midterm 2 solutions.*
