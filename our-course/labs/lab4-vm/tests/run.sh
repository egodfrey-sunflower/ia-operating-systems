#!/usr/bin/env bash
#
# run.sh <workdir>
#
# Lab 4 autograder entry point. Builds the xv6 tree at <workdir>, boots it
# under QEMU via the shared harness, and runs the Lab 4 checks (vmprint,
# ugetpid, cowtest, forkbench, and the usertests -q regression). Prints a
# PASS/FAIL table and exits 0 iff every test passed.
#
# This script and its driver NEVER kill qemu/make globally: each QEMU session
# is torn down by the harness's own process-group close(), and the leftover
# check only inspects (never signals) the sessions this run started -- safe on
# a shared machine with other qemu instances running.
#
# Test tiers (see labs/README.md "Conventions & test tiers"): build+boot (smoke), then the
# lab tier (vmprint/ugetpid/cowtest/forkbench, <~1 min), then the regression
# tier (`usertests -q`) LAST. QUICK=1 stops after the lab tier and the
# regression row shows as SKIPPED in the table (never silently absent).
# Iterate with QUICK=1; a submission counts only with the full run.
#
# Environment:
#   QUICK=1   run only the lab tier; regression tier marked SKIPPED.
#   CPUS=n    number of QEMU CPUs (default 1, for a deterministic regression;
#             set CPUS=3 to also stress copy-on-write locking concurrently).
#
# Examples:
#   ./tests/run.sh /path/to/my-lab4-tree
#   QUICK=1 ./tests/run.sh /path/to/my-lab4-tree

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

exec python3 "$SCRIPT_DIR/driver.py" "$WORKDIR"
