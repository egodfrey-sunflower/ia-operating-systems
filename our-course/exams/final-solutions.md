```
################################################################################
#                                                                              #
#   ####  ####   ###  ##  ##     ###  ####  ####                               #
#  ##     ##  ## ##  ## ####     ##   ##    ##  ##                             #
#   ###   ####   ##  ##  ##  ###  ##   ###   ####                             #
#     ##  ##     ##  ## ####       ##  ##    ##  ##                            #
#  ####   ##      ###  ##  ##    ###   ####  ##  ##                            #
#                                                                              #
#   S P O I L E R  —  MODEL ANSWERS AND MARK SCHEME BELOW                       #
#   Do not read until you have attempted the paper under exam conditions.      #
#                                                                              #
################################################################################
```

# Final Examination — Model Answers & Mark Scheme

**Rubric:** Question 1 is **compulsory**; the candidate answers Q1 plus
**three of Questions 2–7** (four answers in total, 20 marks each). Q1 forces
every script through the IA-calibrated core; Q7 puts memory & I/O in the
optional pool so the IA heartland cannot be dodged entirely by topic choice.

Marking convention (as in the course README): **1 mark ≈ 1 minute ≈ one
distinct point made.** Prose parts list the points a marker credits; award the
mark for the *point*, not the exact wording — "would a supervisor accept this
sentence as a made point?" Numeric parts must show working; a bare number earns
at most half the marks for that sub-part.

All numeric answers in this scheme were verified with `python3`; the checks and
their results are reproduced inline.

---

## Question 1 — Tripos transplant

**y2026p2q3** is graded against the **official Part IA solution notes** for
that paper. No model answer is reproduced here — that is deliberate: it is held
as calibration against the real IA standard, and marking it yourself against a
home-grown scheme would defeat the purpose. Mark it against the published IA
notes, and be harsh on the prose parts. If no solution notes were published
for that paper, fall back in order: mark against the **examiners' report**
remarks on the question if available; otherwise against the course **band
descriptors** plus the relevant **sheet answer keys** — and in that last case
mark deliberately harshly, since a home-grown key drifts lenient.
(Its sealed partner **y2026p2q4** is
not on this paper — it is sat separately as the week-17 timed standalone mock;
see `README.md`.)

---

## Question 2 — Unix case study

### (a) fork/exec/wait and signal delivery *(5 marks)*

**(i) fork/exec/wait.**
- `fork()` creates a **child process** that is a near-exact copy of the parent
  (same address space — copy-on-write in practice — same open file
  descriptors, same program counter); it returns the child's PID to the parent
  and 0 to the child, which is how each learns its role. **[1]**
- `execve()` **replaces** the calling process's memory image with a new
  program, keeping the process (PID, open fds) but discarding the old code,
  data and stack; on success it does not return. **[1]**
- `wait()`/`waitpid()` **blocks the parent until a child terminates** and
  reaps its exit status, freeing the zombie PCB. Without it the shell could not
  tell when a foreground job has finished (so could not print the next prompt)
  and would leak zombies. **[1]**
- The split (rather than a single `spawn`) is what lets the shell run arbitrary
  code *in the child between fork and exec* — redirections, `dup2`, closing
  fds, setting the process group — so that the launched program inherits an
  already-arranged environment. **[½ — credit if the fork/exec rationale is
  made explicit.]**

**(ii) Ctrl-C.**
- The terminal driver turns `Ctrl-C` into **SIGINT**, sent to the **foreground
  process group** of the controlling terminal — not to a single process. **[1]**
- The shell puts each job (pipeline) in its **own process group** (`setpgid`)
  and designates one as foreground via `tcsetpgrp()`; the shell itself is in a
  different group, so SIGINT never reaches it — hence it survives and reprompts,
  while every process in the foreground pipeline receives the signal. **[1]**
- *Credit alternatively:* mention that the default SIGINT action is to
  terminate, and that background jobs (other process groups) are unaffected.

