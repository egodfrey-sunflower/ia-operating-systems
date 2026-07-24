# Week 17 — Files and directories: the interface

> **Part III: Persistence.** Week 17 of 27.

## What you'll learn

One chapter this week — but it is the longest in the book (29 pages), and it
gets the week to itself because the persistence weeks that follow are all
implementations of the interface it defines.

Chapter 39 is about two abstractions and the machinery connecting them. A
**file** is a linear array of bytes with a low-level name — its **inode
number**; a **directory** is a list of (human name → low-level name) pairs,
itself a file with an inode. Everything else follows from taking those
definitions seriously. `open()` returns a **file descriptor** — a
per-process integer that the chapter, quoting the capability literature,
calls "a capability … an opaque handle that gives you the power to perform
certain operations". Descriptors index into the per-process table, which
points into the system-wide **open file table**, whose entries hold the
current offset and a reference count — and the sharing rules (`fork()` and
`dup()` share an entry; two `open()`s don't) explain a whole family of
behaviours that otherwise look arbitrary. `write()` is a promise to write
*eventually*; **`fsync()`** — and, in one of the chapter's best details,
sometimes an fsync of the *directory* — is what durability actually takes.
**`rename()`** is atomic with respect to crashes, which is why every editor
saves via write-temp-then-rename. Deletion is the punchline of the naming
story: `rm` calls **`unlink()`**, because removing a file is just removing
one name→inode link and decrementing a counter; the data goes when the last
link does. **Hard links** share an inode; **symbolic links** are little
files holding a pathname, which is why they alone can dangle. Finally the
sharing machinery: **permission bits** (owner/group/other × rwx, plus the
subtleties of execute-on-directories) and AFS-style **access control
lists**, and the assembly of one tree from many file systems with
`mount()`.

The chapter's TOCTTOU aside introduces this week's short paper — Bishop &
Dilger on **time-of-check-to-time-of-use** races: a privileged service
checks a pathname, then uses it, and an attacker swaps the name's meaning in
the gap. It is week 11–14's race-condition machinery resurfacing with the
*file namespace as the shared mutable state*, and it has never fully been
fixed.

One thing worth pinning down while the namespace is in view: not everything
with a pathname is a file, and Cambridge likes to ask you to compare three
ways processes talk to each other. A **signal** is asynchronous notification and
nothing else — a number delivered to a process, carrying no data, fine for
"stop" and useless for "here are four kilobytes". An **anonymous pipe** is a
one-way byte stream created by `pipe()`, which hands back two descriptors; it
has no name anywhere, so the only way another process can get an end is to
inherit one across `fork()` — which is why a shell can wire up `ls | wc` and
two independently-started programs cannot. A **named pipe**, or **FIFO**, is
the same byte stream given a pathname by `mkfifo`: the directory entry and
inode persist, so unrelated processes rendezvous by agreeing on a path instead
of on an ancestor, and this week's permission bits decide who may join. What
persists is the *name*, not the data — a FIFO stores nothing on disk, its
bytes live in a kernel buffer and are discarded once the last descriptor
closes, and `open()` for reading blocks until a writer arrives (and vice
versa), which makes opening it a rendezvous in its own right. Both kinds of
pipe are unidirectional; a conversation needs two.

**Key ideas:** file · inode number · directory as name→inode map · file
descriptor · open file table, offsets, sharing via `fork()`/`dup()` ·
`fsync()` and directory durability · atomic `rename()` · `unlink()` and
link counts · hard vs symbolic links · dangling references · permission
bits · execute bit on directories · ACLs (AFS) · superuser · `mount` ·
TOCTTOU · signals vs anonymous pipes vs named pipes (FIFOs).

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 39** | Interlude: Files and Directories | 29 | 4.1 h |
| 2 | **TLPI ch. 18** | Directories and Links *(reference — consult, don't read through)* | — | 1.0 h |

**Paper (required, short):** Bishop & Dilger (1996), *Checking for Race
Conditions in File Accesses*, Computing Systems 9:2. OSTEP calls it "a great
description" of the TOCTTOU problem. Read for the attack pattern and the
taxonomy of check/use pairs; the Perl-based detection tool is period
detail you can skim.

> **TLPI ch. 18 is the reference for the gritty parts** — exactly how
> `unlink`, `rename`, `mkdir` and symlink resolution behave at the edges,
> and library machinery like directory scanning and tree walks. Use it when
> a sheet question or the lab makes you want the precise contract; don't
> read it cover to cover.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise17.md`](../exercises/exercise17.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise17-solutions.md`](../exercises/solutions/exercise17-solutions.md) |
| **Lab** | [`../labs/lab06-io-storage/`](../labs/lab06-io-storage/) **ends** · [`../labs/lab07-filesystem/`](../labs/lab07-filesystem/) **starts** (weeks 17–19). Budget 6.0 h across the pair (3.5 h finishing lab 6, 2.5 h opening lab 7). Lab 7 opens on the ch. 39 file *interface* — ch. 40's on-disk structures arrive next week, and the lab is scoped accordingly. Ch. 39's own homework (write your own `stat`, `ls`, `tail`, recursive find) is folded into lab 7's first tasks |
| **Past paper (timed)** | `y2017p2q4` — 35 min timed, then self-mark (~1 h). External fragmentation, a full 4-level page-table walk of a 48-bit address, and TLB/EAT arithmetic. Spaced retrieval of week 9's paging material — kept warm across the concurrency-and-I/O stretch |
| **Untimed drill** | This week unlocks `y2014p2q3` (protection/isolation essay — its filesystem-permissions part is now covered) and most of `y2021p2q4` (scheduling arithmetic plus Unix permission-bit reasoning; its final setuid sub-part waits for week 20's access-control chapter) |

## Week load

```
OSTEP ch. 39            29pp ÷ 7  =  4.1 h
TLPI ch. 18 (reference)           =  1.0 h
Bishop & Dilger [S]               =  0.75 h
Exercise sheet 17                 =  3.0 h
Timed paper (y2017p2q4)           =  1.0 h
Lab 6 ends · Lab 7 starts         =  6.0 h
                                    ------
                                    15.9 h   — over the 12–14 h band (labs are not
                                             trimmed to fit)
```

The lightest reading since week 11 — deliberately, after two weeks at the
ceiling — which is what pays for the shared lab slot as Lab 6 ends and Lab 7
opens. The reference hour is the flex.

## Notes for the curious

- **Ch. 39's homework is code-writing, not a simulator**
  (`ostep-code/file-intro/`): build `stat`, `ls -l`, an efficient `tail`,
  and a recursive tree-walker. This course routes that work into lab 7
  rather than the sheet — the sheet drills the semantics the tools depend
  on.
- The chapter plants two seeds that later weeks harvest. Its inode is
  deliberately a black box — "a persistent data structure kept by the file
  system"; ch. 40 (week 18) opens the box, and the famous Cambridge
  block-arithmetic questions live inside. And its remark that a file
  descriptor is a *capability* is developed properly in week 20 — for now,
  take it as a pointed observation about handles and authority.
- `lseek()` does not seek the disk — it assigns an integer in a kernel
  structure. The chapter's aside on this confusion exists because
  generations of students conflated it with week 16's disk-arm seeks.
- The TOCTTOU gap is not historical: the chapter cites work three decades
  after McPhee's 1974 observation, still concluding there is no clean
  general fix — the namespace is shared mutable state, and `open()`
  by pathname is an unsynchronized read-modify-use of it. The modern
  mitigation pattern — do the check on the *opened descriptor* rather than
  the pathname — falls straight out of this week's OFT/inode model, and
  sheet §B4 walks it.
- Why files need `fsync()` but directories mostly look after themselves —
  and what "the file is durable but its *name* isn't" even means — becomes
  fully answerable in week 18 when directories turn out to be blocks like
  any others.
