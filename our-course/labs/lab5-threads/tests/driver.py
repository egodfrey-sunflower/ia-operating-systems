#!/usr/bin/env python3
"""Lab 5 autograder driver -- xv6 parts (A and B).

Boots the given xv6 workdir under QEMU (via the shared harness) and runs:
  Part A  uthread          -- cooperative user threads interleave to completion
  Part B  kalloctest       -- per-CPU kmem free lists, low contention
          bcachetest       -- bucketed buffer cache, low contention
          usertests -q     -- regression (kalloc/bcache changes break subtly)

Part C (host pthreads) is NOT run here -- see tests/hostsync.sh, invoked by
tests/run.sh.

Test tiers (see labs/README.md "Conventions & test tiers"): boot+lab tier first (uthread,
kalloctest, bcachetest -- the inner-loop tier), then the regression tier
(`usertests -q`) LAST. QUICK=1 stops after the lab tier and reports the
regression row as SKIPPED (it still appears in the table). Iterate with
QUICK=1; a submission counts only with the full run.

Usage: driver.py <workdir>
Env:   CPUS=n   number of QEMU CPUs (default 3; Part B needs >1 to show
                contention and to exercise cross-CPU stealing).
       QUICK=1  skip the long usertests -q regression run.
       USERTESTS_TIMEOUT=secs  deadline for usertests -q (default 900;
                raise it on a heavily loaded machine).

Exits 0 iff every test passed; prints a PASS/FAIL table.
"""

import os
import re
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(_HERE, "..", "..", "common")))

from qemuharness import Xv6Session, build, HarnessError  # noqa: E402

CPUS = int(os.environ.get("CPUS", "3"))
QUICK = os.environ.get("QUICK", "") not in ("", "0", "false", "False")
# usertests -q takes ~3-4 min on an idle machine but can take far longer on a
# loaded one; let the grader extend the deadline without editing code.
USERTESTS_TIMEOUT = int(os.environ.get("USERTESTS_TIMEOUT", "900"))


class Results:
    # Each row is (name, status, detail) with status one of
    # "PASS", "FAIL", "SKIP". SKIP rows (e.g. the regression tier under
    # QUICK=1) appear in the table but do not count against ok().
    def __init__(self):
        self.rows = []

    def add(self, name, passed, detail=""):
        status = "PASS" if passed else "FAIL"
        self.rows.append((name, status, detail))
        print("  [%s] %s%s" % (status, name, ("  -- " + detail) if detail else ""))

    def skip(self, name, detail=""):
        self.rows.append((name, "SKIP", detail))
        print("  [SKIP] %s%s" % (name, ("  -- " + detail) if detail else ""))

    def ok(self):
        return all(s != "FAIL" for _, s, _ in self.rows)

    def table(self):
        width = max((len(n) for n, _, _ in self.rows), default=4)
        lines = ["", "=" * (width + 34),
                 "  Lab 5 (xv6 parts A+B) test results  [CPUS=%d]" % CPUS,
                 "=" * (width + 34)]
        for name, status, detail in self.rows:
            lines.append("  %-*s  %s%s"
                         % (width, name,
                            "SKIPPED" if status == "SKIP" else status,
                            ("   " + detail) if detail else ""))
        lines.append("=" * (width + 34))
        npass = sum(1 for _, s, _ in self.rows if s == "PASS")
        nskip = sum(1 for _, s, _ in self.rows if s == "SKIP")
        counted = len(self.rows) - nskip
        lines.append("  %d/%d passed%s"
                     % (npass, counted,
                        ("  (%d skipped)" % nskip) if nskip else ""))
        lines.append("")
        return "\n".join(lines)


# ---- Part A ---------------------------------------------------------------

