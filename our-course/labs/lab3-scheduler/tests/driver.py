#!/usr/bin/env python3
"""Lab 3 (scheduling) autograder driver.

For each scheduling policy it builds the xv6 tree with the matching SCHED value
(via the compile-time knob the starter wires up), boots it under QEMU with the
shared harness, and runs a self-measuring test program that prints a
`... VERDICT=PASS|FAIL` line which we grep:

  SCHED=PRIO     priotest  -> PRIOSTARVE (high-prio spinner starves low), and
                             PRIOEQUAL  (round robin among equals)
  SCHED=LOTTERY  lotto     -> LOTTERY   (30:10 tickets -> ~3:1 tick share)
  SCHED=MLFQ     mlfqtest  -> MLFQ-RESPONSE (I/O proc in a higher queue than a
                             CPU hog) and MLFQ-BOOST (a bottom-queue hog keeps
                             accruing ticks thanks to the periodic boost)

Task 0 (instrumentation) smoke checks, run on the SCHED=RR build: `schedload`
must print a per-child `rtime=` report line for every child (i.e. getpstat()
works under load), and `pstat` output must be self-consistent (some live
process with ctime <= stime; no live process with a completion time set).
Deterministic and tolerant by design -- it is a smoke check, not a race.

Boot banner: whenever a booted kernel prints `scheduler: <NAME>`, the driver
cross-checks NAME against the SCHED value it just built; a mismatch (stale
objects) is a FAIL. A kernel that prints no banner (e.g. the starter stub)
skips the cross-check -- see the lab README's Setup section for the contract.

Regression: `usertests -q` under SCHED=RR (stock) and SCHED=MLFQ (the most
invasive scheduler).

Scheduling is noisy and this machine may be under load, so every borderline
verdict is retried once before being declared FAIL, deadlines are generous, and
timeouts mean "hung", never "busy".

Cleanup policy: we NEVER pkill/killall qemu -- the harness's close() tears down
only its own process group. The "no leftover qemu" check is passive: it just
confirms the make/qemu process groups WE spawned are gone (and reports, for
information only, a before/after pgrep snapshot).

Usage: driver.py <workdir>
Env:   QUICK=1  skip the long usertests -q regression runs.

Exits 0 iff every test passed; prints a PASS/FAIL table.
"""

import os
import re
import subprocess
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(_HERE, "..", "..", "common")))

from qemuharness import Xv6Session, build, HarnessError  # noqa: E402

QUICK = os.environ.get("QUICK", "") not in ("", "0", "false", "False")

# Deliberately generous: this can be a shared, heavily loaded box, and per the
# ground rules a timeout must mean "hung", never "busy". Override via env if you
# know your machine is idle and want faster failures.
BUILD_TIMEOUT = int(os.environ.get("L3_BUILD_TIMEOUT", "900"))
BOOT_TIMEOUT = int(os.environ.get("L3_BOOT_TIMEOUT", "360"))
TEST_TIMEOUT = int(os.environ.get("L3_TEST_TIMEOUT", "400"))
USERTESTS_TIMEOUT = int(os.environ.get("L3_USERTESTS_TIMEOUT", "2400"))

# make/qemu process-group leader pids for every session we open, so we can
# passively confirm afterwards that our own sessions left nothing behind.
MY_PIDS = []


class Results:
    def __init__(self):
        self.rows = []

    def add(self, name, passed, detail=""):
        self.rows.append((name, passed, detail))
        status = "PASS" if passed else "FAIL"
        print("  [%s] %s%s" % (status, name, ("  -- " + detail) if detail else ""))

    def ok(self):
        return all(p for _, p, _ in self.rows)

    def table(self):
        width = max((len(n) for n, _, _ in self.rows), default=4)
        bar = "=" * (width + 34)
        lines = ["", bar, "  Lab 3 (scheduling) test results", bar]
        for name, passed, detail in self.rows:
            lines.append("  %-*s  %s%s" % (width, name, "PASS" if passed else "FAIL",
                                           ("   " + detail) if detail else ""))
        lines.append(bar)
        npass = sum(1 for _, p, _ in self.rows if p)
        lines.append("  %d/%d passed" % (npass, len(self.rows)))
        lines.append("")
        return "\n".join(lines)


