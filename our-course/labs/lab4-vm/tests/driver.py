#!/usr/bin/env python3
"""Lab 4 (virtual memory) autograder driver.

Boots the given xv6 workdir under QEMU (via the shared harness) and runs the
Lab 4 checks:

  * vmprint     -- vmprintme() prints a well-formed page-table tree: the
                   header line, a top-level ' ..255:' entry, at least one
                   level-1 (' .. ..N:') entry, and 3-level-deep leaves
                   including trampoline(511)/trapframe(510). These are
                   structural markers only; full formatting is human-checked
                   (see tests/README.md).
  * ugetpid     -- pgtbltest's ugetpid_test passes (USYSCALL shared page).
  * cowtest     -- all copy-on-write subtests pass.
  * forkbench   -- runs and reports a ticks line (no threshold; the number is
                   echoed for the write-up).
  * usertests   -- the full `usertests -q` regression still passes (the real
                   gate: COW breaks this in many subtle ways).

Test tiers (see labs/README.md "Conventions & test tiers"): the build+boot is the smoke tier;
vmprint/ugetpid/cowtest/forkbench are the lab tier (<~1 min, the default
inner-loop tier); `usertests -q` is the regression tier, run LAST. QUICK=1
stops after the lab tier -- the regression row then appears as SKIPPED in the
table (never silently absent). Students iterate with QUICK=1; a submission
counts only with the full run.

Usage: driver.py <workdir>
Env:   QUICK=1  run only the lab tier; mark the regression tier SKIPPED.
       CPUS=n   number of QEMU CPUs (default 1, for a deterministic,
                load-tolerant regression run; set CPUS=3 to also stress the
                copy-on-write reference-count locking under true concurrency).

Exits 0 iff every test passed; prints a PASS/FAIL table.

Machine hygiene: this driver NEVER kills qemu/make globally. Each Xv6Session
tears down only its own process group (via close()); at the end we PASSIVELY
scan /proc to confirm none of the sessions WE started leaked a qemu process --
we never signal anyone else's qemu.
"""

import os
import re
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(_HERE, "..", "..", "common")))

from qemuharness import Xv6Session, build, HarnessError  # noqa: E402

CPUS = int(os.environ.get("CPUS", "1"))
QUICK = os.environ.get("QUICK", "") not in ("", "0", "false", "False")

# Process groups we spawn, so we can passively check for leaks at the end.
OUR_PGIDS = []


def open_session(timeout=180):
    sess = Xv6Session(WORKDIR, cpus=CPUS, timeout=timeout, quiet=True)
    try:
        OUR_PGIDS.append(os.getpgid(sess._proc.pid))
    except (ProcessLookupError, OSError):
        pass
    return sess


class Results:
    """PASS/FAIL/SKIPPED table. A SKIPPED row (the QUICK=1 regression tier)
    is shown explicitly in the table -- never silently absent -- and does not
    fail the run."""

    def __init__(self):
        self.rows = []  # (name, status, detail); status in PASS/FAIL/SKIPPED

    def add(self, name, passed, detail=""):
        status = "PASS" if passed else "FAIL"
        self.rows.append((name, status, detail))
        print("  [%s] %s%s" % (status, name, ("  -- " + detail) if detail else ""))

    def skip(self, name, detail=""):
        self.rows.append((name, "SKIPPED", detail))
        print("  [SKIPPED] %s%s" % (name, ("  -- " + detail) if detail else ""))

    def ok(self):
        return all(s != "FAIL" for _, s, _ in self.rows)

    def table(self):
        width = max((len(n) for n, _, _ in self.rows), default=4)
        lines = ["", "=" * (width + 34), "  Lab 4 test results", "=" * (width + 34)]
        for name, status, detail in self.rows:
            lines.append("  %-*s  %s%s"
                         % (width, name, status,
                            ("   " + detail) if detail else ""))
        lines.append("=" * (width + 34))
        npass = sum(1 for _, s, _ in self.rows if s == "PASS")
        nskip = sum(1 for _, s, _ in self.rows if s == "SKIPPED")
        summary = "  %d/%d passed" % (npass, len(self.rows))
        if nskip:
            summary += ", %d skipped" % nskip
        lines.append(summary)
        lines.append("")
        return "\n".join(lines)


# Lines like " .. .. ..511: pte 0x... pa 0x..." -- a level-0 (3-levels-deep)
# leaf entry at PTE index 511.
LEAF3 = re.compile(r"^ \.\. \.\. \.\.(\d+): pte 0x[0-9a-f]+ pa 0x[0-9a-f]+", re.M)
HEADER = re.compile(r"page table 0x[0-9a-f]{16}")
# " ..255: pte 0x... pa 0x..." -- the top-level entry covering the
# trampoline/trapframe region (index 255, not 511, because MAXVA is 1<<38).
TOP255 = re.compile(r"^ \.\.255: pte 0x[0-9a-f]+ pa 0x[0-9a-f]+", re.M)
# " .. ..N: pte 0x... pa 0x..." -- a level-1 (mid-level) entry. Cannot match
# a leaf line: there the second ".." is followed by " ..", not a digit.
LEVEL1 = re.compile(r"^ \.\. \.\.\d+: pte 0x[0-9a-f]+ pa 0x[0-9a-f]+", re.M)


