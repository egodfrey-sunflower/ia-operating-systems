# Exercise Sheet 21 — Crash consistency and log-structured file systems

**Attempt after Week 21.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise21-solutions.md`](solutions/exercise21-solutions.md).

**This sheet leans on:** OSTEP ch. 42–43; xv6 book §10.4–10.6; Rosenblum &
Ousterhout (1991). It draws on ch. 40–41 (week 18) for on-disk structures and
the amortization argument.

**You will need:** the `fsck.py` (ch. 42) and `lfs.py` (ch. 43) simulators
from the `ostep-homework` repo. §B2–B3 are pen-and-paper.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** If a crash leaves a freshly written data block on disk but neither
the updated inode nor the updated bitmap, the file system is inconsistent.

**A2.** Running fsck after a crash recovers the data that was being written
when the crash occurred.

**A3.** In data journaling, checkpointing may begin as soon as all writes to
the journal have been *issued*.

**A4.** Issuing the five blocks of a transaction (TxB, three update blocks,
TxE) to the disk as one large write is safe, because the disk completes the
pieces of a single request in order.

**A5.** Under ordered (metadata) journaling, each user data block is written
to disk once; under data journaling, twice.

**A6.** LFS's principal performance benefit is faster reads, since the log
keeps each file's blocks near its inode.

**A7.** In LFS, moving a file's inode to a new location on disk forces the
directory that names the file to be rewritten as well.

**A8.** Given a large enough disk, an LFS could dispense with its cleaner.

---

## B. Working the mechanisms

**B1. fsck as detective.**
Use `fsck.py`, which corrupts a random file-system image and challenges you
to spot the damage.
  (a) First run `python3 fsck.py -D` (no corruption) on a few file-system
      seeds — vary `-s` (e.g. `-s 1`, `-s 2`); note that `-S` seeds only the
      *corruption* and does nothing under `-D` — until you can read the image
      format fluently. Then run several corrupting seeds
      (e.g. `-S 1`, `-S 3`, `-S 19`, checking with `-c`). Classify each
      inconsistency you find against ch. 42's list of fsck phases
      (superblock, free bitmaps, inode state, link counts, duplicates, bad
      pointers, directory checks), and give the repair for each.
  (b) State fsck's rule for which structure to trust when the allocation
      bitmaps disagree with the inodes, and when an inode's link count
      disagrees with the directory tree. Justify both directions of trust.
  (c) From your runs or ch. 42: which corruptions can a checker detect but
      not repair without losing information, and which can it not even
      *detect*? Give one concrete example of each.
  (d) fsck repairs sometimes destroy information (clearing a suspect inode;
      consigning orphans to `lost+found`). Explain why metadata
      *self-consistency*, rather than correctness, is the only goal a
      checker can honestly pursue — and why even that goal costs O(volume
      size) time to reach.

**B2. Journaling protocols, step by step.**
The canonical append: I[v2], B[v2], Db must reach their home locations; the
journal protocol is (1) journal write, (2) journal commit, (3) checkpoint,
(4) free.
  (a) For **data journaling**, list what is on disk after a crash at each of
      these five instants — during (1), between (1) and (2), between (2) and
      (3), during (3), after (3) — and what recovery does in each case.
  (b) Explain the failure the two-step write of the transaction (TxE
      separately) prevents. The chapter's fix for the resulting extra
      rotation is the **transaction checksum**. State precisely what the
      recovery-time check becomes, and what property of the disk the
      original two-step protocol was refusing to assume.
  (c) Count the disk writes the append costs under (i) data journaling and
      (ii) ordered journaling, separating journal writes from checkpoint
      writes. For a workload that writes large files sequentially, what does
      data journaling do to achievable bandwidth, and why?
  (d) Under ordered journaling, suppose the data write Db were issued but
      the transaction committed *before Db reached disk*, and the machine
      then crashed. Walk through recovery and state the user-visible
      outcome. Which rule of the ordered protocol prevents this?
  (e) Directory data is metadata. Reconstruct ch. 42's block-reuse hazard —
      directory deleted, its block reallocated to a user file, crash, replay
      — and explain how **revoke records** break it.

**B3. LFS arithmetic.**
A disk positions in 8 ms and transfers at 250 MB/s.
  (a) Using the effective-rate model, how much must LFS buffer per segment
      to reach 50%, 90% and 99% of peak bandwidth? Show the formula.
  (b) LFS is configured with 4 MB segments on this disk. What effective
      write rate does it achieve, as an absolute figure and as a fraction of
      peak?
  (c) A colleague proposes 1 GB segments ("closer to 100%!"). Give two
      concrete costs of very large segments that the formula does not show.
  (d) Cleaning: a cleaner reads segments whose live fraction is `u`,
      compacts the live blocks, and writes them out. For u = 0.5, 0.8 and
      0.9, compute how many segments must be *read* to free one segment's
      worth of space, and the total cleaning I/O (read + write) per freed
      segment. What does this say about which segments a policy should
      choose, and about what happens as the disk fills?

**B4. Liveness in the log.**
  (a) Run `python3 lfs.py -L c,/foo:w,/foo,0,1:w,/foo,1,1:w,/foo,2,1:w,/foo,3,1 -o`
      then `python3 lfs.py -L c,/foo:w,/foo,0,4 -o`. Count the blocks each run
      writes to the log, explain the difference, and state what this shows
      about *when* LFS's claimed efficiency actually materialises.
  (b) With `-o -i`, run `python3 lfs.py -L c,/foo:l,/foo,/bar:l,/foo,/goo`. What
      is written when a hard link is created, and why does the imap make
      this cheap compared to a rename-style directory rewrite?
  (c) Paper exercise: a segment's summary block records, for each data
      block, its (inode-number, offset). Given summary entry
      `A5 → (k, 2)`, imap entry `k → A9`, and the inode at A9 whose
      pointer[2] = A5 — is the block at A5 live? Repeat for pointer[2] = A12.
      Give the ch. 43 shortcut that avoids even reading the inode for
      deleted/truncated files.
  (d) LFS writes checkpoint regions only every ~30 s, yet claims to lose
      little on a crash. Explain the two mechanisms that make the CR safe to
      update (why two CRs, and why timestamps at both ends), and what
      roll-forward adds.

---

## C. Discussion and design critique

*This week's discussion questions ask you to **evaluate a proposal** — the
sheet's one "intrepid engineer" outing. Verdicts need conditions attached.*

**C1.** *An intrepid engineer proposes the following.* "Our journaling file
system wastes a disk rotation on every transaction: it writes TxB and the
log blocks, waits, and only then writes TxE. Crashes are rare and recovery
already replays conservatively, so I propose we simply issue all five writes
in one go and eat the microscopic risk. If you're squeamish, note the
recovery scan can sanity-check that a transaction's begin and end markers
carry matching IDs, so a torn transaction will be spotted anyway."

Evaluate this proposal. Address specifically: the exact failure the wait
prevents and why matching TxB/TxE IDs do **not** detect it; what the disk's
internal scheduling is allowed to do to a single large write; how bad the
consequences can get (consider *which* blocks might be replayed); the small
change that makes the proposal essentially correct, and what shipped it;
and what residual assumption about the disk even the fixed version rests on
(the chapter's write-barrier aside is relevant). Conclude with a
recommendation and the conditions under which it would change.

**C2.** LFS versus FFS-with-journaling, under a stated constraint: you run a
mail server whose workload is many small files, constantly created,
appended, `fsync()`ed and deleted. Using Rosenblum & Ousterhout's argument
*and* the cleaning-cost critique (Seltzer et al., cited in ch. 43), argue
which design serves this workload better. Say explicitly: what LFS does
brilliantly here, which cost bites it and why this workload maximises that
cost, what measurement would settle the choice, and which change to the
workload or hardware would flip your verdict.

**C3.** xv6's log commits only when no file-system system call is in
progress, conservatively reserves `MAXOPBLOCKS` per call, journals *data as
well as metadata*, and splits large writes across several transactions.
State what each choice buys in simplicity, and what it costs — in
particular, exactly what atomicity a large `write()` does and does not get,
and what happens to concurrency when the log nears capacity. Is this a
reasonable design point *for xv6*? For ext4?
