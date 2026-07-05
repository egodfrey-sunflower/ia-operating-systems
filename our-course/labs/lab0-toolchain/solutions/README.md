# Lab 0 — Reference solutions

```
╔══════════════════════════════════════════════════════════════════╗
║                          ⚠  SPOILERS  ⚠                           ║
║                                                                    ║
║  This directory contains full working solutions for Lab 0.        ║
║  Do NOT read it until you have implemented the tools yourself.     ║
║  Looking now trades the entire point of the lab for nothing.      ║
╚══════════════════════════════════════════════════════════════════╝
```

Reference implementations of `mycat`, `myls`, `mygrep`, plus the same
`Makefile` the students use. Build and test:

```
make                       # -Wall -Wextra -Werror -std=c11 -g, zero warnings
make test                  # runs ../tests/run.sh on this directory
tests/run.sh solutions/    # equivalent, run from the lab root
```

All three are single translation units, raw syscalls only, no stdio.

## Design notes / why the code looks like this

### Shared conventions

- **No stdio.** Every byte of output goes through `write(2)`. Each file has a
  `write_all()` that loops over short writes and retries `EINTR`, and a
  `warn_errno()` that formats `prog: context: strerror(errno)` to fd 2 by
  hand. `strerror` and `malloc` are libc but not stdio, so they are fair
  game; they are the only concessions.
- **Error/exit contract.** A per-operand error is reported to fd 2, does not
  abort the run, and forces a nonzero exit — matching coreutils and what the
  handout specifies.

### `mycat.c`

Streams in 64 KiB chunks straight from the source fd to fd 1; never buffers a
whole file, so it is binary-safe and memory-flat regardless of input size.
`-` and the no-argument case both route to fd 0. This is the simplest tool
and mirrors real `cat`'s exit behaviour (1 if any file failed).

### `myls.c` — the `getdents64` requirement

This is the pedagogically important one. The obvious way to read a directory
is `opendir`/`readdir` from `<dirent.h>` — and it is **banned** here, because
`readdir(3)` is glibc's *buffered* directory reader: it calls `getdents64`
under the hood and hands you cooked `struct dirent`s. The whole point of the
lab is to touch the raw kernel interface, so we call it ourselves:

```c
long n = syscall(SYS_getdents64, fd, buf, sizeof buf);
```

glibc provides **no** `getdents64` wrapper (there is no `getdents64()` symbol
to call), which is exactly why `syscall(2)` is required here rather than
optional. The kernel fills `buf` with variable-length records; we declare
`struct linux_dirent64` ourselves (glibc does not expose it) and walk the
buffer advancing by each record's `d_reclen` — **not** by `sizeof(struct)`,
since the trailing `d_name` makes every record a different size.

Other choices:

- `.` and `..` are dropped; everything else (including dotfiles) is kept —
  the format is defined this way so it is deterministic and easy to grade.
- Names are `strdup`'d into an array and `qsort`'d with a `strcmp`
  comparator, giving "ascending by raw byte value" (C-locale order) as the
  handout defines.
- Entries are stat'd with `fstatat(dirfd, name, …, AT_SYMLINK_NOFOLLOW)`,
  reusing the already-open directory fd instead of rebuilding path strings,
  and not following symlinks.
- The mode string is built by hand: a type char from the `S_IS*` macros, then
  nine `rwx`/`-` chars from `st_mode` tested MSB-first. Setuid/setgid/sticky
  are deliberately *not* rendered (plain `rwx`), which is why the grader can
  compare against `stat -c '%A'` for ordinary fixtures — those have no special
  bits, so `%A` and our output coincide.
- `st_size` is formatted with a tiny hand-rolled `utoa` (no stdio).

### `mygrep.c`

- The whole input is slurped into a grown `malloc`/`realloc` buffer, then
  scanned for `\n`. This sidesteps the annoying case of a match straddling a
  `read()` boundary.
- Substring test is `memmem(3)` (needs `_GNU_SOURCE`) over the length-bounded
  line, because lines are not NUL-terminated; `strstr` would be wrong on
  embedded NULs.
- Every printed line is `\n`-terminated even when the source line was not —
  matching `grep`.
- Filenames are prefixed (`FILE:`) only when more than one file is given, and
  the exit status is grep's (0 matched / 1 none / 2 error). These are what let
  the autograder diff directly against `grep -F`.

## What the autograder checks

`../tests/run.sh <workdir>` builds the tools, lays down a deterministic
fixture tree (normal file, empty file, a file with no trailing newline, a
NUL-containing "binary" file, a subdirectory, a dotfile), and diffs each
tool against a reference: `cat` for `mycat`, `grep -F` for `mygrep`, and a
listing constructed from `stat -c '%A'`/`'%s'` for `myls`. It also checks
stdin paths and exit codes, wraps every invocation in `timeout 10`, and
prints a PASS/FAIL table. It is validated both ways: all PASS against this
directory, and it FAILs against `../starter/` (unimplemented tools).
