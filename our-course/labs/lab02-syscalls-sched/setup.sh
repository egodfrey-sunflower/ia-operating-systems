#!/usr/bin/env bash
#
# setup.sh [-f] <workdir>
#
# Make a working xv6 tree for Lab 2: copy the course's pinned checkout of
# xv6-riscv (tag xv6-riscv-rev5, upstream 7d7adbb1) into <workdir>, then lay
# the lab's starter files over the top.
#
# The pinned tree at labs/xv6-riscv/ is never modified. Every kernel lab
# copies it, and the copy is what you edit -- which is what makes "start
# again from a clean kernel" one command.
#
# -f  overwrite <workdir> if it already exists. Without it, setup.sh refuses,
#     because the tree you have been editing for six hours looks exactly like
#     the tree you have not.

set -eu

# Options and the workdir may come in either order: `setup.sh ~/lab2 -f`
# reads the same as `setup.sh -f ~/lab2`. So scan every argument rather than
# stopping at the first non-option, and collect the lone positional as we go.
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
#
# .gdbinit.tmpl-riscv is in the list because the Makefile's `.gdbinit` rule
# is what `make qemu-gdb` depends on, and Part 1 tells you to run it.
( cd "$PRISTINE" && cp -a kernel user mkfs Makefile README LICENSE \
      .gdbinit.tmpl-riscv "$WORKDIR/" )
find "$WORKDIR" \( -name '*.o' -o -name '*.d' -o -name '*.asm' -o -name '*.sym' \) -delete
rm -f "$WORKDIR/kernel/kernel" "$WORKDIR/fs.img" "$WORKDIR/mkfs/mkfs"

cp -a "$OVERLAY/." "$WORKDIR/"

echo "setup.sh: xv6 tree ready in $WORKDIR"
echo
echo "  cd $WORKDIR"
echo "  make                       # POLICY=rr by default; single-job, please"
echo "  make qemu                  # Ctrl-A x to quit"
echo
echo "This tree builds and boots as it stands: it is stock xv6 plus the lab's"
echo "stubs and test programs. Every new system call returns -1 until you"
echo "write it."
