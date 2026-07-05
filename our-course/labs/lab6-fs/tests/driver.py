#!/usr/bin/env python3
"""Lab 6 (file systems) autograder driver.

Tiered per labs/README.md "Conventions & test tiers":

  Lab tier (default inner loop; QUICK=1 stops here... mostly):
    * symlinktest              -- Task 2 (symbolic links), fast
    * bigfile                  -- Task 1 (double indirect): 2 passes of
                                  write/verify/unlink of a 65803-block file;
                                  the second pass exhausts the disk if
                                  itrunc() leaks the doubly-indirect tree
                                  (see the arithmetic in user/bigfile.c).
                                  SLOW (minutes per pass): skipped (shown as
                                  SKIP) under QUICK=1
    * crash BEFORE_HEAD /
      crash AFTER_HEAD         -- Task 3, ~a minute each incl. rebuilds

  Regression tier (run LAST):
    * usertests -q             -- skipped (shown as SKIP) under QUICK=1;
                                  a submission counts only with the full run.

Crash-consistency mechanics (Task 3), for BOTH crashpoints:
    1. clean-build the kernel with the crashpoint compiled in
       (`make CRASH=BEFORE_HEAD|AFTER_HEAD`);
    2. boot, run `crashtest phase1` which arms the crashpoint and issues the
       target write -- the kernel freezes mid-commit at the chosen instant;
    3. kill QEMU and REBOOT THE SAME fs.img (no rebuild -- see tests/README
       for how image preservation is guaranteed);
    4. run `crashtest phase2` and assert the recovered state:
         BEFORE_HEAD -> "VERDICT old"  (write lost, file consistent)
         AFTER_HEAD  -> "VERDICT new"  (recovery replayed the committed txn)

Usage: driver.py <workdir>
Env:   QUICK=1  skip the slow tests (bigfile, usertests); rows show as SKIP.
       CPUS=n   QEMU CPUs for the functional sessions (default 1).

Exits 0 iff every non-skipped test passed; prints a PASS/FAIL/SKIP table.

Timeouts are sized for a loaded shared machine: a timeout means "hung",
never "the box was busy".

Machine-sharing note: this driver NEVER uses pkill/killall. Each QEMU is torn
down by the harness's own process-group close(). The "no leftover qemu" check
is a PASSIVE pgrep over the process groups THIS driver created.
"""

import os
import re
import subprocess
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(_HERE, "..", "..", "common")))

from qemuharness import Xv6Session, build, HarnessError  # noqa: E402

QUICK = os.environ.get("QUICK", "") not in ("", "0", "false", "False")
CPUS = int(os.environ.get("CPUS", "1"))

# Process groups this driver launched, for the passive leftover check.
_my_pgids = []


class Results:
    PASS, FAIL, SKIP = "PASS", "FAIL", "SKIP"

    def __init__(self):
        self.rows = []  # (name, status, detail)

    def add(self, name, passed, detail=""):
        status = self.PASS if passed else self.FAIL
        self.rows.append((name, status, detail))
        print("  [%s] %s%s" % (status, name, ("  -- " + detail) if detail else ""),
              flush=True)

    def skip(self, name, why=""):
        self.rows.append((name, self.SKIP, why))
        print("  [SKIP] %s%s" % (name, ("  -- " + why) if why else ""), flush=True)

    def ok(self):
        return all(st != self.FAIL for _, st, _ in self.rows)

    def table(self):
        width = max((len(n) for n, _, _ in self.rows), default=4)
        line = "=" * (width + 34)
        out = ["", line, "  Lab 6 (file systems) test results", line]
        for name, status, detail in self.rows:
            out.append("  %-*s  %s%s" % (width, name, status,
                                         ("   " + detail) if detail else ""))
        out.append(line)
        npass = sum(1 for _, st, _ in self.rows if st == self.PASS)
        nskip = sum(1 for _, st, _ in self.rows if st == self.SKIP)
        counted = len(self.rows) - nskip
        out.append("  %d/%d passed%s"
                   % (npass, counted,
                      (", %d skipped (QUICK=1)" % nskip) if nskip else ""))
        out.append("")
        return "\n".join(out)


def _track(sess):
    """Record a session's QEMU process group for the passive leftover check."""
    try:
        _my_pgids.append(os.getpgid(sess._proc.pid))
    except Exception:
        pass
    return sess


def sh(args, cwd, timeout=900):
    """Run a build/clean command; return (returncode, combined output)."""
    p = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=timeout)
    return p.returncode, p.stdout.decode("utf-8", "replace")


# --------------------------------------------------------------------------
# Lab tier: symlinktest, bigfile
# --------------------------------------------------------------------------

def lab_tier_tests(workdir, res):
    print("== lab tier: clean build (no crashpoint) ==", flush=True)
    sh(["make", "clean"], workdir)
    try:
        build(workdir)
    except HarnessError as e:
        res.add("build (normal)", False, "make failed: %s" % e)
        return

    print("== lab tier: booting for symlinktest ==", flush=True)
    try:
        with _track(Xv6Session(workdir, cpus=CPUS, timeout=300, quiet=True)) as s:
            out = s.run_cmd("symlinktest", timeout=120)
        ok = "symlinktest: ALL TESTS PASSED" in out and "FAIL" not in out
        res.add("symlinktest", ok, "" if ok else repr(out[-300:]))
    except HarnessError as e:
        res.add("symlinktest", False, "harness: %s" % e)

    if QUICK:
        res.skip("bigfile (2x 65803 blocks)", "QUICK=1")
        return
    print("== lab tier: booting for bigfile "
          "(slow: 2 passes of write+verify+unlink of 65803 blocks) ==",
          flush=True)
    try:
        with _track(Xv6Session(workdir, cpus=CPUS, timeout=300, quiet=True)) as s:
            # 2 write/verify/unlink passes (the repetition is what catches an
            # itrunc() that leaks the doubly-indirect tree). Each pass moves
            # ~66k blocks and took well under 15 min even on a loaded box; a
            # 3h ceiling keeps "timeout" meaning HUNG, never busy.
            out = s.run_cmd("bigfile", timeout=10800)
        ok = "bigfile: OK" in out
        res.add("bigfile (2x 65803 blocks)", ok, "" if ok else repr(out[-300:]))
    except HarnessError as e:
        res.add("bigfile (2x 65803 blocks)", False, "harness: %s" % e)


