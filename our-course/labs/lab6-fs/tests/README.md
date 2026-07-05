# Lab 6 autograder (`tests/`)

`run.sh <workdir>` builds the xv6 tree at `<workdir>` in several
configurations and drives it under QEMU via the shared harness
(`../../common/qemuharness.py`). It prints a PASS/FAIL table and exits
non-zero on any failure.

```
tests/run.sh ~/lab6            # everything (lab tier, then regression tier)
QUICK=1 tests/run.sh ~/lab6    # skip the slow tests: bigfile + usertests
```

## Test tiers

Per `labs/README.md` ("Conventions & test tiers"), the autograder is tiered so the
slow runs never sit in your edit-compile-test loop. Tests, in order:

| Tier | Test | Task | What it asserts | QUICK=1 |
|------|------|------|-----------------|---------|
| lab | `symlinktest` | 2 | follow / O_NOFOLLOW / dangling / chain / cycle / depthcap (11-link chain refused) / unlink | runs |
| lab | `bigfile` | 1 | 2 passes of write+verify+unlink of a 65803-block file; the second pass exhausts the disk if `itrunc` leaks the doubly-indirect tree | **SKIP** |
| lab | `crash BEFORE_HEAD` | 3 | crash before the header write → old consistent state | runs |
| lab | `crash AFTER_HEAD` | 3 | crash after the header write → recovery replays it | runs |
| regression (LAST) | `usertests -q` | gate | the fs changes did not break xv6 | **SKIP** |

Skipped tests still appear in the table, marked `SKIP`, and do not count as
failures — but **a submission counts only with the full run**. `bigfile` and
`usertests -q` each move >100k disk blocks with the enlarged inode (bigfile
~131k per pass, two passes) and take
minutes under QEMU (much longer on a loaded machine); the timeouts are
generous on purpose. A timeout should be read as "hung", never "the box was
busy" — if a run dies with *terminating on signal 15* or times out on a
loaded machine, just rerun it. When an fs-code change looks risky, a useful
middle tier before paying for the full regression is a named subtest, e.g.
`usertests writebig`.

## The crash experiment and fs.img preservation (the hard part)

Task 3 must (1) crash the kernel mid-commit, then (2) **reboot the *same*
disk image** so recovery runs on exactly the bytes the crash left behind. Two
mechanics make this work.

**Freezing the image at the crash instant.** The crashpoint is compiled into
`kernel/log.c`'s `commit()` with `make CRASH=BEFORE_HEAD|AFTER_HEAD`. When a
process arms it via the `crashnow(1)` syscall, the next real commit prints a
marker and calls `panic()`, which spins forever *issuing no further disk
I/O*. Every block write that logically precedes the crashpoint has already
been handed to QEMU (xv6's `bwrite` is synchronous), so it is already in the
host's page cache for `fs.img`; nothing after it ever will be. The driver
waits for the marker and then tears QEMU down with the harness's
process-group `close()`. The image is now frozen at precisely that instant.

**Not regenerating fs.img on reboot.** The stock Makefile rebuilds `fs.img`
only when one of its prerequisites (`mkfs/mkfs`, `README`, the `UPROGS`
binaries) is *newer* than `fs.img`:

```
fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)
```

During phase 1, QEMU **writes** `fs.img`, so its mtime becomes newer than
every prerequisite. Therefore the phase-2 reboot — which is just another
`make qemu` inside a fresh `Xv6Session` — finds `fs.img` up to date and does
**not** rebuild it: it boots the frozen, crashed image and runs log recovery.

The driver does not rebuild between phases (it does **not** call the harness
`build()` there), and as a belt-and-suspenders it also `os.utime()`s
`fs.img` right after the crash so its mtime is unambiguously the newest file
in the tree. The consequence is deliberate and documented: `make qemu` after
a crash reuses the on-disk image; only `make clean` (which the driver runs
*before* each crashpoint, to install a fresh baseline) discards it.

**Why one clean build per crashpoint.** Switching `CRASH=BEFORE_HEAD` →
`CRASH=AFTER_HEAD` only changes a `-D` flag, which `make` does not track. The
driver runs `make clean` before each crashpoint build so the new flag is
actually compiled in (and so each crashpoint starts from a fresh baseline
image).

## Machine-sharing rules honoured here

This harness runs on shared machines. It **never** uses `pkill`/`killall`.
Each QEMU is killed only by the harness's own process-group `close()`. The
final "no leftover qemu" row is a **passive** `pgrep -g <pgid>` over the
process groups *this run* created — it reports survivors, it never signals
them.
