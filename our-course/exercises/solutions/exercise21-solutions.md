> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 21 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their
> working; for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. FALSE.** No metadata refers to the block, so the file system's
structures agree perfectly — it is *as if the write never occurred*. Ch. 42
calls this case no problem at all for crash consistency, while noting it is
certainly a problem for the user, whose data is gone.

**A2. FALSE.** fsck repairs **metadata self-consistency** only — bitmaps,
link counts, pointers, directories. It does not know what user data should
be, and cannot even detect the case where consistent metadata points at
garbage (inode + bitmap written, data block not).

**A3. FALSE.** Checkpointing must wait until the transaction — including the
commit block — has *completed*, not merely been issued. Otherwise home
locations could be partially overwritten while the journal is not yet
guaranteed replayable: the crash then leaves neither a usable log nor intact
originals.

**A4. FALSE.** The disk may internally schedule the pieces of a large write
in any order: it can persist TxB + metadata + TxE and *then* the data block.
A crash in the gap leaves a transaction that looks complete but contains
garbage where an update block should be — and recovery will happily copy
that garbage over live structures. Hence the two-step protocol (or a
transaction checksum). See B2(b).

**A5. TRUE.** Ordered journaling writes data once, to its final location
(before commit), and journals only metadata; data journaling writes every
data block to the journal *and* to its home during checkpoint. That doubling
— and the seek between journal and home — is why ordered mode is the common
default.

**A6. FALSE.** Reads perform essentially the same I/Os as a conventional
file system once the (cached) imap has been consulted; LFS's bet is that
growing memory caches absorb reads, making **write** performance the thing
that matters — which sequential segment writes maximise.

**A7. FALSE.** That is exactly the recursive-update problem LFS avoids. The
directory maps name → i-number, and the **imap** maps i-number → inode
location; when the inode moves, only the imap changes. Indirection stops the
ripple at the first level.

**A8. FALSE in spirit.** Without cleaning, free space is only ever consumed;
any finite disk eventually fills with dead versions and the file system
stops. "Big enough" only postpones the day — unless you *redefine* dead
versions as a feature (versioning file systems, WAFL snapshots), which is a
different design with its own reclamation story, eventually.

---

## B. Working the mechanisms

**B1.**
**(a)** Marking note: the classification, not the seed, is the answer. The
classes and repairs: **bitmap vs inode disagreement** (free-bitmap phase;
rebuild bitmaps from the inodes), **bad inode state** (inode-state phase;
clear the suspect inode and fix the bitmap), **wrong link count**
(link-count phase; recount references by walking the directory tree and
overwrite the inode's count; an unreferenced but allocated inode goes to
`lost+found`), **duplicate pointers** (duplicate phase; clear the obviously
bad inode or give each inode its own copy of the block), **out-of-range
pointer** (bad-block phase; clear the pointer — nothing smarter is
possible), **malformed directory** (directory phase; verify `.`/`..`, that
referenced inodes are allocated, and that no directory is linked more than
once). Note: `fsck.py` only *generates* the bitmap-disagreement, inode-state,
link-count and directory corruptions — **duplicate pointers and out-of-range
pointers are ch. 42 phases the simulator never produces**, so know those two
from the chapter, not from a run.
**(b)** Bitmaps vs inodes: **trust the inodes** and rebuild the bitmaps —
the inodes (plus indirect blocks) are the ground truth of what is actually
reachable, while bitmaps are derived acceleration structures; rebuilding
derived data from primary data is safe, the reverse invents or destroys
files. Link count vs directory tree: **trust the tree** — the count is again
a derived summary of how many directory entries reference the inode, so fsck
recounts and overwrites.
**(c)** Detect-but-lossy: duplicate pointers (someone must lose the block or
accept a copy), out-of-range pointers (cleared — bytes beyond it are gone),
corrupt inodes (cleared entirely). Undetectable: **valid-looking garbage in
user data** — e.g. the crash state where inode and bitmap committed but the
data block didn't; every metadata check passes, and the file quietly
contains whatever the old block held.
**(d)** Correctness would require knowing user intent and lost data, which
no scan can recover; self-consistency is the strongest property computable
from the disk alone — and even it requires reading *every* inode, indirect
block and directory to rebuild the reference graph, hence O(volume) time.
Ch. 42's verdict: scanning the whole house for keys dropped in the bedroom —
it works, but as disks grew it became prohibitive, motivating journaling's
O(log-size) recovery.

