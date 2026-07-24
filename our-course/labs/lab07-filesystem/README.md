# Lab 7 — File systems (Part 1: the ch. 39 tools)

**Weeks 17–18 · ~3.5 hours · OSTEP ch. 39 (files and directories) · TLPI ch. 18 (`stat`, `readdir`)**

Userspace C on Linux, from scratch. No kernel work, no xv6, no QEMU, no root.
You will need `bash`, `gcc`, `make`, and — for one `mytail` case — `strace`.

You are writing your own `stat`, `ls -l`, `tail` and a recursive name search:
four small tools built entirely on the file *interface*. They read inode
metadata, walk directories as ordinary readable files, and move a descriptor's
offset with `lseek` — everything chapter 39 calls a file, without ever seeing
what an inode is on disk. That on-disk half is Parts 2–5 of this lab, inside
xv6, and comes as a separate handout; this one is the interface alone.

## Layout

```
lab07-filesystem/
  README.md          this handout
  starter/           Makefile, mystat.c, myls.c, mytail.c, myfind.c   <- work here
  fixtures/          the generator for the test tree
  tests/run.sh       the autograder
  solutions/         SPOILERS. Reference code + answer key. Later.
```

Copy the three working directories somewhere of your own and work there. Copy
all three, not just `starter/` — the autograder and the fixture generator are
referred to as `../tests/…` and `../fixtures/…`, and those paths only resolve if
the layout comes with you:

```sh
cp -r starter tests fixtures ~/lab7
cd ~/lab7/starter
make            # builds mystat, myls, mytail, myfind
make test       # ../tests/run.sh on this directory
```

`solutions/` is deliberately left behind.

## What you hand in

| File | Weight |
|---|---|
| `mystat.c` building `-Werror` clean and passing its cases | 25% |
| `myls.c` building `-Werror` clean and passing its cases | 25% |
| `mytail.c` building `-Werror` clean and passing its cases | 30% |
| `myfind.c` building `-Werror` clean and passing its cases | 20% |

Everything here is machine-checked by diff against the system tools. Build with
the supplied `Makefile`, which uses `-Wall -Wextra -Werror`: a warning is a
build failure, on purpose. The grader compiles each tool with those flags
itself, so weakening them in your own Makefile does not weaken the grade.

Look at the fixtures before you start:

```sh
../fixtures/genfixtures.sh /tmp/fix && find /tmp/fix | sort
```

`fixtures/README.md` says what each one is. Reading that table first is worth
ten minutes of debugging later.

---

## `mystat PATH...` — reference: `stat`

For each `PATH`, print exactly five labelled lines built from the file's inode:

```
name: <path>
type: <human-readable type>
size: <bytes>
links: <hard-link count>
mode: <octal> <symbolic>
```

The output is byte-for-byte identical to

```sh
stat --printf='name: %n\ntype: %F\nsize: %s\nlinks: %h\nmode: %a %A\n' PATH...
```

so you can check any single case yourself by running that `stat` line, and the
autograder diffs against exactly it.

