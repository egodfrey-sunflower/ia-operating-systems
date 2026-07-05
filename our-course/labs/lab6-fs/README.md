# Lab 6 — File systems

**Weeks:** 13–14 · **Budget:** 10–12 hours · **Track:** kernel · **Weight:** 5%

**Syllabus:** IA L10–L12 (the Unix V7 file
system: inodes, the superblock, directories, the block map) · **Reading:**
OSTEP ch. 39–42 (files & directories, FS implementation, FFS, crash
consistency & journaling);
the xv6 book, ch. 8 (file system). Papers 21–23 on `../../reading-list.md`. See
also Sheet 8, whose inode arithmetic this lab makes concrete.

This lab is the file-system counterpart to the theory you meet on IA examples
sheet 3. You take the classic **Unix V7 on-disk layout** — a boot block, a
superblock, an inode array, a free-block bitmap, and data blocks — exactly as
xv6 implements it, and you (1) grow the inode's block map so files can be
large, (2) add symbolic links, and (3) run a controlled experiment on the
**write-ahead log** that makes those updates crash-safe.

Everything runs on the same xv6 tree as Labs 2–5. As before the kernel
print routine is `printk` (not `printf`), and the tree has an orphan-inode
reclaimer (`ireclaim` in `kernel/fs.c`) that runs at mount. Task 0 asks you to
read the real code, so those are the names you will see.

---

## Background

### The V7 on-disk layout

An xv6 disk image is a linear array of 1024-byte **blocks** (`BSIZE`,
`kernel/fs.h`). `mkfs` (`mkfs/mkfs.c`) carves it into regions and records the
map in the **superblock** (block 1):

```
+------+-------+--------------+--------------+-----------+---------------------+
| boot | super |     log      | inode blocks |  bitmap   |     data blocks     |
+------+-------+--------------+--------------+-----------+---------------------+
  0       1     logstart..     inodestart..   bmapstart..
```

Each file is described by an **inode** (`struct dinode`, `kernel/fs.h`): its
type, link count, size, and an array `addrs[]` of block numbers holding the
file's contents. Small files list their data blocks directly; larger files
reach further blocks through an **indirect block** (a data block full of block
numbers). This is essentially the V7 scheme from lectures, simplified — real
V7 carries more levels of indirection than stock xv6 does.

### The write-ahead log

xv6 never modifies a home block in place without first recording the change in
an on-disk **log** (`kernel/log.c`). A file-system call brackets its writes in
`begin_op()` / `end_op()`; the buffers it dirties are pinned, and at commit
the log (1) copies the new blocks into the log region, (2) writes a **header
block** naming them, then (3) installs them at their home locations and clears
the header. If power is lost, the next mount **replays** whatever the header
names. The single write of that header is the atomic commit point — the hinge
this whole lab turns on.

---

## Setup

