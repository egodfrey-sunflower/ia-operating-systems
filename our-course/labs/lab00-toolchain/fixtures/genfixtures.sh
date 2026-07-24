#!/usr/bin/env bash
#
# genfixtures.sh [DEST] -- build the Lab 0 fixture tree.
#
# The tree is *generated*, not committed: it contains a binary file, an empty
# file and an empty directory, none of which survive a git round-trip
# faithfully. Regenerating is deterministic -- same bytes every time, on any
# machine -- which is what lets tests/run.sh diff against the system tools.
#
# DEST defaults to ./tree next to this script. It is removed and rebuilt.
#
# See README.md in this directory for what each fixture is for.

set -eu
export LC_ALL=C

DEST=${1:-"$(cd "$(dirname "$0")" && pwd)/tree"}

rm -rf "$DEST"
mkdir -p "$DEST"

# --- flat files -----------------------------------------------------------

# Ordinary text, two lines, trailing newline.
printf 'hello\nworld\n' > "$DEST/hello.txt"

# Substring targets for mygrep: "green" is a prefix of the other two, so a
# naive whole-line compare fails while a substring search succeeds.
printf 'green\ngreener\ngreenest\nblue\n' > "$DEST/colours.txt"

# Zero bytes. read() returns 0 on the very first call.
: > "$DEST/empty.txt"

# No trailing newline. cat must not invent one; grep must add one when it
# prints the line.
printf 'last line has no newline' > "$DEST/nonewline.txt"

# Binary: embedded NULs and high bytes, no trailing newline. Anything that
# treats the buffer as a C string truncates at the first NUL.
printf 'bin\x00ary\x01\x02\xff\xfe\ntail\x00' > "$DEST/binary.bin"

# Large file: 200000 bytes = 3 x 65536 + 3392, so a 64 KiB buffer needs four
# read() calls and the last one comes back short. Not a multiple of any
# plausible buffer size, on purpose.
: > "$DEST/large.txt"
i=0
while [ "$i" -lt 5000 ]; do
	printf 'line %06d: the quick brown fox jumps over the lazy dog. pad\n' "$i"
	i=$((i + 1))
done > "$DEST/large.txt"
# Trim/extend to exactly 200000 bytes so the short-read arithmetic is exact.
truncate -s 200000 "$DEST/large.txt"

# --- nested directories ---------------------------------------------------

mkdir -p "$DEST/tree/sub/deeper"
printf 'x\n'    > "$DEST/tree/alpha.txt"
printf 'yy\n'   > "$DEST/tree/beta.txt"
printf 'zzz\n'  > "$DEST/tree/zeta.bin"
printf 'M\n'    > "$DEST/tree/Middle"      # uppercase: sorts before lowercase
printf 'h\n'    > "$DEST/tree/.hidden"     # dotfile: myls must NOT list it
printf 'n\n'    > "$DEST/tree/sub/nested.txt"
: > "$DEST/tree/sub/deeper/.keep"          # keeps deeper/ non-empty on disk;
                                           # listing deeper/ is still empty

# An empty directory, to prove myls prints nothing and exits 0.
mkdir -p "$DEST/emptydir"

echo "fixtures built in $DEST"
