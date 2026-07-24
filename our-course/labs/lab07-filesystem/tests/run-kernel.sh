#!/usr/bin/env bash
#
# run-kernel.sh [--fast | --full] <workdir> -- autograder for Lab 7 Parts 2-3
# (large files and symbolic links), inside xv6 under QEMU.
#
# Parts 4-5 (the xfsck checker) are plain userspace and are graded by
# tests/run-xfsck.sh instead. Part 1 (the ch. 39 tools) is graded by
# tests/run.sh. This script covers the two kernel parts only.
#
# The real work is in run-kernel.py, because everything here happens inside a
# QEMU that has to be driven, scraped and reliably killed; the shared driver
# for that (../../common/qemuharness.py) is Python.
#
# Exits 0 only if every case passes, 1 if any case fails or the tree does not
# build, 2 for a usage or setup error. No root needed.

set -u

HERE=$(cd "$(dirname "$0")" && pwd -P)

for PY in python3 python; do
	if command -v "$PY" >/dev/null 2>&1; then
		exec "$PY" -u "$HERE/run-kernel.py" "$@"
	fi
done

echo "run-kernel.sh: no python3 on PATH; the QEMU harness needs it" >&2
exit 2
