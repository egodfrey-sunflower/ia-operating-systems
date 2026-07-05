# ⚠️ SPOILERS — Lab 6 reference solution ⚠️

```
╔══════════════════════════════════════════════════════════════════════╗
║  STOP. This directory contains the complete reference solution for    ║
║  Lab 6. Do the lab yourself first. The whole point is to feel the     ║
║  V7 inode block-map and the write-ahead log in your hands — reading   ║
║  this before you have struggled with them throws that away.           ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## What's here

- `solution.patch` — a unified diff (`-p1`) against a tree created by
  `starter/setup.sh`. It touches five files:
  `kernel/fs.h`, `kernel/file.h`, `kernel/fs.c`, `kernel/sysfile.c`,
  `kernel/syscall.c`.
- `apply.sh <workdir>` — dry-runs then applies `solution.patch`. Does not
  build; run `make` afterwards.
- `files/` — full post-solution copies of the five changed files, for reading
  without applying the patch.

Task 3's crash machinery (the `CRASH=` build flag, the `crashnow()` syscall,
and `crashtest.c`) is **not** part of the solution — it ships complete in the
starter, because Task 3 is a guided experiment whose deliverable is a written
analysis, not code. The solution changes nothing there.

## Apply and test

```
./starter/setup.sh ~/lab6-sol
solutions/apply.sh ~/lab6-sol
tests/run.sh ~/lab6-sol           # expect all rows PASS, incl. both crash cases
```

## Design notes

### Task 1 — large files (`fs.h`, `file.h`, `fs.c`)

The inode is rebalanced from *12 direct + 1 singly-indirect* to
**11 direct + 1 singly-indirect + 1 doubly-indirect**. The number of
`addrs[]` slots is unchanged (`NDIRECT + 2 == 13`), so the on-disk `dinode`
is the same size and `IPB` is unchanged — no on-disk format break beyond the
meaning of the last two slots.

```
NDIRECT    = 11
NINDIRECT  = BSIZE/4        = 256
NDINDIRECT = 256 * 256      = 65536
MAXFILE    = 11 + 256 + 65536 = 65803 blocks  (~64.3 MiB)
```

- `bmap()` grows a third arm: for `bn` in the doubly-indirect range it splits
  `bn` into `idx0 = bn / NINDIRECT` (slot in the top block) and
  `idx1 = bn % NINDIRECT` (slot in the mid block), allocating the top block,
  the mid block, and the data block on demand — exactly as the single-indirect
  arm does one level down.
- `itrunc()` gains a matching third arm: it walks the top block, and for each
  non-zero mid block frees all its data blocks, then the mid block, then
  finally the top block. Freeing a full `MAXFILE` file touches only a handful
  of distinct bitmap blocks (they are contiguous), so it stays well within
  one log transaction.
- `kernel/param.h` was already bumped in the starter (`FSSIZE = 100000`) so
  the 65803-block file fits — while staying small enough that `bigfile`'s
  second pass runs the disk out if `itrunc` leaks the doubly-indirect tree.

### Task 2 — symbolic links (`sysfile.c`, `syscall.c`)

- `sys_symlink(target, path)` creates a `T_SYMLINK` inode with `create()` and
  stores `target` (no trailing NUL) as the inode's file contents with
  `writei`. The target need not exist — dangling links are allowed.
- `sys_open()` resolves symlinks unless `O_NOFOLLOW`: after `namei()`+`ilock`,
  while the inode is a `T_SYMLINK` it reads the stored path, `iunlockput`s the
  link, and `namei()`s the target, re-locking. A depth counter caps the chase
  at 10 hops, so a cycle (`a → b → a`) is refused with −1 instead of looping.
  With `O_NOFOLLOW` the link inode itself is opened, so `fstat` reports
  `T_SYMLINK` and `read` returns the stored path.
- `unlink()` is unchanged: it already operates on the named inode (the link),
  never the target — contrast a hard `link()`, which shares the target's
  inode directly.

### Task 3 — crash consistency (ships in the starter)

`kernel/log.c`'s `commit()` writes the log blocks, then the header (the atomic
commit point), then installs the blocks home. The two compiled-in crashpoints
straddle the header write. `crashtest` makes a durable `'A'` baseline, arms
the crashpoint, then overwrites with `'B'`:

- **BEFORE_HEAD** → the header still names the old (empty) transaction;
  recovery installs nothing; the block reads back `'A'` (`VERDICT old`). The
  write was *lost*, but the file is never torn.
- **AFTER_HEAD** → the header is committed; recovery replays the logged
  blocks; the block reads back `'B'` (`VERDICT new`). The write is *durable*.

That all-or-nothing boundary at the single header write is exactly what the
log buys, and what `fsck` would otherwise have to reconstruct after the fact.
