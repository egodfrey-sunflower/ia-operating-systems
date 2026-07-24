#!/usr/bin/env python3
"""Lab 7 autograder, Parts 2-3 (large files and symbolic links).

Invoked through run-kernel.sh. Unlike Lab 2 this is a SINGLE kernel -- there
are no compile-time policies -- so the tree is built once, from clean, and
every case runs against it.

WHAT IT GRADES

  Part 2   the doubly indirect block, by running user/bigfile.c. By default
           bigfile writes a file of ~4000 blocks (past file block 267, where
           the doubly indirect level begins; --stress writes 60000), reads
           every block back and checks the bytes, then unlinks the file and
           checks the free-block count returned exactly to its pre-write
           baseline -- so a bmap that cannot reach the doubly indirect level
           fails the write, and an itrunc that leaks blocks instead of
           freeing them fails the free-count comparison.

  Part 3   the symlink() system call and open()'s handling of links, by
           running user/symlinktest.c: following a link, a chain of links,
           O_NOFOLLOW opening the link itself, a dangling link, and a cycle
           that must be refused rather than followed forever.

  regress  xv6's own usertests. writebig (in the quick set) writes and reads a
           full MAXFILE file, which is the doubly indirect path exercised end
           to end; the truncate tests exercise itrunc. A subtly wrong bmap or
           itrunc that slips past bigfile is caught here.

HOW IT AVOIDS PASSING VACUOUSLY

  Each test program is SILENT ON FAILURE: it prints its "ok" token only after
  its last check passes, and exits without it on every failure path. So the
  gate is the "ok" line, never a mid-run "done" -- a program that died early
  prints no "ok" and fails. A kernel that loops forever on a symlink cycle
  never returns, so symlinktest never prints anything: run_cmd's deadline
  elapses and the case is reported as not completing, which is the intended
  signal for "no cycle limit", not a hung grader.

ONE EMULATOR AT A TIME, AND NONE AFTERWARDS

  qemuharness enforces both; this script sweeps at the end and says so.
"""

import hashlib
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "..", "common")))

try:
    from qemuharness import (Xv6Session, build, clean, sweep,
                             HarnessError, HarnessTimeout, KernelPanic)
except ImportError as e:                                # pragma: no cover
    sys.stderr.write("run-kernel.py: cannot import the shared QEMU harness "
                     "(labs/common/qemuharness.py): %s\n" % e)
    sys.exit(2)

BOOT_TIMEOUT = 120
SYMLINKTEST_TIMEOUT = 180
# The default bigfile writes ~4000 blocks (a couple of minutes under emulation)
# -- well past the old 268-block maximum, so the doubly indirect level is
# genuinely used, and the free-block check catches an itrunc leak. --stress
# runs the 60000-block version, which is tens of minutes. usertests' writebig
# writes and reads a full 65803-block file, so usertests is slow too. These are
# deadlines after which a wedged kernel is declared wedged, not performance
# budgets; a real bug shows up as a wrong answer or a panic long before them.
BIGFILE_TIMEOUT = 600
BIGFILE_STRESS_NB = 60000
BIGFILE_STRESS_TIMEOUT = 4200
USERTESTS_TIMEOUT_QUICK = 4200
USERTESTS_TIMEOUT_FULL = 6000

RESULTS = []
COUNTS = {"pass": 0, "fail": 0}


def record(name, verdict):
    RESULTS.append((verdict, name))
    COUNTS["pass" if verdict == "PASS" else "fail"] += 1


def ok(name):
    record(name, "PASS")


def bad(name, why, evidence=None):
    record(name, "FAIL")
    sys.stderr.write("  [%s] %s\n" % (name, why))
    if evidence:
        for line in evidence.rstrip("\n").split("\n")[-20:]:
            sys.stderr.write("    | %s\n" % line)


def check(name, condition, why, evidence=None):
    if condition:
        ok(name)
    else:
        bad(name, why, evidence)
    return bool(condition)


