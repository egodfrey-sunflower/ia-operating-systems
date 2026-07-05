#!/usr/bin/env bash
#
# run.sh <workdir>
#
# Lab 3 (scheduling) autograder entry point. Builds the xv6 tree at <workdir>
# once per scheduling policy (make clean between; SCHED=PRIO/LOTTERY/MLFQ/RR),
# boots each under QEMU via the shared harness, and runs the self-measuring
# scheduler test programs plus a usertests -q regression. Prints a PASS/FAIL
# table and exits 0 iff every test passed.
#
# Scheduling tests are noisy and this may be a shared, loaded machine: the
# driver retries any borderline verdict once, uses generous timeouts, and
# NEVER pkills/killalls qemu (the harness tears down only its own process
# group; the leftover-qemu check is a passive comparison of our own sessions).
#
# Environment:
#   QUICK=1   skip the long `usertests -q` regression runs (fast smoke test).
#
# Examples:
#   ./tests/run.sh /path/to/my-lab3-tree
#   QUICK=1 ./tests/run.sh /path/to/my-lab3-tree
#
# Iteration tip: while developing, use the QUICK=1 tier (or boot your tree and
# run priotest/lotto/mlfqtest by hand). To check one specific regression risk
# without the full run, xv6's usertests accepts a named subtest inside the xv6
# shell, e.g.:
#   usertests preempt       # preemption/yield behaviour
#   usertests forkfork      # heavy fork paths through your scheduler
#   usertests reparent      # exit/reparent under your kexit changes
#   usertests exitwait      # exit/wait interlock
# Save the full `usertests -q` (several minutes per SCHED value) for final
# acceptance; this script runs it under SCHED=RR and SCHED=MLFQ.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <workdir>" >&2
  exit 2
fi

WORKDIR="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -d "$WORKDIR" ]; then
  echo "error: workdir '$WORKDIR' does not exist" >&2
  exit 2
fi
if [ ! -f "$WORKDIR/Makefile" ]; then
  echo "error: '$WORKDIR' does not look like an xv6 tree (no Makefile)" >&2
  exit 2
fi

# -u: unbuffered, so progress is visible live even when piped to a file.
exec python3 -u "$SCRIPT_DIR/driver.py" "$WORKDIR"
