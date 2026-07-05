#!/usr/bin/env bash
#
# apply.sh <workdir>
#
# Apply the reference Lab 3 solution to a workdir that was created by
# starter/setup.sh. It patches the five kernel files that implement the
# scheduling instrumentation (Task 0) and the PRIO / LOTTERY / MLFQ policies
# (Tasks 1-3): defs.h, proc.h, proc.c, trap.c, sysproc.c. It does NOT rebuild
# -- run `make` (optionally `make SCHED=...`) yourself afterwards.
#
# SPOILER: this is the full reference solution. Do the lab first.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <workdir>" >&2
  exit 2
fi

WORKDIR="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH="$SCRIPT_DIR/solution.patch"

if [ ! -d "$WORKDIR" ]; then
  echo "error: workdir '$WORKDIR' does not exist" >&2
  exit 2
fi
if [ ! -f "$PATCH" ]; then
  echo "error: solution.patch not found at $PATCH" >&2
  exit 1
fi

# Dry-run first so we fail cleanly if the tree is not a pristine starter tree.
if ! patch -p1 --dry-run -d "$WORKDIR" < "$PATCH" >/dev/null 2>&1; then
  echo "error: solution.patch does not apply cleanly to '$WORKDIR'." >&2
  echo "       Is this a fresh tree from starter/setup.sh with no local edits?" >&2
  exit 1
fi

patch -p1 -d "$WORKDIR" < "$PATCH"
echo "solution applied to $WORKDIR."
echo "Now build a policy, e.g.:  cd $WORKDIR && make clean && make qemu SCHED=MLFQ"
