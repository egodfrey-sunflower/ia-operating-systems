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
#   kernel/vm.c        Parts 1, 3, 4 (vmprint, vmfault, copyout-honours-COW)
#   kernel/proc.c      Part 2 (the read-only per-process page)
#   kernel/kalloc.c    Part 4 (the physical-frame reference counts)
#   kernel/vm.h        Part 4 (PTE_COW)
#   kernel/memlayout.h Part 2 (USYSCALL and struct usyscall)
#   kernel/defs.h      declarations for the above
#   user/ulib.c        Part 2 (ugetpid)
#   user/user.h        ugetpid's prototype
#
# trap.c and sysproc.c are NOT in the list: the pinned tree already wires the
# page-fault path (trap.c) and the lazy branch of sbrk (sysproc.c) to call
# vmfault(). Part 3 is the body of vmfault(), not that plumbing. The Makefile
# and the three test programs are the same ones setup.sh gave you.

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
