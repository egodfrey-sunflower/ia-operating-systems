# Week 21 — Crash consistency: fsck, journaling, and LFS

> **Part III: Persistence** — resumed; ch. 42 depends on ch. 40 (week 18),
> not on the security weeks. Week 21 of 27.

## What you'll learn

Week 18 counted the I/Os in an allocating write: inode, data bitmap, data
block. Chapter 42 asks the question that count begs — what if the machine
dies *between* them? The disk commits one write at a time, so a crash can
strand the file system in any of six partial states, some benign (data block
only: as if nothing happened), some ugly (bitmap only: a space leak), one
actively dangerous (inode and bitmap but no data: consistent-looking
metadata pointing at garbage). This is the **crash-consistency problem**,
and the chapter works two solutions. **fsck** lets inconsistencies happen
and repairs metadata at boot — a whole-disk scan whose phases (superblock,
free bitmaps rebuilt by trusting inodes, inode state, link counts,
duplicates, bad pointers, directory checks) you should know, along with its
two fatal limits: it cannot tell metadata-consistent garbage from data, and
it is O(disk volume) to fix a three-block mistake. **Journaling**
(write-ahead logging) is the modern answer: write a note describing the
update, *commit* it, then checkpoint the real structures. The details are
the point: why TxE is committed separately (or the transaction checksummed —
the ext4 optimisation), why **ordered/metadata journaling** must write data
*before* commit, why deleted-then-reused blocks need **revoke records**, and
how batching and the circular log keep the overhead tolerable.

Chapter 43 takes the logging idea to its extreme: if big caches serve most
reads, performance is write performance — so **never overwrite anything**;
buffer every update into a multi-megabyte **segment** and write it
sequentially to free space. You will re-meet week 18's amortization
arithmetic as the segment-sizing formula, then hit the two problems
log-structuring creates. Finding things: inodes now move, so an **inode
map** (imap) maps i-number → latest inode address, itself written into the
log and found via a fixed **checkpoint region** — indirection that also
kills the recursive-update problem. Reclaiming space: old versions are
garbage, so a **cleaner** reads partly-dead segments, writes back the live
blocks (liveness judged from the **segment summary** against the imap), and
frees the rest — and cleaning policy, not mechanism, is where LFS was
attacked (the Seltzer papers) and where its descendants diverge.

The two chapters are one argument seen from both ends: ext3 bolts a small
log onto an overwrite-in-place file system; LFS makes the log *be* the file
system, which is copy-on-write — ZFS, btrfs and WAFL's lineage, and (next
week) exactly how the flash translation layer inside every SSD works. The
xv6 cross-reading closes the loop from week 18: §10.4–10.6 is ch. 42's
protocol as ~300 lines you can hold in your head — header-block commit,
group commit, `begin_op`/`log_write`/`end_op`, absorption, and recovery as
"replay if the header count is non-zero".

**Key ideas:** crash scenarios · fsck's phases and why it's too slow ·
write-ahead logging · journal write vs commit vs checkpoint · transaction
checksums · ordered/metadata journaling and the data-first rule · revoke
records · circular log · segments and write buffering · imap and the
checkpoint region · segment-summary liveness · cleaning policy · crash
recovery via CR pairs and roll-forward.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 42** | Crash Consistency: FSCK and Journaling | 21 | 3.0 h |
| 2 | **OSTEP ch. 43** | Log-structured File Systems | 16 | 2.3 h |
| 3 | **xv6 book §10.4–10.6** | The write-ahead log, now in full (first read week 18) | 3 | 0.4 h *(second pass)* |

**Paper (required):** ★ Rosenblum & Ousterhout (1991), *The Design and
Implementation of a Log-Structured File System*, SOSP — OSTEP: "cited by
hundreds of other papers and inspired many real systems". Read it against
McKusick's FFS paper from week 18: it is a direct rebuttal, and its cleaning
cost model is what sheet 21 §C2 argues about.

**Paper (optional):** Pillai et al. (2014), *All File Systems Are Not
Created Equal*, OSDI — ch. 42's missing half: crash consistency as seen by
*applications*, which turn out to depend on guarantees file systems don't
make. The relief valve if the week runs long; unmissable if you liked C1.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise21.md`](../exercises/exercise21.md) — budget 3 h. Uses the `fsck.py` (ch. 42) and `lfs.py` (ch. 43) simulators. Self-mark against [`../exercises/solutions/exercise21-solutions.md`](../exercises/solutions/exercise21-solutions.md) |
| **Lab** | [`../labs/lab08-security/`](../labs/lab08-security/) **ends** · [`../labs/lab09-crash/`](../labs/lab09-crash/) **starts** — 6.0 h combined (4.5 h finishing lab 8, 1.5 h opening lab 9). Lab 9's crash-point experiment is ch. 42 §42.1 made real |
| **Timed past paper** | `y2018p2q3` — 35 min closed book, then self-mark (~1 h total). **Filesystem arithmetic**: filesystem/inode sizing arithmetic — maximum file size, metadata and directory-entry sizing, and block-reads-to-offset. Part (b) asks whether Unix file access is ACL- or capability-based, and (c)(iv) contrasts storing protection information with the file versus in the directory entry — both need week 20's access matrix. Marks 4 + 2 + (4+3+3+4) = 20 |
| **Untimed drill** | From the week-20 protection pool, take `y2006p1q2` and `y2007p1q2` this week — the six access-matrix questions are spread across weeks 21–24 so they are spaced rather than crammed |

## Week load

```
OSTEP ch. 42-43     37pp ÷ 7  =  5.3 h
xv6 §10.4-10.6 (2nd pass)       =  0.4 h
Rosenblum & Ousterhout [M]    =  1.5 h
Exercise sheet 21             =  3.0 h
Timed paper (y2018p2q3)         =  1.0 h
Lab 8 ends · Lab 9 starts     =  6.0 h
                                ------
                                17.2 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

The xv6 sections are costed as a re-read: you met them at first-pass level
in week 18, and this week's job is recognising ch. 42's protocol in them,
not decoding them cold. If the week runs long, the Pillai paper is already
optional; the LFS paper is not — §C2 depends on it.

## Notes for the curious

- **The revoke record earns its keep.** Stephen Tweedie (ext3): "Everything
  to do with delete is hairy." The delete-then-reuse replay bug in ch. 42 is
  the best single example in the course of a corner case that only shows up
  when you think adversarially about *recovery*, not normal operation.
- **Write barriers are a trust boundary.** The chapter's aside notes some
  disks acknowledged writes from cache and even ignored barrier requests —
  "the fast almost always beats out the slow, even if the fast is wrong."
  Sheet 21 §C1's proposal turns on exactly this machinery.
- **xv6 journals data too.** Every block written through `log_write` goes to
  the log — data journaling, with a tiny log, which is why large writes are
  split across transactions (`MAXOPBLOCKS`) and why a big `write()` is *not*
  atomic in xv6. Sheet 21 §C3 chases the consequences.
- **Cleaning was the controversy.** Seltzer et al.'s measurements of
  cleaning costs under fsync-heavy workloads "perhaps limited LFS's initial
  impact"; WAFL sidestepped the fight by making old versions a feature
  (snapshots). The idea still won — btrfs, ZFS, F2FS, and every SSD's FTL
  are log-structured at heart, which is next week's opening move.