*Predictable wrong answers:* saying SIGINT "goes to the shell, which forwards
it" (it does not — the kernel delivers it directly to the foreground group);
confusing process groups with sessions; claiming `fork` copies the program from
disk (it copies the *parent's* image; the new program only appears at `exec`).

### (b) The syscall sequence *(8 marks)*

Command: `grep -v '^#' conf.txt | sort | uniq -c > out.txt`

**(i) The two-pipe sequence. [6]**

**Convention:** descriptors are allocated **lowest-free-first**; fds 0,1,2
(stdin/out/err) are open on entry. Each child closes every inherited pipe fd
before its file `open()`s, so those return predictable numbers.

**Parent shell:**

1. `pipe(p1)` → `p1[0] = 3` (read), `p1[1] = 4` (write) — the `grep|sort`
   pipe; `pipe(p2)` → `p2[0] = 5` (read), `p2[1] = 6` (write) — the
   `sort|uniq` pipe. **[1]**
2. `fork()` ×3 → **child A** (`grep`), **child B** (`sort`), **child C**
   (`uniq -c`). **[½]**

**Child A (`grep -v '^#' conf.txt`, writes into pipe 1):**

3. `dup2(4, 1)` — pipe-1 **write** end becomes stdout.
4. `close(3); close(4); close(5); close(6)` — **all four** inherited pipe fds.
5. `execve("/usr/bin/grep", {"grep", "-v", "^#", "conf.txt", NULL}, envp)`.
   → At exec: **fd0 = terminal, fd1 = pipe1-write, fd2 = terminal.** Note
   there is **no stdin redirection**: `conf.txt` is an *argument* — `grep`
   opens it itself after `exec`. **[1 — including the no-redirect point]**

**Child B (`sort`, reads pipe 1, writes pipe 2):**

6. `dup2(3, 0)` — pipe-1 read end becomes stdin.
7. `dup2(6, 1)` — pipe-2 write end becomes stdout.
8. `close(3); close(4); close(5); close(6)`.
9. `execve("/usr/bin/sort", {"sort", NULL}, envp)`.
   → At exec: **fd0 = pipe1-read, fd1 = pipe2-write, fd2 = terminal.**
   **[1½ — B is the fiddly one: it must drop pipe-1's write end (4) and
   pipe-2's read end (5), which belong to its *neighbours*.]**

**Child C (`uniq -c`, reads pipe 2, writes `out.txt`):**

10. `dup2(5, 0)` — pipe-2 read end becomes stdin.
11. `close(3); close(4); close(5); close(6)`.
12. `open("out.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644)` → returns **3** (lowest
    free after the closes); `dup2(3, 1)`; `close(3)`.
13. `execve("/usr/bin/uniq", {"uniq", "-c", NULL}, envp)`.
    → At exec: **fd0 = pipe2-read, fd1 = out.txt, fd2 = terminal.** **[1]**

**Parent, after the forks:**

14. `close(3); close(4); close(5); close(6)` — the parent must close **all
    four** pipe ends.
15. `waitpid(A, …); waitpid(B, …); waitpid(C, …)`. **[1 for 14–15]**

**What goes wrong if a pipe fd is left open — and for whom.** A pipe's read
end returns EOF only when *every* write-end descriptor is closed. If the
parent keeps 4 or 6 open, `sort` (resp. `uniq`) never sees EOF and the
pipeline hangs even after its upstream stage exits. The subtler case is the
**middle stage deadlocking on itself**: `sort` inherits pipe-1's write end
(fd 4); if it fails to close it, then after `grep` exits `sort` is *itself*
the last writer of the pipe it is reading — it waits forever for an EOF that
its own open descriptor is withholding. Likewise `uniq` holding fd 6 wedges
itself on pipe 2. With two pipes the bookkeeping is per-child: every process
must close **every** pipe end it does not use, and the parent uses none of the
four. **[the discriminating point — credit within the per-child marks above;
an answer that closes only "the other end of my own pipe" in B and C has
missed it.]**

*Acceptable variations:* forking each child immediately after creating the
pipe it reads (then fd numbers differ — award full marks if the student
**states** their convention and is internally consistent); a `fork` chain in
which each stage forks the next; `close` orderings interleaved differently.
The **non-negotiable** points are: two `pipe` calls; three children; the
correct fds on 0/1/2 at each `exec`; all four pipe ends closed in the parent;
and B closing both foreign ends (4 and 5).

**(ii) `2>&1 > log.txt` vs `> log.txt 2>&1`. [2]**

Redirections are processed **left to right**, and `n>&m` is
`dup2(m, n)` **at that moment** — it copies what fd *m* currently designates;
it does not create a lasting alias.

- `cmd > log.txt 2>&1` — `open("log.txt", …) = f`; `dup2(f, 1)`; then
  `dup2(1, 2)` copies the *already-redirected* fd 1: **both 1 and 2 →
  log.txt**. This is the one she wants. **[1]**
- `cmd 2>&1 > log.txt` — `dup2(1, 2)` first: fd 2 becomes a copy of the
  **current** fd 1, i.e. the terminal; then `dup2(f, 1)` sends only stdout to
  the file. **fd1 → log.txt, fd2 → terminal** — stderr still appears on
  screen. **[1]**

*Predictable wrong answers:* redirecting stdin for `grep` (the file is an
argument, not a redirection); closing only one's own pipe's other end and
leaving the neighbour pipe's fds open in child B (the self-deadlock above);
using `dup` (arbitrary lowest fd) where the specific number matters; reading
`2>&1` as "and from now on 2 follows 1 wherever it goes" (it is a one-shot
descriptor copy — hence the ordering trap); claiming the two orderings in
*(ii)* are equivalent.

### (c) Snapshot critique + copy-on-write fix *(7 marks)*

Award up to **2 marks** for the flaws (≈⅔ each) and up to **5 marks** for the
corresponding CoW mechanisms (≈1⅔ each). The weight sits deliberately on the
mechanisms: this scenario revisits IA Examples Sheet 3 Q6 (sheet 8 B4), so
merely *naming* the three flaws is rehearsed material. A flaw earns its ≈⅔
only with the **inode-level mechanism** the question demands (what is shared —
inode number, data-block pointers, metadata fields — and which operation
therefore propagates); a bare flaw name ("in-place edits leak through") earns
half of that. The CoW side must name the structures and the moment of the copy
to earn its marks. A strong answer pairs each flaw with its fix. Marker
credits any three of the following flaw/fix pairs:

1. **In-place edits are *not* protected (undermines "recover from mistaken
   edits").** A hard link is a second name for the *same inode and the same
   data blocks*. If a user edits a file by opening it and writing in place, the
   change is visible through *every* snapshot's hard link — the "old version"
   is gone. The scheme only preserves files whose editor writes a new temp file
   and `rename()`s over the old (unlink of the old name), which not all editors
   do. **[flaw 1]**
   - **CoW fix:** a snapshot pins the current **block pointers**; the first
     write to any shared block **allocates a new block** and updates only the
     live tree's pointer, leaving the snapshot's pointer (and block) untouched.
     In-place edits are therefore preserved regardless of how the editor writes.
     **[fix 1]**

2. **Metadata is shared, not versioned (undermines edit/deletion recovery).**
   Because both names share one inode, `chmod`, `chown`, ownership and mtime
   changes apply to all snapshots at once; the link count is shared; you cannot
   see the *old* permissions. **[flaw 2]**
   - **CoW fix:** inodes and directory blocks are themselves part of the
     copy-on-write tree, so metadata is versioned along with data — the
     snapshot sees the old inode. **[fix 2]**

3. **The nightly copy is not atomic (undermines all three — snapshot may be
   inconsistent).** The recursive walk takes time; a file modified *during* the
   walk may be captured half-old/half-new, and directory contents can shift
   under the walker. **[flaw 3]**
   - **CoW fix:** a snapshot is an **O(1)** operation that atomically records
     the current **root** of the file-system tree (the uberblock / tree root),
     freezing a globally consistent point instantly — no walk. **[fix 3]**

4. **No protection against disk failure (undermines "resilience to a head
   crash").** All snapshots live on the **same disk**, and hard links *share
   the very same physical blocks*, so a bad block corrupts the file in every
   snapshot simultaneously. There is zero redundancy. **[flaw 4]**
   - **CoW fix (with nuance):** CoW *alone* does not solve this either — but ZFS
     adds **block checksums** (detecting silent corruption) and integrates
     **RAID-Z / mirroring** for redundancy, and snapshots can be `zfs send` to
     a *different* pool/host. The honest point: snapshots protect against
     *software/user* error, replication against *hardware* error, and they are
     orthogonal. **[fix 4 — award the extra insight for noting CoW≠redundancy.]**

5. **Cost: O(files) work every night** — the recursive walk creates a new
   directory entry (and directory blocks) for every file, even unchanged ones;
   space for directories grows with snapshot count × file count.
   - **CoW fix:** snapshots share **all** unchanged structure by reference; new
     space is consumed only by blocks that actually change ("only pay for the
     delta").

*Predictable wrong answers:* claiming hard links save no space (they do share
data blocks — the flaw is sharing, not space); asserting CoW makes snapshots
free forever (they pin space against reclamation until deleted); saying you can
hard-link directories to fix atomicity (Unix forbids user directory hard links,
and it would not help).

---

## Question 3 — Crash consistency

### (a) Journaling modes and fsck *(5 marks)*

**(i)**
- **Data journaling:** *all* blocks of a transaction — data **and** metadata —
  are written to the journal first, then checkpointed to their home locations.
  Strongest consistency, but every data block is written twice. **[1]**
- **Ordered metadata journaling:** data blocks are written to their **home**
  locations first (not journalled); only metadata is journalled, and the
  journal commit is ordered *after* the data reaches disk. Data written once;
  guarantees metadata never points at stale/garbage data. **[1]**

**(ii) fsck.** Can repair (any two): **[1 for two]**
- bitmap inconsistencies (a block/inode marked free but referenced, or vice
  versa) — recomputed by scanning;
- wrong **link counts** in inodes (recount references);
- **orphan** inodes (allocated, in no directory) — reclaim or move to
  `lost+found`;
- blocks claimed by two inodes (duplicate pointers).

Cannot repair (any two): **[1 for two]**
- **lost or torn user data** — fsck restores *structural* consistency, not the
  *contents* of a half-written file;
- which of two versions of a block was the "intended" one — it has no record of
  intent;
- a write that landed at the **wrong location** silently (no redundancy to
  detect it);
- data in a file whose inode was never written — the data blocks look free.

**Why journaling wins:** recovery time is **O(journal size)** (replay the tail),
not **O(file-system size)** (scan every inode and block), so it is far faster;
and it is *stronger* because the log records **intent**, letting recovery finish
a transaction atomically rather than merely guessing at a repair. **[1]**

### (b) Write ordering for an append of 8 blocks *(8 marks)*

Let **N = 8** data blocks; the append also dirties **1 inode** and **1 bitmap
block**. Each transaction has a **descriptor** block and a **commit** block.

**(i) Ordered lists.**

**Data journaling** *(everything through the log first)*: **[2]**

| Phase | Writes | Count |
|-------|--------|------:|
| Journal | descriptor, 8×data, inode, bitmap | 11 |
| Journal | **commit** (after a **barrier** ensuring the above are on disk) | 1 |
| — | *barrier before checkpoint* | — |
| Checkpoint (home) | 8×data, inode, bitmap | 10 |
| **Total** | | **22** |

Order: `descriptor → 8 data → inode → bitmap` to log · **barrier** · `commit`
to log · **barrier** · then in place `8 data → inode → bitmap`.

**Ordered metadata journaling** *(data to home first, then journal metadata)*: **[2]**

| Phase | Writes | Count |
|-------|--------|------:|
| Data to home | 8×data | 8 |
| — | *barrier: data must precede the commit* | — |
| Journal | descriptor, inode, bitmap | 3 |
| Journal | **commit** (after a **barrier**) | 1 |
| — | *barrier before checkpoint* | — |
| Checkpoint (home) | inode, bitmap | 2 |
| **Total** | | **14** |

Order: `8 data` to home · **barrier** · `descriptor → inode → bitmap` to log ·
**barrier** · `commit` to log · **barrier** · `inode → bitmap` in place.

**(ii) Totals and amplification.** **[4 — 2 for the totals, 1 for the ratios,
1 for the "why".]**

- Data journaling: **22** block writes. Amplification = 22 / 8 = **2.75×**.
- Ordered journaling: **14** block writes. Amplification = 14 / 8 = **1.75×**.
- Difference = 22 − 14 = **8 blocks**, i.e. **exactly N**.
- **Why:** the *only* structural difference is that data journaling writes each
  of the 8 data blocks **twice** (once to the log, once to its home), whereas
  ordered mode writes data **once** (straight to home) and journals only
  metadata. The descriptor+commit+inode+bitmap traffic is essentially the same
  in both; the whole gap is the second copy of the data. **[the "8 = N"
  observation is the discriminating point]**

Python verification (reproduced):

```
N = 8
dj = (1 + N + 1 + 1 + 1) + (N + 1 + 1)   # journal(desc,data,inode,bitmap,commit)+home
   = 12 + 10 = 22
oj = N + (1 + 1 + 1 + 1) + (1 + 1)        # data-home + journal(desc,inode,bitmap,commit) + checkpoint
   = 8 + 4 + 2 = 14
dj - oj = 8  (== N)      amplification: 22/8 = 2.75 , 14/8 = 1.75
```

*Predictable wrong answers:* forgetting the checkpoint (home) writes entirely
(then data journaling looks like 12, not 22); journalling data in "ordered"
mode (that *is* data journaling); omitting descriptor/commit (acceptable **only
if applied consistently to both modes** — the difference is still 8, and full
marks may be awarded if the reasoning is sound; deduct if applied to one mode
only); putting the data write *after* the metadata commit in ordered mode
(breaks the ordering guarantee — that is the whole point of the mode).

### (c) LFS reads, writes, cleaning; win/lose *(7 marks)*

**(i) Mechanics.** **[4]**
- **Read:** LFS finds an inode through the **inode map (imap)**, which records
  the current disk address of every inode; because inodes move on every write,
  the imap is the indirection layer. The imap is cached in memory (and
  checkpointed to disk), so a read is: consult imap → read inode → read data
  blocks — **no slower than FFS once the imap is cached**. **[1½]**
- **Write:** new/modified data, its inode, and the imap changes are **buffered
  and written sequentially** as a large **segment**, turning many random writes
  into one big sequential transfer (the win on rotating disks). Nothing is
  overwritten in place. **[1½]**
- **Cleaning:** because writes only append, the disk fills with **dead**
  (superseded) blocks interleaved with live ones. The **segment cleaner** reads
  a set of old segments, uses each segment's **summary block** (+ the imap) to
  determine which blocks are still **live**, compacts the live ones into a new
  segment, and frees the old segments for reuse. Without it the log-structured
  disk would run out of contiguous free space. **[1]**

**(ii) Win / lose.** **[3]**
- **Wins:** a **small-random-write / metadata-heavy** workload (e.g. many small
  file creates, or an NFS server). LFS converts scattered small writes into one
  sequential segment write, so seek/rotational cost is amortised — exactly what
  FFS pays dearly for. Crash recovery is also fast (roll forward from the last
  checkpoint). **[1½]**
- **Loses:** a **near-full disk under sustained overwrite**, or a
  **read-after-random-overwrite** workload where a file's blocks end up
  scattered across segments (poor read locality). The cleaner's cost is the
  killer: as segment **utilisation `u`** (fraction of live blocks) rises, each
  cleaned segment yields fewer free blocks, so the write cost multiplier grows
  roughly like **2/(1 − u)** — as the disk approaches full, cleaning must
  read-and-rewrite ever more live data per byte of free space recovered, and it
  competes with foreground writes for bandwidth. **[1½ — award the extra credit
  for tying the loss to utilisation `u`.]**

*Predictable wrong answers:* claiming LFS reads are inherently slow (they are
not, given a cached imap); forgetting the imap and asserting inodes are at fixed
locations (they move — that is the crux); saying cleaning is free or optional;
crediting LFS with beating FFS on large sequential reads (FFS with good layout
is competitive there — LFS's edge is *writes*).

---

## Question 4 — Concurrency II

### (a) Four conditions + what lock ordering denies *(4 marks)*

The four **necessary** conditions (all must hold): **[2, i.e. ½ each]**
1. **Mutual exclusion** — a resource is held in a non-shareable mode.
2. **Hold and wait** — a process holds ≥1 resource while waiting for more.
3. **No preemption** — resources are released only voluntarily.
4. **Circular wait** — a cycle of processes each waiting on the next.

**Which condition the rank discipline denies: circular wait.** **[1]** In any
would-be cycle T₁ → T₂ → … → Tₖ → T₁ ("→" = waits for a lock held by), each
thread waits for a lock of rank **strictly greater** than one it already holds
(it may only *acquire* upwards, so anything it holds has lower rank than what
it requests). Following the cycle, the ranks of the contested locks must
strictly increase all the way around and return to the start — a strictly
increasing cycle in a total order is impossible. Hence no cycle can ever form.
**[the "why exactly": the strictly-increasing-around-a-cycle contradiction.]**

**The other three still hold — and it does not matter.** **[1]** Locks remain
**mutually exclusive** (that is their job); a thread legitimately **holds one
lock while waiting** for a higher-ranked one (the discipline permits exactly
that); and nothing **preempts** a held lock — the kernel never revokes it. The
four conditions are individually *necessary*, so deadlock requires **all
four** simultaneously; denying any single one — here the cycle — suffices, and
the other three may hold harmlessly.

*Predictable wrong answers:* claiming lock ordering denies **hold-and-wait**
(it does not — threads hold lower-ranked locks while waiting, by design; the
common confusion); arguing the other three must *also* be broken for safety
(necessity ≠ sufficiency — one denied condition is enough); stating the
conditions as *sufficient* for deadlock (they are necessary; all four can hold
without an actual deadlock occurring).

### (b) The buggy frame allocator *(8 marks)*

**(i) The bug and an interleaving. [4]**
- **Bug:** `cond_signal` at line R1 wakes **one arbitrary waiter**, but the
  waiters have **heterogeneous predicates** (different `n`). The condition
  variable knows nothing about *which* waiter's predicate a `release` has just
  made true, so the single wakeup can be spent on a thread whose demand is
  still unsatisfiable — which re-tests `avail < n`, finds it true, and goes
  back to sleep — while a waiter that **could** proceed is never woken. The
  wakeup is consumed and lost: a **liveness** failure (deadlock with resources
  free), not a safety failure. The `while` loops are **correct** — that is the
  trap for pattern-matchers. `cond_signal` must be **`cond_broadcast`**. **[2]**
- **Interleaving** (specific `n`): all 64 frames are allocated, so
  `avail = 0`.
  1. **W1** calls `alloc(32)`: `avail (0) < 32` → waits on `cv`.
  2. **W2** calls `alloc(8)`: `0 < 8` → waits. (Queue order: W1 first.)
  3. **T** calls `release(16)`: `avail = 16`; `cond_signal` wakes exactly one
     waiter — say W1, the head of the queue.
  4. W1 re-acquires `m`, re-tests: `16 < 32` → waits again. Correct behaviour
     of the `while` loop — but the only wakeup is now gone.
  5. W2, whose predicate `avail(16) ≥ 8` **is true**, is never woken. If no
     further `release` arrives, **W1 and W2 sleep forever with 16 free
     frames**. **[2 — the interleaving must show a satisfiable waiter left
     asleep, not merely a wrong-order wakeup.]**

**(ii) Fix and justification. [4]**
- **Fix:** replace `cond_signal(&cv)` with **`cond_broadcast(&cv)`** at R1.
  **[1]**
- **Why over-waking is harmless:** every woken thread re-acquires `m` and
  re-evaluates its own predicate at **line A1** — the `while` loop. Threads
  whose demand is still unsatisfiable simply re-sleep; at worst broadcast
  costs a thundering herd of O(waiters) wakeups per release. The asymmetry is
  the point: under Mesa semantics the `while` makes **over**-waking safe, but
  nothing can make **under**-waking safe — a thread that is never made
  runnable can never re-test anything. So when the signaller cannot cheaply
  identify *which* waiter it has enabled, it must wake them all. **[2 —
  1 for tying safety to line A1, 1 for the over/under-waking asymmetry or the
  stated cost.]**
- **Same fixed `n` for every caller?** Then the original code is **correct**:
  all predicates are identical, so any waiter is as good as any other — a
  wakeup can never be "spent on the wrong thread". Each `release(n)` restores
  capacity for exactly one caller and posts exactly one signal; if a barging
  (non-waiting) thread steals the frames first, the woken waiter re-sleeps,
  but the enabling condition was destroyed with it, so no *satisfiable* waiter
  is ever left asleep. (Still fragile style: real `cond_wait`s permit spurious
  wakeups, and the next maintainer will vary `n` — `broadcast` is the robust
  idiom.) **[1]**

*Predictable wrong answers:* "the bug is `if` where `while` is needed" (the
loops **are** `while` — re-tested and correct; this is the reflex answer from
the bounded-buffer drill and earns nothing here); "use two condition
variables" (there are not two fixed classes of waiter — predicates vary
per-thread with `n`, so no static split of `cv` routes signals correctly;
per-waiter queues would work but is a redesign, not the one-word fix);
"signal must be called after `unlock`" (signalling with the mutex held is
legal and conventional); blaming a data race on `avail` (it is accessed only
under `m` — the failure is lost wakeup/liveness, not a race).

### (c) Banker's algorithm *(5 marks)*

**(i) Available, Need, and a safe sequence. [2]**

Sum of Allocation = (1+2+4+0+1, 0+1+0+2+1, 2+0+3+1+0) = **(8, 4, 6)**.
**Available = Total − Allocated = (12,6,8) − (8,4,6) = (4, 2, 2).** **[½]**

**Need = Max − Allocation:** **[½]**

| Process | Need (A B C) |
|:-------:|:------------:|
| P0 | 4 2 2 |
| P1 | 2 2 3 |
| P2 | 5 1 2 |
| P3 | 1 2 1 |
| P4 | 2 2 6 |

Safe sequence (one valid order): **⟨P0, P1, P2, P3, P4⟩** — index order works
here. **[1]**
Trace: Work=(4,2,2). P0 Need(4,2,2)≤Work → +Alloc(1,0,2) → Work=(5,2,4).
P1 Need(2,2,3)≤ → +(2,1,0) → (7,3,4). P2 Need(5,1,2)≤ → +(4,0,3) → (11,3,7).
P3 Need(1,2,1)≤ → +(0,2,1) → (11,5,8). P4 Need(2,2,6)≤ → +(1,1,0) → (12,6,8).
All finish. *(Other orders also work — accept any sequence whose trace is
valid; note P3 could equally go first.)*

**(ii) The two requests. [3]**

**Request X — P3 asks (1, 2, 1):** legal (= Need (1,2,1) exactly, and
≤ Available (4,2,2)). Tentatively grant: Allocation[P3]=(1,4,2),
Need[P3]=(0,0,0), **Available=(3,0,1)**. Safety check finds a sequence —
**⟨P3, P0, P1, P2, P4⟩**:
Work=(3,0,1). P3 Need(0,0,0)≤Work → P3 finishes, **releases its full
Allocation (1,4,2)** → Work=(4,4,3). P0 Need(4,2,2)≤ → +(1,0,2) → (5,4,5).
P1 Need(2,2,3)≤ → +(2,1,0) → (7,5,5). P2 Need(5,1,2)≤ → +(4,0,3) → (11,5,8).
P4 Need(2,2,6)≤ → +(1,1,0) → (12,6,8). **SAFE → grant X.** **[1½]**

**Request Y — P1 asks (1, 1, 1):** legal (≤ Need (2,2,3) and ≤ Available
(4,2,2)). Tentatively grant: Allocation[P1]=(3,2,1), Need[P1]=(1,1,2),
**Available=(3, 1, 1)**. Safety check — no process can take even one step:
P0 Need(4,2,2)? A:4>3, B:2>1, C:2>1 — no. P1 Need(1,1,2)? C:2>1 — no.
P2 Need(5,1,2)? A:5>3, C:2>1 — no. P3 Need(1,2,1)? B:2>1 — no.
P4 Need(2,2,6)? B:2>1, C:6>1 — no. **No process can proceed → no safe
sequence exists → UNSAFE → refuse Y** (P1 must wait). **[1½]**

Note the trap built into the numbers: Y asks for *fewer* total units (3) than
X (4), yet Y is the unsafe one — "small request, so it must be fine" is
exactly the heuristic the banker's algorithm exists to replace. Credit a
student who remarks on this.

Python verification (reproduced):

```
Total (12,6,8); Allocated (8,4,6); Available = [4, 2, 2]
base state SAFE, seq = [P0, P1, P2, P3, P4]
X: P3 req [1,2,1] -> avail_after [3,0,1]  SAFE  seq [P3,P0,P1,P2,P4]
Y: P1 req [1,1,1] -> avail_after [3,1,1]  UNSAFE
   (P0 blocked A,B,C; P1 blocked C; P2 blocked A,C; P3 blocked B; P4 blocked B,C)
```

*Predictable wrong answers:* granting a request just because `request ≤
Available` **without** running the safety check (that is the whole point of the
banker's algorithm — availability is necessary, not sufficient; Y passes the
availability test and is still refused); computing Need as Allocation − Max;
forgetting that a granted process **releases** its full Allocation (not just
its Need) when it finishes — in X's trace it is P3's released (1,4,2), not its
request, that unblocks P0; declaring Y grantable because it is the smaller
request (it fits but leaves an unsafe state).

### (d) RCU *(3 marks)*

- **Reader side:** readers dereference shared pointers with **no lock and no
  atomic write** — just a lightweight `rcu_read_lock()`/`unlock()` that, on many
  kernels, compiles to little or nothing (it only demarcates a read-side
  critical section; it does not exclude anyone). **[1]**
- **Writer side — publish then defer:** a writer **copies** the object, modifies
  the copy, and **atomically swaps the pointer** so new readers see the new
  version while readers already traversing still see the old one. The old
  version is **not freed immediately**. **[1]**
- **Grace period:** the writer waits for a **grace period** — the time until
  every CPU has passed through a quiescent state (so no reader can still hold a
  reference to the old version) — and only *then* reclaims the old object. This
  is what RCU trades away: **reclamation is deferred** (memory is held longer),
  writers are more expensive/complex, and it suits **read-mostly** data where
  stale-but-consistent reads are acceptable. **[1]**

*Predictable wrong answers:* claiming readers take a lock "but a cheap one"
(the point is no mutual exclusion at all); saying writers never synchronise
(they do — with each other, and they must wait out the grace period); asserting
RCU gives readers the *latest* value (a reader may legitimately see the old
version for the duration of its critical section).

---

## Question 5 — Virtualization

### (a) Popek–Goldberg / trap-and-emulate *(5 marks)*

- **Sensitive** instruction: one whose behaviour depends on, or changes, the
  configuration of resources (privilege level, memory mapping, interrupt
  state) — "control-sensitive" (changes system state) or "behaviour-sensitive"
  (reads it). **[1]**
- **Privileged** instruction: one that traps when executed in user mode. **[1]**
- **Criterion:** a machine is classically virtualizable by trap-and-emulate iff
  the set of **sensitive instructions is a subset of the privileged
  instructions** — i.e. every sensitive instruction traps to the VMM when the
  guest (running deprivileged) executes it, so the VMM can emulate it. **[1]**
- **x86 failure + fix:** 32-bit x86 has **sensitive-but-unprivileged**
  instructions — the classic example is **`POPF`**, which silently *ignores*
  the interrupt-enable flag change in user mode instead of trapping (also
  `SGDT`/`SIDT`/`SMSW` read privileged state without trapping). So the VMM never
  sees them. **[1]** Fixes (any one): **binary translation** (VMware) or
  **paravirtualization** (Xen) worked around it in software; hardware later
  added **Intel VT-x / AMD-V** (a root/non-root mode with VMCS-controlled
  traps) to make x86 classically virtualizable. **[1]**

### (b) Shadow vs nested page walk *(8 marks)*

**(i) Shadow paging. [2]** The VMM maintains a **single** page table that maps
**guest-virtual → host-physical** directly. The hardware walks it exactly as it
would a native table of depth `d`, so a TLB miss costs **d = 4 memory
accesses** — the same as running native. The guest's own page tables are not
walked by hardware at all (the VMM keeps the shadow in sync with them).

**(ii) Nested paging — the 2-D walk. [4]** Every guest-physical address produced
during the guest walk (the guest CR3 base, and the guest-physical address read
at each of the `d` guest levels, and finally the guest-physical of the data
page) must itself be translated by the host's `h`-level table before it can be
used. There are `(d + 1)` guest-physical pointers to translate; translating
each costs `h` accesses (to walk the host table) plus **1** access (to read the
guest entry / the final datum) — i.e. `(h + 1)` accesses per guest step. The
top-left step (the guest CR3 value already sits in a register, not memory)
saves one access, giving:

> **memory accesses per TLB miss = (d + 1)(h + 1) − 1**

For **d = 4, h = 3**: `(4 + 1)(3 + 1) − 1 = 20 − 1 = **19** accesses`. **[1½
for the formula, 1½ for the correct 19 with the 5×4-grid-minus-1 reasoning.]**

The contrast is stark: **4 (shadow) vs 19 (nested)** — nested paging makes a
TLB miss ~4.75× more expensive on this machine.

**The marginal-cost comparison. [1]** From the formula, adding one **guest**
level (d 4→5) adds `(h + 1) = 4` accesses; adding one **host** level (h 3→4)
adds `(d + 1) = 5` accesses — so on this machine an extra **host** level is
dearer. Structurally: an extra guest level adds **one more guest-physical
pointer to resolve**, i.e. one extra host sub-walk (`h` accesses) plus the
read of the new guest entry; an extra host level lengthens **every one of the
`(d + 1)` host sub-walks** by one access, because the host walk is nested
*inside* each step of the guest walk. Each dimension's marginal cost is set by
the **other** table's depth — deep guests make host-table depth the expensive
axis, and vice versa.

Python verification (reproduced):

```
nested(d,h) = (d+1)*(h+1) - 1
nested(4,3) = 19        shadow = d = 4       ratio = 19/4 = 4.75
marginal: nested(5,3)-nested(4,3) = 4 (= h+1);  nested(4,4)-nested(4,3) = 5 (= d+1)
(sanity: nested(2,2)=8, nested(3,4)=19, nested(4,4)=24)
```

**(iii) What shadow pays instead. [2]** Shadow paging buys the cheap walk at the
cost of **keeping the shadow tables synchronised with the guest's**:
- every guest modification of its own page tables must be **intercepted** —
  either by write-protecting the guest tables and taking a **VM exit** on each
  write, or by trapping the tlb-flush that follows — which is expensive and
  frequent for fork/exec/mmap-heavy workloads;
- the VMM must maintain a **separate shadow per guest address space** (per
  process context), costing memory and adding context-switch overhead.

Nested paging exists precisely because it makes those updates **free** (the
guest edits its own tables with no traps — the hardware walks both tables), at
the price of the longer walk. The choice is a **walk-cost vs update-cost**
trade: nested wins when the guest changes mappings often; shadow can win for a
static, TLB-friendly workload. *(Award full marks for any coherent statement of
the sync/trap cost + the trade-off.)*

*Predictable wrong answers:* saying nested is "always faster" (it has the
slower walk); giving `d × h` = 12 (misses the +1 per level and the final data
access); quoting the memorised symmetric value 24 without noticing `h = 3`
(the formula must actually be evaluated); calling the marginal costs equal
"by symmetry" (the formula is symmetric in *form*, but at `d = 4, h = 3` the
two margins differ — 4 vs 5); forgetting that shadow requires trapping guest
PTE writes; claiming the TLB removes the difference (the question stipulates a
TLB miss — the TLB hides both costs equally when it hits).

### (c) Container isolation and its limits *(7 marks)*

**(i) Mechanisms. [3, 1 each]**
1. **Namespaces** — the **PID namespace** isolates the visible set of processes
   (the container's init is PID 1 and cannot see or signal host processes); the
   **UTS namespace** isolates the hostname; (mount/net/IPC/user namespaces
   isolate their respective globals).
2. **The mount namespace + `pivot_root(2)`** (over an image, e.g. an OverlayFS
   stack) gives the container its **own root filesystem** — `pivot_root` swaps
   the root mount and the old root is unmounted so nothing of the host `/`
   remains reachable (unlike `chroot`, which can be escaped).
3. **cgroups v2** account for and **cap** resources — e.g. `memory.max` bounds
   RAM (a hog is OOM-killed at the cap) and `cpu.max` bounds CPU (a quota/period
   pair, e.g. `50000 100000` for half a core).

**(ii) What a container cannot isolate that a VM can. [4]** The root cause is
that **all containers share the single host kernel**; a VM has its own kernel
on virtual hardware. Any two of the following, with the shared-kernel reason:
**[2 each]**
- **The kernel itself / the system-call surface.** A container cannot run a
  **different kernel or kernel version** from the host, cannot load its own
  kernel modules, and a **kernel bug or 0-day is a shared attack surface** — a
  container escape via a kernel vulnerability compromises the host and every
  sibling. A VM, isolated at the hardware boundary, contains such a bug to the
  guest.
- **The clock / kernel-global state and true resource partitioning.** Things
  the kernel exposes globally leak or cannot be per-container virtualized —
  e.g. the **system time** (`settimeofday` affects the host; the time namespace
  only offsets the clock), certain `/proc` and `/sys` entries, kernel keyrings,
  and hardware. cgroups *limit* CPU/memory but do not give a container a
  **dedicated** core or guaranteed physical memory the way a VM's virtual
  hardware does.
- *(Also creditable:)* a container cannot run a **different OS** (Windows guest
  on a Linux host); shares the **page cache / kernel schedulers**; and (from the
  lab) capabilities inside a **user namespace** only bite against files whose
  owner is *mapped* (`capable_wrt_inode_uidgid`) — a reminder that the isolation
  is a kernel *policy over shared state*, not a hardware partition.

*Predictable wrong answers:* "containers can't isolate memory" (cgroups do cap
memory — the real gap is a *dedicated* kernel, not memory limits); claiming a
VM shares the host kernel (it does not — that is the distinction); listing
"security" without naming the shared-kernel mechanism that causes it.

---

## Question 6 — Design / synthesis essay *(20 marks)*

Mark holistically against the grading bands (README): **75%+** takes a defended
position with specific system evidence and trade-offs; **60–75%** is correct and
well-organised but thin on judgement; **<50%** is a list of facts. Below is a
**marking-points list** per prompt: each bullet ≈ **2 marks** for a point that
is *made and substantiated with a named system*. A student cannot reach the top
band by breadth alone — reward a clear thesis and at least one genuine
trade-off or counter-argument.

### Prompt A — The in/out pendulum of OS structure

*(Fresh prompt — deliberately distinct from the sheet 11 essay ("microkernel =
hypervisor with better marketing"), which the student has already written. Do
not credit a recycled sheet 11 answer that ignores the pendulum framing,
unikernels, or eBPF.)*

Credit the following (≈2 marks each; ~10 substantive points available, cap at
20):
- **States a thesis** about the pendulum and sustains it (e.g. "the pendulum is
  real but its axis has shifted: the modern question is not *where* code runs
  but *how it is proved safe to run there*" — any defended position is fine,
  including "there is no pendulum, only coexisting niches").
- **Monolithic Unix as the starting position:** everything — FS, drivers,
  network stack — inside one kernel address space; cross-subsystem calls are
  **function calls**, so it is fast, but any driver bug crashes the whole
  system: one fault domain, maximal TCB. (Ritchie & Thompson; Linux today.)
- **First swing out — Mach and its failure mode:** move services to user-space
  servers for fault isolation; former function calls become **IPC** (two
  context switches, cache/TLB damage), and first-generation microkernels were
  slow enough to discredit the idea commercially.
- **Liedtke's correction (L4):** the slowness was an artefact, not fundamental —
  a minimal, cache-conscious kernel with register-based IPC makes the
  out-swing affordable; **seL4** then cashes the safety cheque: a kernel small
  enough to *formally verify*. The out-swing's payoff is proof, not just
  isolation.
- **Exokernels push furthest out:** remove not just code but **abstractions** —
  the kernel only multiplexes raw hardware securely; library OSes implement
  policy in user space (Engler et al.). Identifies this as an *extreme* of the
  same swing: minimal in-kernel functionality, maximal application control.
- **Unikernels twist the axis:** collapse application + library OS into a
  **single-address-space, single-purpose image** — everything is "in the
  kernel" again (no protection boundary inside the image, so calls are fast),
  but safety is outsourced **downwards to the hypervisor** and to build-time
  specialisation. The pendulum positions of "in" and "out" stop being opposites:
  it is *in* w.r.t. its own image, *out* w.r.t. the host.
- **eBPF as the synthesis:** user-supplied code moves **into** the running
  kernel for performance (tracing, networking/XDP, scheduling policy), but is
  admitted only through a **static verifier** that bounds loops, checks memory
  access, and restricts the API. Safety by *verification* rather than by
  *address-space separation* — the safety mechanism the pendulum was swinging
  to avoid paying for at run time.
- **Names the forces precisely:** the cost of an out-swing is **boundary
  crossings** (IPC, syscalls, mode switches — made *more* expensive by
  Meltdown-era mitigations like KPTI), and the cost of an in-swing is **TCB
  growth and shared fault domain**; notes hardware trends (fast devices,
  io_uring/kernel-bypass) keep re-pricing the trade.
- **A complication engaged** (the question demands one): e.g. Linux never
  swung — it won as a monolith with loadable modules while microkernels won in
  niches (QNX, seL4 in secure enclaves); or kernel-bypass (DPDK/exokernel
  ideas) and eBPF are *both* mainstream in the same kernel, i.e. the pendulum
  swings both ways simultaneously at different subsystem granularity.
- **A defensible resting-point verdict:** e.g. "it settles at *verified
  admission* — code goes wherever it is fastest, and safety moves from spatial
  isolation to proofs (seL4 at the bottom, eBPF at the top)"; or "it never
  settles, because the performance/safety exchange rate is set by hardware,
  which keeps changing." Reward the argument, not the conclusion.

*Flag as incomplete:* a chronology with no forces (what pushed each swing);
missing two or more of the five named systems/technologies; treating eBPF as
"just a tool" without the safety-by-verifier point; or recycling the
microkernel-vs-hypervisor essay wholesale without addressing unikernels/eBPF or
the resting-point question.

### Prompt B — Design an OS for an unusual target

The student picks **one** target. Credit (≈2 marks each), scaling to the depth
of justification; a top answer keeps *some* Unix and discards the rest **with
reasons**, and names a real system.

**(i) Thousands of cores:**
- Rejects a single **big-kernel-lock / shared-memory monolith**; identifies
  **cache-coherence and lock contention** as the wall.
- Proposes a **message-passing / "multikernel"** structure — cites **Barrelfish**
  (treat the machine as a network of cores, replicate state, agree by messages).
- **Per-core data structures**, avoid shared mutable state; **RCU**/lock-free
  for the unavoidable sharing.
- Scheduler: **per-core run queues**, gang/space scheduling, NUMA-aware
  placement; keeps the Unix *process/thread* abstraction but not global
  scheduling.
- Keeps: the file/process abstractions and syscall API where cheap. Discards:
  global locks, a single shared page-table/run-queue.

**(ii) No MMU (microcontroller):**
- No virtual memory → **single physical address space**, all code trusted or
  isolated only by **software** (SFI / language safety / MPU regions if
  present).
- Keeps a **process abstraction** only in a degraded form; likely **static**
  memory allocation, no `fork` (nothing to copy-on-write); cites **µC/OS**,
  **FreeRTOS**, or **Tock** (Rust-based, uses the MPU + type safety).
- Protection by **capabilities/language** rather than page tables; real-time
  scheduler (**RM/EDF**) rather than fair-share.
- Discards: demand paging, `mmap`, protection rings via MMU. Keeps: a small
  syscall/driver model, cooperative or preemptive threads.

**(iii) Formally verified safety-critical controller:**
- **Minimise the TCB** so proof is tractable — cites **seL4** (verified
  microkernel) and its capability model; everything else in unprivileged,
  separately-argued components.
- **Deterministic, bounded** behaviour: static memory, bounded loops, **WCET
  analysis**, no dynamic allocation, no demand paging (unpredictable latency).
- **Spatial + temporal isolation** between criticality levels (partitioning,
  cf. **ARINC 653**); a fault in one partition cannot affect another.
- Discards: rich dynamic Unix features (`fork`, paging, an unbounded scheduler)
  that defeat verification/determinism. Keeps: a minimal, well-specified syscall
  surface.
- **A cross-cutting "keep vs discard" table** with justification, plus at least
  one named real system, earns the top band.

*Flag as incomplete:* choosing a target but proposing "just Unix" with no
departures; hand-waving ("make it more parallel") without a mechanism; naming no
system; or ignoring the *why* behind each departure — the marks are for
**justified** design choices, not a feature wishlist.

---

## Question 7 — Memory & I/O

### (a) Four-level translation and effective access time *(7 marks)*

**(i) Why not a linear table; what the TLB buys. [3]**
- A single-level table for a 48-bit space with 4 KiB pages needs
  2⁴⁸ / 2¹² = 2³⁶ entries; at 8 bytes each that is **2³⁹ bytes = 512 GiB per
  process** — vastly more than the memory it maps, and it must be contiguous.
  **[1]**
- A multi-level (radix) table allocates a lower-level table **only for regions
  that are actually mapped**; since real address spaces are extremely sparse,
  the table's size tracks the *used* portion, and the top level is a single
  page. **[1]**
- The **TLB** caches recent virtual→physical translations so the table is
  walked only on a miss. Without it, *every* reference below would pay the
  full 4-access walk — a 5× slowdown on every load and store. **[1]**

**(ii) The split. [1]** Offset = log₂(4096) = **12 bits**. A table entry is
8 bytes, so one 4 KiB page holds 4096/8 = **512 = 2⁹ entries** → each index is
**9 bits**. Remaining bits: 48 − 12 = 36 = **4 × 9**, so exactly **four**
levels. *(Check: 4·9 + 12 = 48.)*

**(iii) EAT. [3]**
- Hit (95%): 1 access = **80 ns**. Miss (5%): 4 walk accesses + the reference
  = 5 × 80 = **400 ns**. **[1]**
- **EAT = 0.95 × 80 + 0.05 × 400 = 76 + 20 = 96 ns.**
  (Equivalently 80 + 0.05 × 320.) **[1]**
- Required hit rate for EAT ≤ 90 ns: 80 + (1 − h)·320 ≤ 90 →
  (1 − h) ≤ 10/320 = 0.03125 → **h ≥ 96.875%**. **[1]**

Python verification (reproduced):

```
entries/table = 4096/8 = 512 -> 9-bit index;  4*9 + 12 = 48  ✓
EAT = 0.95*80 + 0.05*(4*80+80) = 76 + 20 = 96.0 ns
h for EAT<=90:  1 - 10/320 = 0.96875  ->  96.875%
```

*Predictable wrong answers:* charging the walk **instead of** the access on a
miss (gives 0.95·80 + 0.05·320 = 92 ns — the reference itself still happens);
using 10-bit indices from the 32-bit examples (the PTE here is 8 bytes, not 4);
forgetting that a hit still costs the 80 ns memory access.

### (b) Replacement trace, working set, thrashing *(7 marks)*

**(i) LRU vs OPT, 3 frames.** Reference string `1 2 3 4 2 1 5 2 4 1`. **[4 —
1½ per correct trace, 1 for both fault counts]**

**LRU** (frames listed least-recent first; F = fault):

| ref | 1 | 2 | 3 | 4 | 2 | 1 | 5 | 2 | 4 | 1 |
|-----|---|---|---|---|---|---|---|---|---|---|
| frames | 1 | 1 2 | 1 2 3 | 2 3 4 | 3 4 2 | 4 2 1 | 2 1 5 | 1 5 2 | 5 2 4 | 2 4 1 |
| fault | F | F | F | F |  | F | F |  | F | F |

**LRU: 8 faults** (hits on the 5th and 8th references).

**OPT** (evict the page whose next use is farthest away / never):

| ref | 1 | 2 | 3 | 4 | 2 | 1 | 5 | 2 | 4 | 1 |
|-----|---|---|---|---|---|---|---|---|---|---|
| frames | 1 | 1 2 | 1 2 3 | 1 2 4 | 1 2 4 | 1 2 4 | 2 4 5 | 2 4 5 | 2 4 5 | 4 5 1 |
| fault | F | F | F | F |  |  | F |  |  | F |

**OPT: 6 faults.** Key evictions: at the 4th reference (page 4) evict **3**
(never used again, while 1 and 2 both recur); at the 7th (page 5) evict **1**
(next used later — position 10 — than 2 at 8 or 4 at 9); at the 10th (page 1)
any victim is optimal (no future references) — accept any frame set of size 3
containing 1. As always, **OPT ≤ LRU**: 6 vs 8.

**(ii) Working set. [2]** **W(t, Δ)** = the set of **distinct pages referenced
in the last Δ references** at time t (Denning). With Δ = 4:
- after the **5th** reference: refs 2–5 = 2, 3, 4, 2 → **W = {2,3,4},
  size 3**;
- after the **7th** reference: refs 4–7 = 4, 2, 1, 5 → **W = {1,2,4,5},
  size 4**.
The growth says the process's **locality has broadened** (a locality
transition: page 3 ages out of the window while pages 1 and 5 enter): its
frame demand has risen from 3 to 4. With only **3** allocated frames the
working set now **exceeds** its allocation, so the OS should grant it a fourth
frame to hold the fault rate down — and this under-provisioning is exactly the
mechanism that tips a process into thrashing in *(iii)*. The working-set size
is the frame allocation that avoids thrashing.

Python verification (reproduced):

```
ref = [1,2,3,4,2,1,5,2,4,1]
LRU 3 frames: 8 faults      OPT 3 frames: 6 faults    (FIFO sanity: 9)
W(5,Δ=4) = {2,3,4} size 3;   W(7,Δ=4) = {1,2,4,5} size 4
```

**(iii) Thrashing. [1]** Thrashing = the system spends its time **servicing
page faults rather than executing**: each process has fewer frames than its
working set, so every scheduling quantum begins with a fault storm, CPU
utilisation collapses, and (classically) the OS responds to the idle CPU by
admitting *more* processes, making it worse. Response: **reduce the degree of
multiprogramming** — suspend/swap out entire processes until every remaining
process holds its working set (working-set/page-fault-frequency admission
control).

*Predictable wrong answers:* running FIFO when LRU was asked (FIFO gives 9
here); evicting the *most* recently used under "LRU"; for OPT at the 7th
reference, evicting 2 or 4 instead of 1 (compare **next-use distances**, not
past use); defining the working set over *future* references (it is a
trailing window — OPT looks forward, the working set looks back).

### (c) Disk vs SSD, and the I/O path *(6 marks)*

**(i) Service times and IOPS. [3]**

**HDD**, one random 4 KiB read:
- seek = **5 ms**;
- rotational latency: 6000 RPM → one revolution = 60/6000 s = **10 ms**; on
  average the target sector is **half a revolution** away once the head lands
  on the track (uniformly distributed angular position) → **5 ms**; **[1 incl.
  the half-revolution justification]**
- transfer = 4096 B / 128 × 10⁶ B/s = **32 µs ≈ 0.03 ms**;
- **total ≈ 5 + 5 + 0.032 = 10.03 ms** → **≈ 100 IOPS** (1/0.01003 s = 99.7).
  **[1]**

**SSD:** 50 µs per random 4 KiB read → **20 000 IOPS**.
**Ratio ≈ 10.03 ms / 50 µs ≈ 200×.** The dominant HDD terms are *mechanical*
(seek + rotation, 10 ms of the 10.03) — the electronics are irrelevant. **[1]**

**(ii) Provisioning and the I/O path. [3]**
- **Provisioning:** 5000 IOPS ÷ 99.7 IOPS/disk = 50.2 → **51 hard disks**
  (striped, ignoring overheads) versus **one SSD** (20 000 ≥ 5000, with 4×
  headroom). This is why random-read-bound databases moved to flash. **[1]**
- **Elevator on HDD vs SSD:** SCAN reorders the queue to sweep the head across
  the platter, converting long random seeks into short ordered ones — it
  attacks exactly the 10 ms mechanical term, so HDD throughput rises
  substantially. The SSD has **no head and no rotational position**: a random
  read costs the same 50 µs regardless of the logical block address, so
  LBA-reordering buys ~nothing (and adds queueing latency); the useful ordering
  decisions (erase blocks, wear, garbage collection) happen **inside the FTL**,
  which is why Linux defaults NVMe to the `none` scheduler. **[1]**
- **Write-behind:** the kernel acknowledges the write once it is in the buffer
  cache and flushes later. *Benefit:* absorbs bursts, batches and reorders
  writes (and on the HDD lets the scheduler amortise seeks), decoupling the
  application from the 10 ms device. *Risk:* on power loss/crash the
  acknowledged-but-unflushed data is **lost** — for a database this can break
  its transaction durability unless it forces ordering with
  `fsync`/barriers (which is exactly what Q3's journaling machinery is for).
  **[1]**

Python verification (reproduced):

```
rev = 60/6000 = 10 ms -> avg rot = 5 ms;  xfer = 4096/128e6 = 32 us
HDD = 5 + 5 + 0.032 = 10.032 ms -> 99.7 IOPS;  SSD = 50 us -> 20000 IOPS
ratio = 200.6x;   disks for 5000 IOPS: 5000/99.7 = 50.2 -> 51;  SSDs: 1
```

*Predictable wrong answers:* using a **full** revolution (10 ms) for rotational
latency; forgetting the transfer term entirely (small here, but must appear);
mixing units (128 MB/s with MiB blocks — the paper states 1 MB = 10⁶ B and the
block is 4096 B, giving exactly 32 µs); claiming SCAN helps the SSD "a little
because of readahead" (readahead is a *cache* effect, not queue reordering);
calling write-behind risk-free because "the disk has a cache" (that only moves
the volatility one level down).

---

*End of mark scheme.*
```
################################################################################
#  All numeric answers above were python-verified:                             #
#    Q3  data-journal=22, ordered=14, diff=8=N; amplification 2.75x vs 1.75x   #
#    Q4  totals (12,6,8), Avail (4,2,2), base SAFE <P0..P4>;                    #
#        X: P3(1,2,1) SAFE <P3,P0,P1,P2,P4>; Y: P1(1,1,1) UNSAFE (all blocked)  #
#    Q5  nested (d=4,h=3) = (d+1)(h+1)-1 = 19; shadow = d = 4; margins 4 vs 5   #
#    Q7  4x9+12=48; EAT=96ns, h>=96.875% for 90ns; LRU=8, OPT=6 faults;         #
#        W(5)= {2,3,4}, W(7)={1,2,4,5}; HDD 10.032ms/99.7 IOPS vs SSD 50us/     #
#        20000 IOPS (200.6x); 51 HDDs vs 1 SSD for 5000 IOPS                    #
################################################################################
```
