#!/usr/bin/env python3
"""Lab 4 autograder. Invoked through run.sh.

WHAT IT GRADES

  Part 1   vmprint(). Boots the kernel and checks the page table it printed for
           the first user process against the exact three-level shape /init
           produces -- indices, flag bits and indentation, ignoring the
           physical addresses (which move with the kernel's size). This catches
           a walk that is off by a level, that mislabels a leaf, or that prints
           the wrong permissions. The PROSE deliverable (PGTBL-NOTES.md,
           annotating what each level is) is still yours to self-mark against
           solutions/README.md.

  Part 2   the read-only per-process page, via user/pgtbltest.c.
  Part 3   lazy heap allocation, via user/lazytests.c.
  Part 4   copy-on-write fork, via user/cowtest.c -- including the free-page
           leak check, which is the one test in the lab that sees a reference
           count that never decrements.

  all      xv6's own usertests, the regression gate. A VM bug that leaves the
           common path working while corrupting an edge case is exactly what
           usertests catches and the part tests do not.

HOW IT AVOIDS PASSING VACUOUSLY

  Each test program prints one "<prog>: FAIL <reason>" line per failed check,
  prints "<prog>: OK" only if every check passed, and prints "<prog>: done" as
  the very last thing it does. "done" is the completion marker: it is printed
  after the last check and from no abort path, so a kernel that panics or kills
  the program part way through never prints it. The grader therefore treats a
  missing "done" as "did not run to completion" and fails every case in that
  part -- a crashed program cannot score anything. Only when "done" is present
  are individual cases judged, by whether the reason that owns them appeared.

  This is the discipline Lab 2 learned the hard way: never gate on a token a
  broken run can also print. "OK" is the all-passed token; "done" is the
  ran-to-completion token; the FAIL reasons attribute the damage.

  Part 2's read-only case is only awarded if the page was proved mapped and
  readable first -- by reading the USYSCALL page DIRECTLY at its fixed address,
  not through the student's ugetpid(). An unmapped page faults on a write too,
  so "the store faulted" means nothing unless the page is known present. Whether
  ugetpid() reads the page without a system call cannot be seen from user space;
  that property is hand-marked, and pgtbltest checks the page, not the wrapper.

ONE BOOT PER TEST PROGRAM, AND NONE AFTERWARDS

  Each program gets its own emulator, so a panic in one does not take the
  others down and report several failures for one bug. qemuharness enforces one
  emulator at a time and tears it down on close, on exception and at exit; this
  script also sweeps at the end and says so.
"""

import hashlib
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "..", "common")))

try:
    from qemuharness import (Xv6Session, build, clean, sweep,
                             HarnessError, HarnessTimeout, KernelPanic)
except ImportError as e:                                # pragma: no cover
    sys.stderr.write("run.py: cannot import the shared QEMU harness "
                     "(labs/common/qemuharness.py): %s\n" % e)
    sys.exit(2)

BOOT_TIMEOUT = 120
PGTBLTEST_TIMEOUT = 90
LAZYTESTS_TIMEOUT = 150
COWTEST_TIMEOUT = 300
USERTESTS_TIMEOUT_QUICK = 1200
USERTESTS_TIMEOUT_FULL = 3600


# ---------------------------------------------------------------------------
# reporting
# ---------------------------------------------------------------------------

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
        for line in evidence.rstrip("\n").split("\n")[:20]:
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


# ---------------------------------------------------------------------------
# building
# ---------------------------------------------------------------------------

def build_tree(workdir):
    print("== building %s (from clean) ==" % workdir)
    clean(workdir)
    try:
        build(workdir, [], timeout=900)
        build(workdir, ["fs.img"], timeout=900)
    except HarnessError as e:
        sys.stderr.write("%s\n" % e)
        print()
        print("RESULT: build failure")
        sys.exit(1)
    print("build OK")


# The programs every Part 2-4 verdict is scraped out of live in the student's
# own editable tree. If they are not the ones we shipped, nothing below means
# anything -- the most tempting edit at hour four of a kernel lab is deleting
# the assertion that will not pass.
GIVEN = ["user/pgtbltest.c", "user/lazytests.c", "user/cowtest.c"]


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
              "the grader reads its output line by line; restore it with "
              "setup.sh or by copying it back." % rel)


