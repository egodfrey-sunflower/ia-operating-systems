#!/usr/bin/env python3
"""Lab 2 autograder, Parts 2-5. Invoked through run.sh.

WHAT IT GRADES

  Part 2   the trace() system call, by running user/tracetest.c and reading
           the trace lines the KERNEL printed between the markers that
           program prints. tracetest cannot check itself: the lines it is
           being judged on go straight to the console and no user process can
           read them back.

  Part 3   the sysinfo() system call, by running user/sysinfotest.c, which
           can check itself. Its FAIL lines are sorted into one case per
           group of checks so that a failure names which property broke.

  Part 4   the lottery scheduler, by running user/schedtest.c, which measures
           two spinning children through getpinfo(). The headline case is
           statistical -- see THE STATISTICAL CASE below -- and the one
           beside it, on two children holding one ticket each, is exact.
           Two further cases run schedtest under the OTHER two policies, for
           the one property the specification demands of all three: that
           ticks[] is counted at all.

  Part 5   the MLFQ scheduler, by running user/mlfqtest.c, which watches two
           CPU-bound children sink to the bottom queue and be lifted out of
           it by the periodic boost, and watches its own interactive self --
           which blocks every tick -- stay at the top. The three-way
           comparison Part 5 also asks for is prose (SCHED-RESULTS.md) and is
           marked by hand.

  all      xv6's own usertests, under EVERY policy, as the regression check.
           Almost every way of breaking this kernel still boots and still
           runs a shell; usertests is what notices, and a scheduler is the
           easiest thing in the lab to get subtly, intermittently wrong.

THREE KERNELS, NOT ONE

  The policy is a compile-time flag, so Parts 4 and 5 are not two tests of
  one kernel: they are three kernels. This script builds each in turn --
  rr for Parts 2-3, lottery for Part 4, mlfq for Part 5 -- and always from
  clean, because a CFLAGS change makes nothing look out of date and a
  half-rebuilt tree grades a kernel nobody asked for. Every session also
  asserts the "sched: policy=..." boot line, which comes from the flag, so a
  phase can prove it graded the kernel it meant to.

HOW IT AVOIDS PASSING VACUOUSLY

  Every assertion of the form "no such line appeared" is paired with a
  requirement that the program which should have printed it ran to
  completion. A test program that never started prints no bad lines either,
  and a grader that cannot tell those apart is worse than no grader.

  "Ran to completion" is never read off the program's final `done` line.
  schedtest and mlfqtest print that line on their abort paths as well as on
  their success path, so a run that died at its first getpinfo() prints it
  too. Each part is gated instead on a SUMMARY line that only a completed
  measurement can print -- schedtest's two `share ...` lines, mlfqtest's
  `samples at each level` histogram -- each of them after the loop that
  produces the numbers and unreachable from every abort above it.

THE STATISTICAL CASE

  Part 4's share case is the one case in this lab that cannot be an equality
  check: a lottery is random, and over any finite run the winner's share is a
  binomial sample around its target. The band below is therefore stated in
  the handout, in per mille of the two children's selections, and is set at
  about four and a half standard deviations of that sample -- wide enough
  that a correct kernel does not flake, narrow enough that the two ways of
  being wrong on purpose (ignoring the tickets, which lands on 500, and
  always running the highest-ticket process, which lands on 1000) are each
  many standard deviations outside it. Every failure prints the observed
  share and ratio next to the expected ones.

ONE BOOT PER TEST PROGRAM

  Each test program gets its own emulator. A kernel that panics in sysinfo
  would otherwise take usertests down with it and report two failures where
  there is one bug. A boot costs about four seconds; the isolation is worth
  more than that.

ONE EMULATOR AT A TIME, AND NONE AFTERWARDS

  qemuharness enforces both: a second Xv6Session raises rather than opening,
  and the emulator's process group is killed on close, on exception and at
  interpreter exit. This script additionally sweeps at the end and says so.
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
                             HarnessError, HarnessTimeout, KernelPanic,
                             SchedulerStall)
except ImportError as e:                                # pragma: no cover
    sys.stderr.write("run.py: cannot import the shared QEMU harness "
                     "(labs/common/qemuharness.py): %s\n" % e)
    sys.exit(2)

# The three kernels, in the order they are graded. Parts 2-3 are specified to
# be graded under the stock scheduler; Parts 4 and 5 each own a policy.
RR, LOTTERY, MLFQ = "rr", "lottery", "mlfq"

BOOT_TIMEOUT = 120
TRACETEST_TIMEOUT = 60
SYSINFOTEST_TIMEOUT = 120
SCHEDTEST_TIMEOUT = 240
MLFQTEST_TIMEOUT = 180
# Generous on purpose. These are the deadline after which a wedged kernel is
# declared wedged, not a performance budget: QEMU emulates RISC-V instruction
# by instruction, and the same suite that takes four minutes on one machine
# takes fifteen on a loaded one.
USERTESTS_TIMEOUT_FULL = 3600
USERTESTS_TIMEOUT_QUICK = 1200

TRACE_LINE = re.compile(r"^(\d+): syscall (\S+) -> (-?\d+)\s*$")

# ---- Part 4's numbers, which must agree with kernel/sched.h and with the
# ---- constants at the top of user/schedtest.c.
DEFAULT_TICKETS = 10          # kernel/sched.h
TICKETS_A, TICKETS_B = 30, 10  # user/schedtest.c
SCHEDTEST_TICKS = 400         # the measurement window, in timer ticks
SCHEDTEST_SHORT = 100         # and the window used under the other two
                              # policies, where all that is being asked is
                              # whether the counter moves at all

# The target share, in per mille of the selections the two children received
# between them, and the half-width of the band around it. See THE
# STATISTICAL CASE in the module docstring, and the handout, which states
# both numbers so the student knows what is being asked.
#
# 750 per mille is 30/(30+10). At 400 ticks the sample standard deviation of
# that share is sqrt(.75*.25/400) = 21.7 per mille, so a band of +/-100 is
# +/-4.6 sd: a correct kernel misses it about once in a million runs. The two
# interesting wrong answers are 500 (a scheduler that ignores tickets: 11.5
# sd out) and 1000 (one that always runs the highest-ticket process: 11.5 sd
# out the other way). Tightening the band to catch anything subtler than
# those would start failing correct kernels, which is worse.
SHARE_TARGET = 1000 * TICKETS_A // (TICKETS_A + TICKETS_B)
SHARE_BAND = 100

# The second window schedtest measures: two children holding ONE ticket each.
# Half of this case is not statistical at all, and that is the half it is for:
# the exact assertion that both children were selected at least once --
# which a correct lottery cannot fail and an `acc >= winner` off-by-one
# cannot pass: at 1:1 that off-by-one gives every draw to whichever child
# sits first in the process table, and the other is never selected at all.
# The band on the share is therefore deliberately loose, four standard
# deviations of a 100-tick window, and is only there to catch a policy that
# is grossly unfair without starving anybody outright.
SHARE_EQ_TARGET = 500
SHARE_EQ_BAND = 200


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
    """Name which of the four ways a session can end actually happened.

    The harness distinguishes a panic from a stalled scheduler from a wedge
    from a QEMU that exited, and the difference is the difference between
    looking for a bad pointer and looking for a loop. Collapsing them into
    "did not survive" throws that away.
    """
    if isinstance(exc, SchedulerStall):
        return ("the scheduler chose no process while processes were "
                "runnable, while running %s -- the kernel said so itself "
                "and then idled" % what)
    if isinstance(exc, KernelPanic):
        return "the kernel panicked while running %s" % what
    if isinstance(exc, HarnessTimeout):
        return ("the kernel stopped responding while running %s (a deadline "
                "elapsed; nothing more arrived on the console)" % what)
    return "QEMU exited before %s finished" % what


def reasons_from(out, prog):
    """The 'PROG: FAIL <reason>' lines a test program printed."""
    return re.findall(r"^%s: FAIL (.*?)\s*$" % re.escape(prog), out,
                      re.MULTILINE)


def why_incomplete(reasons, prog):
    """A clause naming what a test program printed just before it stopped.

    schedtest and mlfqtest abort by printing a FAIL reason and exiting, so the
    reason is on the console; saying it here means the "ran to completion"
    case names its own cause -- `getpinfo failed`, `fork failed`, `out of
    memory` -- rather than leaving a student to find it in the transcript.
    """
    if not reasons:
        return ""
    return (" %s printed '%s' and stopped there."
            % (prog, "; ".join(reasons)))


def grade_groups(reasons, groups, evidence_prog):
    """One case per group; a group fails if any reason it owns was printed."""
    for name, owned in groups:
        hit = [r for r in reasons if r in owned]
        check(name, not hit, "; ".join(hit),
              "\n".join("%s: FAIL %s" % (evidence_prog, r) for r in hit))


# ---------------------------------------------------------------------------
# building
# ---------------------------------------------------------------------------

def build_tree(workdir, policy, fatal):
    """Rebuild from clean under `policy`. True if it built.

    ALWAYS from clean. Changing a CFLAG makes nothing look out of date, so an
    incremental build after a policy change links a kernel that is half one
    policy and half the other -- and grades it.

    `fatal` is set for the first (rr) build only: if the tree will not build
    at all there is nothing to grade and saying so once is kinder than
    printing thirty identical failures. A later policy failing to build is a
    real result about that part, so it fails that part's cases instead.
    """
    print("== building %s (POLICY=%s), from clean ==" % (workdir, policy))
    clean(workdir)
    try:
        # The kernel, then the disk image: `make` alone builds only the
        # kernel, and the user programs this grades live in fs.img.
        build(workdir, ["POLICY=%s" % policy], timeout=900)
        build(workdir, ["fs.img", "POLICY=%s" % policy], timeout=900)
    except HarnessError as e:
        sys.stderr.write("%s\n" % e)
        if fatal:
            print()
            print("RESULT: build failure")
            sys.exit(1)
        print("build FAILED under POLICY=%s" % policy)
        return False
    print("build OK")
    return True


def session(workdir, policy, timeout=BOOT_TIMEOUT):
    """A booted session that has proved which kernel it is talking to."""
    return Xv6Session(workdir, cpus=1, timeout=timeout, policy=policy,
                      makeargs=["POLICY=%s" % policy])


def check_werror(workdir):
    """The -Werror rule is part of the spec, so weakening it must not help.

    A tripwire, not a proof: it says the flag is still in the Makefile, not
    that every file was compiled with it.
    """
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


# The four programs every Part 2-5 verdict is scraped out of. They live in
# the student's own tree, which means they are editable, which means that
# without this check the most tempting edit at hour four of a kernel lab --
# deleting the assertion that will not pass -- silently scores the part.
GIVEN = ["user/tracetest.c", "user/sysinfotest.c",
         "user/schedtest.c", "user/mlfqtest.c"]


def check_given(workdir):
    """The graded output comes out of these two programs. If they are not the
    ones we shipped, nothing below means anything."""
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


# ---------------------------------------------------------------------------
# Part 2 -- trace
# ---------------------------------------------------------------------------

def block(out, letter):
    """Lines strictly between 'tracetest: X begin' and 'tracetest: X end'.

    None if either marker is missing -- which is how a case tells "nothing
    was traced" from "the program never got there".
    """
    begin = "tracetest: %s begin" % letter
    end = "tracetest: %s end" % letter
    lines = out.split("\n")
    try:
        i = next(n for n, l in enumerate(lines) if l.strip() == begin)
        j = next(n for n, l in enumerate(lines) if l.strip() == end)
    except StopIteration:
        return None
    if j < i:
        return None
    return [l.strip() for l in lines[i + 1:j]]


def traces(lines):
    return [m for m in (TRACE_LINE.match(l) for l in lines) if m]


def part2(workdir, policy):
    prefix = "Part 2"
    names = ["%s: tracetest runs to completion" % prefix,
             "%s: nothing is traced before trace() is called" % prefix,
             "%s: a traced call prints one line per call" % prefix,
             "%s: the mask selects which calls are traced" % prefix,
             "%s: a failed call traces its return value, not its argument"
             % prefix,
             "%s: the trace mask is inherited across fork" % prefix,
             "%s: one process's mask does not affect another's" % prefix,
             "%s: trace(0) turns tracing off again" % prefix,
             "%s: trace() itself returns 0" % prefix]

    print("== Part 2: trace ==")
    try:
        with session(workdir, policy) as s:
            out = s.run_cmd("tracetest", timeout=TRACETEST_TIMEOUT)
    except HarnessError as e:
        for n in names:
            bad(n, why_died("tracetest", e), str(e))
        return

    if not check(names[0], "tracetest: done" in out,
                 "tracetest did not print its final line -- if the shell said "
                 "'exec tracetest failed', $U/_tracetest is missing from "
                 "UPROGS", out):
        for n in names[1:]:
            bad(n, "tracetest did not run to completion")
        return

    m = re.search(r"^tracetest: start pid (\d+)\s*$", out, re.MULTILINE)
    pid = int(m.group(1)) if m else None
    m = re.search(r"^tracetest: D child pid (\d+)\s*$", out, re.MULTILINE)
    cpid = int(m.group(1)) if m else None
    m = re.search(r"^tracetest: F child pid (\d+)\s*$", out, re.MULTILINE)
    fpid = int(m.group(1)) if m else None

    # A -- trace() has not been called yet, so nothing may be printed.
    a = block(out, "A")
    check(names[1], a is not None and not traces(a),
          "system calls were traced before trace() was ever called",
          "\n".join(a or []))

    # B -- exactly the three getpid calls, each reporting this pid.
    b = block(out, "B")
    tb = traces(b or [])
    want_b = (b is not None and len(tb) == 3 and pid is not None and
              all(int(t.group(1)) == pid and t.group(2) == "getpid" and
                  int(t.group(3)) == pid for t in tb))
    check(names[2], want_b,
          "expected exactly three lines of the form '%s: syscall getpid -> "
          "%s' after trace(1 << SYS_getpid); got %d trace line(s)"
          % (pid, pid, len(tb)), "\n".join(b or []))

    # C -- the mask now names close only, and close(99) fails.
    c = block(out, "C")
    tc = traces(c or [])
    check(names[3],
          c is not None and len(tc) == 1 and
          not any(t.group(2) == "getpid" for t in tc),
          "after trace(1 << SYS_close) the block must contain exactly one "
          "trace line and no getpid line; got %d line(s)" % len(tc),
          "\n".join(c or []))
    check(names[4],
          len(tc) == 1 and tc[0].group(2) == "close" and
          int(tc[0].group(3)) == -1,
          "close(99) fails, so its trace line must end '-> -1'; a line ending "
          "'-> 99' is printing the argument instead of the result",
          "\n".join(c or []))

    # D -- the child inherits the mask; the parent makes no traced call here.
    d = block(out, "D")
    td = traces(d or [])
    check(names[5],
          d is not None and cpid is not None and len(td) == 1 and
          int(td[0].group(1)) == cpid and td[0].group(2) == "getpid" and
          int(td[0].group(3)) == cpid,
          "expected exactly one trace line, from the child (pid %s), naming "
          "getpid; got %d line(s)" % (cpid, len(td)),
          "\n".join(d or []))

    # F -- the mask is per process. The child changed its own mask after the
    # fork; the parent's must be exactly what it was. This is the only block
    # in which two processes hold DIFFERENT masks, so it is the only one that
    # can tell a per-process mask from a single kernel-global one: with one
    # global mask the child's trace(1 << SYS_close) silences the parent's
    # getpid and makes the parent's close print instead.
    f = block(out, "F")
    tf = traces(f or [])
    from_child = [t for t in tf if fpid is not None and
                  int(t.group(1)) == fpid]
    from_parent = [t for t in tf if pid is not None and
                   int(t.group(1)) == pid]
    check(names[6],
          f is not None and fpid is not None and pid is not None and
          len(tf) == 2 and
          len(from_child) == 1 and from_child[0].group(2) == "close" and
          int(from_child[0].group(3)) == -1 and
          len(from_parent) == 1 and from_parent[0].group(2) == "getpid" and
          int(from_parent[0].group(3)) == pid,
          "expected exactly two trace lines here: 'close -> -1' from the "
          "child (pid %s), which set its own mask to SYS_close, and 'getpid "
          "-> %s' from the parent (pid %s), whose mask is still SYS_getpid. "
          "A parent that traced close instead, or that traced nothing, is "
          "sharing one mask with its child rather than owning one; got %d "
          "line(s)" % (fpid, pid, pid, len(tf)),
          "\n".join(f or []))

    # E -- trace(0) clears it.
    e = block(out, "E")
    check(names[7], e is not None and not traces(e),
          "system calls were still traced after trace(0)",
          "\n".join(e or []))

    # trace() is specified to return 0; tracetest says so if it did not.
    check(names[8],
          "tracetest: FAIL trace returned non-zero" not in out,
          "trace() set the mask but did not return 0, which the "
          "specification requires", out)


# ---------------------------------------------------------------------------
# Part 3 -- sysinfo
# ---------------------------------------------------------------------------
#
# sysinfotest prints one 'sysinfotest: FAIL <reason>' line per failed check.
# Each case below owns a set of reasons; it fails if any of its reasons was
# printed. The last case owns the program's overall verdict, so a reason no
# case below claims still fails something.

PART3_GROUPS = [
    ("Part 3: sysinfo fills the struct with plausible numbers", [
        "sysinfo returned non-zero for a valid pointer",
        "freemem is zero",
        "freemem is not a multiple of the page size",
        "nproc is outside 2..NPROC",
    ]),
    ("Part 3: freemem tracks allocation and freeing", [
        "sbrk failed; cannot test freemem",
        "sysinfo failed before the allocation",
        "sysinfo failed after the allocation",
        "freemem went UP after allocating memory",
        "freemem fell by less than the memory just allocated",
        "sysinfo failed after freeing",
        "freemem did not rise again after the memory was freed",
    ]),
    ("Part 3: nproc counts processes exactly", [
        "fork failed",
        "sysinfo failed before forking",
        "sysinfo failed with children running",
        "nproc did not rise by exactly the number of children forked",
        "sysinfo failed after the children exited",
        "nproc did not return to its old value once the children were reaped",
    ]),
    ("Part 3: a user pointer the kernel may not write to is rejected", [
        "an address above the top of the address space was accepted",
        "an unmapped address below MAXVA was accepted",
        "address 0 -- the read-only text page -- was accepted",
    ]),
]

PART3_OVERALL = "Part 3: sysinfotest reports no failures at all"


def part3(workdir, policy):
    names = ["Part 3: sysinfotest runs to completion"] + \
            [n for n, _ in PART3_GROUPS] + [PART3_OVERALL]

    print("== Part 3: sysinfo ==")
    try:
        with session(workdir, policy) as s:
            out = s.run_cmd("sysinfotest", timeout=SYSINFOTEST_TIMEOUT)
    except HarnessError as e:
        # The commonest way to fail this part is to assign through the user's
        # pointer, which panics the kernel rather than returning -1.
        for n in names:
            bad(n, why_died("sysinfotest", e), str(e))
        return

    if not check(names[0], "sysinfotest: done" in out,
                 "sysinfotest did not print its final line", out):
        for n in names[1:]:
            bad(n, "sysinfotest did not run to completion")
        return

    reasons = re.findall(r"^sysinfotest: FAIL (.*?)\s*$", out, re.MULTILINE)

    # sysinfotest only prints that line when the first call succeeded, so its
    # absence is itself a failure -- and a kernel that returns 0 without
    # writing anything would otherwise slip through every check below.
    synth = "sysinfo returned non-zero for a valid pointer"
    if (not re.search(r"^sysinfotest: freemem \d+ nproc \d+\s*$", out,
                      re.MULTILINE)) and synth not in reasons:
        reasons.append(synth)

    # The first group is a precondition for the rest. A sysinfo() that
    # returns -1 for every pointer rejects the three bad addresses too, and
    # would score the trust-boundary case for free -- so that case is only
    # awarded to a kernel that accepts a good pointer as well.
    basic = [r for r in reasons if r in PART3_GROUPS[0][1]]

    for i, (name, owned) in enumerate(PART3_GROUPS):
        hit = [r for r in reasons if r in owned]
        if i > 0 and basic:
            bad(name, "sysinfo does not work for an ordinary valid pointer, "
                      "so this case is not awarded: %s" % "; ".join(basic))
            continue
        check(name, not hit, "; ".join(hit) if hit else "",
              "\n".join("sysinfotest: FAIL " + r for r in hit))

    check(PART3_OVERALL, "sysinfotest: OK" in out,
          ("sysinfotest reported %d failure(s)" % len(reasons)) if reasons
          else "sysinfotest finished without printing 'sysinfotest: OK'",
          "\n".join("sysinfotest: FAIL " + r for r in reasons) or out)


# ---------------------------------------------------------------------------
# Part 4 -- the lottery scheduler
# ---------------------------------------------------------------------------
#
# schedtest prints one 'schedtest: FAIL <reason>' line per failed check, plus
# the three lines the two statistical cases are computed from. As in Part 3,
# each case below owns a set of reasons and the last owns the overall
# verdict, so a reason no case claims still fails something.

P4_COMPLETE = "Part 4: schedtest runs to completion"
P4_GROUPS = [
    ("Part 4: settickets rejects a ticket count outside [1, MAX_TICKETS]", [
        "settickets accepted a ticket count outside [1, MAX_TICKETS]",
        "settickets rejected a valid ticket count",
    ]),
    ("Part 4: getpinfo rejects a pointer the kernel may not write through", [
        "getpinfo accepted a pointer the kernel may not write through",
    ]),
    ("Part 4: a newly forked process starts with the default tickets", [
        "getpinfo does not report a newly forked child",
        "a newly forked child does not hold DEFAULT_TICKETS",
        "a newly forked child does not start at priority 0",
    ]),
]
P4_TICKS = "Part 4: getpinfo reports the scheduler's per-process tick counts"
P4_SHARE = "Part 4: the CPU share the children get matches their tickets"
P4_EQUAL = "Part 4: two processes holding one ticket each are both selected"
P4_OVERALL = "Part 4: schedtest reports no failures at all"

# ticks[] is specified to be counted under EVERY policy, not just under the
# one whose part is about scheduling, because Part 5's write-up compares the
# three. Those two cases are graded from the kernels Parts 2-3 and Part 5
# already build, so they cost one extra boot each and no extra build.
def p4_elsewhere_name(policy):
    return "Part 4: getpinfo counts selections under POLICY=%s too" % policy

# The Part 5 case that reads the SHARE of the schedtest-under-mlfq boot the
# ticks[] case above already pays for. schedtest's two children are identical
# CPU-bound spinners that differ only in a ticket count MLFQ disregards, so a
# correct MLFQ serves them equally: they sink to the same level and are taken
# in turn there. "In turn there" is per LEVEL -- each level keeps its own scan
# cursor -- and that is the one thing this case pins down. An MLFQ that keeps a
# single cursor shared across all levels reaches every graded milestone (it
# demotes, it boosts, mlfqtest scores a clean run) and yet is not round robin
# at all: the higher-priority process that runs almost every tick keeps
# dragging the one cursor back up the table, so the first spinner scanned wins
# over and over and the other is nearly starved. Nothing but the share sees it.
# A fair kernel splits the CPU ~500:500 per mille; the shared-cursor bug lands
# far outside the band (measured ~820:180 both at this window and at 400 ticks).
MLFQ_FAIR = ("Part 5: MLFQ round-robins equal-priority processes fairly "
             "(a scan cursor per level, not one shared across levels)")
# Per mille around 500. MLFQ is deterministic, the two children are symmetric,
# and the reference sits on 500 exactly, so this is a wide guard rail rather
# than a statistical band: it passes anything from 300 to 700 and fails the
# ~820 the shared-cursor bug produces. A band this loose cannot flake on a busy
# machine -- the guest's scheduling is emulated tick by tick and does not
# depend on host load -- and is still 120 per mille clear of the bug.
MLFQ_FAIR_BAND = 200

RESULT_LINE = re.compile(
    r"^schedtest: result (\S+) pid (\d+) tickets (\d+) ticks (\d+)\s*$",
    re.MULTILINE)
NEWBORN_LINE = re.compile(
    r"^schedtest: newborn pid (\d+) tickets (-?\d+) prio (-?\d+)\s*$",
    re.MULTILINE)
# The two post-loop summaries. Each is printed once its measurement window has
# been sampled to the end, and from nowhere else -- so they, and not
# `schedtest: done`, are what "ran to completion" means here.
SHARE_A_LINE = re.compile(r"^schedtest: share A ", re.MULTILINE)
SHARE_C_LINE = re.compile(r"^schedtest: share C ", re.MULTILINE)


def schedtest_waits(dur):
    """Timer ticks a `schedtest dur` waits through, not the ones it measures.

    60 for the newborn check, 10 of warm-up, `dur` measured, 10 more of
    warm-up, then the equal-ticket window of max(dur/4, 40). At the graded
    length that is 580 ticks for a 400-tick measurement -- which is why the
    banner cannot be written as dur/10 seconds. Kept in step with
    user/schedtest.c by hand; it feeds a printed estimate and nothing else.
    """
    return 60 + 10 + dur + 10 + max(dur // 4, 40)


def ratio(x, y):
    """'3.00:1', or '(all of it):1' when the loser got nothing at all."""
    if y <= 0:
        return "(all of it):1" if x > 0 else "0:0"
    return "%.2f:1" % (float(x) / y)


def part4(workdir, policy):
    names = [P4_COMPLETE] + [n for n, _ in P4_GROUPS] + \
            [P4_TICKS, P4_SHARE, P4_EQUAL, P4_OVERALL]

    # This is the line a student watches while nothing happens, so it quotes
    # the ticks schedtest WAITS through, not the ones it measures.
    waits = schedtest_waits(SCHEDTEST_TICKS)
    print("== Part 4: lottery (schedtest measures %d ticks and waits through "
          "%d, so about %d s) ==" % (SCHEDTEST_TICKS, waits, waits // 10))
    try:
        with session(workdir, policy) as s:
            out = s.run_cmd("schedtest %d" % SCHEDTEST_TICKS,
                            timeout=SCHEDTEST_TIMEOUT)
    except HarnessError as e:
        # The commonest way to fail this part is a lottery that never
        # selects anybody, which the kernel's own watchdog reports at boot.
        for n in names:
            bad(n, why_died("schedtest", e), str(e))
        return

    reasons = reasons_from(out, "schedtest")

    # NOT `schedtest: done`: schedtest prints that on all sixteen of its abort
    # paths as well as on its success path, so a run that died at its first
    # getpinfo() prints it, prints none of the FAIL reasons the groups below
    # own, and would be handed this whole part for three lines of output. The
    # two `share` lines are printed after the two measurement loops and are
    # unreachable from every abort. See HOW IT AVOIDS PASSING VACUOUSLY.
    complete = (SHARE_A_LINE.search(out) is not None and
                SHARE_C_LINE.search(out) is not None)
    if not check(names[0], complete,
                 "schedtest did not finish both of its measurement windows.%s "
                 "If the shell said 'exec schedtest failed', $U/_schedtest is "
                 "missing from UPROGS."
                 % why_incomplete(reasons, "schedtest"), out):
        for n in names[1:]:
            bad(n, "schedtest did not run to completion")
        return

    grade_groups(reasons, P4_GROUPS[:1], "schedtest")

    # The newborn line, which two cases below read: it carries the numbers the
    # newborn case is graded on, and its mere presence is the evidence that
    # getpinfo() ever succeeded.
    m = NEWBORN_LINE.search(out)

    # Part 3's rule at run.py's PART3_GROUPS, applied to Part 4, which has the
    # identical hazard: a getpinfo() that returns -1 for EVERY pointer rejects
    # the bad one too, and would score the trust-boundary case for free. So
    # award it only to a kernel that has also been seen to accept a good
    # pointer -- the newborn line is printed only after a getpinfo() returned
    # 0. The completion gate above implies this today; the two guard different
    # things and neither should be left resting on the other.
    if m is None:
        bad(P4_GROUPS[1][0],
            "getpinfo never returned 0 for a valid pointer during this run, "
            "so rejecting an invalid one proves nothing: a getpinfo that "
            "fails for every address would score this case for free")
    else:
        grade_groups(reasons, P4_GROUPS[1:2], "schedtest")

    # The newborn case is graded on the numbers as well as on the FAIL
    # reasons: the reasons come out of the given program, the numbers come
    # out of the kernel, and this case is the only one in the lab that can
    # see whether allocproc() initialised the fields it was told to.
    tickets = int(m.group(2)) if m else None
    prio = int(m.group(3)) if m else None
    hit = [r for r in reasons if r in P4_GROUPS[2][1]]
    check(P4_GROUPS[2][0],
          not hit and tickets == DEFAULT_TICKETS and prio == 0,
          "a child of a bare fork() reports %s ticket(s) at priority %s; it "
          "must be born holding DEFAULT_TICKETS = %d at priority 0. fork() "
          "does not copy the ticket count -- the parent here holds a "
          "thousand -- so this number comes from allocproc(), and a process "
          "born with no tickets can never win a lottery"
          % (tickets, prio, DEFAULT_TICKETS),
          "\n".join(([m.group(0)] if m else []) +
                    ["schedtest: FAIL " + r for r in hit]))

    got = dict((m.group(1), int(m.group(4))) for m in RESULT_LINE.finditer(out))
    a, b = got.get("A"), got.get("B")

    if a is None or b is None:
        for n in (P4_TICKS, P4_SHARE):
            bad(n, "schedtest did not report a result line for each child; "
                   "without both there is nothing to compare", out)
    else:
        total = a + b
        # A whole tick of CPU went somewhere on every one of the window's
        # ticks, and both children were runnable throughout, so between them
        # they should have been selected about SCHEDTEST_TICKS times. A
        # kernel that never increments the counter reports 0 and 0, and a
        # kernel that increments it in only one of the three policy branches
        # reports 0 and 0 here too.
        floor = SCHEDTEST_TICKS // 2
        check(P4_TICKS, total >= floor,
              "the two children were selected %d time(s) between them over "
              "a %d-tick window, which is too few to be a real measurement "
              "(expected roughly %d). If both are 0, the tick counter is "
              "not being incremented on the path that chooses a process "
              "under this policy"
              % (total, SCHEDTEST_TICKS, SCHEDTEST_TICKS),
              "\n".join(l for l in out.split("\n") if "result" in l))

        share = (1000 * a // total) if total > 0 else -1
        lo, hi = SHARE_TARGET - SHARE_BAND, SHARE_TARGET + SHARE_BAND
        check(P4_SHARE, total > 0 and lo <= share <= hi,
              "A holds %d tickets and B holds %d, so A should receive "
              "%d per mille of the CPU the pair gets (%s). Observed %d per "
              "mille (%s): %d selections to %d over %d ticks. The band is "
              "%d to %d per mille (%s to %s), which is about four and a "
              "half standard deviations of a run this length -- a share "
              "outside it is a policy that is not proportional to tickets, "
              "not bad luck. 500 per mille (%s) is a scheduler ignoring the "
              "ticket counts; 1000 is one that always runs the highest-"
              "ticket process."
              % (TICKETS_A, TICKETS_B, SHARE_TARGET,
                 ratio(TICKETS_A, TICKETS_B), share, ratio(a, b), a, b,
                 SCHEDTEST_TICKS, lo, hi,
                 ratio(lo, 1000 - lo), ratio(hi, 1000 - hi), ratio(1, 1)),
              "\n".join(l for l in out.split("\n")
                        if "result" in l or "share" in l or "sample" in l))

    c, d = got.get("C"), got.get("D")
    if c is None or d is None:
        bad(P4_EQUAL, "schedtest did not report a result line for each of the "
                      "two children holding one ticket each", out)
    else:
        total = c + d
        share = (1000 * c // total) if total > 0 else -1
        lo = SHARE_EQ_TARGET - SHARE_EQ_BAND
        hi = SHARE_EQ_TARGET + SHARE_EQ_BAND
        check(P4_EQUAL, c > 0 and d > 0 and lo <= share <= hi,
              "two children holding one ticket each were selected %d and %d "
              "time(s). Both must be selected -- a process holding a ticket "
              "cannot be starved outright by a lottery -- and the share must "
              "be within %d of %d per mille; this run gives %d. A child that "
              "is NEVER selected is the signature of 'acc >= winner' in the "
              "ticket walk: with one ticket each the first process in the "
              "table then wins every draw and the second wins none. At 30:10 "
              "the same bug moves the share by 25 per mille and nothing can "
              "see it."
              % (c, d, SHARE_EQ_BAND, SHARE_EQ_TARGET, share),
              "\n".join(l for l in out.split("\n")
                         if "equal" in l or "result C" in l or
                         "result D" in l or "share C" in l))

    check(P4_OVERALL, not reasons,
          "schedtest reported %d failure(s)" % len(reasons),
          "\n".join("schedtest: FAIL " + r for r in reasons))


def part4_ticks_elsewhere(workdir, policy):
    """ticks[] under a policy whose part is not being graded here.

    A short schedtest, and one assertion: the two children were selected a
    plausible number of times between them. Nothing about proportions -- rr
    and mlfq are not proportional and are not supposed to be. This is the
    case behind the handout's "ticks[i] must be counted under every policy",
    and it is what stops a counter that lives only in the lottery branch from
    scoring full marks and then producing a table of zeroes when the student
    sits down to write SCHED-RESULTS.md.
    """
    name = p4_elsewhere_name(policy)
    # Under mlfq this same boot also grades the per-level-cursor case: it reads
    # the SHARE the two children got, which the ticks[] case above does not.
    extra = [MLFQ_FAIR] if policy == MLFQ else []

    print("== Part 4 under POLICY=%s: does ticks[] move at all? (%d ticks) =="
          % (policy, SCHEDTEST_SHORT))
    try:
        with session(workdir, policy) as s:
            out = s.run_cmd("schedtest %d" % SCHEDTEST_SHORT,
                            timeout=SCHEDTEST_TIMEOUT)
    except HarnessError as e:
        for n in [name] + extra:
            bad(n, why_died("schedtest", e), str(e))
        return

    # The gate is the post-loop summary rather than `schedtest: done`, for the
    # reason given in part4(): every case that reads this boot must fail if the
    # measurement never finished, or it would pass vacuously.
    if SHARE_A_LINE.search(out) is None:
        for n in [name] + extra:
            bad(n, "schedtest did not finish its measurement window under "
                   "POLICY=%s.%s"
                   % (policy, why_incomplete(reasons_from(out, "schedtest"),
                                             "schedtest")), out)
        return

    got = dict((m.group(1), int(m.group(4))) for m in RESULT_LINE.finditer(out))
    a, b = got.get("A"), got.get("B")
    if a is None or b is None:
        for n in [name] + extra:
            bad(n, "schedtest did not report a result line for each child", out)
        return

    floor = SCHEDTEST_SHORT // 2
    check(name, a + b >= floor,
          "the two children were selected %d time(s) between them over a "
          "%d-tick window under POLICY=%s, and %d is the least this could "
          "plausibly be. If it is 0, ticks[] is being counted in one policy "
          "branch of scheduler() and not in this one -- which the "
          "specification forbids, and which leaves two of the three rows of "
          "SCHED-RESULTS.md's table empty"
          % (a + b, SCHEDTEST_SHORT, policy, floor),
          "\n".join(l for l in out.split("\n") if "result" in l))

    if policy == MLFQ:
        total = a + b
        share = (1000 * a // total) if total > 0 else -1
        lo, hi = 500 - MLFQ_FAIR_BAND, 500 + MLFQ_FAIR_BAND
        check(MLFQ_FAIR, total > 0 and lo <= share <= hi,
              "the two children are identical CPU-bound spinners -- their "
              "ticket counts (%d and %d) are a Part 4 concern that MLFQ "
              "disregards -- so a fair MLFQ sinks them to the same level and "
              "takes them in turn there, splitting the CPU near 500 per mille "
              "each. This run gave A %d per mille (%d selections to %d over %d "
              "ticks), outside the %d-to-%d band. A share near 800 with both "
              "children still running is the signature of ONE scan cursor "
              "shared across all levels: the interactive process that runs "
              "almost every tick drags it back up the table, so the first "
              "spinner it scans wins over and over. Round robin within a level "
              "needs a cursor PER level."
              % (TICKETS_A, TICKETS_B, share, a, b, SCHEDTEST_SHORT, lo, hi),
              "\n".join(l for l in out.split("\n") if "result" in l))


# ---------------------------------------------------------------------------
# Part 5 -- MLFQ
# ---------------------------------------------------------------------------

P5_COMPLETE = "Part 5: mlfqtest runs to completion"
P5_GROUPS = [
    ("Part 5: a process that never blocks is demoted to the bottom queue", [
        "a process that never blocks was never demoted to the bottom queue",
    ]),
    # Two reasons, one case. The second is what the second spinner is for:
    # a boost that lifts only the process that happens to be running when it
    # fires keeps lifting whichever spinner is running and starves the other,
    # so with two of them the same case sees it -- either as "one of them
    # never came back up" or as "they were never both up at once".
    ("Part 5: the periodic boost lifts every process out of the bottom queue "
     "again", [
        "once at the bottom the process never came back up",
        "the boost never lifted both children out of the bottom queue at once",
    ]),
    ("Part 5: the allotment at each level is 1, 2 and 4 ticks", [
        "a process sinks to the bottom queue faster than its allotments allow",
    ]),
]
P5_BLOCKER = "Part 5: a process that blocks before its tick is up is not demoted"
P5_NEWBORN = "Part 5: a newly forked process enters at the top level"
P5_OVERALL = "Part 5: mlfqtest reports no failures at all"

# Part 4's contract, but only mlfqtest can see it, so it is graded from
# mlfqtest's output in Part 5's phase (as the POLICY=mlfq case above already
# is). ticks[] counts SCHEDULING DECISIONS, and the one process in the lab for
# which decisions and consumed ticks differ is mlfqtest's own parent: it is
# chosen once a sample and blocks in pause() before its tick is up every time,
# so it never reaches yield(). A counter incremented at selection reports
# roughly one per sample; a counter incremented in yield() reports zero, and
# every other case in the lab reads ticks[] only for processes that spin,
# where the two coincide.
P4_DECISIONS = ("Part 4: ticks[] counts scheduling decisions, not whole "
                "ticks consumed")
P4_DECISIONS_REASON = ("the sampling parent's ticks[] barely moved, so "
                       "ticks[] is not counting scheduling decisions")

MLFQ_NEWBORN_LINE = re.compile(r"^mlfqtest: newborn pid (\d+) prio (-?\d+)\s*$",
                               re.MULTILINE)
# The post-loop summary that stands for "the sampling loop finished".
HISTOGRAM_LINE = re.compile(r"^mlfqtest: samples at each level ",
                            re.MULTILINE)
PARENT_LINE = re.compile(
    r"^mlfqtest: the sampling parent was selected (\d+) time\(s\) over "
    r"(\d+) samples\s*$", re.MULTILINE)


def part5(workdir, policy):
    names = [P5_COMPLETE] + [n for n, _ in P5_GROUPS] + \
            [P5_BLOCKER, P5_NEWBORN, P4_DECISIONS, P5_OVERALL]

    print("== Part 5: MLFQ ==")
    try:
        with session(workdir, policy) as s:
            out = s.run_cmd("mlfqtest", timeout=MLFQTEST_TIMEOUT)
    except HarnessError as e:
        for n in names:
            bad(n, why_died("mlfqtest", e), str(e))
        return

    reasons = reasons_from(out, "mlfqtest")

    # NOT `mlfqtest: done`: mlfqtest prints that on all seven of its abort
    # paths as well as on its success path, so a run that died at its first
    # getpinfo() prints it, prints none of the FAIL reasons the groups below
    # own, and would be handed five of this part's cases for three lines of
    # output. The histogram is printed once the sampling loop has finished
    # and from nowhere else. See HOW IT AVOIDS PASSING VACUOUSLY.
    if not check(names[0], HISTOGRAM_LINE.search(out) is not None,
                 "mlfqtest did not finish its sampling loop.%s If the shell "
                 "said 'exec mlfqtest failed', $U/_mlfqtest is missing from "
                 "UPROGS." % why_incomplete(reasons, "mlfqtest"), out):
        for n in names[1:]:
            bad(n, "mlfqtest did not run to completion")
        return

    # The demotion case is a precondition for the boost case: a process that
    # never reaches the bottom queue cannot demonstrate being lifted out of
    # it, and awarding the boost case to a kernel with no demotion at all
    # would be scoring Rule 5 for the absence of Rule 4.
    sank = P5_GROUPS[0][1][0] not in reasons
    boosted = sank and not [r for r in reasons if r in P5_GROUPS[1][1]]
    for i, (name, owned) in enumerate(P5_GROUPS):
        hit = [r for r in reasons if r in owned]
        if i > 0 and not sank:
            bad(name, "the process never reached the bottom queue, so this "
                      "case is not awarded: there was nothing to be lifted "
                      "out of")
            continue
        # And the allotment case measures how long the descent after a boost
        # takes, which is not a question at all on a kernel where no boost
        # happens: there is one descent and it never repeats.
        if i > 1 and not boosted:
            bad(name, "nothing lifted the process out of the bottom queue, "
                      "so this case is not awarded: with no boost there is "
                      "no repeated descent to measure")
            continue
        check(name, not hit, "; ".join(hit),
              "\n".join(l for l in out.split("\n") if "mlfqtest: " in l))

    # The other half of Rule 4, and the half the handout flags in bold:
    # the allotment is charged where a process GIVES UP the CPU, not where it
    # is chosen. mlfqtest's own parent is the interactive process here -- it
    # blocks in pause() every tick and so is almost never charged -- and a
    # kernel that charges at selection demotes it to the bottom instead.
    # This one does not depend on the children sinking, and mlfqtest makes
    # the check before the two that give up, so it is never awarded by
    # default.
    check(P5_BLOCKER,
          "a process that blocks before its tick is up was demoted anyway"
          not in reasons,
          "a process that gives the CPU up before its tick is over was "
          "demoted anyway. Rule 4 charges the allotment in yield(), which "
          "only a process that used a WHOLE tick reaches; charging it where "
          "a process is selected demotes the processes MLFQ exists to keep "
          "at the top",
          "\n".join(l for l in out.split("\n") if "mlfqtest: " in l))

    # Rule 3, checked on the number as well as on the FAIL reason. The slot
    # this child gets is the one the sunk child just gave back, so a level
    # that allocproc did not reset is visible here and nowhere else.
    m = MLFQ_NEWBORN_LINE.search(out)
    prio = int(m.group(2)) if m else None
    check(P5_NEWBORN,
          prio == 0 and
          "a newly forked process does not start at the top level"
          not in reasons,
          "a process forked into a recycled process-table slot reports level "
          "%s; Rule 3 says a new process enters at the top level, which is "
          "allocproc's job -- the slot it was given had been left at the "
          "bottom" % prio,
          "\n".join(l for l in out.split("\n") if "mlfqtest: " in l))

    # Part 4's "ticks[] counts scheduling decisions" contract, which nothing
    # else in the lab can see: every other process any case reads through
    # ticks[] spins, and for a process that spins a decision and a whole tick
    # consumed are the same event. mlfqtest's parent is the exception.
    m = PARENT_LINE.search(out)
    picked = int(m.group(1)) if m else None
    taken = int(m.group(2)) if m else None
    check(P4_DECISIONS,
          m is not None and picked * 4 >= taken and
          P4_DECISIONS_REASON not in reasons,
          "mlfqtest's parent was chosen at least once for each of its %s "
          "samples -- it wakes, calls getpinfo() and blocks again on every "
          "one of them -- but getpinfo reports it selected only %s time(s). "
          "ticks[] must be incremented where scheduler() sets a process "
          "RUNNING. A kernel that increments it in yield() instead is "
          "counting whole ticks CONSUMED, which is a different quantity: a "
          "process that gives the CPU up early never reaches yield(), and "
          "that is exactly the population MLFQ exists to serve"
          % (taken, picked),
          "\n".join(l for l in out.split("\n") if "mlfqtest: " in l))

    check(P5_OVERALL, "mlfqtest: OK" in out,
          ("mlfqtest reported %d failure(s)" % len(reasons)) if reasons
          else "mlfqtest finished without printing 'mlfqtest: OK'",
          "\n".join(l for l in out.split("\n") if "mlfqtest: " in l) or out)


# ---------------------------------------------------------------------------
# regression -- xv6's own usertests, under every policy
# ---------------------------------------------------------------------------

def usertests(workdir, policy, full):
    name = "Regression: usertests passes under POLICY=%s" % policy
    cmd = "usertests" if full else "usertests -q"
    timeout = USERTESTS_TIMEOUT_FULL if full else USERTESTS_TIMEOUT_QUICK

    print("== regression: %s under POLICY=%s (this is the slow one) =="
          % (cmd, policy))
    try:
        with session(workdir, policy) as s:
            out = s.run_cmd(cmd, timeout=timeout)
    except HarnessTimeout as e:
        # usertests is the one case long enough that a heavily loaded machine
        # can miss the deadline with a perfectly correct kernel. Say so, rather
        # than pointing the student at a kernel that may be fine.
        bad(name,
            "%s did not finish within %d s under POLICY=%s. On a busy or "
            "low-memory machine this is usually the machine, not your kernel: "
            "%s normally finishes in a couple of minutes. Re-run it alone "
            "(close other VMs/builds) before suspecting your scheduler; a real "
            "scheduling bug shows up first in the Part 4/5 cases above, not here."
            % (cmd, timeout, policy, cmd),
            str(e))
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
        "  (default)  Parts 2-5, then 'usertests -q' under each policy\n"
        "  --fast     Parts 2-5 only; skips the regression checks entirely\n"
        "  --full     Parts 2-5, then the whole of 'usertests' under each\n")
    sys.exit(code)


def main(argv):
    full = False
    fast = False
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
        def regression(policy):
            if fast:
                print("== regression under POLICY=%s: skipped (--fast) =="
                      % policy)
                bad("Regression: usertests passes under POLICY=%s" % policy,
                    "not run: --fast was given. This case cannot pass "
                    "without it.")
            else:
                usertests(workdir, policy, full)

        # Phase 1 -- the stock scheduler. Parts 2 and 3 are graded here
        # because they are not about scheduling, and grading them under a
        # scheduler the student has just written would report their bugs as
        # system-call bugs.
        build_tree(workdir, RR, fatal=True)
        check_werror(workdir)
        check_given(workdir)
        print()
        part2(workdir, RR)
        print()
        part3(workdir, RR)
        print()
        part4_ticks_elsewhere(workdir, RR)
        print()
        regression(RR)
        print()

        # Phase 2 -- the lottery kernel.
        if build_tree(workdir, LOTTERY, fatal=False):
            print()
            part4(workdir, LOTTERY)
            print()
            regression(LOTTERY)
        else:
            for n in ([P4_COMPLETE] + [n for n, _ in P4_GROUPS] +
                      [P4_TICKS, P4_SHARE, P4_EQUAL, P4_OVERALL,
                       "Regression: usertests passes under POLICY=%s"
                       % LOTTERY]):
                bad(n, "the tree does not build with POLICY=%s" % LOTTERY)
        print()

        # Phase 3 -- the MLFQ kernel.
        if build_tree(workdir, MLFQ, fatal=False):
            print()
            part5(workdir, MLFQ)
            print()
            part4_ticks_elsewhere(workdir, MLFQ)
            print()
            regression(MLFQ)
        else:
            for n in ([P5_COMPLETE] + [n for n, _ in P5_GROUPS] +
                      [P5_BLOCKER, P5_NEWBORN, P5_OVERALL,
                       p4_elsewhere_name(MLFQ), MLFQ_FAIR,
                       "Regression: usertests passes under POLICY=%s"
                       % MLFQ]):
                bad(n, "the tree does not build with POLICY=%s" % MLFQ)
    finally:
        # verbose=False: sweep() would otherwise print the same warning in
        # different words immediately before this one.
        strays = sweep(verbose=False)
        if strays:
            sys.stderr.write(
                "run.sh: %d qemu-system-riscv64 process(es) that this run did "
                "not start are alive: %s\n"
                % (len(strays), " ".join(str(p) for p in strays)))

    elapsed = time.time() - started
    print()
    print("== results ==")
    for verdict, name in RESULTS:
        print("  %-6s %s" % (verdict, name))
    print()
    print("%d passed, %d failed" % (COUNTS["pass"], COUNTS["fail"]))
    print("(Part 1 and Part 5's SCHED-RESULTS.md are prose, marked against "
          "solutions/README.md, and are not counted here.)")
    if fast:
        print("(--fast skipped the three regression checks, so they are "
              "counted as failures. Do not hand in against --fast.)")
    elif not full:
        print("(The regression checks ran 'usertests -q'. Run once with "
              "--full before you hand in.)")
    print("run took %d min %02d s" % (int(elapsed) // 60, int(elapsed) % 60))
    return 0 if COUNTS["fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