def qemu_pids():
    """Passive snapshot of running qemu-system-riscv pids (never killed)."""
    try:
        out = subprocess.run(["pgrep", "-f", "qemu-system-riscv"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             text=True)
        return set(out.stdout.split())
    except OSError:
        return set()


def clean(workdir):
    subprocess.run(["make", "clean"], cwd=workdir,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)


def build_policy(workdir, sched, res):
    """make clean && SCHED=<sched> make. Records a build PASS/FAIL row."""
    os.environ["SCHED"] = sched
    clean(workdir)
    try:
        build(workdir, timeout=BUILD_TIMEOUT)
    except HarnessError as e:
        res.add("build SCHED=%s" % sched, False, str(e).splitlines()[-1][:120])
        return False
    res.add("build SCHED=%s" % sched, True)
    return True


def open_session(workdir, cpus):
    s = Xv6Session(workdir, cpus=cpus, timeout=BOOT_TIMEOUT, quiet=True)
    MY_PIDS.append(s._proc.pid)
    return s


def grep_verdicts(out, tags):
    d = {}
    for t in tags:
        m = re.search(re.escape(t) + r"[^\n]*VERDICT=(PASS|FAIL)", out)
        d[t] = (m.group(1) == "PASS") if m else None
        if m:
            # Echo the raw verdict line: it carries the measured numbers
            # (tick counts, share ratio, queue levels) for the lab report.
            print("    | %s" % m.group(0))
    return d


def run_verdict_test(sess, cmd, tags, res):
    """Run `cmd`, grep each tag's verdict; retry once for any non-PASS."""
    try:
        out = sess.run_cmd(cmd, timeout=TEST_TIMEOUT)
    except HarnessError as e:
        for t in tags:
            res.add(t, False, "harness error: %s" % str(e).splitlines()[-1][:100])
        return
    d = grep_verdicts(out, tags)
    if any(v is not True for v in d.values()):
        # Noisy scheduling / machine load: give it one more shot.
        try:
            out2 = sess.run_cmd(cmd, timeout=TEST_TIMEOUT)
            d2 = grep_verdicts(out2, tags)
            for t in tags:
                if d.get(t) is not True:
                    d[t] = d2.get(t)
        except HarnessError:
            pass
    for t in tags:
        v = d.get(t)
        detail = "" if v is True else ("no verdict line" if v is None else "reported FAIL (after retry)")
        res.add(t, v is True, detail)


def do_policy(workdir, sched, cmd, tags, res):
    if not build_policy(workdir, sched, res):
        for t in tags:
            res.add(t, False, "skipped: build failed")
        return
    try:
        with open_session(workdir, cpus=1) as s:
            # Boot-banner cross-check (documented in the lab README, Setup):
            # if the kernel announces `scheduler: <NAME>`, NAME must match the
            # policy we just built. No banner (starter stub) -> no check, so
            # the starter fails on the functional tests, not on the banner.
            m = re.search(r"scheduler: (\w+)", s.output)
            if m and m.group(1) != sched:
                res.add("SCHED=%s banner" % sched, False,
                        "booted scheduler '%s'" % m.group(1))
            run_verdict_test(s, cmd, tags, res)
    except HarnessError as e:
        for t in tags:
            res.add(t, False, "boot/run error: %s" % str(e).splitlines()[-1][:100])


def test_task0(sess, res):
    """Task 0 instrumentation smoke checks (run on the SCHED=RR build).

    Cheap and deliberately tolerant -- this grades that the Task 0 plumbing
    (getpstat, tick accounting, ctime/stime/etime) exists and is sane, not
    any scheduling property. Two rows:

      1. `schedload 2 1 20` -- every child must print its report line with a
         non-negative rtime (getpstat works under a mixed load), and the run
         must complete ("schedload: done").
      2. `pstat` -- parse the table; at least one live, already-scheduled
         process must satisfy 0 <= ctime <= stime, and no live (non-zombie)
         process may have a completion time set (etime must be 0, or -1 if a
         solution treats it as unset; it is set only in kexit()).
    """
    name1 = "Task 0: schedload rtime reports (RR)"
    try:
        out = sess.run_cmd("schedload 2 1 20", timeout=TEST_TIMEOUT)
    except HarnessError as e:
        res.add(name1, False, "harness error: %s" % str(e).splitlines()[-1][:100])
        out = ""
    if out:
        reports = re.findall(r"schedload: pid=\d+ kind=(CPU|IO)\s+wall=\d+ "
                             r"rtime=(-?\d+)", out)
        done = "schedload: done" in out
        bad = [r for r in reports if int(r[1]) < 0]
        if done and len(reports) == 3 and not bad:
            res.add(name1, True, "3/3 children reported rtime >= 0")
        else:
            res.add(name1, False,
                    "expected 3 report lines with rtime>=0 and 'schedload: "
                    "done'; got %d line(s), %d with rtime<0, done=%s"
                    % (len(reports), len(bad), done))

    name2 = "Task 0: pstat ctime/stime/etime sanity (RR)"
    try:
        out = sess.run_cmd("pstat", timeout=60)
    except HarnessError as e:
        res.add(name2, False, "harness error: %s" % str(e).splitlines()[-1][:100])
        return
    # Rows as printed by user/pstat.c:
    #   pid state prio tickets level ticks ctime stime etime
    rows = re.findall(r"^(\d+)\t(\w+)\t(-?\d+)\t(-?\d+)\t(-?\d+)\t(-?\d+)\t"
                      r"(-?\d+)\t(-?\d+)\t(-?\d+)\s*$", out, re.M)
    if not rows:
        res.add(name2, False,
                "no parseable pstat rows (getpstat unimplemented?); out=%r"
                % out[-200:])
        return
    ordered = [r for r in rows if r[1] != "zombie"
               and int(r[7]) >= 0 and 0 <= int(r[6]) <= int(r[7])]
    live_done = [r for r in rows if r[1] != "zombie" and int(r[8]) > 0]
    if ordered and not live_done:
        res.add(name2, True,
                "%d live proc(s) with ctime <= stime; no live proc has etime set"
                % len(ordered))
    else:
        res.add(name2, False,
                "expected >=1 live proc with 0 <= ctime <= stime (got %d) and "
                "no live proc with etime > 0 (got %d); rows=%r"
                % (len(ordered), len(live_done), rows[:6]))


def do_usertests(workdir, sched, res, rebuild=True):
    label = "usertests -q (SCHED=%s)" % sched
    if QUICK:
        print("  [SKIP] %s  -- QUICK=1 set" % label)
        return
    if rebuild and not build_policy(workdir, sched, res):
        res.add(label, False, "skipped: build failed")
        return
    try:
        with open_session(workdir, cpus=3) as s:
            s.send_line("usertests -q")
            m = s.wait_for(r"ALL TESTS PASSED|SOME TESTS FAILED|\bFAILED\b|panic",
                           timeout=USERTESTS_TIMEOUT)
            ok = m.group(0) == "ALL TESTS PASSED"
            res.add(label, ok, "" if ok else "reported %r" % m.group(0))
    except HarnessError as e:
        res.add(label, False, "harness error/timeout: %s" % str(e).splitlines()[-1][:120])


def check_cleanup(res, baseline):
    # Passive: confirm every make/qemu process group WE spawned is gone.
    alive = [pid for pid in MY_PIDS if os.path.exists("/proc/%d" % pid)]
    now = qemu_pids()
    new_global = now - baseline  # informational only (other agents share this box)
    detail = ""
    if new_global:
        detail = "(%d qemu pid(s) appeared globally during the run -- may be " \
                 "other users; not ours to touch)" % len(new_global)
    res.add("no leftover qemu (our sessions)", len(alive) == 0,
            ("own pids still alive: %s" % alive) if alive else detail)


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <workdir>\n" % sys.argv[0])
        return 2
    workdir = sys.argv[1]

    baseline = qemu_pids()
    res = Results()

    print("== SCHED=PRIO: static priority (Task 1) ==")
    do_policy(workdir, "PRIO", "priotest 40", ["PRIOSTARVE", "PRIOEQUAL"], res)

    print("== SCHED=LOTTERY: lottery scheduling (Task 2) ==")
    do_policy(workdir, "LOTTERY", "lotto 150", ["LOTTERY"], res)

    print("== SCHED=MLFQ: multi-level feedback queue (Task 3) ==")
    if build_policy(workdir, "MLFQ", res):
        try:
            with open_session(workdir, cpus=1) as s:
                m = re.search(r"scheduler: (\w+)", s.output)
                if m and m.group(1) != "MLFQ":
                    res.add("SCHED=MLFQ banner", False, "booted '%s'" % m.group(1))
                run_verdict_test(s, "mlfqtest 60",
                                 ["MLFQ-RESPONSE", "MLFQ-BOOST"], res)
        except HarnessError as e:
            for t in ("MLFQ-RESPONSE", "MLFQ-BOOST"):
                res.add(t, False, "boot/run error: %s" % str(e).splitlines()[-1][:100])
        # Reuse the MLFQ build for the regression run (no rebuild).
        print("== usertests regression under MLFQ ==")
        do_usertests(workdir, "MLFQ", res, rebuild=False)
    else:
        for t in ("MLFQ-RESPONSE", "MLFQ-BOOST"):
            res.add(t, False, "skipped: build failed")

    print("== SCHED=RR: Task 0 instrumentation smoke + usertests regression ==")
    if build_policy(workdir, "RR", res):
        try:
            with open_session(workdir, cpus=1) as s:
                m = re.search(r"scheduler: (\w+)", s.output)
                if m and m.group(1) != "RR":
                    res.add("SCHED=RR banner", False, "booted '%s'" % m.group(1))
                test_task0(s, res)
        except HarnessError as e:
            for t in ("Task 0: schedload rtime reports (RR)",
                      "Task 0: pstat ctime/stime/etime sanity (RR)"):
                res.add(t, False, "boot/run error: %s"
                        % str(e).splitlines()[-1][:100])
        # Reuse the RR build for the regression run (no rebuild).
        do_usertests(workdir, "RR", res, rebuild=False)
    else:
        for t in ("Task 0: schedload rtime reports (RR)",
                  "Task 0: pstat ctime/stime/etime sanity (RR)",
                  "usertests -q (SCHED=RR)"):
            res.add(t, False, "skipped: build failed")

    check_cleanup(res, baseline)

    print(res.table())
    return 0 if res.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
