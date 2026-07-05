#!/usr/bin/env bash
#
# setup.sh <destdir>
#
# Create a fresh Lab 4 working tree at <destdir>: copy the pristine vendored
# xv6-riscv, overlay this lab's starter files, and register the lab's user
# programs in the Makefile's UPROGS list. The resulting tree builds and boots
# as-is. The new functionality (vmprintme, ugetpid, copy-on-write fork) is
# NOT implemented yet -- the tests fail until you implement it.
#
# Refuses to overwrite an existing <destdir>.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <destdir>" >&2
  exit 2
fi

DEST="$1"

# Resolve the directory this script lives in (labs/lab4-vm/starter).
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

echo "setup: registering lab user programs in Makefile UPROGS"
MAKEFILE="$DEST/Makefile"
if ! grep -q '^UPROGS=\\$' "$MAKEFILE"; then
  echo "error: could not find 'UPROGS=\\' line in $MAKEFILE to patch" >&2
  exit 1
fi
if grep -q '_pgtbltest' "$MAKEFILE"; then
  echo "  (UPROGS already patched, skipping)"
else
  # Insert the three lab programs immediately after the "UPROGS=\" line.
  sed -i 's|^UPROGS=\\$|UPROGS=\\\n\t$U/_pgtbltest\\\n\t$U/_cowtest\\\n\t$U/_forkbench\\|' "$MAKEFILE"
  echo "  added _pgtbltest, _cowtest, _forkbench"
fi

echo "setup: done. Build and boot with:"
echo "    cd $DEST && make qemu"
echo "(quit qemu with Ctrl-a x)"