def test_vmprint(out, res):
    name = "vmprint: tree format"
    if not HEADER.search(out):
        res.add(name, False, "no 'page table 0x...' header line")
        return
    # Structural markers only -- full formatting is human-checked against the
    # README's required format (see tests/README.md).
    if not TOP255.search(out):
        res.add(name, False,
                "no top-level ' ..255: pte ...' line "
                "(trampoline/trapframe region)")
        return
    if not LEVEL1.search(out):
        res.add(name, False, "no level-1 (' .. ..N: pte ...') lines")
        return
    leaves = set(int(m.group(1)) for m in LEAF3.finditer(out))
    if not leaves:
        res.add(name, False, "no 3-level-deep leaf lines ('.. .. ..N: pte ...')")
        return
    # The trampoline (511) and trapframe (510) leaves sit near the top of the
    # address space; both must be present in a fresh process's page table.
    missing = [idx for idx in (510, 511) if idx not in leaves]
    if missing:
        res.add(name, False,
                "missing expected leaf indices %s near MAXVA (saw %s)"
                % (missing, sorted(leaves)))
        return
    res.add(name, True,
            "header + ..255 top + level-1 + 3-level leaves incl. "
            "trampoline(511)/trapframe(510)")


def test_ugetpid(out, res):
    name = "ugetpid: USYSCALL shared page"
    if "pgtbltest: all tests succeeded" in out or "ugetpid_test: OK" in out:
        res.add(name, True)
    else:
        res.add(name, False,
                "ugetpid_test did not pass; got: %r" % out[-300:])


def test_pgtbl(sess, res):
    try:
        out = sess.run_cmd("pgtbltest", timeout=90)
    except HarnessError as e:
        res.add("vmprint: tree format", False, "harness error: %s" % e)
        res.add("ugetpid: USYSCALL shared page", False, "harness error: %s" % e)
        return
    test_vmprint(out, res)
    test_ugetpid(out, res)


def test_cow(sess, res):
    name = "cowtest: copy-on-write fork"
    try:
        out = sess.run_cmd("cowtest", timeout=180)
    except HarnessError as e:
        res.add(name, False, "harness error / timeout: %s" % e)
        return
    if "ALL COW TESTS PASSED" in out:
        res.add(name, True)
    else:
        res.add(name, False, "cowtest failed; got: %r" % out[-400:])


def test_forkbench(sess, res):
    name = "forkbench: runs + reports ticks"
    try:
        out = sess.run_cmd("forkbench", timeout=180)
    except HarnessError as e:
        res.add(name, False, "harness error / timeout: %s" % e)
        return
    m = re.search(r"forkbench:.*?(\d+)\s*ticks", out)
    if m:
        res.add(name, True, "%s ticks (report this in your write-up)" % m.group(1))
    else:
        res.add(name, False, "no ticks line; got: %r" % out[-300:])


def test_usertests(sess, res):
    name = "usertests -q regression"
    if QUICK:
        res.skip(name, "QUICK=1 set (regression tier deferred; a submission "
                       "counts only with the full run)")
        return
    try:
        sess.send_line("usertests -q")
        # Generous deadline: usertests -q finishes in a couple of minutes on an
        # idle box at CPUS=1, but this machine is often heavily shared. A big
        # timeout means a trip here signals a genuine hang, never a busy box.
        m = sess.wait_for(r"ALL TESTS PASSED|SOME TESTS FAILED|\bFAILED\b",
                          timeout=1200)
    except HarnessError as e:
        res.add(name, False, "harness error / timeout: %s" % e)
        return
    if m.group(0) == "ALL TESTS PASSED":
        res.add(name, True)
    else:
        res.add(name, False, "usertests reported failure (%r)" % m.group(0))


def check_no_leftover_qemu(res):
    """Passively confirm none of the sessions WE started leaked a qemu.

    Scans /proc for live processes whose process group is one we spawned and
    whose command is qemu-system-ri*. Never sends a signal to anything.
    """
    name = "cleanup: no leftover qemu (ours)"
    leaked = []
    ourpg = set(OUR_PGIDS)
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open("/proc/%s/stat" % entry) as f:
                raw = f.read()
            # comm is field 2 in parens; pgrp is field 5 (1-indexed). Because
            # comm may contain spaces/parens, locate the last ')'.
            rp = raw.rfind(")")
            comm = raw[raw.find("(") + 1:rp]
            rest = raw[rp + 2:].split()
            pgrp = int(rest[2])  # (state, ppid, pgrp, ...)
        except (OSError, ValueError, IndexError):
            continue
        if pgrp in ourpg and comm.startswith("qemu-system-ri"):
            leaked.append(entry)
    if leaked:
        res.add(name, False, "leaked qemu pids from our sessions: %s" % leaked)
    else:
        res.add(name, True)


def main():
    global WORKDIR
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <workdir>\n" % sys.argv[0])
        return 2
    WORKDIR = sys.argv[1]

    # NOTE: a stale fs.img can produce spurious usertests failures (e.g. in
    # iref) unrelated to the kernel under test. If the regression tier fails
    # oddly in file-system tests, `rm fs.img` in the workdir and re-run.
    print("== building %s ==" % WORKDIR)
    try:
        build(WORKDIR)
    except HarnessError as e:
        print("BUILD FAILED:\n%s" % e)
        return 1

    res = Results()
    print("== booting xv6 (CPUS=%d) ==" % CPUS)
    try:
        with open_session(timeout=180) as sess:
            print("== running tests ==")
            test_pgtbl(sess, res)
            test_cow(sess, res)
            test_forkbench(sess, res)
            test_usertests(sess, res)
    except HarnessError as e:
        print("FATAL: could not boot/keep session: %s" % e)
        if not res.rows:
            res.add("boot", False, str(e))

    check_no_leftover_qemu(res)

    print(res.table())
    return 0 if res.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
