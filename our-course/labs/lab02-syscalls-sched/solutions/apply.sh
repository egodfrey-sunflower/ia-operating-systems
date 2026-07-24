#!/usr/bin/env bash
#
# apply.sh <workdir>
#
# SPOILERS. Lay the reference solution over a tree made by ../setup.sh.
#
# The solution ships as a set of whole files rather than as a patch. A patch
# applies to the tree it was cut against; by the time you read this your tree
# has your own edits in it and every hunk would be a conflict. Whole files
# always apply, and `diff -u` against ../starter/overlay/ or against the
# pristine labs/xv6-riscv/ shows exactly the same information a patch would.
#
# Files replaced:
#   Makefile
#   kernel/syscall.h  syscall.c  proc.h  proc.c  sysproc.c  kalloc.c  defs.h
#   user/user.h       user/usys.pl
#
# The Makefile is in that list for one line: $U/_tracetest in UPROGS, which
# Part 2 asks you to add.
#
# Everything else in the tree -- the test programs, sched.h, rand.c,
# sysinfo.h, pinfo.h -- is what setup.sh already gave you.

set -eu

if [ $# -ne 1 ]; then
	echo "usage: apply.sh <workdir>" >&2
	exit 2
fi

WORKDIR=$1
[ -d "$WORKDIR/kernel" ] || {
	echo "apply.sh: '$WORKDIR' does not look like an xv6 tree (no kernel/)" >&2
	exit 1
}
SOLDIR=$(cd "$(dirname "$0")" && pwd -P)

cp -a "$SOLDIR/overlay/." "$WORKDIR/"
echo "apply.sh: reference solution applied to $WORKDIR"