- `type` is `stat`'s `%F` wording: `regular file`, `directory`, `symbolic
  link`, `character special file`, and so on. Run `stat -c %F` on each fixture
  to see every spelling you must reproduce.
- `mode` is the permission bits in octal, a space, then the ten-character
  symbolic string (`stat`'s `%A`, the first column of `ls -l`), which encodes
  the type and the set-user-ID / set-group-ID / sticky bits.
- Report the metadata of the **symbolic link itself**, not the file it points
  to — as `stat` does by default. A dangling link is therefore not an error.
- Take one or more operands and print a block for each, in order.
- A `PATH` that cannot be examined goes to stderr as
  `mystat: cannot stat '<path>': <strerror(errno)>`; carry on with the rest and
  exit 1. With no operands, exit 2.

## `myls -l [DIR]` — reference: `ls -l`, one entry at a time

List a directory's entries, one per line, in the form

```
<symbolic-mode> <link-count> <size> <name>
```

sorted by raw byte value, omitting every name that begins with `.` (which
covers `.`, `..` and dotfiles, exactly as `ls` does). No `DIR` means `.`.

The graded target is, for each entry `E` of directory `D`, byte-for-byte

```sh
( cd D && stat --printf='%A %h %s %n\n' E )
```

taken over the sorted, non-dot entries. The owner, group and timestamp columns
of a real `ls -l` are **not** printed — they are not reproducible between
machines or runs, so there is nothing stable to diff, and the metadata that
teaches the lesson (type, permissions, links, size) is all here.

- Read the directory with `opendir`/`readdir`/`closedir`; measure each entry
  with `lstat`. A symlink shows as a symlink, with its own size.
- Sort by raw byte value — `qsort` with a `strcmp` comparator. `readdir` returns
  entries in whatever order the filesystem chose; without a defined order the
  diff is a coin flip. (The grader runs under `LC_ALL=C`.)
- Copy each `d_name` before the next `readdir`: the returned struct may be
  reused.
- A missing `-l`, or more than one path, is a usage error: exit 2. A directory
  that cannot be opened goes to stderr and exits 2.

## `mytail -n N FILE` — reference: `tail -n N FILE`

Print the last `N` lines of `FILE`. Output is byte-for-byte `tail -n N FILE`.

- `N` is a non-negative integer. `N` = 0 prints nothing; `N` larger than the
  file's line count prints the whole file.
- The last line must come out exactly as stored, whether or not the file ends
  in a newline.
- `FILE` is required — there is no stdin mode. Reach the last `N` lines by
  **seeking near the end of the file with `lseek` and reading backward**, not by
  reading the file from the beginning. This is graded: the autograder runs your
  `mytail` under `strace` on a 4 MiB file and fails a tool that pulls in far
  more of the file than its last `N` lines occupy — whether by `read`, `pread`,
  `readv` or `mmap` — *even if the printed output is correct*. A tool that
  streams the whole file and then prints the right lines does not pass this
  lab.
- A `FILE` that cannot be opened goes to stderr and exits 1. Bad arguments are a
  usage error: exit 2.

## `myfind START NAME` — reference: `find START -name NAME`

Walk the tree rooted at `START` and print every path whose final component
equals `NAME`. `NAME` is a literal (no glob metacharacters), so this is
`find START -name NAME` for a plain name. The autograder sorts both outputs
before comparing, so the order you print in is your own.

- Recurse into every subdirectory, to any depth. Test `START` itself too.
- Match the **whole** final component, not a substring: searching for `foo` must
  not report `foobar` — nor `FOO`; the comparison is byte-exact, case included,
  as `find -name` is for a literal. A directory whose name matches is a match
  as well as a file.
- Walk with `lstat` and **do not follow symbolic links** — match a symlink by
  its own name and never descend through it. (`find` without `-L` does the same;
  it is what keeps a link pointing back up its own tree from looping the walk
  forever.)
- Skip `.` and `..` so the walk terminates.
- Finding nothing is not an error: exit 0. A `START` that does not exist goes to
  stderr and exits 1.

---

## Running the tests

```sh
make                              # must be warning-free
../tests/run.sh .                 # or: make test
```

`run.sh` compiles each tool itself with `-Wall -Wextra -Werror`, generates the
fixture tree into a temporary directory, and then for every case runs your tool
and the system tool (`stat`, `tail`, `find`, or a `stat`-built oracle for
`myls`) with identical arguments and diffs stdout and exit status. It prints a
PASS/FAIL table, a `N passed, M failed, K skipped` line, and exits non-zero if
anything failed; a failing case shows both outputs.

The one case that is not a plain diff is `mytail`'s read-budget check, which
runs under `strace`. On a machine where `strace` cannot trace (some containers
restrict `ptrace`), that single case reports **SKIP** rather than PASS or FAIL,
and prints a line saying the seek requirement went unchecked. Everything else
still runs. If you can, run the tests somewhere `strace` works at least once.

---

## If you get stuck

- `make test` says it cannot find the autograder: you copied only `starter/`.
  Copy `tests/` and `fixtures/` alongside it, or pass
  `make test TESTS=/path/to/lab07-filesystem/tests/run.sh`.
- A `mystat` line matches by eye but the case still fails: `cmp` your output
  against `stat --printf=…` for that path and look at the exact bytes — a
  wrong `type` word or a single wrong character in the mode string is the usual
  cause. Compare `stat -c %F` and `stat -c %A` on the offending file.
- `myls` output looks right but the order is wrong on one machine only: the
  grader runs under `LC_ALL=C` — compare the order your comparator produces
  with what `LC_ALL=C ls -1` prints for the same directory, entry by entry,
  and look at where the two orders first disagree.
- `mytail`'s output cases pass but `mytail: seeks from end` fails: the case is
  not about your output — re-read how it is measured, and look at
  `strace -y -e trace=read,pread64,readv,mmap,lseek ./mytail -n 10
  /tmp/fix/tail/big`.
- `myfind` finds too many or too few paths: `diff <(./myfind START NAME | sort)
  <(find START -name NAME | sort)` and look at which side has the extra lines —
  they tell you whether the fault is in matching or in walking.
- A `mytail` or `myfind` case hangs until the timeout kills it: look at what
  your walk or your backward scan does at the edge of the file or the tree.

<!-- ===================================================================== -->
<!-- Parts 2-5: the xv6 extensions and the checker. Separate handout below. -->
<!-- ===================================================================== -->

---

# Lab 7 — File systems (Parts 2–5: the xv6 extensions and a checker)

**Weeks 18–19 · ~9 hours · OSTEP ch. 40 (FS implementation) · xv6 rev5 ch. 10, §10.1–10.6**

Now the implementation beneath the interface. Parts 2 and 3 change xv6's
on-disk format from inside the kernel; Parts 4 and 5 are plain userspace C
that reads an image and checks it. Parts 2–3 need the RISC-V cross-compiler
and QEMU (as in Lab 4); Parts 4–5 need only `gcc` and `make`.

Where the file system *comes from*: the on-disk image is not built by the
kernel at boot. It is built offline by `mkfs/mkfs.c`, a host program the
Makefile runs to produce `fs.img` — it writes the super block into block 1,
lays out the log, inode and bitmap regions, and copies the user programs in.
If you go looking for the super-block write in the kernel, you will not find
it: it is in `mkfs`. Read `mkfs/mkfs.c` before Part 4.

## Layout for Parts 2–5

```
lab07-filesystem/
  setup.sh              copies the pinned xv6 tree + the Parts 2–3 starter
  starter/overlay/      the xv6 tree overlay you edit for Parts 2–3
  starter/xfsck/        Parts 4–5: xv6fs.h, xv6img.c, mkimg, corrupt, xfsck.c  <- work in xfsck.c
  tests/run-kernel.sh   autograder for Parts 2–3 (xv6 under QEMU)
  tests/run-xfsck.sh    autograder for Parts 4–5 (userspace)
  solutions/            SPOILERS. Reference kernel + reference xfsck + key.