def check_werror(workdir):
    name = "Build: the kernel is still built with -Werror"
    path = os.path.join(workdir, "Makefile")
    try:
        with open(path, "r", errors="replace") as f:
            text = f.read()
    except OSError as e:
        bad(name, "cannot read %s: %s" % (path, e))
        return
    check(name, "-Werror" in text,
          "-Werror is gone from the Makefile's CFLAGS; the lab is graded "
          "with it on")


# ---------------------------------------------------------------------------
# a booted session
# ---------------------------------------------------------------------------

def session(workdir, timeout=BOOT_TIMEOUT):
    return Xv6Session(workdir, cpus=1, timeout=timeout)


# ---------------------------------------------------------------------------
# Part 1 -- vmprint
# ---------------------------------------------------------------------------
#
# The exact tree /init produces, as (indent-depth, index, flags), with the
# physical addresses left out because they move with the kernel's size. Depth 1
# is a top-level (root) entry, 2 the middle level, 3 a leaf. An interior node
# has flag string "-----"; a leaf names its permissions. The guard page at leaf
# 2 (mapped, no u) and the read-only text at leaf 0 are the tell-tales an
# off-by-one walk gets wrong.
#
# The USYSCALL leaf (depth 3, index 507) is DELIBERATELY not in this list. It is
# mapped by Part 2, not Part 1, so a student who has done Part 1 correctly but
# not yet Part 2 prints a tree without it -- and Part 1, which grades only the
# walk it is responsible for, must not fail them for a leaf that is Part 2's job.
# part1() therefore ignores whatever appears (or does not) at (3, 507): a walk
# bug is caught by the other ten entries, none of which it can selectively drop.
P1_EXPECTED = [
    (1, 0,   "-----"),
    (2, 0,   "-----"),
    (3, 0,   "r-xu-"),
    (3, 1,   "rw-u-"),
    (3, 2,   "rw---"),
    (3, 3,   "rw-u-"),
    (1, 255, "-----"),
    (2, 511, "-----"),
    (3, 510, "rw---"),
    (3, 511, "r-x--"),
]

# The USYSCALL leaf: Part 2's page, ignored by Part 1's shape check (see above).
P1_USYSCALL_LEAF = (3, 507)

# A vmprint line: some number of " .." groups, then "IDX: pte PA pa PA FLAGS".
P1_LINE = re.compile(r"^((?: \.\.)+)(\d+): pte \S+ pa \S+ ([rwxuc-]{5})\s*$")


def parse_vmprint(boot):
    """Pull the (depth, index, flags) tuples out of the boot console."""
    lines = boot.split("\n")
    tree = []
    seen_header = False
    for l in lines:
        if l.startswith("page table "):
            seen_header = True
            continue
        m = P1_LINE.match(l.rstrip("\r"))
        if m:
            depth = m.group(1).count(" ..")
            tree.append((depth, int(m.group(2)), m.group(3)))
        elif seen_header and tree and not l.strip().startswith(".."):
            # first non-tree line after the tree ends the block
            break
    return seen_header, tree


def part1(workdir):
    name = "Part 1: vmprint prints /init's page table with the right shape"
    print("== Part 1: vmprint (read from the boot console) ==")
    try:
        with session(workdir) as s:
            s.wait_for_boot()
            boot = s.output
    except HarnessError as e:
        bad(name, why_died("boot", e), str(e))
        return

    seen, tree = parse_vmprint(boot)
    if not seen:
        bad(name, "no 'page table 0x...' line appeared at boot: vmprint() was "
                  "not called for the first process, or printed nothing",
            "\n".join(boot.split("\n")[-20:]))
        return
    # The USYSCALL leaf belongs to Part 2; ignore it whether present or not, so
    # a correct Part 1 done before Part 2 is not failed for a missing leaf.
    core = [t for t in tree if (t[0], t[1]) != P1_USYSCALL_LEAF]
    if core == P1_EXPECTED:
        ok(name)
        return
    # Give a legible diff.
    exp = "\n".join("  depth %d  idx %-3d  %s" % t for t in P1_EXPECTED)
    got = "\n".join("  depth %d  idx %-3d  %s" % t for t in core) or "  (no PTE lines parsed)"
    bad(name,
        "the printed tree does not match /init's known shape. A depth that is "
        "off by one is an indentation bug; a missing or extra leaf is a walk "
        "that recurses into leaves or stops early; wrong flags are a "
        "misdecoded PTE. (Physical addresses are not checked -- they vary with "
        "your kernel's size.)",
        "expected:\n%s\n--- got ---\n%s" % (exp, got))


