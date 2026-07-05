#!/usr/bin/env bash
#
# setup.sh <destdir>
#
# Create a fresh Lab 5 working tree for the xv6 parts (A and B) at <destdir>:
# copy the pristine vendored xv6-riscv, overlay this lab's starter files, and
# register the lab's user programs in the Makefile. The resulting tree builds
# and boots as-is: the lock-contention counters and the statistics() system
# call are already wired up, so `kalloctest`/`bcachetest` run and report big
# "kmem"/"bcache" contended-acquire numbers immediately. Making those numbers
# small (Part B) and making `uthread` work (Part A) is the lab.
#
# Part C (host pthreads) does NOT use this script -- it lives in
# starter/hostsync/ and builds on your Linux box with plain make.
#
# Refuses to overwrite an existing <destdir>.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <destdir>" >&2
  exit 2
fi

DEST="$1"

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
rm -rf "$DEST/.git"

echo "setup: overlaying starter files"
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
if grep -q '_kalloctest' "$MAKEFILE"; then
  echo "  (UPROGS already patched, skipping)"
else
  sed -i 's|^UPROGS=\\$|UPROGS=\\\n\t$U/_uthread\\\n\t$U/_kalloctest\\\n\t$U/_bcachetest\\|' "$MAKEFILE"
  echo "  added _uthread, _kalloctest, _bcachetest"
fi

echo "setup: adding special link rule for _uthread (needs uthread_switch.o)"
if grep -q '_uthread:' "$MAKEFILE"; then
  echo "  (rule already present, skipping)"
else
  cat >> "$MAKEFILE" <<'MK'

# Lab 5, Part A: uthread is two objects (the C scheduler plus the assembly
# context switch), so it needs its own link rule instead of the generic _%.
$U/_uthread: $U/uthread.o $U/uthread_switch.o $(ULIB) $U/user.ld
	$(LD) $(LDFLAGS) -T $U/user.ld -o $U/_uthread $U/uthread.o $U/uthread_switch.o $(ULIB)
	$(OBJDUMP) -S $U/_uthread > $U/uthread.asm
MK
  echo "  added \$U/_uthread rule"
fi

echo "setup: done. Build and boot with:"
echo "    cd $DEST && make qemu     # boots with CPUS=3 by default"
echo "(quit qemu with Ctrl-a x)"
echo
echo "Try:  uthread        (Part A -- fails until you finish it)"
echo "      kalloctest     (Part B task 1)"
echo "      bcachetest     (Part B task 2)"