**B2.**
**(a)**
- *During (1):* journal partially written, no TxE. Recovery finds no
  committed transaction (count/marker absent) and ignores it. Home
  locations untouched — as if the append never happened.
- *Between (1) and (2):* same as above: all update blocks are in the
  journal but the commit block is not — the transaction is not committed,
  recovery skips it.
- *Between (2) and (3):* the danger window journaling exists for. The
  journal holds a complete committed transaction; home locations are stale.
  Recovery **replays** I[v2], B[v2], Db to their final addresses — redo
  logging — and the append survives.
- *During (3):* some home writes done, some not. Recovery cannot tell and
  does not care: replaying the whole transaction is idempotent, so the
  half-checkpointed blocks are simply written again.
- *After (3) (before/after free):* structures are consistent on disk; replay,
  if the transaction is still marked live, is redundant but harmless. After
  the free step the journal space is reusable and recovery ignores it.
**(b)** The prevented failure is A4's: one big write lets the disk persist
TxB…TxE *without* Db; the transaction then has matching begin/end IDs — the
IDs are metadata *about* the transaction and say nothing about whether the
middle blocks' contents arrived — so ID-matching recovery replays garbage,
possibly over critical structures. With a **transaction checksum** of the
log blocks' contents stored in TxB/TxE, recovery recomputes the checksum
over what is actually in the journal and discards the transaction on
mismatch: the write can then be issued in one go safely. The two-step
protocol was refusing to assume the disk persists the pieces of a request
in order — the checksum replaces that ordering assumption with a content
check. (This is the Prabhakaran et al. tweak that Linux ext4 adopted.)
**(c)** Data journaling: journal TxB, I, B, Db, TxE = **5 journal writes**,
then checkpoint I, B, Db = **3**, total **8** (the later free step updates
the journal superblock). Ordered: Db in place = **1**, journal TxB, I, B,
TxE = **4**, checkpoint I, B = **2**, total **7**, with Db written **once**.
For large sequential writes, data journaling writes every byte twice (log
then home) plus a seek between the two regions — sustained bandwidth is at
best halved, which is exactly why ordered mode is the common default.
**(d)** Recovery sees a committed transaction containing I[v2] and B[v2],
replays them, and produces a **metadata-consistent** file system in which
the inode's new pointer addresses a block that never received Db — the file
ends in garbage (whatever previously occupied that address). This is the
silent case fsck also cannot catch. The rule that prevents it: data is
written and **completed before the commit block is issued** (steps may be
*issued* concurrently with journal writes, but Db must be on disk before
TxE is) — "write the pointed-to object before the object that points to it."
**(e)** Directory `foo`'s data (metadata!) at block 1000 is journaled in
Tx1. The directory is deleted; block 1000 is reallocated to file `bar`,
whose *data* — under ordered journaling — is written in place, not
journaled, while Tx2 journals only bar's inode. Crash; recovery replays the
log in order, including Tx1's copy of block 1000 — overwriting bar's live
data with defunct directory contents. **Revoke records**: deleting the
directory appends a revoke for block 1000; recovery scans revokes first and
suppresses replay of any revoked block, so the stale directory image is
never restored.