# ---------------------------------------------------------------------------
# the test-program parts
# ---------------------------------------------------------------------------

def reasons_from(out, prog):
    return re.findall(r"^%s: FAIL (.*?)\s*$" % re.escape(prog), out,
                      re.MULTILINE)


def grade_program(workdir, prog, timeout, cases, gates=()):
    """Run `prog` and grade it.

    cases: list of (case_name, [substrings]); the case fails if any of its
           substrings appears in a FAIL reason.
    gates: list of (dependent_case_name, required_case_name); the dependent is
           awarded only if the required one passed -- for a check that proves
           nothing unless a precondition held (Part 2's read-only case needs the
           page to be readable first).
    """
    complete_name = "%s: runs to completion" % prog
    overall_name = "%s: reports no failures" % prog
    all_names = [complete_name] + [c for c, _ in cases] + [overall_name]

    print("== %s ==" % prog)
    try:
        with session(workdir) as s:
            out = s.run_cmd(prog, timeout=timeout)
    except HarnessError as e:
        for n in all_names:
            bad(n, why_died(prog, e), str(e))
        return

    done = ("%s: done" % prog) in out
    if not check(complete_name, done,
                 "%s did not print its completion marker '%s: done'. If the "
                 "shell said 'exec %s failed', $U/_%s is missing from UPROGS; "
                 "otherwise the kernel killed or wedged the program before it "
                 "finished." % (prog, prog, prog, prog), out):
        for n in all_names[1:]:
            bad(n, "%s did not run to completion" % prog)
        return

    reasons = reasons_from(out, prog)
    passed = {}
    for cname, subs in cases:
        hit = [r for r in reasons if any(s in r for s in subs)]
        passed[cname] = check(cname, not hit, "; ".join(hit),
                              "\n".join("%s: FAIL %s" % (prog, r) for r in hit))

    for dep, req in gates:
        if not passed.get(req, False):
            # Re-record the dependent case as not-awarded (it may have been
            # recorded PASS above because no failing reason was printed).
            for i, (v, n) in enumerate(RESULTS):
                if n == dep:
                    if v == "PASS":
                        COUNTS["pass"] -= 1
                        COUNTS["fail"] += 1
                    RESULTS[i] = ("FAIL", n)
                    break
            sys.stderr.write(
                "  [%s] not awarded: '%s' did not pass, so this check proves "
                "nothing (an unmapped page faults on a write just as a "
                "read-only one does)\n" % (dep, req))

    check(overall_name, ("%s: OK" % prog) in out,
          ("%s reported %d failure(s)" % (prog, len(reasons))) if reasons
          else "%s finished without printing '%s: OK'" % (prog, prog),
          "\n".join("%s: FAIL %s" % (prog, r) for r in reasons) or out)


# pgtbltest reads the USYSCALL page DIRECTLY (casting the fixed address), so
# case 1 proves the kernel's mapping without going through the student's
# ugetpid(). ugetpid() is a separate case checked only for the value it returns:
# whether it avoids the system call cannot be seen from user space and is
# hand-marked against the source (see solutions/README.md). The read-only case
# is gated on the DIRECT read, because a store to an unmapped page faults just
# as a store to a read-only one does -- "the store faulted" proves read-only
# only once the page is known mapped and readable.
PART2_CASES = [
    ("Part 2: the USYSCALL page is mapped and holds this process's pid",
     ["USYSCALL page holds"]),
    ("Part 2: ugetpid() returns the pid (syscall-avoidance is hand-marked)",
     ["ugetpid returned"]),
    ("Part 2: the pid page is per-process",
     ["the pid page is not per-process"]),
    ("Part 2: the pid page is read-only",
     ["the pid page is writable"]),
]
PART2_GATES = [
    ("Part 2: the pid page is read-only",
     "Part 2: the USYSCALL page is mapped and holds this process's pid"),
]

PART3_CASES = [
    ("Part 3: sbrk grows the heap lazily (allocates on the fault, not the call)",
     ["consumed", "before any were touched"]),
    ("Part 3: a freshly faulted-in page is zeroed",
     ["was not zeroed"]),
    ("Part 3: a faulted-in page reads back what was written",
     ["did not read back"]),
    ("Part 3: a lazy page passed to a system call is faulted in",
     ["from an untouched lazy page"]),
    ("Part 3: an access outside the allocated region stays fatal",
     ["far outside the heap was satisfied"]),
]

