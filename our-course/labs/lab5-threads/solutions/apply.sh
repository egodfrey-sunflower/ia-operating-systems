#!/usr/bin/env bash
#
# apply.sh <workdir>
#
# Apply the reference Lab 5 solution (xv6 Parts A and B) to a workdir created
# by starter/setup.sh. This patches the Part A user files (uthread.c,
# uthread_switch.S) and the Part B kernel files (kalloc.c, bio.c, buf.h). It
# does NOT rebuild -- run `make` (or the autograder) yourself afterwards.
#
# Part C (host pthreads) is separate: its reference code is in
# solutions/hostsync/ and builds with plain `make` on your Linux box.
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
echo "solution applied to $WORKDIR. Now: cd $WORKDIR && make qemu"
