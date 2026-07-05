#!/usr/bin/env python3
"""Lab 2 autograder driver.

Boots the given xv6 workdir under QEMU (via the shared harness) and runs the
Lab 2 checks: trace() output format, trace-mask inheritance across fork/exec,
per-process trace isolation (an untraced command must not be traced),
sysinfo(), getppid(), and finally a regression run of `usertests -q`.

Usage: driver.py <workdir>
Env:   QUICK=1  skip the long usertests regression run.

Exits 0 iff every test passed; prints a PASS/FAIL table.
"""

import os
import re
import sys

# Make the shared harness importable: it lives in ../../common relative to
# this file (labs/common/qemuharness.py).
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(_HERE, "..", "..", "common")))

from qemuharness import Xv6Session, build, HarnessError  # noqa: E402

CPUS = int(os.environ.get("CPUS", "1"))
QUICK = os.environ.get("QUICK", "") not in ("", "0", "false", "False")


class Results:
    def __init__(self):
        self.rows = []  # (name, passed, detail)

    def add(self, name, passed, detail=""):
        self.rows.append((name, passed, detail))
        status = "PASS" if passed else "FAIL"
        print("  [%s] %s%s" % (status, name, ("  -- " + detail) if detail else ""))

    def ok(self):
        return all(p for _, p, _ in self.rows)

    def table(self):
        width = max((len(n) for n, _, _ in self.rows), default=4)
        lines = ["", "=" * (width + 30), "  Lab 2 test results", "=" * (width + 30)]
        for name, passed, detail in self.rows:
            lines.append("  %-*s  %s%s"
                         % (width, name, "PASS" if passed else "FAIL",
                            ("   " + detail) if detail else ""))
        lines.append("=" * (width + 30))
        npass = sum(1 for _, p, _ in self.rows if p)
        lines.append("  %d/%d passed" % (npass, len(self.rows)))
        lines.append("")
        return "\n".join(lines)


def test_trace_read(sess, res):
    name = "trace: read syscall format"
    try:
        out = sess.run_cmd("trace 32 grep hello README", timeout=40)
    except HarnessError as e:
        res.add(name, False, "harness error: %s" % e)
        return
    lines = [l for l in out.splitlines()
             if re.match(r"^\d+: syscall read -> \d+\s*$", l.strip())]
    if lines:
        res.add(name, True, "%d matching trace line(s)" % len(lines))
    else:
        res.add(name, False, "no '<pid>: syscall read -> <n>' lines; got: %r"
                % out[:200])


def test_trace_inherit(sess, res):
    name = "trace: mask inherited across fork/exec"
    try:
        out = sess.run_cmd("trace 2 ppidtest", timeout=60)
    except HarnessError as e:
        res.add(name, False, "harness error: %s" % e)
        return
    pids = set(re.findall(r"(\d+): syscall fork -> \d+", out))
    adopted = "ppidtest: adopt OK" in out
    if len(pids) >= 2 and adopted:
        res.add(name, True,
                "fork traced from pids %s; ppidtest ran under trace"
                % ",".join(sorted(pids)))
    elif len(pids) < 2:
        res.add(name, False,
                "expected fork traced from >=2 distinct pids "
                "(inheritance), saw pids %s" % (sorted(pids) or "none"))
    else:
        res.add(name, False, "ppidtest did not report 'adopt OK' under trace")


def test_trace_per_process(sess, res):
    """The rubric says the mask is per-process: a traced command must emit
    trace lines, and a subsequent UNTRACED command in the same boot must
    emit none. A kernel using one global mask fails this (the mask set by
    the traced run leaks into every later process, including the shell)."""
    name = "trace: mask is per-process (untraced cmd silent)"
    trace_line = re.compile(r"^\s*\d+: syscall \w+ -> -?\d+\s*$")
    try:
        traced = sess.run_cmd("trace 32 grep hello README", timeout=40)
        untraced = sess.run_cmd("grep hello README", timeout=40)
    except HarnessError as e:
        res.add(name, False, "harness error: %s" % e)
        return
    # Only the untraced command's own output window is inspected: run_cmd
    # captures from the moment the command is sent to its next prompt.
    if not any(trace_line.match(l) for l in traced.splitlines()):
        res.add(name, False,
                "traced run produced no trace lines, cannot judge isolation")
        return
    leaked = [l for l in untraced.splitlines() if trace_line.match(l)]
    if leaked:
        res.add(name, False,
                "untraced command emitted %d trace line(s) -- mask is not "
                "per-process; e.g. %r" % (len(leaked), leaked[0]))
    else:
        res.add(name, True, "no trace lines from untraced command")


def test_sysinfo(sess, res):
    name = "sysinfo: freemem + nproc"
    try:
        out = sess.run_cmd("sysinfotest", timeout=60)
    except HarnessError as e:
        res.add(name, False, "harness error: %s" % e)
        return
    if "sysinfotest: OK" in out and "FAIL" not in out:
        res.add(name, True)
    else:
        res.add(name, False, "sysinfotest did not print OK; got: %r" % out[:300])


def test_ppid(sess, res):
    name = "getppid: basic + adoption"
    try:
        out = sess.run_cmd("ppidtest", timeout=60)
    except HarnessError as e:
        res.add(name, False, "harness error: %s" % e)
        return
    basic = "ppidtest: basic OK" in out
    adopt = "ppidtest: adopt OK" in out
    if basic and adopt and "FAIL" not in out:
        res.add(name, True)
    else:
        missing = []
        if not basic:
            missing.append("basic OK")
        if not adopt:
            missing.append("adopt OK")
        if "FAIL" in out:
            missing.append("saw FAIL")
        res.add(name, False, "missing/failed: %s; got: %r"
                % (", ".join(missing), out[:300]))


def test_usertests(sess, res):
    name = "usertests -q regression"
    if QUICK:
        print("  [SKIP] %s  -- QUICK=1 set" % name)
        return
    try:
        sess.send_line("usertests -q")
        # ~3-4 min on an idle machine, but scale for loaded/slow hosts:
        # a timeout here must mean "hung", never "busy box".
        m = sess.wait_for(r"ALL TESTS PASSED|SOME TESTS FAILED|\bFAILED\b",
                          timeout=1200)
    except HarnessError as e:
        res.add(name, False, "harness error / timeout: %s" % e)
        return
    if m.group(0) == "ALL TESTS PASSED":
        res.add(name, True)
    else:
        res.add(name, False, "usertests reported failure (%r)" % m.group(0))


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <workdir>\n" % sys.argv[0])
        return 2
    workdir = sys.argv[1]

    print("== building %s ==" % workdir)
    try:
        build(workdir)
    except HarnessError as e:
        print("BUILD FAILED:\n%s" % e)
        return 1

    res = Results()
    print("== booting xv6 (CPUS=%d) ==" % CPUS)
    try:
        with Xv6Session(workdir, cpus=CPUS, timeout=120, quiet=True) as sess:
            print("== running tests ==")
            test_trace_read(sess, res)
            test_trace_inherit(sess, res)
            test_trace_per_process(sess, res)
            test_sysinfo(sess, res)
            test_ppid(sess, res)
            test_usertests(sess, res)
    except HarnessError as e:
        # A harness error that escapes the per-test handlers means the run
        # is incomplete: fail unconditionally, even if every row recorded
        # so far happened to pass.
        print("FATAL: could not boot/keep session: %s" % e)
        print(res.table())
        print("FATAL: run incomplete -- failing regardless of the rows above.")
        return 1

    print(res.table())
    return 0 if res.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