PART4_CASES = [
    ("Part 4: a COW write is private and not lost",
     ["child's COW read or write failed", "changed the PARENT's pages"]),
    ("Part 4: three-way sharing keeps the reference count right",
     ["shared by three processes", "after three-way COW"]),
    ("Part 4: fork of a near-full process succeeds (pages are shared, not copied)",
     ["near-full address space failed", "child of bigfork"]),
    ("Part 4: the free-page count returns to baseline (no leak, no double-free)",
     ["free pages before="]),
]


# ---------------------------------------------------------------------------
# regression -- usertests
# ---------------------------------------------------------------------------

def usertests(workdir, full):
    name = "Regression: usertests passes"
    cmd = "usertests" if full else "usertests -q"
    timeout = USERTESTS_TIMEOUT_FULL if full else USERTESTS_TIMEOUT_QUICK
    print("== regression: %s (this is the slow one) ==" % cmd)
    try:
        with session(workdir) as s:
            out = s.run_cmd(cmd, timeout=timeout)
    except HarnessTimeout as e:
        bad(name,
            "%s did not finish within %d s. On a busy or low-memory machine "
            "this is usually the machine, not your kernel: %s normally "
            "finishes in a couple of minutes. Re-run it alone before "
            "suspecting your VM code; a real bug shows up first in the part "
            "tests above." % (cmd, timeout, cmd), str(e))
        return
    except HarnessError as e:
        bad(name, why_died(cmd, e), str(e))
        return
    check(name, "ALL TESTS PASSED" in out,
          "%s did not print ALL TESTS PASSED" % cmd,
          "\n".join(out.rstrip("\n").split("\n")[-25:]))


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def usage(code):
    sys.stderr.write(
        "usage: run.sh [--fast | --full] <workdir>\n"
        "  (default)  Parts 1-4, then 'usertests -q'\n"
        "  --fast     Parts 1-4 only; skips the usertests regression\n"
        "  --full     Parts 1-4, then the whole of 'usertests'\n")
    sys.exit(code)


def main(argv):
    full = fast = False
    args = []
    for a in argv[1:]:
        if a == "--full":
            full = True
        elif a == "--fast":
            fast = True
        elif a in ("-h", "--help"):
            usage(0)
        elif a.startswith("-"):
            sys.stderr.write("run.sh: unknown option '%s'\n" % a)
            usage(2)
        else:
            args.append(a)
    if len(args) != 1 or (full and fast):
        usage(2)

    workdir = os.path.abspath(args[0])
    if not os.path.isdir(os.path.join(workdir, "kernel")):
        sys.stderr.write("run.sh: '%s' does not look like an xv6 tree "
                         "(no kernel/)\n" % workdir)
        sys.exit(2)

    started = time.time()
    try:
        build_tree(workdir)
        check_werror(workdir)
        check_given(workdir)
        print()
        part1(workdir)
        print()
        grade_program(workdir, "pgtbltest", PGTBLTEST_TIMEOUT,
                      PART2_CASES, PART2_GATES)
        print()
        grade_program(workdir, "lazytests", LAZYTESTS_TIMEOUT, PART3_CASES)
        print()
        grade_program(workdir, "cowtest", COWTEST_TIMEOUT, PART4_CASES)
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
                "run.sh: %d qemu-system-riscv64 process(es) this run did not "
                "start are alive: %s\n"
                % (len(strays), " ".join(str(p) for p in strays)))

    elapsed = time.time() - started
    print()
    print("== results ==")
    for verdict, name in RESULTS:
        print("  %-6s %s" % (verdict, name))
    print()
    print("%d passed, %d failed" % (COUNTS["pass"], COUNTS["fail"]))
    print("(Part 1's PGTBL-NOTES.md prose is marked by hand against "
          "solutions/README.md; the shape of its tree is checked above.)")
    if fast:
        print("(--fast skipped usertests, counted as a failure. Do not hand in "
              "against --fast.)")
    elif not full:
        print("(The regression ran 'usertests -q'. Run once with --full before "
              "you hand in.)")
    print("run took %d min %02d s" % (int(elapsed) // 60, int(elapsed) % 60))
    return 0 if COUNTS["fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
