# Exercise Sheet 18 — File system implementation: vsfs and FFS

**Attempt after Week 18.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise18-solutions.md`](solutions/exercise18-solutions.md).

**This sheet leans on:** OSTEP ch. 40–41; xv6 book §10.1–10.6; ch. 37 (week 16)
for seek/rotation costs.

**You will need:** the `vsfs.py` (ch. 40) and `ffs.py` (ch. 41) simulators from
the `ostep-homework` repo. §B2–B3 are pen-and-paper — no tooling.

> **Note.** §B2–B3 are deliberately arithmetic-heavy: inode indexing and
> path-resolution costs are the most-drilled calculation family in the
> Cambridge bank (five past-paper questions unlock this week alone).

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** Reading an existing file causes the file system to consult the data
bitmap, to confirm that the file's blocks are allocated.

**A2.** A file's name is stored in its inode.

**A3.** Doubling the number of direct pointers in an inode roughly doubles the
maximum file size.

**A4.** The FAT file system is a linked-allocation scheme whose per-block next
pointers have been lifted into a single table.

**A5.** Buffering writes in memory for several seconds can reduce the total
number of disk writes performed, not merely delay them.

**A6.** FFS achieved its performance gains partly by improving the file-system
API.

**A7.** Larger blocks are always better for file-system performance.

**A8.** FFS keeps a copy of the superblock in every cylinder group in order to
reduce seek time when mounting.

---

## B. Working the structures

**B1. vsfs under the microscope.**
Run the ch. 40 simulator: `python3 vsfs.py -n 6 -s 16`.
  (a) For each state transition shown, name the operation that must have
      caused it (`mkdir`, `creat`, `write`, `link`, `unlink`, …) and say which
      on-disk structures changed.
  (b) Re-run with `-r` (you see the operation, predict the state). From a few
      runs, characterise the simulator's **allocation policy** for inodes and
      data blocks: which free entry does it pick?
  (c) Run with very few data blocks (e.g. `-d 2`) and watch the data region
      fill. Which operations still succeed once no data block is free, and why
      can files still be created? (Note: `vsfs.py` calls `exit` the instant
      the data bitmap empties rather than modelling `ENOSPC`, so it aborts *at*
      exhaustion — reason from the structures each operation touches rather
      than expecting the simulator to run on past the wall.)
  (d) Explain why a successful `unlink` of a file with two hard links changes
      exactly one directory data block and one inode — and no bitmaps.

**B2. Inode arithmetic.**
Take vsfs's geometry: 4 KB blocks, 256-byte inodes, 512-byte sectors, and an
inode table starting at byte address 12 KB (superblock at 0 KB, bitmaps at
4 KB and 8 KB, five inode-table blocks).
  (a) Compute the sector address the file system must read to fetch
      **inode 32**, showing the two-step calculation. Then do the same for
      **inode 50**.
  (b) How many files can this file system hold, and why?
  (c) Now the index. Assume an inode holds **12 direct pointers**, one single
      indirect and one double indirect pointer; pointers are 4 bytes. Compute
      the maximum file size (i) using direct pointers only, (ii) adding the
      single indirect, (iii) adding the double indirect. State what a triple
      indirect pointer would add.
  (d) With the file's inode already in memory, how many **disk reads** are
      needed to fetch the byte at offset (i) 20,000, (ii) 1,000,000,
      (iii) 100,000,000? Show which pointer chain each offset resolves
      through.
  (e) What is the largest file such that *every* byte is reachable in at most
      two disk reads (inode in memory)?

**B3. The cost of names.**
Assume vsfs, cold caches (only the superblock is in memory), one data block
per directory, and the chapter's accounting: a `read()` costs an inode read, a
data read, and an inode write (last-accessed time); an allocating `write()`
costs five I/Os.
  (a) Count the disk reads performed by `open("/home/alice/notes.txt")`.
      Generalise: for a path with `d` directories below the root (here `home`
      and `alice`, so `d = 2`), how many reads?
  (b) The file is 8 KB (two blocks). Count the I/Os for reading it in full
      after the open.
  (c) Creating `/home/alice/new.txt` writes four distinct structures before
      any data is written. Name them. Why does the trace also show a *read* of
      the block holding the newly allocated inode, even though the inode is
      about to be initialised?
  (d) A second `open("/home/alice/notes.txt")` immediately afterwards
      performs no disk I/O at all. Say precisely what must now be in the page
      cache for that to hold, and which single change to the workload would
      make the *directory-read* cost reappear at scale.

**B4. FFS locality and amortization.**
  (a) Run `python3 ffs.py -f in.largefile -L 4 -T -c`, then again with `-L 100`.
      Predict, then confirm: how does the **filespan** of `/a` change, and
      why is *neither* extreme obviously right?
  (b) A disk positions (seek + rotate) in 5 ms and transfers at 200 MB/s.
      How large must FFS's large-file chunks be to achieve 50% of peak
      bandwidth? 90%? 99%? Show the working.
  (c) FFS did not use that calculation. State the rule it actually used, and
      compute the resulting chunk sizes for 4 KB blocks and 4-byte disk
      addresses.
  (d) Transfer rates improve quickly; positioning improves slowly. What does
      that trend do to your answers in (b), and what does it imply for any
      layout policy that trades contiguity for locality?

---

## C. Discussion and design critique

*This week's discussion questions ask you to argue the strongest case **against**
a claim the chapter makes. A good answer states the claim fairly, attacks it with the
chapter's own evidence where possible, and ends with the conditions under
which the claim survives.*

**C1.** Ch. 40 justifies the imbalanced multi-level index thus: *"most files
are small … it makes sense to optimize for this case."* Using the chapter's
own measurement summary (most files are small, **but** most bytes are stored
in large files, and file systems are ~half full), argue the strongest case
against this design decision. Your argument should engage with the
extent-based alternative the chapter describes. Finish with the conditions
under which the classic 12-direct-pointer design is nonetheless the right
call.

**C2.** Ch. 40 reports that most file systems buffer writes in memory for
five to thirty seconds before issuing them. Argue the strongest case against
this default — be concrete about what is lost, for whom, and what the
interface forces applications to do about it. Then state the workload
properties under which the default is clearly correct, and reconcile your two
answers as a single policy recommendation.

**C3.** xv6 ch. 10 opens by promising "situations where well-chosen
abstractions at lower layers ease the design of higher ones." Identify one
place in §10.1–10.6 where the layering genuinely pays, and one place where a
higher layer must reach *through* an abstraction into a lower one for
correctness. What does the second example tell you about layered designs in
general?
