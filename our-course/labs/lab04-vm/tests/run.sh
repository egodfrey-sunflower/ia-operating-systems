#!/usr/bin/env bash
#
# run.sh [--fast | --full] <workdir> -- autograder for Lab 4, Parts 1-4.
#
# Builds your tree from clean, checks the page table vmprint() printed at boot
# (Part 1's tree shape), then runs pgtbltest, lazytests and cowtest (Parts 2-4)
# and, unless --fast, xv6's own usertests as the regression gate. Part 1's
# PGTBL-NOTES.md prose is marked by hand against solutions/README.md.
#
# The real work is in run.py, because everything here happens inside a QEMU
# that has to be driven, scraped and reliably killed, and the shared driver for
# that (../../common/qemuharness.py) is Python. This wrapper exists so every
# lab in the course is run the same way: `tests/run.sh <workdir>`.
#
#   default   Parts 1-4, then 'usertests -q'      (a few minutes)
#   --fast    Parts 1-4 only; skips usertests      (iterate with this)
#   --full    Parts 1-4, then the whole usertests  (before you hand in)
#
# Exits 0 only if every case passes, 1 if any case fails or the tree does not
# build, 2 for a usage or setup error. No root needed.
#
# Bringing the shared QEMU driver up on a new machine? It has a self-test:
#
#     python3 ../../common/qemuharness.py <a built xv6 tree>
#
# which boots the tree, runs one shell command and tears the emulator down.
# Run that first: it separates "my grader is wrong" from "QEMU is wrong".

set -u

HERE=$(cd "$(dirname "$0")" && pwd -P)

for PY in python3 python; do
	if command -v "$PY" >/dev/null 2>&1; then
		# -u: unbuffered, so a student redirecting to a file can watch the
		# slow usertests phase make progress.
		exec "$PY" -u "$HERE/run.py" "$@"
	fi
done

echo "run.sh: no python3 on PATH; the Lab 4 harness needs it" >&2
exit 2
