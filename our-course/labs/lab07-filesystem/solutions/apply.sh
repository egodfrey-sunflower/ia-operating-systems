#!/usr/bin/env bash
#
# apply.sh <workdir>
#
# SPOILERS. Lay the reference solution for Lab 7 Parts 2-3 over a tree made by
# ../setup.sh.
#
# The solution ships as whole files rather than as a patch: by the time you
# read this your tree has your own edits in it, and every hunk of a patch
# would be a conflict. Whole files always apply, and `diff -u` against
# ../starter/overlay/ or the pristine labs/xv6-riscv/ shows the same
# information a patch would.
#
# Files replaced (relative to the pristine tree):
#   kernel/fs.h        NDIRECT -> 11, addrs[NDIRECT+2], NDINDIRECT, MAXFILE
#   kernel/file.h      the in-memory inode's addrs[] grows to match
#   kernel/fs.c        bmap()/itrunc() walk/free the doubly indirect block;
#                      plus the given freeblocks() helper
#   kernel/sysfile.c   sys_symlink(), sys_open() follows links, sys_freeblocks()
#   kernel/param.h     FSSIZE bumped so a maximum file fits (given in starter)
#   kernel/defs.h stat.h fcntl.h syscall.h syscall.c   the symlink and
#                      freeblocks plumbing (given)
#   user/user.h user/usys.pl                    the symlink()/freeblocks() decls
#   user/bigfile.c user/symlinktest.c           the tests (given)
#   Makefile                                    _bigfile, _symlinktest (given)
#
# The Part 4/5 checker (xfsck) is separate userspace code; its reference lives
# in ../solutions/xfsck/, not here.

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
echo "apply.sh: reference solution (Parts 2-3) applied to $WORKDIR"
