#!/usr/bin/env bash
#
# mkwww.sh <destination> -- build the graded document root.
#
# Copies www/ and adds two deterministic files that are generated rather than
# committed, because neither is the sort of thing to keep in a git repository:
# big.bin, 256 KB, which is larger than one read() and larger than most
# people's first buffer, and huge.bin, 8 MB. Why 256 KB is not enough on its
# own -- and it is not -- is in the comment above the generator below.
#
# The harness builds the graded document root with this script into its own
# temporary directory, so the files your server is asked for are the ones
# this script made -- not whatever is in your copy of www/. Run it yourself
# to get the same set for your own testing and for PERF.md.

set -eu
export LC_ALL=C

if [ $# -ne 1 ]; then
	echo "usage: mkwww.sh <destination>" >&2
	exit 2
fi
DEST=$1
HERE=$(cd "$(dirname "$0")/.." && pwd -P)

mkdir -p "$DEST"
cp "$HERE/www/index.html" "$HERE/www/hello.txt" "$HERE/www/page.html" \
   "$HERE/www/bytes.bin" "$DEST/"

# big.bin: 256 KB, deterministic. Larger than one read() and larger than most
# people's first buffer.
#
# huge.bin: 8 MB, and the size is not arbitrary. A 256 KB response fits
# whole into a socket send buffer on this kind of machine, so it does NOT
# force write() to return short; 8 MB is larger than the kernel's maximum
# send buffer (net.ipv4.tcp_wmem's third number, 4 MB here) and does. The
# partial-write case is the only thing that fetches it, and it only ever
# catches the EVENT server: the threaded server's socket is blocking, and
# Linux blocks rather than returning short.
#
# IF YOU RUN THIS ON A DIFFERENT MACHINE, check the third number of
# `cat /proc/sys/net/ipv4/tcp_wmem`. Linux autotunes it from the amount of
# RAM, and if it is 8 MB or more here then huge.bin fits in the send buffer
# after all, write() stops going short, and the partial-write case silently
# goes back to checking nothing -- with no symptom at all, because the
# reference still passes. Raise huge.bin to twice that number if so.
python3 - "$DEST" <<'PY'
import sys
dest = sys.argv[1]


def blob(nbytes):
    out = bytearray()
    i = 0
    while len(out) < nbytes:
        out += bytes(((i * 37 + j * 11) % 256) for j in range(64))
        i += 1
    return bytes(out[:nbytes])


open(dest + '/big.bin', 'wb').write(blob(256 * 1024))
open(dest + '/huge.bin', 'wb').write(blob(8 * 1024 * 1024))
PY

exit 0
