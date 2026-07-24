> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 18 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their
> working; for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. FALSE.** Allocation structures are consulted only when allocating. The
inode (and any indirect blocks) already contain everything a read needs; if
the inode points to a block, it is allocated by invariant. Ch. 40 flags this
as a standard student confusion.

**A2. FALSE.** Names live in **directory entries** — (name, inode-number)
pairs in the directory's data blocks. The inode holds size, ownership, times
and block pointers, but not the name; that is precisely why a file can have
several names (hard links) and one link count.

**A3. FALSE.** Direct pointers cover 12 × 4 KB = 48 KB; doubling to 24 adds
another 48 KB. The maximum file size is dominated by the indirect terms
(a single indirect adds 4 MB, a double indirect ~4 GB), so the change is
negligible — its real effect is on how many *small* files avoid an indirect
block, at the cost of a bigger inode.

**A4. TRUE.** In linked allocation each data block ends with a pointer to the
next; FAT keeps exactly those next-pointers in one table indexed by block
address, held in memory so random access does not chase pointers on disk. A
consequence worth stating: with per-file metadata in the directory entry and
no inodes, hard links are impossible.

**A5. TRUE.** Batching merges repeated updates to the same block (e.g. one
inode-bitmap write for two file creations); scheduling reorders what remains;
and some writes are avoided entirely (create then delete within the buffering
window writes nothing). This is the performance half of the
durability/performance trade-off — see C2.

**A6. FALSE.** FFS's explicit strategy was to keep the interface — the same
`open()`/`read()`/`write()` — and change the *implementation*. That
compatibility is a large part of why it could win, and it set the pattern for
file-system research since.

**A7. FALSE.** Larger blocks amortise positioning cost per byte transferred,
but waste space to **internal fragmentation**: ch. 41 notes typical files
around 2 KB against 4 KB blocks — roughly half the disk wasted — which is why
FFS introduced 512-byte sub-blocks (and dodged the copy cost by having `libc`
buffer into 4 KB writes).

**A8. FALSE.** The per-group superblock copies are for **reliability**: if
the primary copy is corrupted, the file system can still be mounted from a
replica. Mounting reads one superblock either way.

---

## B. Working the structures