**B3.**
**(a)** D = F/(1−F) × R_peak × T_position, with R·T = 250 MB/s × 8 ms
= 2 MB:
50% → 1 × 2 MB = **2 MB**; 90% → 9 × 2 MB = **18 MB**;
99% → 99 × 2 MB = **198 MB**.
**(b)** T_write = 0.008 + 4/250 = 0.008 + 0.016 = 0.024 s;
R_eff = 4 MB / 0.024 s ≈ **167 MB/s ≈ 67% of peak**.
**(c)** (i) **Buffering that much dirty state widens the crash-loss
window** — a gigabyte of acknowledged-but-unwritten updates is 4+ seconds of
work lost on failure (and `fsync` of anything forces the segment out early,
destroying the whole scheme for sync-heavy workloads). (ii) **Memory cost
and burstiness**: a 1 GB in-memory segment plus multi-second write bursts
that starve concurrent reads; the cleaner must also read and rewrite in
gigabyte units. (Also acceptable: latency of the first byte, and diminishing
returns — 67% → 99.8% is not worth 250× the buffer.)
**(d)** Cleaning M segments at utilisation u yields M(1−u) segments of free
space, so freeing one segment's worth requires reading **1/(1−u)** segments
and writing back **u/(1−u)** segments of live data:
u = 0.5 → read 2, write 1 → **3 segment-I/Os** per segment freed;
u = 0.8 → read 5, write 4 → **9**;
u = 0.9 → read 10, write 9 → **19**.
Policy: clean **cold, mostly-dead segments** when possible (ch. 43's
hot/cold segregation — wait on hot segments, since their blocks are dying
anyway); and as the disk fills, average u rises, cleaning I/O per freed
segment explodes, and write performance collapses — the effect behind the
Seltzer critique and C2.

**B4.**
**(a)** Four one-block writes: each `write` persists a data block, a fresh
inode, *and* a fresh imap chunk — 3 blocks apiece, so 4 data + 4 inode + 4
imap = **12 blocks** (the log keeps all four inode versions; only the last is
live). Counting the create that precedes them (dir data + dir inode + file
inode + imap = 4), the run writes log blocks 4–19 = **16** in total. One
four-block write: 4 data blocks + **one** inode + **one** imap chunk = **6**
(log blocks 4–13 = **10** with the create). LFS's efficiency materialises only
when updates are **batched in memory** before the segment is written — issued
singly (or fsync'd), every logical write drags its metadata with it.
**(b)** A link writes the **directory's** new data block and inode (the new
(name, i-number) entry), a new version of the linked file's inode (link
count incremented), and the imap piece mapping both inodes. Cheap because
the directory still stores only the *i-number*, which never changes — the
imap absorbs the moving inode locations, so no other directory referencing
the file needs touching.
**(c)** Live: look up the summary's (k, 2), read k's inode via the imap
(k → A9), check pointer[2]. If it is **A5**, the block is live — the file's
current version-of-record still claims it. If it is **A12**, the file has
been updated and A5 is garbage: cleanable. Shortcut: LFS bumps a **version
number** in the imap when a file is deleted or truncated and stamps versions
into segments; a version mismatch condemns the whole entry without reading
any inode.
**(d)** Two CRs at opposite ends of the disk, updated **alternately**, so a
crash during a CR write still leaves one intact CR; each CR write is
bracketed by **timestamps in header and trailer**, so a torn CR is detected
by inconsistent timestamps and the most recent *consistent* CR is used.
That bounds loss to ~30 s; **roll-forward** then reads past the CR's end of
log through subsequent segments, adopting any valid complete updates found,
recovering much of the final seconds as well.

---

## C. Discussion and design critique

**C1.** Marking notes — the mechanism, not the slogan:
- **The failure:** within one large issued write, the disk's internal
  scheduler may persist TxB, the metadata blocks and TxE, and crash before a
  middle block (the data) is durable. Matching IDs *cannot* detect this: the
  IDs live in TxB/TxE, which made it to disk; the missing block is arbitrary
  content the recovery scan has no way to validate. The transaction is
  structurally perfect and semantically garbage.
- **Consequences:** recovery *actively copies* the garbage to its final
  address. Ch. 42's escalation: unpleasant for user data, catastrophic if
  the block was destined for a critical structure — a superblock or
  directory — since replay corrupts it with high confidence precisely when
  the system was mid-commit. "Crashes are rare" mis-prices this: the cost is
  not frequency but silent, unbounded damage at recovery time.
