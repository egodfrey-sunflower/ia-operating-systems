# Week 16 — Hard disks and RAID

> **Part III: Persistence.** Week 16 of 27.

## What you'll learn

Two chapters, one message: performance and reliability at the bottom of the
storage stack are *geometry and arithmetic*, and you are expected to be able
to do the arithmetic.

Chapter 37 builds a functional model of the hard disk: platters and tracks,
and the three components of every I/O — **seek**, **rotational delay**,
**transfer**: `T_I/O = T_seek + T_rotation + T_transfer`. From that model
falls the single most consequential number in storage: sequential access
beats random by a factor of **hundreds**, because a random 4 KB read pays
milliseconds of positioning for microseconds of transfer. The rest of the
chapter is what OS and drive do about it: track skew and zoning, the drive's
cache and the write-back-vs-write-through hazard, and **disk scheduling** —
SSTF and its starvation problem, SCAN/C-SCAN (the elevator), and SPTF, which
needs head-position knowledge only the drive has, which is why modern
scheduling is split between OS (merging, rough ordering) and controller.

Chapter 38 asks how to make a big, fast, reliable disk out of small, slow,
unreliable ones. **RAID**'s deep design lesson is **transparency**: it looks
exactly like a disk, so it deployed without changing one line of software.
The levels are an exercise in three-axis evaluation — capacity, reliability,
performance — under a stated **fail-stop fault model**: striping (RAID-0)
as the performance/capacity upper bound; mirroring (RAID-1) at half
capacity; parity (RAID-4) at (N−1)/N capacity but with a **small-write
problem** — every small write is four I/Os, and the single parity disk
serialises them; RAID-5 rotates the parity to spread that load, leaving the
4× write penalty but restoring parallelism. The chapter's summary table —
throughput and latency per level for sequential/random, read/write — is the
one piece of this course you should be able to reproduce from first
principles on demand.

This week's paper is the original: Patterson, Gibson & Katz coined "RAID" and
its levels, and framed the argument economically — arrays of inexpensive
disks against the large expensive drives of the day. The terms changed
("inexpensive" quietly became "independent"); the taxonomy stuck.

**Key ideas:** seek/rotation/transfer · sequential ≫ random ·
dimensional-analysis I/O arithmetic · SSTF, SCAN, C-SCAN, SPTF · where
scheduling lives now · I/O merging · fail-stop fault model · striping,
chunk size · mirroring and the consistent-update problem · parity, XOR ·
additive vs subtractive parity update · **the small-write problem** ·
RAID-5 rotated parity · the capacity/reliability/performance table.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 37** | Hard Disk Drives | 16 | 2.3 h |
| 2 | **OSTEP ch. 38** | Redundant Arrays of Inexpensive Disks | 18 | 2.6 h |
| 3 | **FreeBSD ch. 8** | Devices *(reference — consult, don't read through)* | — | 1.0 h |

**Paper (required):** ★ Patterson, Gibson & Katz (1988), *A Case for
Redundant Arrays of Inexpensive Disks (RAID)*, SIGMOD. OSTEP calls it
"*the* RAID paper". Read for the economic argument and the level taxonomy;
the per-level analysis you will largely have met in ch. 38's cleaner
notation.

> **FreeBSD ch. 8 is this week's reference text**, not a read-through: the
> best of the four supporting books on how a production kernel structures
> device drivers, autoconfiguration, and the character and disk-device interfaces.
> Dip into it when ch. 36–37 leave you wanting the real thing; budget an
> hour, follow curiosity, stop.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise16.md`](../exercises/exercise16.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise16-solutions.md`](../exercises/solutions/exercise16-solutions.md) |
| **Lab** | [`../labs/lab06-io-storage/`](../labs/lab06-io-storage/) — **continues** (weeks 15–17). Budget 6.0 h: disk-scheduling policies against a workload generator, and the RAID simulation begins |
| **Past papers** | **None timed this week.** This is a two-simulator week — `disk.py` and `raid.py` both feature on the sheet — and the lab runs at full weight; a timed paper would push the week over budget. The one question this week unlocks, `y2010p2q4` (address-space layout, blocking/non-blocking I/O, and a disk device driver with scheduling), joins the **untimed drill pool** — it is a good self-test after §B of the sheet |

## Week load

```
OSTEP ch. 37–38     34pp ÷ 7  =  4.9 h
FreeBSD ch. 8 (reference)     =  1.0 h
Patterson RAID [M]            =  1.5 h
Exercise sheet 16             =  3.0 h
Lab 6                         =  6.0 h
                                ------
                                16.4 h   — over the 12–14 h band (labs are not
                                         trimmed to fit)
```

The reference hour is the flex: if the week runs long, FreeBSD ch. 8 keeps.

## Notes for the curious

- **Both chapters ship good simulators** — `disk.py` (with a graphical
  animator, `-G`: seek, rotation and transfer drawn in real time) and
  `raid.py` (mappings both directions, and a timing mode; it too ships a
  separate `raid-graphics.py` animator). The sheet leans on both; run them.
  One wrinkle worth knowing before you start: `disk.py` `import`s Tkinter at
  the top of the file even for text-only `-c` runs, so you need `python3-tk`
  installed or it aborts before printing anything.
- Ch. 37's **dimensional-analysis aside** is quietly one of the most
  useful pages in the book: RPM → ms-per-rotation → average rotational
  delay without ever memorising a formula. The Cambridge I/O questions are
  exactly this manipulation under time pressure.
- The "average seek is one-third of a full seek" folklore has an actual
  derivation — an integral over all track pairs — and ch. 37 walks it.
- The **consistent-update problem** (mirror writes interrupted by a crash
  leave the copies disagreeing) is quietly the first appearance of
  write-ahead logging in this course; hardware RAID hides it behind
  battery-backed RAM. The idea returns at full size with journaling in
  week 21.
- RAID-4 is not dead: systems that only ever do full-stripe writes never
  hit the small-write problem, and ch. 38 notes at least one landmark
  filer built exactly that way.
- Ch. 38's fault model is deliberately clean — disks fail loudly and
  entirely (**fail-stop**). Silent corruption and latent sector errors are
  real, are excluded by assumption here, and get their own chapter (45) in
  week 22. The sheet's §C2 pushes on this seam.