From this directory, create a fresh working tree (a copy of the vendored xv6
with the lab's starter files overlaid):

```
./starter/setup.sh ~/lab6
cd ~/lab6
make qemu          # boots xv6; quit with Ctrl-a x
```

`setup.sh` refuses to overwrite an existing directory. The starter tree
**builds and boots as-is.** Relative to stock xv6 it already:

- bumps `FSSIZE` to 100000 blocks (`kernel/param.h`) so a large-file test can
  fit (you do **not** change this);
- ships the test programs `bigfile`, `symlinktest`, `crashtest` in `UPROGS`;
- wires up the user side of a new `symlink()` syscall (number, `user.h`
  prototype, `usys.pl` stub) — but **not** its kernel side, so calling it
  prints `unknown sys call 23` until you implement it (Task 2);
- ships the complete Task 3 crash-injection machinery (the `crashnow()`
  syscall and a `CRASH=` build flag), disabled and harmless by default.

Because of that, on the starter tree `symlinktest` and `bigfile` **fail**
(that is the lab), while `usertests -q` and the crash experiment already pass.

Run the autograder at any time:

```
tests/run.sh ~/lab6           # full run
QUICK=1 tests/run.sh ~/lab6   # skip the slow tests
```

The autograder is tiered (see `tests/README.md`): the lab-specific tests and
the crash experiment run first; the `usertests -q` regression runs last.
`QUICK=1` skips the two slow tests — `bigfile` and `usertests -q` — marking
them `SKIP` in the table; iterate with it, but a submission counts only with
the full run. `bigfile` and `usertests` each take minutes under QEMU — the
timeouts are generous. See `tests/README.md` for how the crash experiment
reboots the same disk image without regenerating it.

---

## Tasks

### Task 0 — Layout safari (10%, written)

Read `mkfs`'s build-time output and `kernel/fs.h`, and draw the exact on-disk
map of *this* image. When you build, `mkfs` prints (for `FSSIZE = 100000`):

```
nmeta 59 (boot, super, log blocks 31, inode blocks 13, bitmap blocks 13) blocks 99941 total 100000
```

Your write-up (`answers.md`) must:

1. **Draw the map.** Give the first and last block number of every region
   (boot, super, log, inodes, bitmap, data). Cross-check your inode/bitmap
   start numbers against `sb.logstart`, `sb.inodestart`, `sb.bmapstart` as
   computed in `mkfs/mkfs.c` (cite the lines), and against the `IBLOCK` /
   `BBLOCK` macros in `kernel/fs.h`.
2. **Bytes per inode slot.** From `struct dinode` (`kernel/fs.h`) compute
   `sizeof(struct dinode)` and hence `IPB` (inodes per block). Confirm that
   200 inodes need the 13 inode blocks `mkfs` reports. What is the largest
   file the *stock* inode can address, in blocks and in bytes?
3. **Why is the log before the inodes?** The regions are laid out
   `... | log | inodes | bitmap | data`. Argue why the log lives *ahead* of
   the metadata it protects — what would go wrong if a crash struck while the
   log itself were being written, and why does putting it at a fixed known
   offset (`sb.logstart`) matter for recovery at mount time
   (`initlog` → `recover_from_log`, `kernel/log.c`)?
4. **What's in the superblock?** List the fields of `struct superblock` and
   say, for each, who writes it (`mkfs`) and who reads it (`readsb` /
   `fsinit`, `kernel/fs.c`). Why must the kernel trust these numbers rather
   than recompute them?

Human-graded. Anchor every claim with a `file:line`.

### Task 1 — Large files (35%)

The stock inode has 12 direct slots and one singly-indirect block, capping a
file at `12 + 256 = 268` blocks. Rebalance it to **11 direct + 1
singly-indirect + 1 doubly-indirect**, so:

```
MAXFILE = 11 + 256 + 256*256 = 65803 blocks
```

Edit `kernel/fs.h`: set `NDIRECT` to 11, add `NDINDIRECT = NINDIRECT *
NINDIRECT`, redefine `MAXFILE`, and enlarge the address array to
`addrs[NDIRECT + 2]` (one slot each for the singly- and doubly-indirect
blocks). Make the **same** array-size change to the in-memory inode in
`kernel/file.h`. (Note the slot count stays 13, so the on-disk inode size and
`IPB` do not change.)

Then teach the block map about the new arm:

- **`bmap()`** (`kernel/fs.c`) — must resolve any block number up to the new
  `MAXFILE`, allocating lazily at every level so a file can grow one block at
  a time into the doubly-indirect range. Read the existing singly-indirect
  case until you can say what every line is for, then model the new arm on it.
- **`itrunc()`** (`kernel/fs.c`) — must free *every* block the file holds:
  the data blocks and each level of indirect block, at both arms.
  If you leak here, `bigfile` will notice: it runs its
  write/verify/unlink cycle twice, and an unlink that leaks the
  doubly-indirect tree (~65.8k blocks) runs the ~100k-block disk out
  during the second pass.

The provided `user/bigfile.c` writes 65803 blocks, reads them all back
checking a per-block tag, then unlinks the file — twice over. Under
QEMU this is slow (minutes per pass) — that is expected.

**Written part** (put in `answers.md`): pick a byte offset that lands in the
doubly-indirect range of a *sparse* file and trace the **first** `write()` to
it: list exactly which blocks the kernel must *read* and which it must *newly
allocate* on the way from the inode down to the data block — count both.
Then contrast with a later **re-read** of the same offset: which of those
steps disappear, and why? Anchor every claim in your `bmap()`.

### Task 2 — Symbolic links (30%)

Add `int symlink(const char *target, const char *path)`, which creates a new
inode of type `T_SYMLINK` (already defined in `kernel/stat.h`) at `path` and
stores the string `target` as the inode's contents. `target` need not exist.

Then make `open()` **follow** symlinks: opening a `T_SYMLINK` opens whatever
its target names, chasing chains of links. Requirements:

- Unless `O_NOFOLLOW` (already defined in `kernel/fcntl.h`) is set, `open()`
  resolves the link. With `O_NOFOLLOW`, it opens the link inode itself (so
  `fstat` reports `T_SYMLINK` and `read` returns the stored target path).
- Cap the chase at a depth of **10**; a cycle (`a → b → a`) must return −1,
  not hang.
- Opening a **dangling** link (target missing) returns −1 without
  `O_NOFOLLOW`.
- `unlink()` needs no change: it already removes the *link*, never the target.

Wire the kernel side yourself: add the handler in `kernel/sysfile.c` and its
`extern` + table entry in `kernel/syscall.c` (the user side is already done).
The provided `user/symlinktest.c` checks follow, `O_NOFOLLOW`, dangling,
chains, cycle detection, and that removing a link leaves its target intact.

**Written part** (`answers.md`, ties to IA sheet 3 Q2): contrast symbolic links
with the **hard** links that `link()` already makes. For each of — *where the
name-to-inode mapping lives*, *whether the link can cross to another device*,
and *what happens when the target is deleted* — say how the two differ and
why.

### Task 3 — Crash consistency: a guided experiment (25%)

The write-ahead log in `kernel/log.c` is the object of study. Nothing to
implement — the machinery ships in your starter — but you run it and write it
up.

**(a) Trace one `write()` (written, `answers.md`).** Follow a single one-block
`write()` from `sys_write` through `begin_op` → `log_write` → `end_op` →
`commit` (`kernel/sysfile.c`, `kernel/log.c`). Background already told you the
atomicity point is the header write; your job is to argue **why it is that
write and not `install_trans`**: for a crash landing *just before* the header
write and one landing *just after* it, say exactly what the next mount's
`recover_from_log` does in each case, and show how those two behaviours make
the header write — and nothing else — the hinge. Cite lines.

**(b) The experiment (run it, report the result in `answers.md`).** The
starter kernel has two compile-time crashpoints straddling the header write:

```
make CRASH=BEFORE_HEAD   # freeze after log blocks written, before the header
make CRASH=AFTER_HEAD    # freeze after the header, before installing home blocks
```

A crashpoint does nothing until a process arms it with the `crashnow(1)`
syscall; the next commit then prints a marker and freezes (via `panic`),
issuing no further disk I/O — a clean power-loss simulation. `user/crashtest.c`
drives it: `phase1` writes a durable `'A'` baseline, arms the crashpoint, and
overwrites with `'B'` (that commit is the one that crashes); after a reboot on
the *same disk image*, `phase2` reports whether the block came back `'A'`
(`VERDICT old`) or `'B'` (`VERDICT new`). The autograder automates both
crashpoints end to end; you can also do it by hand:

```
make clean && make CRASH=BEFORE_HEAD qemu
$ crashtest phase1          # ... prints "CRASHPOINT: BEFORE_HEAD reached", freezes
Ctrl-a x                    # kill qemu; do NOT rebuild
make qemu                   # reboots the SAME fs.img
$ crashtest phase2          # -> VERDICT old
```

Report both verdicts and explain them in terms of your part (a) trace: why
`BEFORE_HEAD` loses the write yet leaves the file **consistent** (never a torn
mix of `'A'` and `'B'`), and why `AFTER_HEAD` makes it **durable** because
recovery replays it. This is the difference between *atomicity* (always) and
*durability* (only once the header lands) that the log provides.

Close your write-up with: what would `fsck` have to do at mount time if there
were **no** log, and why is that both slower and weaker? Point to the
journaling and log-structured designs on the reading list (papers 22–23) as
the industrial answer.

---

## Hints

- **Task 1.** The slot count is unchanged — `NDIRECT + 2 == 13 == NDIRECT_old
  + 1` — so you are re-*interpreting* two slots, not resizing the inode. Model
  the doubly-indirect arm on the singly-indirect one — read that case first;
  everything the new arm needs (including the locking and buffer-release
  discipline) is demonstrated there. Don't forget the `addrs[NDIRECT+2]`
  change in **both** `fs.h` and `file.h`.
- **Task 1 (log size).** A single `write()` of one block only ever dirties a
  handful of blocks (data, the two indirect blocks, a bitmap block, the
  inode), well under `MAXOPBLOCKS`. So does freeing a whole file — the bitmap
  blocks it touches are contiguous. You do **not** need to change the log.
- **Task 2.** Everything you need is already used by the neighbouring handlers
  in `kernel/sysfile.c` — read how `sys_open`'s `O_CREATE` path makes a fresh
  inode and how `sys_write` moves bytes into one, and note carefully what
  locked/unlocked state each helper expects and returns. The classic bug in
  the follow loop is going around again while still holding a lock from the
  previous hop.
- **Task 3.** Nothing to code. When doing it by hand, the trick is to **not**
  rebuild between the crash and the reboot: `make qemu` regenerates `fs.img`
  only when a source is newer than it, and QEMU's own writes make `fs.img` the
  newest file — so a plain `make qemu` reuses the crashed image. `make clean`
  (or editing a user program) is what discards it.
- **Rebuild after each task**; `make clean` if the disk image looks stale.

---

## Deliverables

1. Your modified xv6 tree (or a patch): `kernel/fs.h`, `kernel/file.h`,
   `kernel/fs.c`, `kernel/sysfile.c`, `kernel/syscall.c`.
2. `answers.md` — the Task 0 map, the Task 1 first-write vs re-read trace,
   the Task 2 hard-vs-symbolic contrast, and the Task 3 trace + experiment
   write-up.
3. `tests/run.sh` passes on your tree (all functional tests, both crash
   cases, and `usertests -q`).

---

## Rubric (100%)

| Task | Weight | What is graded |
|------|-------:|----------------|
| 0 — layout safari | 10% | Correct region map with block numbers; inode-slot arithmetic; log-ordering and superblock arguments, all anchored. Human-graded. |
| 1 — large files | 35% | `bigfile` writes+verifies 65803 blocks; `bmap`/`itrunc` correct and leak-free; first-write vs re-read block trace correct and anchored in `bmap()`. |
| 2 — symbolic links | 30% | `symlinktest` passes all subtests (follow, `O_NOFOLLOW`, dangling, chain, cycle, unlink); hard-vs-symbolic contrast. |
| 3 — crash consistency | 25% | Correct atomicity-point trace; both crash cases produce the predicted verdict and are explained (atomicity vs durability; the fsck counterfactual). |
| Regression | gate | `usertests -q` must still pass — a change that breaks the fs forfeits the coding marks. |

The autograder prints a PASS/FAIL/SKIP table and exits non-zero on any
failure. `QUICK=1` skips the slow `bigfile` and `usertests -q` while you
iterate (they show as SKIP); the final check runs everything.
