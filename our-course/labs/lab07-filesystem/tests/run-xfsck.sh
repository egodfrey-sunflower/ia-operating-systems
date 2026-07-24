#!/usr/bin/env bash
#
# run-xfsck.sh <workdir> -- autograder for Lab 7 Parts 4-5 (the checker).
#
# <workdir> is a copy of starter/xfsck/ with your xfsck.c in it. The harness:
#
#   * compiles YOUR xfsck.c ITSELF, with -Wall -Wextra -Werror, against the
#     given image-reader library -- so a Makefile that drops -Werror, or an
#     edited xv6img.c, cannot change the grade;
#   * builds a clean image with the given mkimg and checks xfsck reports it
#     clean -- exactly the one line "xfsck: clean" the spec requires, nothing
#     else on stdout -- the clean image contains a hard link and the reserved
#     metadata region, so a checker that flags either fails here;
#   * damages a copy in each way the given corrupt tool knows, and checks
#     xfsck reports the matching violation class and exits non-zero;
#   * confirms the one corruption that is UNDETECTABLE (in-place data damage)
#     is, in fact, reported clean -- the boundary Part 5 is about.
#
# A checker that prints nothing and exits 0 (the starter stub) therefore FAILS
# the clean case and every corrupted case. No root, no QEMU, no cross-compiler.
#
# Exits 0 only if every case passes.

set -u
export LC_ALL=C

if [ $# -ne 1 ]; then
	echo "usage: run-xfsck.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run-xfsck.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
LABDIR=$(cd "$(dirname "$0")/.." && pwd -P)
GIVEN="$LABDIR/starter/xfsck"

[ -f "$WORKDIR/xfsck.c" ] || { echo "run-xfsck.sh: no xfsck.c in $WORKDIR" >&2; exit 2; }

TMP=$(mktemp -d); TMP=$(cd "$TMP" && pwd -P)
trap 'rm -rf "$TMP"' EXIT
TO="timeout 60"

pass=0; fail=0
declare -a RESULTS
record() { RESULTS+=("$2|$1"); case $2 in PASS) pass=$((pass+1));; *) fail=$((fail+1));; esac; }

# ---------------------------------------------------------------------------
# graded build: the given tools from the LAB's sources, xfsck from the student
# ---------------------------------------------------------------------------

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g'
echo "== building =="
cp "$GIVEN/xv6fs.h" "$GIVEN/xv6img.c" "$GIVEN/mkimg.c" "$GIVEN/corrupt.c" "$TMP/"
cp "$WORKDIR/xfsck.c" "$TMP/"

if ! gcc $GRADE_CFLAGS -o "$TMP/mkimg" "$TMP/mkimg.c" 2>"$TMP/mkimg.build"; then
	echo "internal error: given mkimg.c failed to build" >&2; cat "$TMP/mkimg.build" >&2; exit 2
fi
if ! gcc $GRADE_CFLAGS -o "$TMP/corrupt" "$TMP/corrupt.c" 2>"$TMP/corrupt.build"; then
	echo "internal error: given corrupt.c failed to build" >&2; cat "$TMP/corrupt.build" >&2; exit 2
fi
if ! gcc $GRADE_CFLAGS -o "$TMP/xfsck" "$TMP/xfsck.c" "$TMP/xv6img.c" >"$TMP/xfsck.build" 2>&1; then
	echo "graded build FAILED (xfsck.c):" >&2; cat "$TMP/xfsck.build" >&2
	echo; echo "RESULT: build failure"; exit 1
fi
if grep -q -F "warning:" "$TMP/xfsck.build"; then
	echo "xfsck.c produced warnings (the spec is -Werror clean):" >&2
	grep -F "warning:" "$TMP/xfsck.build" >&2
	echo; echo "RESULT: build failure"; exit 1
fi
echo "build OK"; echo

