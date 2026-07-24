#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder entry point for Lab 8, Parts 1-3.
#
# The lab is three command-line tools -- pwstore, crack, accessmatrix -- driven
# entirely through their text interface (argv/stdin in, stdout out), so the
# grader never imports the submission and the language is invisible to it.  The
# grading logic lives in grade.py alongside this script (Part 2 is a property
# test that generates random matrices and compares each projection against an
# oracle -- far cleaner in Python than in bash), and this wrapper just locates it
# and runs it.  It prints a PASS/FAIL table and 'N passed, M failed', and exits
# non-zero if anything failed.
#
# No root, no large allocations, a timeout on every case.

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
