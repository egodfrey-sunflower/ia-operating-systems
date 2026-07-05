#!/usr/bin/env bash
#
# setup.sh <destdir>
#
# Create a fresh Lab 3 working tree at <destdir>: copy the pristine vendored
# xv6-riscv, overlay this lab's starter files, register the lab's user programs
# in the Makefile's UPROGS list, and wire up the compile-time scheduler-select
# knob (make SCHED=RR|PRIO|LOTTERY|MLFQ). The resulting tree builds and boots
# under all four SCHED values; the three new system calls are stubbed out (they
# return -1) and the scheduler behaves as stock round-robin until you implement
# Tasks 0-3.
#
# Refuses to overwrite an existing <destdir>.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <destdir>" >&2
  exit 2
fi

DEST="$1"

# Resolve the directory this script lives in (labs/lab3-scheduler/starter).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OVERLAY="$SCRIPT_DIR/overlay"
PRISTINE="$SCRIPT_DIR/../../xv6-riscv"

if [ ! -d "$PRISTINE" ]; then
  echo "error: pristine xv6 tree not found at $PRISTINE" >&2
  exit 1
fi
if [ ! -d "$OVERLAY" ]; then
  echo "error: overlay dir not found at $OVERLAY" >&2
  exit 1
fi
if [ -e "$DEST" ]; then
  echo "error: destination '$DEST' already exists; refusing to overwrite." >&2
  echo "       remove it or choose another path." >&2
  exit 1
fi

echo "setup: copying pristine xv6 -> $DEST"
cp -r "$PRISTINE" "$DEST"
# Drop the upstream git history from the working copy.
rm -rf "$DEST/.git"

echo "setup: overlaying starter files"
# Copy every file under overlay/ into the destination, preserving structure.
( cd "$OVERLAY" && find . -type f -print ) | while read -r rel; do
  rel="${rel#./}"
  mkdir -p "$DEST/$(dirname "$rel")"
  cp "$OVERLAY/$rel" "$DEST/$rel"
  echo "  overlay: $rel"
done

MAKEFILE="$DEST/Makefile"

echo "setup: registering lab user programs in Makefile UPROGS"
if ! grep -q '^UPROGS=\\$' "$MAKEFILE"; then
  echo "error: could not find 'UPROGS=\\' line in $MAKEFILE to patch" >&2
  exit 1
fi
if grep -q '_schedload' "$MAKEFILE"; then
  echo "  (UPROGS already patched, skipping)"
else
  sed -i 's|^UPROGS=\\$|UPROGS=\\\n\t$U/_pstat\\\n\t$U/_schedload\\\n\t$U/_priotest\\\n\t$U/_lotto\\\n\t$U/_mlfqtest\\|' "$MAKEFILE"
  echo "  added _pstat, _schedload, _priotest, _lotto, _mlfqtest"
fi

echo "setup: wiring up the SCHED scheduler-select knob"
if grep -q 'DSCHED_' "$MAKEFILE"; then
  echo "  (SCHED knob already present, skipping)"
else
  # Insert after the '-I.' CFLAGS line. `SCHED ?= RR` takes its value from the
  # environment or the make command line if set, defaulting to RR otherwise;
  # -DSCHED_$(SCHED) reaches kernel/proc.c which #ifdef-selects the policy.
  if ! grep -q '^CFLAGS += -I\.$' "$MAKEFILE"; then
    echo "error: could not find 'CFLAGS += -I.' line in $MAKEFILE to patch" >&2
    exit 1
  fi
  sed -i '/^CFLAGS += -I\.$/a \
\
# Lab 3: select the scheduling policy at compile time.\
#   make SCHED=RR      classic round robin (default, stock behaviour)\
#   make SCHED=PRIO    static priority (Task 1)\
#   make SCHED=LOTTERY lottery scheduling (Task 2)\
#   make SCHED=MLFQ    multi-level feedback queue (Task 3)\
SCHED ?= RR\
CFLAGS += -DSCHED_$(SCHED)' "$MAKEFILE"
  echo "  added SCHED knob (default RR)"
fi

echo "setup: done. Build and boot with (default scheduler is RR):"
echo "    cd $DEST && make qemu"
echo "  or pick a policy:"
echo "    make clean && make qemu SCHED=MLFQ"
echo "(quit qemu with Ctrl-a x; run 'make clean' when switching SCHED values)"