MKIMG="$TMP/mkimg"; CORRUPT="$TMP/corrupt"; XFSCK="$TMP/xfsck"
CLEAN="$TMP/clean.img"
if ! "$MKIMG" "$CLEAN" >/dev/null 2>&1; then
	echo "run-xfsck.sh: mkimg could not build a test image" >&2; exit 2
fi

# ---------------------------------------------------------------------------
# clean image: must be reported clean, on stdout, exit 0
# ---------------------------------------------------------------------------

out=$($TO "$XFSCK" "$CLEAN" 2>/dev/null); rc=$?
if [ "$rc" = 0 ] && [ "$out" = "xfsck: clean" ]; then
	record "clean image is reported clean (hard link + reserved region OK)" PASS
else
	record "clean image is reported clean (hard link + reserved region OK)" FAIL
	{ echo "  [clean] wanted exactly one line 'xfsck: clean' on stdout and exit 0, got exit $rc:"; printf '%s\n' "$out" | sed 's/^/    | /'; } >&2
fi

# ---------------------------------------------------------------------------
# each corruption: must report the matching class and exit non-zero
# ---------------------------------------------------------------------------

corrupt_case() { # <name> <mode> <expected-token>
	local name=$1 mode=$2 token=$3
	local img="$TMP/c.img"
	cp "$CLEAN" "$img"
	if ! "$CORRUPT" "$img" "$mode" >"$TMP/cor.log" 2>&1; then
		record "$name" FAIL
		{ echo "  [$name] corrupt '$mode' failed:"; sed 's/^/    | /' "$TMP/cor.log"; } >&2
		return
	fi
	local o; o=$($TO "$XFSCK" "$img" 2>/dev/null); local r=$?
	if [ "$r" != 0 ] && printf '%s\n' "$o" | grep -q -F "$token"; then
		record "$name" PASS
	else
		record "$name" FAIL
		{ echo "  [$name] wanted a '$token' line and non-zero exit, got exit $r:"
		  printf '%s\n' "$o" | sed 's/^/    | /'; } >&2
	fi
}

corrupt_case "bitmap: an in-use block marked free is caught"       bitmap-free  "FAIL block-free-but-used"
corrupt_case "bitmap: a free block marked in use is caught"        bitmap-leak  "FAIL bitmap-leak"
corrupt_case "link count that disagrees with the entries is caught" linkcount   "FAIL link-count"
corrupt_case "an orphaned inode (no entry) is caught"              orphan       "FAIL orphan-inode"
corrupt_case "a directory entry to a freed inode is caught"        dangling     "FAIL dangling-entry"
corrupt_case "two inodes sharing one block is caught"              double-claim "FAIL block-double-claim"
corrupt_case "a wrong '..' entry is caught"                        dotdot       "FAIL dotdot"
corrupt_case "a non-directory root inode is caught"                root         "FAIL root"

# ---------------------------------------------------------------------------
# the undetectable corruption: xfsck must NOT flag in-place data damage
# ---------------------------------------------------------------------------

cp "$CLEAN" "$TMP/d.img"
"$CORRUPT" "$TMP/d.img" data >/dev/null 2>&1
out=$($TO "$XFSCK" "$TMP/d.img" 2>/dev/null); rc=$?
if [ "$rc" = 0 ] && [ "$out" = "xfsck: clean" ]; then
	record "in-place data damage is (correctly) NOT detected" PASS
else
	record "in-place data damage is (correctly) NOT detected" FAIL
	{ echo "  [data] a purely structural checker cannot see data corruption; wanted exactly one line 'xfsck: clean' and exit 0, got exit $rc:"
	  printf '%s\n' "$out" | sed 's/^/    | /'; } >&2
fi

# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------

echo
echo "== results =="
for r in "${RESULTS[@]}"; do printf '  %-6s %s\n' "${r%%|*}" "${r#*|}"; done
echo
echo "$pass passed, $fail failed"
echo "(Part 5's UNDETECTABLE.md is prose, marked against solutions/, not counted here.)"
[ "$fail" -eq 0 ]
