# Week 22 — Flash-based SSDs and data integrity

> **Part III: Persistence** — the last of the single-machine storage weeks.
> Week 22 of 27.

## What you'll learn

Chapter 44 opens the storage device back up. NAND flash is genuinely strange
hardware: you **read** and **program** in pages (a few KB), but before any
page can be programmed you must **erase** its whole containing block
(128 KB–2 MB), erasing is milliseconds against tens of microseconds for a
read, and every program/erase cycle wears the block towards death. A
**flash translation layer (FTL)** turns this into the ordinary block device
the file system sees — and the punchline of the chapter is that the good
FTL design is one you already know: **last week's log-structuring, reused
wholesale at device level**. Writes append to the current block; an
in-memory mapping table (the imap's twin) tracks logical→physical
placement; overwrites strand garbage that a **garbage collector** (the
cleaner's twin) compacts; and the device even faces the same
mapping-persistence problem LFS solved with its checkpoint region. On top of
that come the device-specific concerns: **write amplification**, the
mapping-table memory problem and its answers (block mapping, hybrid
log/data mapping with switch/partial/full merges, page mapping plus
caching), **wear leveling**, and the **TRIM** command — an interface change
forced by an implementation. The performance table is worth memorising in
shape: SSDs beat hard disks by ~100× on random I/O, only modestly on
sequential, and random *writes* can beat random reads because the log turns
them sequential — while the cost gap keeps hard disks alive.

Chapter 45 asks what happens when storage lies. The fail-stop model of the
RAID chapter gives way to the **fail-partial** model: disks mostly work but
suffer **latent sector errors** (detected by in-disk ECC — you get an error)
and **silent corruption** (you get wrong bytes and no warning). The
Bairavasundaram studies of 1.5 million drives put numbers on this — roughly
9.4% of cheap drives develop an LSE within three years, 0.5% exhibit
corruption — which is exactly often enough to have to engineer for. LSEs
need only redundancy (and motivate double parity: an LSE met *during* RAID
reconstruction is otherwise fatal). Corruption needs **detection**:
per-block **checksums** (XOR, addition, Fletcher, CRC — the sheet drills the
first three by hand), stored either in out-of-band bytes or packed sectors, checked
on every read. Two failure modes then sharpen the design: **misdirected
writes** defeat a bare checksum and require a physical ID (disk, block)
alongside it; **lost writes** defeat even that — the stale block has a
valid checksum and the right address — and require write-verify or a
checksum kept *above* the data, as ZFS does in its inodes. Because most
data is rarely read, **scrubbing** patrols it in the background; and the
whole apparatus costs almost nothing in space (~0.19%) but real CPU and I/O
time.

The two chapters share one deep habit: stop trusting the layer below, and
engineer the distrust. The FTL assumes flash wears out and manages it; the
checksum machinery assumes the disk misplaces and forgets writes and
catches it. FreeBSD's ZFS chapter is on the shelf as the worked example of
both strands in one production system — copy-on-write from last week,
end-to-end checksums from this one.

**Key ideas:** pages vs erase blocks · read/program/erase · wear-out and
disturbance · the FTL · log-structured FTLs and write amplification ·
garbage collection and over-provisioning · mapping-table size: block,
hybrid, cached · wear leveling · TRIM · SSD vs HDD performance and cost ·
fail-partial: LSEs vs corruption · checksum functions and layout ·
misdirected and lost writes · scrubbing.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 44** | Flash-based SSDs | 24 | 3.4 h |
| 2 | **OSTEP ch. 45** | Data Integrity and Protection | 14 | 2.0 h |
| 3 | **FreeBSD ch. 10** | The Zettabyte Filesystem (ZFS) — *reference*: consult for how CoW + end-to-end checksums combine; don't read cover to cover | — | 1.0 h |

**Paper (optional):** Bairavasundaram et al. (2008), *An Analysis of Data
Corruption in the Storage Stack*, FAST — the 1.5-million-drive study behind
ch. 45's numbers; OSTEP: "the first paper to truly study disk corruption".
The relief valve if the week runs long; read at least the findings lists,
which sheet 22 §B5 and §C3 lean on (the headline figures are reproduced in
ch. 45 itself).

**Paper (also optional):** Prabhakaran et al. (2005), *IRON File Systems*,
SOSP — the other half of ch. 45's empirical substrate. Where Bairavasundaram
measures how often disks lie, this measures how file systems *react* when they
do: it taxonomises detection and recovery policies (ignore, propagate, retry,
repair, remap, or fall back on redundancy) and finds real file systems wildly
and silently inconsistent in which they choose. Take it if the corruption
material grabs you — sheet 21's solutions already reason in the same terms,
classifying failures by what the file system detects and what it can repair.

**Not this week:** ch. 46 is Part III's two-page summary dialogue; it lands
with ch. 47–48 in week 23, where the course turns to distribution.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise22.md`](../exercises/exercise22.md) — budget 3 h. Uses `ssd.py` (ch. 44) and `checksum.py` plus a short code-writing component (ch. 45, like ch. 39, ships a Homework (Code) section). This is the only week in the second half with no required paper, so the sheet runs slightly fuller — §B has five questions |
| **Lab** | [`../labs/lab09-crash/`](../labs/lab09-crash/) — **ends this week.** Budget ~6.0 h — the bulk of lab 9: journaling recovery measurements, and the ordered-vs-data journaling comparison |
| **Timed past paper** | `y2025p2q3` — 35 min closed book, then self-mark (~1 h total). **Access control, applied**: state the access matrix and its two sparse representations, and say which suits a personal laptop; then *construct* an access matrix for read/write over four peripherals from three prose policy statements; then configure users, groups and Unix permission bits to realise three specific access patterns; finally, the consequences of changing root's UID from 0 to 1000. Marks 4 + 5 + 7 + 4 = 20 |
| **Untimed drill** | Newly attemptable this week: `y2022p2q4` (file descriptors as capabilities + a defragmentation-on-modern-hardware critique that needs exactly ch. 41 + ch. 44) — that one in full — and `y2008p1q7` (Unix/FAT32/NTFS metadata), whose NTFS-log-on-flash critique part is now in reach, though **NTFS's MFT structure itself this course never teaches**: attempt that half from first principles and self-mark leniently, as in week 9. From the protection pool: `y2011p2q3` (`y2015p2q3` is week 24's timed paper — leave it) |

## Week load

```
OSTEP ch. 44-45     38pp ÷ 7  =  5.4 h
FreeBSD ch. 10 (reference)    =  1.0 h
Exercise sheet 22             =  3.0 h
Timed paper (y2025p2q3)         =  1.0 h
Lab 9 (ends)                  =  6.0 h
                                ------
                                16.4 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

Carrying no required paper is deliberate: it is what pays for lab 9's closing
week, where the crash campaign and the journaling-mode comparison both land.
The Bairavasundaram paper is optional and fits only if the lab goes smoothly.

## Notes for the curious

- **Ch. 44 is ch. 43's vindication.** OSTEP notes that cleaning concerns
  limited LFS's initial impact — yet every SSD ships a log-structured FTL,
  and recent work argues large sequential writes are what SSDs want from
  file systems too (He et al., *The Unwritten Contract of Solid State Drives*, EuroSys
  '17). The idea lost the file-system battle in 1995 and won the storage
  war.
- **TRIM is an interface lesson.** A device with a static mapping never
  needs to be told a block is dead; a device with a dynamic mapping does.
  When implementations change enough, interfaces follow — compare the file
  system's own `unlink`, which is TRIM seen from above.
- **Why random writes can beat random reads** on a modern SSD (visible in
  ch. 44's performance table): writes are absorbed into the log and become
  sequential programs; reads must go wherever the data physically landed.
  Students find this backwards until they've internalised the FTL.
- **The checksum is not where you think.** ZFS's checksums live in the
  *parent* (inode/indirect block), not beside the data — the only placement
  that catches lost writes, and a small end-to-end argument: the check
  belongs at the layer that knows what the data was supposed to be. (The
  classic statement is Saltzer, Reed & Clark's end-to-end argument, cited by
  ch. 48 — optional depth if this hooked you.)
- Disk **scrubbing** typically runs nightly or weekly; ch. 45 notes most
  LSEs in the study were found *by* scrubbing — patrol reads work.