# --------------------------------------------------------------------------
# Lab tier: crash-consistency experiment (Task 3)
# --------------------------------------------------------------------------

def crash_test(workdir, which, res):
    name = "crash %s" % which
    expected = "old" if which == "BEFORE_HEAD" else "new"

    # 1. Clean build with the crashpoint compiled into kernel/log.c.
    print("== crash %s: clean build with CRASH=%s ==" % (which, which), flush=True)
    sh(["make", "clean"], workdir)
    rc, out = sh(["make", "CRASH=" + which, "CPUS=1"], workdir, timeout=900)
    if rc != 0:
        res.add(name, False, "build with CRASH=%s failed\n%s" % (which, out[-400:]))
        return

    # 2. Boot, arm the crashpoint, issue the target write -> kernel freezes.
    print("== crash %s: phase 1 (arm + crash) ==" % which, flush=True)
    try:
        with _track(Xv6Session(workdir, cpus=1, timeout=300, quiet=True)) as s1:
            s1.send_line("crashtest phase1")
            s1.wait_for(r"CRASHPOINT: %s reached" % re.escape(which), timeout=300)
            # Kernel is now spinning in panic() issuing no further I/O.
            # Leaving the `with` block kills QEMU (process-group close),
            # freezing fs.img at exactly this instant.
    except HarnessError as e:
        res.add(name, False, "phase1 never reached crashpoint: %s" % e)
        return

    # Belt-and-suspenders: bump fs.img's mtime so `make qemu` on the reboot
    # can never decide to regenerate it (its prereqs are now strictly older).
    try:
        os.utime(os.path.join(workdir, "fs.img"), None)
    except OSError:
        pass

    # 3. Reboot the SAME fs.img (Xv6Session runs `make qemu`, which does NOT
    #    rebuild fs.img because it is newer than every prerequisite).
    print("== crash %s: phase 2 (reboot same image + verify) ==" % which, flush=True)
    try:
        with _track(Xv6Session(workdir, cpus=1, timeout=300, quiet=True)) as s2:
            boot = s2.output
            out = s2.run_cmd("crashtest phase2", timeout=120)
    except HarnessError as e:
        res.add(name, False, "phase2 boot/run: %s" % e)
        return

    m = re.search(r"VERDICT (\w+)", out)
    verdict = m.group(1) if m else "<none>"
    recovered = "recovering tail" in boot
    ok = (verdict == expected)
    # For AFTER_HEAD the log must have been replayed on reboot.
    if which == "AFTER_HEAD" and not recovered:
        ok = False
    res.add(name, ok, "verdict=%s expected=%s recovery-replayed=%s"
            % (verdict, expected, recovered))


# --------------------------------------------------------------------------
# Regression tier (LAST): usertests -q
# --------------------------------------------------------------------------

def regression_tier(workdir, res):
    if QUICK:
        res.skip("usertests -q (regression)", "QUICK=1")
        return
    # The crash tests left the tree built with a CRASH= flag; rebuild clean so
    # the regression runs the normal kernel.
    print("== regression tier: clean build (no crashpoint) ==", flush=True)
    sh(["make", "clean"], workdir)
    try:
        build(workdir)
    except HarnessError as e:
        res.add("usertests -q (regression)", False, "make failed: %s" % e)
        return
    print("== regression tier: usertests -q (slow) ==", flush=True)
    try:
        with _track(Xv6Session(workdir, cpus=CPUS, timeout=300, quiet=True)) as s:
            s.send_line("usertests -q")
            # With the enlarged inode, usertests's writebig moves ~131k blocks;
            # legitimately slow, slower still on a busy shared box. A generous
            # ceiling: a timeout here means "hung", never "busy".
            m = s.wait_for(r"ALL TESTS PASSED|SOME TESTS FAILED|\bFAILED\b",
                           timeout=5400)
        ok = m.group(0) == "ALL TESTS PASSED"
        res.add("usertests -q (regression)", ok, "" if ok else "got %r" % m.group(0))
    except HarnessError as e:
        res.add("usertests -q (regression)", False, "harness: %s" % e)


# --------------------------------------------------------------------------
# Passive leftover-qemu check (never kills anything)
# --------------------------------------------------------------------------

def leftover_check(res):
    survivors = []
    for pgid in sorted(set(_my_pgids)):
        try:
            p = subprocess.run(["pgrep", "-g", str(pgid)],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            survivors += p.stdout.decode().split()
        except Exception:
            pass
    ok = not survivors
    res.add("no leftover qemu (own sessions)", ok,
            "" if ok else "surviving pids in my groups: %s" % survivors)


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <workdir>\n" % sys.argv[0])
        return 2
    workdir = os.path.abspath(sys.argv[1])
    res = Results()

    lab_tier_tests(workdir, res)
    crash_test(workdir, "BEFORE_HEAD", res)
    crash_test(workdir, "AFTER_HEAD", res)
    regression_tier(workdir, res)  # run LAST per README.md test tiers
    leftover_check(res)

    print(res.table())
    return 0 if res.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