def test_uthread(sess, res):
    name = "A: uthread interleaves to completion"
    try:
        out = sess.run_cmd("uthread", timeout=120)
    except HarnessError as e:
        res.add(name, False, "harness error/timeout: %s" % str(e)[:160])
        return
    # Each of the three threads must start, run to its target (100), and exit,
    # and the scheduler must report clean completion. We also check
    # interleaving: all three "started" and the a/b/c counter lines are mixed
    # rather than one thread running to 100 before another starts.
    exits = re.findall(r"thread_([abc]): exit after (\d+)", out)
    done = "uthread: all threads finished" in out
    started = set(re.findall(r"thread_([abc]) started", out))
    # Interleaving heuristic: the first 30 "thread_X N" counter lines should
    # mention at least two distinct threads (a correct cooperative scheduler
    # round-robins them).
    counts = re.findall(r"thread_([abc]) \d+", out)
    interleaved = len(set(counts[:30])) >= 2
    if len(exits) == 3 and done and started == {"a", "b", "c"} \
            and all(int(n) >= 100 for _, n in exits) and interleaved:
        res.add(name, True, "a/b/c each reached 100, interleaved, and exited")
    else:
        res.add(name, False,
                "expected all of thread_a/b/c to start, interleave, count to "
                "100 and exit; got exits=%s finished=%s interleaved=%s; out=%r"
                % (exits, done, interleaved, out[-200:]))


# ---- Part B ---------------------------------------------------------------

def _statline(out, key):
    # Pull the contended-acquire count for the lock named by `key` from a
    # stats line such as
    #   "test1: kmem #acquire = 42017, #test-and-set (contended) = 12"
    # (kalloctest) or the equivalent "bcache" line (bcachetest). Selecting by
    # key matters: the tests print stats for their own lock, and matching any
    # stats line would silently report the wrong lock's count.
    m = re.search(r"%s #acquire = \d+, #test-and-set \(contended\) = (\d+)"
                  % re.escape(key), out)
    return int(m.group(1)) if m else None


def test_kalloctest(sess, res):
    name = "B1: kalloctest (per-CPU kmem)"
    try:
        out = sess.run_cmd("kalloctest", timeout=300)
    except HarnessError as e:
        res.add(name, False, "harness error/timeout: %s" % str(e)[:160])
        return
    # Trust kalloctest's own verdict (it embeds the contention threshold and
    # the correctness/stealing checks).
    if "kalloctest: OK" in out and "FAIL" not in out:
        nts = _statline(out, "kmem")
        res.add(name, True, "kmem contended=%s" % nts)
    else:
        res.add(name, False, "kalloctest did not report OK; out=%r" % out[-300:])


def test_bcachetest(sess, res):
    name = "B2: bcachetest (bucketed bcache)"
    try:
        out = sess.run_cmd("bcachetest", timeout=300)
    except HarnessError as e:
        res.add(name, False, "harness error/timeout: %s" % str(e)[:160])
        return
    if "bcachetest: OK" in out and "FAIL" not in out:
        nts = _statline(out, "bcache")
        res.add(name, True, "bcache contended=%s" % nts)
    else:
        res.add(name, False, "bcachetest did not report OK; out=%r" % out[-300:])


def test_usertests(sess, res):
    name = "B: usertests -q regression"
    if QUICK:
        res.skip(name, "QUICK=1 set (regression tier deferred)")
        return
    try:
        sess.send_line("usertests -q")
        # Same matching as Lab 4's driver: the final verdicts plus a fail-fast
        # "\bFAILED\b". usertests only prints FAILED when a subtest actually
        # fails (it is not part of any test's expected output), so failing
        # fast here saves minutes on a broken kernel; a pass still requires
        # the literal "ALL TESTS PASSED" verdict.
        m = sess.wait_for(r"ALL TESTS PASSED|SOME TESTS FAILED|\bFAILED\b",
                          timeout=USERTESTS_TIMEOUT)
    except HarnessError as e:
        res.add(name, False, "harness error/timeout: %s" % str(e)[:160])
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
        with Xv6Session(workdir, cpus=CPUS, timeout=180, quiet=True) as sess:
            print("== running tests ==")
            test_uthread(sess, res)
            test_kalloctest(sess, res)
            test_bcachetest(sess, res)
            test_usertests(sess, res)
    except HarnessError as e:
        print("FATAL: could not boot/keep session: %s" % e)
        if not res.rows:
            return 1

    print(res.table())
    return 0 if res.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
