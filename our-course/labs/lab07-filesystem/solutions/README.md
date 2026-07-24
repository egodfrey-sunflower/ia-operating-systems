# Lab 7 Part 1 — Reference solutions and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  Reference code for the four ch. 39 tools. Read it AFTER you have  ║
║  attempted them, not before. The corners these tools have are the ║
║  lesson; reading the key first trades that for twenty minutes.    ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g, zero warnings
make test       # ../tests/run.sh on this directory: 41 passed, 0 failed, 0 skipped
```

(The `skipped` count is 1, not 0, on a machine where `strace` cannot trace — see
`mytail` below. It is never a *failure*; it means one property went unchecked.)

Everything in Part 1 rests on one distinction from chapter 39: the file
*interface* — an inode's metadata, a directory read as a file, a descriptor's
offset — versus the on-disk *implementation* underneath it, which is Parts 2–5.
None of these four tools ever sees a disk block. They ask the kernel four
questions — *what is this inode?* (`lstat`), *what names does this directory
hold?* (`readdir`), *what is at this offset?* (`lseek`+`read`) — and that is the
whole of the interface.

---

## `mystat` — decode what the inode already knows

<details>
<summary>The type word, and why an empty file is special</summary>

`stat`'s `%F` is a fixed vocabulary keyed on `st_mode & S_IFMT`: `regular
file`, `directory`, `symbolic link`, `character special file`, `block special
file`, `fifo`, `socket`. The one that surprises people is that a **zero-length
regular file** is `regular empty file`, not `regular file` — GNU `stat` spells
it out. The `empty` fixture is there precisely so a tool that returns `regular
file` for everything regular fails one case and no others.

</details>

<details>
<summary>The symbolic mode string, bit by bit</summary>

`%A` is ten characters: a type character, then three groups of `rwx`. The catch
is the special bits, which are folded into the execute slots:

- set-user-ID (`S_ISUID`) turns the owner-execute slot into `s` (if execute is
  also set) or `S` (if not);
- set-group-ID (`S_ISGID`) does the same to the group-execute slot;
- sticky (`S_ISVTX`) turns the other-execute slot into `t` or `T`.

The `setuid`, `setgid` and `sticky` fixtures each carry one of these, so a
decoder that prints plain `rwx` and ignores the high bits fails exactly those
three. `st_mode & 07777` for `%a` keeps those top three bits, which is why the
octal for `setuid` is `4755`, not `755`.

</details>

<details>
<summary>lstat, not stat — the whole reason symlinks are interesting</summary>

`mystat` uses `lstat`. `stat` would follow a symlink and report the *target*,
so a symlink would show as `regular file` and a **dangling** symlink would fail
with `ENOENT` — the target isn't there. `lstat` reports the link's own inode:
type `symbolic link`, size = the length of the target path string, and it
succeeds whether or not the target exists. That is the concrete meaning of "a
symlink is a file whose contents are a path": it has its own inode, independent
of what it points at. The `link_to_reg` and `dangling` fixtures both fail a tool
that used `stat`.

</details>

The reference is `mystat.c`. Errors go to stderr and set the exit status to 1
but do not stop the loop, matching `stat`'s "report and carry on". No operands
at all is a usage error, exit 2 — that path has its own case, checked directly
because `stat`'s own no-operand status differs. The grader compares stdout and
exit status only; the exact stderr wording drifts between coreutils releases
(some say `cannot stat`, newer ones `cannot statx`), so it is not diffed.

---

## `myls -l` — a directory is a file you read

<details>
<summary>Why the oracle is `stat` per entry, not `ls -l`</summary>

A real `ls -l` line carries owner, group and a timestamp, none of which is
reproducible between machines or runs — so there is nothing stable to diff. The
metadata that *is* the lesson (type, permissions, link count, size) is exactly
what `stat --printf='%A %h %s %n'` prints, so the grader builds its oracle from
`stat`, one call per entry, over the sorted non-dot names. `myls` reproduces
that: `readdir` the directory, drop names starting with `.`, sort by raw byte
value, `lstat` each, print the four columns.

</details>

<details>
<summary>The two easy ways to make the diff flaky</summary>

`readdir` returns entries in filesystem order, which differs between machines
and even between runs — hence the `qsort`/`strcmp` sort, under `LC_ALL=C`, so
`Upper` sorts before `alpha` the way byte order (not locale collation) demands.
And the `struct dirent*` that `readdir` returns may be overwritten by the next
call, so each `d_name` is `strdup`'d before the loop continues. Both are shared
with Lab 0's `myls`; the new thing here is the per-entry `lstat`.

</details>

`myls` reuses `mystat`'s mode-string decoder verbatim — the same `%A` logic — so
the `setuid`/`setgid`/`sticky` and hard-link entries in `meta/` exercise it a
second time inside a listing. Reference: `myls.c`.

---

## `mytail` — seek to the answer, don't read to it

This is the tool the part is built around, and the one place where correct
output is **not** enough.

<details>
<summary>The backward scan</summary>

`tail -n N` prints the last `N` lines. To find where they begin without reading
the whole file: `fstat` for the size, then read fixed blocks from the end
backward, counting `\n`. When the count reaches `N`, the byte after that newline
is the start of the last `N` lines; copy from there to EOF. If the file has
fewer than `N` lines the scan runs off the front and the start is 0 — the whole
file, which is what `tail` does too.

The one subtlety is the trailing newline: a `\n` that is the *very last byte* of
the file terminates the final line rather than starting a new one, so it is not
counted. Miss that and every file that ends in a newline is off by one — which
is exactly what the `lines20` cases (trailing newline) catch while the `nonl`
cases (no trailing newline) still pass, pinning the bug to that one rule. The
reference does the scan with `lseek`+`read` rather than `pread` on purpose: it
keeps the demonstration of moving the descriptor's offset explicit, and it is
the offset moving backward that makes the reads bounded.

</details>

<details>
<summary>Why output cannot be the whole test — and how the seek is enforced</summary>

A tool that reads the entire file into memory and then prints the last `N` lines
produces byte-identical output. No diff can tell it from a seeking tool. So the
seek requirement is enforced by *measurement*, not output: the grader runs
`mytail` on the 4 MiB `big` fixture under `strace` and charges it for the file
bytes it pulls in — the bytes returned by the whole read family (`read`,
`pread64`, `readv`, `preadv`, `preadv2`) on any descriptor, plus the length of
any `mmap` of the file itself. The `mmap` charge exists because mapped bytes
arrive by page fault, invisible to read-syscall accounting: a tool that maps
the file and touches every page issues no `read` at all, so the mapping is
charged up front (`strace -y` ties each descriptor to its file). The total must
stay under 1 MiB. The reference is charged about 9.5 KiB (one block to find ten
lines, plus the few hundred bytes it prints); a whole-file streamer — by
`read`, `pread` or `mmap` — is charged at least 4 MiB and fails. That case also
re-checks the output, so a tool that reads nothing and prints nothing fails it
too rather than sneaking under the byte budget.

What the accounting deliberately does not chase: kernel-side copies that never
surface the bytes to the process (`sendfile`, `splice`, `copy_file_range`,
io_uring). To pick the right lines a tool still has to see the bytes, and
reading back a staged copy is charged like any other read; a determined enough
evasion through those calls is out of scope for this grader.

If `strace` cannot trace (restricted `ptrace` in a container), the case reports
**SKIP** and says so loudly: the requirement is real but unenforced on that
machine, and you should run the tests somewhere `strace` works at least once.

</details>

There is deliberately no stdin mode. `tail` reads stdin, but a pipe is not
seekable, and the entire point here is `lseek` on a seekable file. Reference:
`mytail.c`.

---

## `myfind` — the recursive walk

<details>
<summary>Whole name, every depth, and directories count</summary>

`find START -name NAME` tests the last path component of every node — `START`
included — for equality with `NAME`, and recurses through every directory. Four
ways to get it wrong, each caught by its own fixture:

- **substring instead of equality** — `strstr` where `strcmp` belongs reports
  `foobar` and `barfoo` when asked for `foo`;
- **ignoring case** — `strcasecmp` where `strcmp` belongs reports `FOO` when
  asked for `foo` (and the three `foo`'s when asked for `FOO`); the `FOO`
  fixture and the `case matters` case exist for exactly this;
- **not recursing** — a walk that only lists `START` misses `a/foo`, `a/b/foo`
  and `a/b/c/deep_target`;
- **matching files only** — skipping directories in the test misses
  `a/needle_dir`.

The grader sorts both outputs before diffing, because `find`'s order (and yours)
is `readdir` order and not defined.

</details>

<details>
<summary>lstat again: do not follow links, or the walk loops</summary>

The walk decides whether to recurse with `lstat` and `S_ISDIR`. Because `lstat`
does not follow symlinks, a symlink is `S_ISLNK`, not `S_ISDIR`, so the walk
matches it by name and never descends through it. The `loop` fixture is a
symlink to its own directory: a walk that used `stat` (following links) would
recurse into it forever and be killed by the timeout. `find` without `-L`
behaves exactly this way. This is the same `stat`-vs-`lstat` distinction as
`mystat`, seen from the walking side.

</details>

Reference: `myfind.c`. The recursion carries the target name in a file-scope
variable to keep the walk function to one argument; passing it down explicitly
is just as good.

---

## What the fixtures and cases guarantee

Every tool has at least one case that a do-nothing stub (prints nothing, exits
0) fails: `mystat: regular file`, `myls: mixed directory`, `mytail: last 1 of
20 lines`, `myfind: whole-name match at three depths`. The handful of cases a
do-nothing stub *passes* — empty directory, `n = 0`, empty file, no matches —
are there to pin down correct behaviour on empty input, not to carry the grade.

The mode-string, link-count, `lstat`, trailing-newline, read-budget, recursion
and substring corners each have a case that fails when that one thing is wrong
and passes otherwise, which is what lets a failing run point at a single cause.

<!-- ===================================================================== -->

---

# Parts 2–5 answer key

The reference kernel (Parts 2–3) is in `solutions/overlay/`; apply it with
`solutions/apply.sh <workdir>`. The reference checker (Part 4) is
`solutions/xfsck/xfsck.c` — build it in place of your own in a copy of
`starter/xfsck/`. The Part 5 essay key is `solutions/xfsck/UNDETECTABLE.md`.

## Part 2 — large files

The on-disk inode must stay 64 bytes so `IPB` (inodes per block) stays 16.
`addrs[]` already had `NDIRECT+1 = 13` entries; keep 13 entries but re-split
them by setting `NDIRECT` to **11**, so `addrs[]` is `addrs[NDIRECT+2]` = 11
direct + one singly indirect (`addrs[NDIRECT]`) + one doubly indirect
(`addrs[NDIRECT+1]`). `sizeof(struct dinode)` is unchanged at 64, which is why
`mkfs` and the inode-per-block macros need no edits. `MAXFILE` becomes
`NDIRECT + NINDIRECT + NINDIRECT*NINDIRECT = 11 + 256 + 65536 = 65803`. The
in-memory `struct inode` in `file.h` mirrors `addrs[]`, so it changes to match.

`bmap()` grows a third branch. After the direct and singly indirect branches
subtract their ranges from `bn`, the doubly indirect branch reads
`addrs[NDIRECT+1]` (allocating it if zero), indexes it by `bn / NINDIRECT` to
find the singly indirect block (allocating that if zero, and `log_write`-ing
the doubly indirect block when it does), then indexes that by `bn % NINDIRECT`
for the data block. Two `bread`/`brelse` pairs, one per level.

`itrunc()` grows a symmetric block. After freeing the direct and singly
indirect blocks, if `addrs[NDIRECT+1]` is set, read it; for each nonzero
first-level pointer, read that singly indirect block, free each nonzero data
block it names, then free the singly indirect block itself; finally free the
doubly indirect block and zero the slot. **The failure mode here is silent**:
a leak frees nothing visible and the file still reads back correctly. That is
why `bigfile` takes the free-block count (via the given `freeblocks()` syscall)
before writing and again after unlinking, and fails if the two differ: a leaky
`itrunc` reads back perfectly but leaves the doubly indirect blocks marked in
use. Freeing is the direction that is easy to get wrong.

`freeblocks()` (in `fs.c`, wired through `sys_freeblocks`) is given test
support — it scans the free bitmap and returns the count. Students do not write
it; it exists so `bigfile` can prove `itrunc` returned every block `bmap` took.
`bigfile` writes a few thousand blocks by default (fast, and already deep in
the doubly indirect index); `bigfile 60000` / the grader's `--stress` mode is
the optional large-file demonstration.

`FSSIZE` is bumped to 70000 in `param.h` (given). It has to hold a full
`MAXFILE` file — 65803 data blocks plus the ~258 index blocks that address
them, plus metadata and the initial programs — which `writebig` in `usertests`
actually creates. 70000 keeps the bitmap at nine blocks, which also keeps a
full-file `itrunc` inside one log transaction.

## Part 3 — symbolic links

`sys_symlink()` (in `sysfile.c`): read the two string arguments, `create()` an
inode of type `T_SYMLINK` at `path` — `create()` returns it locked — then
`writei()` the target string as the inode's data, `iunlockput`, done, all
inside one `begin_op`/`end_op`.

`open()`: in the non-`O_CREATE` branch, after `namei` and `ilock`, if
`O_NOFOLLOW` is *not* set, loop while the inode is a `T_SYMLINK`: read its data
(the target path) into the path buffer, **`iunlockput` the link**, `namei` the
target, `ilock` it. The `iunlockput` before `namei` is essential — holding the
link's lock while resolving a target that may be the same inode deadlocks. A
counter bounds the loop (the reference uses 10); on overflow, `iunlockput` and
return `-1`, which is what turns a cycle from a hang into an error. The
directory-vs-`O_RDONLY` check then runs on the *final* target.

With `O_NOFOLLOW` the loop is skipped, so a symlink is opened as itself and a
read returns the stored path. `symlinktest` checks both readings.

## Part 4 — the checker

`solutions/xfsck/xfsck.c`, ~300 lines, given the reader library. The shape:

1. **Super block + root.** Magic must be `FSMAGIC` (bail if not — nothing else
   can be trusted). Inode 1 must exist and be `T_DIR`.
2. **Ownership pass.** For every allocated inode, walk its `addrs[]` — direct
   blocks, the singly indirect block *and* the data blocks it names, the
   doubly indirect block *and* every singly indirect block it names *and* their
   data blocks — and record each block's owner in an array. A block already
   owned when a second inode claims it is a `block-double-claim`; a block
   outside the data region is a `range` error.
3. **Bitmap cross-check**, over the data region only (`img_first_data_block()`
   to `sb.size`): in-use-but-unowned is a `bitmap-leak`; free-but-owned is
   `block-free-but-used`. The metadata region is skipped precisely so the
   reserved blocks are not mistaken for leaks — the classic false positive.
4. **Directory pass.** For every directory inode, read its entries; count the
   references to each inode (so hard links are counted, not flagged), flag an
   entry that names a free inode (`dangling-entry`), and record each
   subdirectory's parent from its real name so `..` can be checked.
5. **`.` and `..`.** First entry must be `.` → self; second must be `..` →
   the recorded parent (the root is its own parent).
6. **Orphans and link counts.** An allocated inode with zero references is an
   `orphan-inode` (the root excepted). Otherwise its `nlink` must equal the
   reference count, **minus one for a directory** — the kernel does not count a
   directory's own `.`. Getting that `-1` wrong is the way a healthy image
   fails the link-count check on directories only.

The false positives to avoid — reserved blocks, hard links, the directory
`-1` — are each guarded above, and `run-xfsck.sh`'s clean-image case (the
image has a hard link and a reserved region) fails a checker that trips on any
of them.

## Part 5 — what a structural checker cannot see

See `solutions/xfsck/UNDETECTABLE.md`. In one line: structural checking
verifies that the *bookkeeping* is self-consistent — pointers, counts,
bitmaps — and file *contents* are not bookkeeping. `corrupt data` flips bytes
inside a block that is correctly allocated, correctly owned, correctly counted;
every invariant still holds, so `xfsck` is right to say `clean`. Detecting it
needs redundancy the format does not carry (a checksum) — which is a
consistency mechanism, not a checker, and the subject of the journaling lab.

## The mutation table (how each corner is a firing test)

Kernel (Parts 2–3), each mutant fails the named case:

| mutant | fails |
|---|---|
| `bmap` without the doubly indirect branch | `bigfile` (panics / write stops at block 267); `usertests` `writebig` |
| `itrunc` that skips the doubly indirect block (leak) | `bigfile` (free-block count does not return to baseline after unlink — caught at the fast ~4000-block size, ~90 s) |
| `sys_symlink` left as the stub | `symlinktest` (open through a link) |
| `open` that does not follow links | `symlinktest` (open through a link) |
| `open` that ignores `O_NOFOLLOW` | `symlinktest` (O_NOFOLLOW opens the link) |
| `open` with no depth limit | `symlinktest` does not return → the case is reported as not completing (a bounded timeout, not a hung grader) |

Checker (Parts 4–5), each `corrupt` mode fails a case, and a stub fails all:

| corruption | class asserted |
|---|---|
| `bitmap-free` | `block-free-but-used` |
| `bitmap-leak` | `bitmap-leak` |
| `linkcount` | `link-count` |
| `orphan` | `orphan-inode` |
| `dangling` | `dangling-entry` |
| `double-claim` | `block-double-claim` |
| `dotdot` | `dotdot` |
| `root` | `root` |
| `data` | none — asserted `clean` (the Part 5 boundary) |

A do-nothing `xfsck` (prints nothing, exits 0) fails the clean case (no
`xfsck: clean`) and every corruption case (no class line), so the stub scores
zero.