- **The fix:** put a **checksum of the transaction's contents** in TxB/TxE.
  Recovery recomputes and discards on mismatch; now all five writes can be
  issued together, saving the rotation — safely. This is not hypothetical:
  it is Prabhakaran et al.'s optimisation, adopted in ext4 and shipped on
  millions of machines. The engineer's instinct (the wait is expensive) was
  right; the risk analysis (recovery will cope) was wrong; the repair is a
  content check, not bravado.
- **Residual assumption:** even the checksummed protocol needs the
  *checkpoint* not to overtake the *commit* — ordering enforced with write
  barriers/cache flushes. The chapter's aside records disks that lied about
  or ignored barriers; on such hardware every journaling variant is built on
  sand ("the fast almost always beats out the slow, even if the fast is
  wrong").
- **Recommendation:** reject as stated; adopt with the checksum. Conditions:
  if the device offers no trustworthy flush/barrier, neither version is
  sound and the design question moves down a layer; if transactions are tiny
  and the device has a non-volatile write cache, the saved rotation may not
  matter and the simple two-step protocol's transparency can win.

**C2.** Marking notes — the expected shape:
- **LFS's brilliance here:** the workload is FFS's worst case — each small
  create/append touches inode bitmap, inode, directory data, directory
  inode, data block; FFS pays scattered small writes (plus, journaled, the
  log traffic too), while LFS batches all of it into one sequential segment
  write. On pure throughput of small updates, LFS wins by design.
- **The cost that bites:** `fsync()` per message forces segments (or partial
  segments) out constantly, shrinking the batch that amortises positioning —
  B4(a)'s effect — and mail's constant delete/re-create churn keeps segment
  utilisation mixed-hot, maximising cleaning cost per freed segment
  (B3(d)); the cleaner's I/O then competes with foreground traffic. This is
  precisely Seltzer et al.'s measured attack ("particularly for workloads
  with many calls to fsync, such as database workloads"), which ch. 43
  credits with limiting LFS's initial impact.
- **The measurement:** replay a captured mail trace (with real fsync
  frequency and file-size distribution) against both file systems on the
  target disk at realistic *fullness* (cleaning cost depends on u), and
  measure sustained messages/s and tail latency over hours, not seconds —
  long enough for the cleaner to reach steady state. Short benchmarks
  flatter LFS; that is the historical lesson.
- **Verdict-flippers:** hardware where positioning is cheap relative to
  transfer (SSDs) revives LFS's other advantage (no small-write penalty,
  device-friendly sequential writes); ample free space (low u) tames
  cleaning; group-commit mail servers (batched fsync) remove the forcing;
  conversely a nearly-full disk or strict per-message durability entrenches
  the journaling design. Either final choice earns credit if these
  conditions are stated; an unconditional verdict does not.

**C3.** Expected points:
- **Commit only at quiescence** buys a trivially correct rule — no
  transaction ever splits a system call — at the cost of latency (a call
  can stall waiting for others to drain) and lost concurrency near commit.
- **Conservative `MAXOPBLOCKS` reservation** makes deadlock-free admission
  a one-line check, but under-utilises the log: calls that would fit are
  made to wait because the *worst case* wouldn't.
- **Journaling data** gives clean redo-only recovery and sidesteps ordered
  mode's data-before-commit subtleties and revoke machinery entirely —
  bought with doubled write traffic, acceptable for a teaching system,
  ruinous for a production file server.
- **Split large writes:** a big `write()` is a sequence of transactions, so
  a crash can persist a *prefix* — atomic per transaction, not per call.
  Metadata is always consistent (no fsck), but applications get no
  all-or-nothing guarantee on data; that contract surprise is the Pillai
  et al. observation writ small.
- **Judgement:** for xv6 — the right point: the entire crash-consistency
  story is auditable in an afternoon, which is its pedagogical job. For
  ext4 — indefensible: real workloads demand ordered mode, per-call
  batching without quiescence stalls, revoke records, and checksummed
  commits; i.e. exactly the machinery ch. 42 catalogues. Good answers name
  the pattern: simplicity is a *design budget*, and xv6 and ext4 spend it
  on different masters.
