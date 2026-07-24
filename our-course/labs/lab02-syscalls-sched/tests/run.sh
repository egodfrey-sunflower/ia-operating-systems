#!/usr/bin/env bash
#
# run.sh [--fast | --full] <workdir> -- autograder for Lab 2, Parts 2-5.
#
# Part 1 is prose and is not graded here, and neither is Part 5's
# SCHED-RESULTS.md; both are marked against solutions/README.md. Everything
# else is, under all three scheduler policies -- the script builds your tree
# from clean once per policy, so a single invocation is three kernels.
#
# The real work is in run.py, because everything this grades happens inside a
# QEMU that has to be driven, scraped and reliably killed, and the shared
# driver for that (../../common/qemuharness.py) is Python. This wrapper exists
# so that every lab in the course is run the same way: `tests/run.sh <workdir>`.
#
# Exits 0 only if every case passes, 1 if any case fails or the tree does not
# build, 2 for a usage or setup error. No root needed.
#
# Bringing the shared QEMU driver up on a new lab? It has a self-test:
#
#     python3 ../../common/qemuharness.py <a built xv6 tree>
#
# which boots the tree, runs one shell command, and tears the emulator down.
# Run that first: it separates "my grader is wrong" from "QEMU is wrong".

set -u

HERE=$(cd "$(dirname "$0")" && pwd -P)

for PY in python3 python; do
	if command -v "$PY" >/dev/null 2>&1; then
		# -u: unbuffered. The regression check can run for minutes, and a
		# student who redirects the output to a file should be able to see
		# how far it has got while it is still going.
		exec "$PY" -u "$HERE/run.py" "$@"
	fi
done

echo "run.sh: no python3 on PATH; the Lab 2 harness needs it" >&2
exit 2
