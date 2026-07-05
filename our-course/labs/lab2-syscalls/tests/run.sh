#!/usr/bin/env bash
#
# run.sh <workdir>
#
# Lab 2 autograder entry point. Builds the xv6 tree at <workdir>, boots it
# under QEMU via the shared harness, and runs the Lab 2 checks. Prints a
# PASS/FAIL table and exits 0 iff every test passed.
#
# Environment:
#   QUICK=1   skip the long `usertests -q` regression run (fast smoke test).
#   CPUS=n    number of QEMU CPUs (default 1, for deterministic sysinfo).
#
# Examples:
#   ./tests/run.sh /path/to/my-lab2-tree
#   QUICK=1 ./tests/run.sh /path/to/my-lab2-tree

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