def why_died(what, exc):
    if isinstance(exc, KernelPanic):
        return "the kernel panicked while running %s" % what
    if isinstance(exc, HarnessTimeout):
        return ("the kernel stopped responding while running %s (a deadline "
                "elapsed; nothing more arrived on the console)" % what)
    return "QEMU exited before %s finished" % what


def build_tree(workdir):
    print("== building %s, from clean ==" % workdir)
    clean(workdir)
    try:
        build(workdir, [], timeout=900)          # the kernel
        build(workdir, ["fs.img"], timeout=900)  # the disk image (70 MB)
    except HarnessError as e:
        sys.stderr.write("%s\n" % e)
        print()
        print("RESULT: build failure")
        sys.exit(1)
    print("build OK")


def session(workdir):
    return Xv6Session(workdir, cpus=1, timeout=BOOT_TIMEOUT)


# The two given test programs. They live in the student's editable tree, so
# without this check the most tempting edit at hour three of a kernel lab --
# deleting the assertion that will not pass -- silently scores the part.
GIVEN = ["user/bigfile.c", "user/symlinktest.c"]


def check_given(workdir):
    for rel in GIVEN:
        name = "Build: %s is unmodified" % rel
        ours = os.path.join(HERE, "..", "starter", "overlay", rel)
        theirs = os.path.join(workdir, rel)
        try:
            with open(ours, "rb") as f:
                a = hashlib.sha256(f.read()).hexdigest()
            with open(theirs, "rb") as f:
                b = hashlib.sha256(f.read()).hexdigest()
        except OSError as e:
            bad(name, "cannot read %s: %s" % (rel, e))
            continue
        check(name, a == b,
              "%s differs from the copy in starter/overlay/. It is given, and "
              "the grader reads its output; restore it with setup.sh or by "
              "copying it back." % rel)


def part2(workdir, stress):
    name = "Part 2: bigfile writes a large file, reads it back, and frees it"
    if stress:
        cmd = "bigfile %d" % BIGFILE_STRESS_NB
        timeout = BIGFILE_STRESS_TIMEOUT
        print("== Part 2: large files (bigfile %d -- STRESS, tens of minutes) =="
              % BIGFILE_STRESS_NB)
    else:
        cmd = "bigfile"
        timeout = BIGFILE_TIMEOUT
        print("== Part 2: large files (bigfile -- a couple of minutes) ==")
    try:
        with session(workdir) as s:
            out = s.run_cmd(cmd, timeout=timeout)
    except HarnessError as e:
        bad(name, why_died("bigfile", e), str(e))
        return
    check(name, "bigfile: ok" in out,
          "bigfile did not print its success line. A 'FAIL write of block N' "
          "means bmap could not reach that block (the doubly indirect level "
          "is missing or wrong); a 'FAIL itrunc leaked blocks' means the write "
          "and read-back were fine but unlinking the file did not return its "
          "blocks to the free list -- itrunc frees fewer levels than bmap "
          "allocates. If the shell said 'exec bigfile failed', $U/_bigfile is "
          "missing from UPROGS.", out)


def part3(workdir):
    name = "Part 3: symlink() and open() follow links correctly"
    print("== Part 3: symbolic links (symlinktest) ==")
    try:
        with session(workdir) as s:
            out = s.run_cmd("symlinktest", timeout=SYMLINKTEST_TIMEOUT)
    except HarnessTimeout as e:
        bad(name,
            "symlinktest did not return. The last thing it does is open a "
            "cycle of symbolic links (sta -> stb -> sta); a kernel with no "
            "depth limit follows that forever. open() must give up after a "
            "bounded number of links and return -1.", str(e))
        return
    except HarnessError as e:
        bad(name, why_died("symlinktest", e), str(e))
        return
    check(name, "symlinktest: ok" in out,
          "symlinktest did not print its success line. The 'FAIL ...' line "
          "above names the first check that failed -- following a link, a "
          "chain of links, O_NOFOLLOW, a dangling link, or a cycle. If the "
          "shell said 'exec symlinktest failed', $U/_symlinktest is missing "
          "from UPROGS.", out)


