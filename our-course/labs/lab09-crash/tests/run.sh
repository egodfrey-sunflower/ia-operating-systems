#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder entry point for Lab 9, Parts 1-5.
#
# The lab is three Python command-line tools (blockdev.py, crashfs, journal
# with wal.py behind it) driven entirely through argv/stdin, so the grader
# never imports the submission.  Its consistency oracle is Lab 7's REFERENCE
# xfsck, which it compiles ITSELF from tests/oracle/ with -Wall -Wextra
# -Werror -- never the student's checker, never the student's Makefile.  The
# campaign logic lives in grade.py alongside this script; this wrapper just
# locates it and runs it.  It prints a PASS/FAIL table and 'N passed, M
# failed', and exits non-zero if anything failed.
#
# No root, no QEMU, a timeout on every case.

set -u
export LC_ALL=C

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
HERE=$(cd "$(dirname "$0")" && pwd -P)

exec python3 "$HERE/grade.py" "$WORKDIR"
