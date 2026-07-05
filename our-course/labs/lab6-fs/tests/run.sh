#!/usr/bin/env bash
#
# run.sh <workdir>
#
# Lab 6 autograder entry point. Drives the xv6 tree at <workdir> through the
# functional file-system tests (symlinktest, bigfile), the usertests -q
# regression, and the Task 3 crash-consistency experiment (both crashpoints).
# Prints a PASS/FAIL table and exits 0 iff every test passed.
#
# This script builds the tree itself in several configurations (including
# `make CRASH=...` for the crash experiment), so pass it a fresh tree created
# by starter/setup.sh.
#
# Test tiers (see labs/README.md "Conventions & test tiers"): the lab tier (symlinktest,
# bigfile, both crash cases) runs first; the regression tier (usertests -q)
# runs LAST. QUICK=1 skips the two slow tests -- bigfile and usertests --
# showing them as SKIP rows; a submission counts only with the full run.
#
# Environment:
#   QUICK=1   skip the slow tests (bigfile, usertests -q); rows show as SKIP.
#   CPUS=n    QEMU CPUs for the functional sessions (default 1).
#
# Examples:
#   ./tests/run.sh /path/to/my-lab6-tree
#   QUICK=1 ./tests/run.sh /path/to/my-lab6-tree
#
# NOTE (shared machines): this harness never uses pkill/killall. Every QEMU is
# torn down via the Python harness's own process-group close(); the final
# "no leftover qemu" check is a passive pgrep over the process groups this run
# created, and it never signals anything.

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
