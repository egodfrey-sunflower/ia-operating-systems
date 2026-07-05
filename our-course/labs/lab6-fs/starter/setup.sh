#!/usr/bin/env bash
#
# setup.sh <destdir>
#
# Create a fresh Lab 6 working tree at <destdir>: copy the pristine vendored
# xv6-riscv, then overlay this lab's starter files. The overlay includes a
# Makefile that already registers the lab's user programs (bigfile,
# symlinktest, crashtest) in UPROGS and adds the `CRASH=` crash-injection
# build flag.
#
# The resulting tree builds and boots as-is:
#   * FSSIZE is pre-bumped to 100000 blocks so a large-file test can fit;
#   * the crashnow() syscall and the (disabled) CRASHPOINT machinery are wired
#     up and harmless;
#   * bigfile and symlinktest are present but FAIL until you do Tasks 1 and 2.
#
# Refuses to overwrite an existing <destdir>.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <destdir>" >&2
  exit 2
fi

DEST="$1"

# Resolve the directory this script lives in (labs/lab6-fs/starter).
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
( cd "$OVERLAY" && find . -type f -print ) | while read -r rel; do
  rel="${rel#./}"
  mkdir -p "$DEST/$(dirname "$rel")"
  cp "$OVERLAY/$rel" "$DEST/$rel"
  echo "  overlay: $rel"
done

echo "setup: done. Build and boot with:"
echo "    cd $DEST && make qemu"
echo "(quit qemu with Ctrl-a x)"