```

```sh
./setup.sh ~/lab7-xv6         # a fresh xv6 tree for Parts 2–3
cd ~/lab7-xv6
make                          # single-job only — never make -j
make qemu                     # Ctrl-A x to quit

cp -r starter/xfsck ~/lab7-xfsck   # Parts 4–5
cd ~/lab7-xfsck && make
```

The Parts 2–3 graders are run as
`tests/run-kernel.sh [--fast|--full] ~/lab7-xv6`; the Parts 4–5 grader as
`tests/run-xfsck.sh ~/lab7-xfsck`. **Run both from this lab directory** — they
live in `tests/` and reach their supporting files by relative path
(`run-kernel.sh` the shared QEMU driver, `run-xfsck.sh` the given checker
sources), so they only work in place. You pass your tree (or checker
directory) as the argument; you do **not** copy `tests/` for these parts the
way Part 1 does.

## What you hand in (Parts 2–5)

| Part | What is graded | Weight (of the whole lab) |
|---|---|---|
| Part 2 | `bigfile` and the `usertests` regression | 20% |
| Part 3 | `symlinktest` | 16% |
| Part 4 | the `xfsck` cases | 28% |
| Part 5 | `UNDETECTABLE.md` — prose, marked against the key | 8% |

Part 1 (the separate handout above) carries the remaining 28% of the lab; the
percentages in its own hand-in table are weights within that share.

---

## Part 2 — Large files

xv6's inode indexes its data blocks through `addrs[]`: the first `NDIRECT`
entries name data blocks directly, and the last names a singly indirect block
of `NINDIRECT = 256` pointers. So a file is at most `NDIRECT + 256` blocks.
Add a **doubly indirect** block — one more `addrs[]` entry, naming a block of
256 pointers, each of which names a singly indirect block — to raise the
maximum file size to exactly

```
11 + 256 + 256×256 = 65 803 blocks.
```

Contract:

- The on-disk inode (`struct dinode` in `kernel/fs.h`) must stay **exactly 64
  bytes**, so that a whole number of inodes fits in a block and the
  inode-per-block arithmetic is unchanged. `addrs[]` therefore does not simply
  grow: `NDIRECT` changes so that `addrs[]` still has the same number of
  entries, now split 11 direct / 1 singly indirect / 1 doubly indirect.
- The in-memory inode (`struct inode` in `kernel/file.h`) mirrors the on-disk
  `addrs[]`, so it changes too.
- `bmap()` maps a file-block number to a disk-block number, allocating on the
  way; extend it to walk the doubly indirect level.
- `itrunc()` frees every block of a file; extend it to free the doubly
  indirect block, the singly indirect blocks it names, and their data blocks.

The disk is already sized (`FSSIZE` in `kernel/param.h`) to hold a maximum
file — you do not need to change it. `mkfs` needs no change either; it only
ever writes small files.

The grader runs `user/bigfile.c`. By default it writes a file of a few
thousand blocks — well past the old 268-block maximum, so the doubly indirect
level is genuinely used — reads every block back and checks the bytes, then
**deletes the file and confirms the free-block count returned to exactly what
it was before**. That last step is the one that catches a leak: a file whose
data reads back perfectly can still have been truncated by an `itrunc` that
frees the direct and singly indirect blocks but forgets the doubly indirect
ones. The free-block count comes from a given `freeblocks()` system call — test
support, already wired up, nothing for you to write. (`bigfile n` writes `n`
blocks; the grader's `--stress` mode passes a large `n`, which is optional and
slow.) `usertests` (`writebig`) additionally writes and reads a *full-size*
file, and the truncate tests exercise `itrunc`; both must pass.

*Sheet 18 owns the arithmetic (maximum size per index level, reads to reach an
offset). This part is the walk, and freeing is the harder direction.*

## Part 3 — Symbolic links

A **symbolic link** is a file whose contents are a path. Add:

- a `symlink(char *target, char *path)` system call that creates a new inode
  of type `T_SYMLINK` at `path` whose data is the string `target` — the bytes
  of the path with no trailing NUL, so the inode's size is `strlen(target)` and
  a read of the link returns exactly that many bytes. The target is stored
  verbatim and is not resolved or checked, so a link may name a path that does
  not exist (it *dangles*) — unlike a hard link, which cannot.
- handling in `open()`: opening a symbolic link **follows** it to its target,
  and if the target is itself a link, follows that, to a bounded depth. A
  chain longer than that bound — in particular a cycle — is refused with `-1`,
  not followed forever.
- the `O_NOFOLLOW` flag (already defined in `kernel/fcntl.h`): when set,
  `open()` opens the **link itself**, not its target. A read of that
  descriptor returns the stored target path.

The `T_SYMLINK` type, the `O_NOFOLLOW` flag, the `SYS_symlink` number and the
user-space `symlink()` stub are already wired up; `sys_symlink()` in
`kernel/sysfile.c` is a stub that returns `-1`, and `open()` does not yet
follow links. That is the work.

The grader runs `user/symlinktest.c`, which follows a link, follows a chain,
opens a link with `O_NOFOLLOW`, opens a dangling link, and opens a cycle. The
cycle case is why the depth limit matters: without it, `open()` never returns
and the grader reports the test as not completing.

*Exercises ch. 39's hard-versus-soft distinction: a symlink can dangle and can
cross file systems because it holds a name, not a reference.*

## Part 4 — A structural checker

Write `xfsck` (in `starter/xfsck/xfsck.c`), a userspace program that reads an
xv6 image — it never mounts or boots it — and verifies the file system's
static structural invariants:

- the super block's magic is correct and the root inode (inode 1) is an
  allocated directory;
- **every data block is either free in the bitmap or reachable from exactly
  one inode** — never both, never neither, never two;
- every allocated inode is named by at least one directory entry, and every
  directory entry names an allocated inode;
- each inode's link count equals the number of directory entries that refer to
  it — noting how the kernel manages `nlink` for directories (look at what
  `create()` does to the parent's and the child's counts, and what it does
  *not* do for `.`);
- every directory has `.` naming itself and `..` naming its parent.

**The reporting format is part of the spec.** Report each violation as one
line of the form

```
FAIL <class>: <detail naming the inode or block at fault>
```

and exit non-zero. The `<class>` tokens are the violation classes of the
invariants above, and the grader looks for them literally — a correct check
reported under a name of your own does not score. The eight graded classes:

| class token | the violation |
|---|---|
| `block-free-but-used` | a block an inode references is marked free in the bitmap |
| `bitmap-leak` | a block marked in use that no inode references |
| `block-double-claim` | a block referenced by two inodes |
| `orphan-inode` | an allocated inode that no directory entry names |
| `dangling-entry` | a directory entry naming a free inode |
| `link-count` | an inode whose `nlink` disagrees with its entries |
| `dotdot` | a directory whose `..` does not name its parent |
| `root` | a root inode that is not an allocated directory |

The detail after the colon is your own wording, and you may invent further
classes for violations outside these eight (a bad magic number, a malformed
`.`). On a clean image print exactly one line, `xfsck: clean`, and exit 0.

You are given the tedious half: `xv6fs.h` (the on-disk format) and `xv6img.c`
(a library that reads the image and hands you the super block, any inode, any
block, and any bitmap bit). The checking logic is yours. Two facts about the
format that the invariants above are stated in terms of:

- the blocks below the data region — boot block, super block, log, inode
  blocks, bitmap — are metadata: always in use, belonging to no inode.
  `img_first_data_block()` marks where the data region begins, and the
  block invariant is about that region.
- a single inode may be named by several directory entries; that is a hard
  link, which is why the reference-count and link-count invariants count
  entries rather than assuming one.

This checker is **static structure only**. It says nothing about crash
recovery, the log, or ordering — that is a journaling checker's job, and a
later lab's subject. Keep `xfsck` away from it.

## Part 5 — Break it and detect it

`corrupt` (given) damages a copy of a `mkimg` image in one named way:
`bitmap-free`, `bitmap-leak`, `linkcount`, `orphan`, `dangling`,
`double-claim`, `dotdot`, `root`. Run each against a fresh image and confirm
`xfsck` reports it:

```sh
./mkimg clean.img
cp clean.img c.img && ./corrupt c.img linkcount && ./xfsck c.img
```

Then look at the ninth mode, `data`, which flips bytes *inside* a file's data
block. Run `xfsck` on it. Write **`UNDETECTABLE.md`**: which corruption
`xfsck` cannot detect, and the argument for why no purely structural checker
could. This is the boundary between a checker and a consistency mechanism, and
it sets up the journaling lab.

---

## Running the Parts 2–5 tests

```sh
tests/run-kernel.sh --fast ~/lab7-xv6   # Parts 2–3 only (build, bigfile,
                                        # symlinktest) — the quick loop
