#!/usr/bin/env bash
#
# setup.sh [-f] <workdir>
#
# Make a working xv6 tree for Lab 7 Parts 2-3 (large files and symbolic
# links): copy the course's pinned checkout of xv6-riscv into <workdir>, then
# lay the lab's starter files over the top.
#
# The pinned tree at labs/xv6-riscv/ is never modified. Every kernel lab
# copies it, and the copy is what you edit -- which is what makes "start
# again from a clean kernel" one command.
#
# This sets up Parts 2 and 3 only. Parts 4 and 5 (xfsck and the corruption
# injector) are plain userspace C -- no kernel, no QEMU -- and live in
# starter/xfsck/; copy that directory somewhere and build it with its own
# Makefile.
#
# -f  overwrite <workdir> if it already exists. Without it, setup.sh refuses,
#     because the tree you have been editing for hours looks exactly like the
#     tree you have not.

set -eu

FORCE=0
ARGS=""
END=0
for arg in "$@"; do
	if [ "$END" -eq 0 ]; then
		case "$arg" in
			-f|--force) FORCE=1; continue ;;
			-h|--help)
				echo "usage: setup.sh [-f] <workdir>"
				exit 0 ;;
			--) END=1; continue ;;
			-*) echo "setup.sh: unknown option '$arg'" >&2; exit 2 ;;
		esac
	fi
	ARGS="$ARGS $arg"
done

# shellcheck disable=SC2086
set -- $ARGS
if [ $# -ne 1 ]; then
	echo "usage: setup.sh [-f] <workdir>" >&2
	exit 2
fi

WORKDIR=$1
LABDIR=$(cd "$(dirname "$0")" && pwd -P)
PRISTINE=$(cd "$LABDIR/../xv6-riscv" 2>/dev/null && pwd -P) || {
	echo "setup.sh: cannot find the pinned xv6 tree at $LABDIR/../xv6-riscv" >&2
	exit 1
}
OVERLAY="$LABDIR/starter/overlay"

if [ -e "$WORKDIR" ]; then
	if [ "$FORCE" != 1 ]; then
		echo "setup.sh: '$WORKDIR' already exists." >&2
		echo "          Pass -f to replace it -- this DELETES whatever is there." >&2
		exit 1
	fi
	echo "setup.sh: removing existing $WORKDIR"
	rm -rf "$WORKDIR"
fi

mkdir -p "$WORKDIR"
WORKDIR=$(cd "$WORKDIR" && pwd -P)

# Copy the source, not the build products: a stale kernel/*.d in the copy
# would refer to paths in the pristine tree.
( cd "$PRISTINE" && cp -a kernel user mkfs Makefile README LICENSE \
      .gdbinit.tmpl-riscv "$WORKDIR/" )
find "$WORKDIR" \( -name '*.o' -o -name '*.d' -o -name '*.asm' -o -name '*.sym' \) -delete
rm -f "$WORKDIR/kernel/kernel" "$WORKDIR/fs.img" "$WORKDIR/mkfs/mkfs"

cp -a "$OVERLAY/." "$WORKDIR/"

echo "setup.sh: xv6 tree ready in $WORKDIR"
echo
echo "  cd $WORKDIR"
echo "  make                       # single-job, please -- never make -j"
echo "  make qemu                  # Ctrl-A x to quit"
echo
echo "This tree builds and boots as it stands: it is stock xv6 plus the two"
echo "test programs (bigfile, symlinktest) and the symlink() plumbing. Until"
echo "you do the work, bigfile cannot store a file that large and symlink()"
echo "returns -1, so both tests fail."