**B1.**
**(a)** Marking note: what matters is the mapping from state change to
operation — a new inode + directory-entry pair with type `d` is `mkdir` (its
link count starts at 2 because of `.` and the parent's entry); a new inode of
type `f` referenced from a directory block is `creat`; a data-bitmap bit plus
a pointer appearing in an existing inode is `write`; a second directory entry
naming an existing inode number, with that inode's link count incremented, is
`link`; an entry disappearing with the count decremented (and structures
freed only when it reaches 0) is `unlink`.
**(b)** Both allocators are **lowest-free-first**: the simulator scans the
bitmap and picks the first clear bit, so freed low-numbered slots are reused
before untouched higher ones.
**(c)** With the data region full, any operation needing a data block fails —
`write`, and any `mkdir`/`creat` that must *grow* a directory. But `creat`
into a directory with slack in its existing block still succeeds (an inode
plus a directory entry need no new data block), as does `link`. That is the
conceptual point — but note the simulator does **not** let you watch it play
out: `vsfs.py` calls `exit(1)` ("File system out of data blocks…") the instant
the data bitmap hits zero, so it aborts *at* exhaustion rather than reporting
`ENOSPC` on the next data-consuming operation. Reason it through from the
structures each operation touches; the tool won't run past the wall.
**(d)** With two links, `unlink` removes one directory entry (one data-block
write) and decrements the inode's link count (one inode write). Nothing is
freed — the other name still references the inode — so neither bitmap
changes. Bitmaps change only when the count reaches zero.

**B2.**
**(a)** Two steps, per ch. 40:
`blk = (inumber × inodeSize) / blockSize`, then
`sector = (blk × blockSize + inodeStartAddr) / sectorSize`.
Inode 32: offset 32 × 256 = 8192 → blk 2; (8192 + 12288) / 512 = **sector 40**.
Inode 50: offset 50 × 256 = 12800 → blk 3; (12288 + 12288) / 512 = **sector 48**.
(16 inodes per 4 KB block: inodes 48–63 share block 3.)
**(b)** 5 blocks × 16 inodes = **80 files** — one inode per file, and the
inode table is fixed at format time. (A larger disk could simply be formatted
with a larger table.)
**(c)** Pointers per indirect block: 4096 / 4 = 1024.
(i) 12 × 4 KB = **48 KB**.
(ii) (12 + 1024) × 4 KB = **4144 KB ≈ 4.05 MB**.
(iii) (12 + 1024 + 1024²) × 4 KB = 4,198,448 KB ≈ **4 GB**.
A triple indirect adds 1024³ × 4 KB = **4 TB** of further reach.
**(d)** Offsets → block numbers (÷ 4096, floor):
(i) 20,000 → block 4: direct pointer → **1 read** (the data block).
(ii) 1,000,000 → block 244: blocks 12–1035 resolve via the single indirect →
**2 reads** (indirect block, then data).
(iii) 100,000,000 → block 24,414: beyond 1035, within the double-indirect
range → **3 reads** (double indirect, indirect, data).
**(e)** At most two reads means at worst one pointer block on the path:
everything reachable via direct or single-indirect pointers —
(12 + 1024) × 4 KB = **4144 KB**.

**B3.**
**(a)** Root inode, root data, `home` inode, `home` data, `alice` inode,
`alice` data, `notes.txt` inode = **7 reads**. Generally: two reads (inode +
data) per directory level including the root, plus one read for the final
inode — `2(d + 1) + 1` for `d` directories below the root. The open cost is
proportional to path length; large directories make it worse (several data
blocks to scan per level).
**(b)** Each `read()` costs inode read + data read + inode write
(last-accessed update) = 3 I/Os; two blocks → **6 I/Os** (plus the open).
**(c)** **Four** distinct structures, in seven I/Os: the inode **bitmap** (read
to find a free inode, write to claim it), the **new inode** (read–modify–write
to initialise), the **directory data** block (write to add the name→inumber
entry), and the directory **inode** (read then write — new length, mtime).
That is four writes plus three reads for the create proper. The extra read of
the inode block occurs because inodes are smaller than a block: the file
system must **read–modify–write** the 4 KB block containing the new 256-byte
inode, even though that inode's own bytes are about to be overwritten.
**(d)** The cache must hold the inodes and data blocks of `/`, `home` and
`alice`, plus `notes.txt`'s inode — everything the first traversal touched.
The cost reappears when the directory working set exceeds the cache: e.g. a
workload opening files spread over very many directories (or one enormous
directory whose data blocks don't fit), so traversal misses return.

**B4.**
**(a)** With `-L 4`, `/a`'s 40 blocks are split into 4-block chunks spread
across successive groups, so filespan is large (**372** in this run); with
`-L 100` the large-file exception never bites, so the blocks pack as
contiguously as they can and filespan collapses to **59**. Note it does *not*
collapse to a single group: a group here holds only ~30 data blocks, so a
40-block file still spills into the next group (hence 59, not 0) — the point
is the contrast, 372 vs 59, not that everything lands in one place. Neither
extreme is right: small `-L` hurts sequential reads of *this* file (a seek
per chunk); large `-L` lets one big file fill its group and evicts the
namespace locality of everything else in that directory.
**(b)** For 50%, transfer time must equal positioning time: 200 MB/s × 5 ms =
**1 MB**. In general D = F/(1−F) × R × T:
90%: 9 × 200 MB/s × 0.005 s = **9 MB**. 99%: 99 × 1 MB = **99 MB**.
**(c)** The rule was structural: the 12 direct blocks (**48 KB**) stay in the
inode's group; thereafter each *indirect block and everything it points to*
goes to a new group — with 4 KB blocks and 4-byte addresses that is 1024
blocks, i.e. **4 MB per group**.
**(d)** F/(1−F) × R × T grows with R at fixed T, so chunks must keep growing
(the chapter's point: mechanical costs get relatively more expensive every
year). Any locality-vs-contiguity policy must therefore be *re-parameterised
over time* — a fixed chunk size that was 90%-efficient becomes steadily less
so as transfer rates rise.

---

## C. Discussion and design critique

**C1.** Marking notes — the strongest case against runs roughly:
- The measurement table cuts both ways: most *files* are small, but **most
  bytes — and hence most data I/O — live in large files**. Optimising the
  index for file count optimises metadata operations, not the traffic the
  disk actually carries.
- For exactly those large files the multi-level index is at its worst: one
  4-byte pointer per 4 KB block is a large, seek-prone metadata structure,
  and a 4 GB file drags a million pointers behind it.
- The chapter's own aside supplies the alternative: **extents** (pointer +
  length) are compact, match the contiguous layout every allocator is trying
  to achieve anyway, and are what ext4 and XFS actually adopted — evidence
  the field judged the classic design insufficient.
- Conditions under which the claim survives: workloads dominated by small
  files and metadata operations; fragmented free space (extents degrade
  towards pointers exactly when contiguity is unavailable); and the value of
  per-block flexibility (any block can go anywhere) for a simple,
  predictable implementation. Best answers note the designs converge: 12
  direct pointers *are* an optimisation for the small-file common case, and
  extent trees keep an indirect-like structure for worst cases.
A recitation of how the multi-level index works earns little; the marks are
for turning the chapter's own data against its conclusion, then bounding the
attack.

**C2.** The case against: 5–30 s of buffered writes is 5–30 s of *silent*
data loss on crash, chosen unilaterally by the file system; the application
cannot tell which of its completed `write()`s survived; and the escape
hatches (`fsync()`, direct I/O) push the burden onto exactly the applications
least able to get it right — the chapter's database example insists on
controlling durability itself. Worse, the interface makes the *default*
unsafe and the safe path opt-in, inverting fail-safe expectations. The case
for: most data is re-creatable or low-value (the browser-download example);
batching, scheduling and write-avoidance are large real wins (A5); and
making every write durable would put a 5–10 ms positioning cost on every
call — orders of magnitude slower. Reconciliation to look for: durability is
a *per-application* requirement, so the right policy is a fast default plus
an explicit, honoured durability primitive — i.e. what Unix does — with the
caveat that the default's window should be tunable and the primitive must
actually reach the platter (a promise week 21 revisits). Answers that argue
only one side, or "just make everything durable", earn little.

**C3.** Layering pays in the **buffer cache**: because bread/bwrite/brelse
serialise all access to a disk block and guarantee a single in-memory copy,
every layer above can treat "the block" as a memory object with mutual
exclusion for free — the inode and directory code never reasons about I/O
concurrency. The reach-through is in **logging**: `log_write` must **pin**
the buffer in the cache until commit (and absorption depends on recognising
repeated writes to one block), so the logging layer depends on cache
*internals* — eviction policy and reference counts — not just its interface;
equally, callers must bracket operations with `begin_op`/`end_op`, so the
"transparent" log leaks into every system call above it. The general lesson:
clean layers hold only while the invariants a layer needs can be expressed
in the interface below; crash-ordering invariants famously cannot, which is
why crash consistency (week 21) keeps breaking abstractions. Credit any
correct pair of examples; the named ones are the cleanest.