tests/run-kernel.sh ~/lab7-xv6          # …then usertests -q as well
tests/run-kernel.sh --full ~/lab7-xv6   # …with the whole of usertests
tests/run-kernel.sh --stress ~/lab7-xv6 # bigfile at 60000 blocks (optional)
tests/run-xfsck.sh  ~/lab7-xfsck        # Parts 4–5: clean image + every
                                        # corruption, in a second or two
```

Use `--fast` while you work: it builds from clean and runs `bigfile`
(a couple of minutes) and `symlinktest`, and skips the regression. The skipped
regression is counted as a failure, so a fully correct tree still reports
`4 passed, 1 failed` under `--fast` — the one failure is the skipped
regression, not a bug. A full run
adds `usertests`, whose `writebig` writes a *full-size* file and so takes tens
of minutes under emulation — run it before you hand in, not on every edit.
`run-xfsck.sh` needs no QEMU and finishes in seconds.

## If you get stuck (Parts 2–5)

Every entry here says *where to look*, never what to change.

- `bigfile` fails on a write partway through: which file-block number does it
  stop at, and which level of `addrs[]` should be indexing that block? Compare
  it with `NDIRECT` and `NDIRECT + NINDIRECT`.
- `bigfile` writes and reads back fine but fails at the end with `itrunc leaked
  blocks`: the data path is right but the free path is not. Re-read what
  `itrunc()` walks and compare it with what `bmap()` can allocate — every level
  `bmap` can create, `itrunc` must free, and a level it skips leaks silently.
- `usertests`' `writebig` fails but `bigfile` passes (or the reverse): the two
  write files of very different sizes. Which index levels does each one reach?
- `symlinktest` never returns: it is on the cycle case. What bounds the number
  of links `open()` will follow?
- `symlinktest` fails at `O_NOFOLLOW`: is `open()` looking at the flag before
  it decides to follow, and does it open the link or the target when the flag
  is set?
- `open()` deadlocks or panics while resolving a link: what lock does `open()`
  hold while it looks up the target, and can that lookup need the same lock?
- `xfsck` flags a healthy image: which blocks or inodes does it complain
  about? If they are low-numbered blocks, compare against
  `img_first_data_block()`; if it is an inode with two names, that is a hard
  link.
- `xfsck` misses a corruption `corrupt` injected: run `corrupt` with that mode
  on a fresh copy and diff the image bytes (or the affected inode/block) before
  and after — the check that would have noticed the change is the one you are
  missing.
- `xfsck`'s link-count check is off by one on directories only: count the
  directory entries that name a directory inode by hand — its own `.`, its
  entry in its parent, and each child's `..` — and compare with what the
  kernel stores in `nlink`.
