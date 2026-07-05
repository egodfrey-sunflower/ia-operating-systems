#!/usr/bin/env bash
#
# run.sh <xv6-workdir> [hostsync-dir]
#
# Lab 5 autograder entry point. Runs all three parts:
#   Parts A+B (xv6):  builds and boots <xv6-workdir> under QEMU and runs
#                     uthread, kalloctest, bcachetest, usertests -q
#                     (via tests/driver.py, CPUS=3 by default).
#   Part C  (host):   compiles and stress-tests the pthreads programs in
#                     [hostsync-dir] (via tests/hostsync.sh).
#
# If [hostsync-dir] is omitted, it defaults to <xv6-workdir>/hostsync when
# that exists. Otherwise Part C cannot run, and because Part C is 30% of the
# rubric that is a FAIL (nonzero exit), not a skip -- pass the directory you
# copied out of the tree (the handout uses ~/lab5-hostsync as the example).
# Set SKIP_PARTC=1 to skip Part C explicitly (loud SKIP, exit 0; NOT a
# submission run).
#
# Test tiers (see labs/README.md "Conventions & test tiers"): the xv6 driver runs the lab
# tier first (uthread, kalloctest, bcachetest -- fast, for the inner loop)
# and the regression tier (usertests -q) LAST. QUICK=1 stops after the lab
# tier and shows the regression row as SKIPPED. Iterate with QUICK=1; a
# submission counts only with the full run.
#
# Environment (passed through to the xv6 driver):
#   CPUS=n    number of QEMU CPUs (default 3).
#   QUICK=1   skip the usertests -q regression tier (row shown as SKIPPED).
#   USERTESTS_TIMEOUT=secs  regression-tier deadline (default 900).
#   NOXV6=1   skip Parts A+B (host-only quick check of Part C).
#   SKIP_PARTC=1  explicitly skip Part C (otherwise a missing hostsync dir
#             is a FAIL -- Part C is graded).
#
# Prints a combined PASS/FAIL summary and exits 0 iff every part passed.
#
# NOTE ON CLEANUP: this script NEVER does a global `pkill`/`killall` of qemu --
# other users/agents may have unrelated qemu sessions on the same machine. The
# Python harness tears down its OWN qemu process group on exit; we only take a
# passive before/after snapshot of qemu pids and, if (and only if) a qemu that
# appeared *during our run* has leaked, we signal that specific process group.

set -uo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "usage: $0 <xv6-workdir> [hostsync-dir]" >&2
  exit 2
fi

WORKDIR="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NOXV6="${NOXV6:-}"

# Resolve the Part C directory.
if [ "$#" -eq 2 ]; then
  HOSTSYNC="$2"
elif [ -d "$WORKDIR/hostsync" ]; then
  HOSTSYNC="$WORKDIR/hostsync"
else
  HOSTSYNC=""
fi

if [ "$NOXV6" != "" ]; then
  echo "NOXV6 set: skipping xv6 parts A+B"
fi

xv6_rc=0
c_rc=0

# ---- helper: passive snapshot of this user's qemu pids --------------------
# NB: the process name shows up in `comm` truncated to 15 chars
# ("qemu-system-ris"), so match on that substring, not the full argv name.
qemu_pids() { pgrep -u "$USER" qemu-system-ri 2>/dev/null | sort -u; }

if [ "$NOXV6" = "" ]; then
  if [ ! -d "$WORKDIR" ] || [ ! -f "$WORKDIR/Makefile" ]; then
    echo "error: '$WORKDIR' does not look like an xv6 tree (no Makefile)" >&2
    exit 2
  fi

  before_pids="$(qemu_pids)"

  echo "############################################################"
  echo "# Parts A+B: xv6 under QEMU"
  echo "############################################################"
  python3 "$SCRIPT_DIR/driver.py" "$WORKDIR"
  xv6_rc=$?

  # Passive leak check. The harness tears down its own qemu on exit, so
  # normally there is nothing left. We compare before/after snapshots and, for
  # any NEW qemu, we only touch it if it is provably OURS: the harness launches
  # qemu with cwd == the xv6 workdir, so a new qemu whose /proc/<pid>/cwd is
  # our WORKDIR is ours to clean up. Anything else (another user's/agent's
  # qemu that happened to start during our run) is left strictly alone.
  after_pids="$(qemu_pids)"
  wd_abs="$(cd "$WORKDIR" && pwd)"
  leaked=""
  for pid in $(comm -13 <(printf '%s\n' "$before_pids") \
                        <(printf '%s\n' "$after_pids") 2>/dev/null); do
    cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null || true)"
    [ "$cwd" = "$wd_abs" ] && leaked="$leaked $pid"
  done
  if [ -n "$leaked" ]; then
    echo "WARNING: our qemu (workdir $wd_abs) did not exit; cleaning up only ours:"
    for pid in $leaked; do
      pgid="$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ')"
      if [ -n "$pgid" ]; then
        echo "  kill -TERM -- -$pgid  (our qemu leak, pid $pid)"
        kill -TERM -- "-$pgid" 2>/dev/null || true
      fi
    done
  else
    echo "qemu cleanup: OK (no leftover qemu of ours from this run)"
  fi
fi

# ---- Part C ---------------------------------------------------------------
echo
echo "############################################################"
echo "# Part C: host pthreads"
echo "############################################################"
if [ "${SKIP_PARTC:-}" = "1" ]; then
  echo "Part C: SKIPPED -- SKIP_PARTC=1 set."
  echo "        *** Part C (30% of the rubric) was NOT tested. ***"
  echo "        *** This is NOT a submission run. ***"
  c_rc=0
  c_skipped=1
elif [ -z "$HOSTSYNC" ]; then
  echo "Part C: FAIL -- no hostsync dir given and none at <workdir>/hostsync." >&2
  echo "        Part C is 30% of the rubric; pass your hostsync copy" >&2
  echo "        (e.g. tests/run.sh <workdir> ~/lab5-hostsync), or set" >&2
  echo "        SKIP_PARTC=1 to skip it explicitly (not a submission run)." >&2
  c_rc=1
elif [ ! -d "$HOSTSYNC" ]; then
  echo "error: hostsync dir '$HOSTSYNC' does not exist" >&2
  c_rc=1
else
  bash "$SCRIPT_DIR/hostsync.sh" "$HOSTSYNC"
  c_rc=$?
fi

# ---- combined verdict -----------------------------------------------------
echo
echo "############################################################"
echo "# Lab 5 overall"
echo "############################################################"
if [ "$NOXV6" = "" ]; then
  echo "  Parts A+B (xv6):   $( [ "$xv6_rc" -eq 0 ] && echo PASS || echo FAIL )"
else
  echo "  Parts A+B (xv6):   SKIPPED (NOXV6)"
fi
if [ -n "${c_skipped:-}" ]; then
  echo "  Part C  (host):    SKIPPED (SKIP_PARTC=1 -- not a submission run)"
else
  echo "  Part C  (host):    $( [ "$c_rc" -eq 0 ] && echo PASS || echo FAIL )"
fi

if [ "$xv6_rc" -ne 0 ] || [ "$c_rc" -ne 0 ]; then
  exit 1
fi
exit 0