def usertests(workdir, full):
    name = "Regression: usertests passes"
    cmd = "usertests" if full else "usertests -q"
    timeout = USERTESTS_TIMEOUT_FULL if full else USERTESTS_TIMEOUT_QUICK
    print("== regression: %s (this is the slow one; writebig exercises the "
          "doubly indirect path end to end) ==" % cmd)
    try:
        with session(workdir) as s:
            out = s.run_cmd(cmd, timeout=timeout)
    except HarnessTimeout as e:
        bad(name,
            "%s did not finish within %d s. On a busy or low-memory machine "
            "this is usually the machine, not your kernel -- but writebig now "
            "writes a full-size file, which is genuinely slow under emulation. "
            "Re-run it alone before suspecting your code." % (cmd, timeout),
            str(e))
        return
    except HarnessError as e:
        bad(name, why_died(cmd, e), str(e))
        return
    check(name, "ALL TESTS PASSED" in out,
          "%s did not print ALL TESTS PASSED. A leak in itrunc shows up here "
          "as a later test running out of disk; a wrong bmap shows up in "
          "writebig or the truncate tests." % cmd,
          "\n".join(out.rstrip("\n").split("\n")[-25:]))


def usage(code):
    sys.stderr.write(
        "usage: run-kernel.sh [--fast | --full] [--stress] <workdir>\n"
        "  (default)  Parts 2-3, then 'usertests -q'\n"
        "  --fast     Parts 2-3 only; skips the regression check\n"
        "  --full     Parts 2-3, then the whole of 'usertests'\n"
        "  --stress   run bigfile at %d blocks instead of the default ~4000\n"
        "             (optional; tens of minutes under emulation)\n"
        % BIGFILE_STRESS_NB)
    sys.exit(code)


def main(argv):
    full = False
    fast = False
    stress = False
    args = []
    for a in argv[1:]:
        if a == "--full":
            full = True
        elif a == "--fast":
            fast = True
        elif a == "--stress":
            stress = True
        elif a in ("-h", "--help"):
            usage(0)
        elif a.startswith("-"):
            sys.stderr.write("run-kernel.sh: unknown option '%s'\n" % a)
            usage(2)
        else:
            args.append(a)
    if len(args) != 1 or (full and fast):
        usage(2)

    workdir = os.path.abspath(args[0])
    if not os.path.isdir(os.path.join(workdir, "kernel")):
        sys.stderr.write("run-kernel.sh: '%s' does not look like an xv6 tree "
                         "(no kernel/)\n" % workdir)
        sys.exit(2)

    started = time.time()
    try:
        build_tree(workdir)
        check_given(workdir)
        print()
        part2(workdir, stress)
        print()
        part3(workdir)
        print()
        if fast:
            print("== regression: skipped (--fast) ==")
            bad("Regression: usertests passes",
                "not run: --fast was given. This case cannot pass without it.")
        else:
            usertests(workdir, full)
    finally:
        strays = sweep(verbose=False)
        if strays:
            sys.stderr.write(
                "run-kernel.sh: %d qemu-system-riscv64 process(es) that this "
                "run did not start are alive: %s\n"
                % (len(strays), " ".join(str(p) for p in strays)))

    elapsed = time.time() - started
    print()
    print("== results ==")
    for verdict, name in RESULTS:
        print("  %-6s %s" % (verdict, name))
    print()
    print("%d passed, %d failed" % (COUNTS["pass"], COUNTS["fail"]))
    print("(Parts 4-5 are graded separately by tests/run-xfsck.sh.)")
    if fast:
        print("(--fast skipped the regression check, so it is counted as a "
              "failure. Do not hand in against --fast.)")
    elif not full:
        print("(The regression check ran 'usertests -q'. Run once with --full "
              "before you hand in.)")
    print("run took %d min %02d s" % (int(elapsed) // 60, int(elapsed) % 60))
    return 0 if COUNTS["fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
