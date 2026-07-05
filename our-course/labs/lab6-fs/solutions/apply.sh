#!/usr/bin/env bash
#
# apply.sh <workdir>
#
# Apply the reference Lab 6 solution to a workdir created by
# starter/setup.sh. This patches the five files that implement Task 1 (large
# files) and Task 2 (symbolic links):
#   kernel/fs.h  kernel/file.h  kernel/fs.c  kernel/sysfile.c  kernel/syscall.c
# It does NOT touch the Task 3 crash machinery, which already ships complete
# in the starter. It does NOT rebuild -- run `make` yourself afterwards.
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
