#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 3, Parts 1-5.
#
# Compiles <workdir>/mymalloc.c HERE, with the spec's own CFLAGS and this
# script's own gcc command line -- your Makefile is never the graded build, so
# a Makefile that quietly drops -Werror, adds -w, or builds some other source
# file cannot quietly change the grade. Your Makefile is still required to
# work: it is run as a separate gate, because 'make' is the documented
# workflow. Then compiles the case driver and the trace driver against YOUR
# header and the library built here, and runs each case in its own process,
# under its own timeout.
#
# Nothing here inspects your internals. Every assertion is made through the
# published API: returned addresses, block contents, mym_get_stats(),
# mym_check_heap() and mym_last_error(). Boundary tags or an address-ordered
# list, one free list or ten -- the harness cannot tell and does not care.
#
# Part 4's report (FITS.md) is checked for existence and coverage only. The
# numbers in it are yours and are marked against the rubric in
# solutions/README.md, not by this script.
#
# Exits 0 only if every case passes. No root needed.

set -u
export LC_ALL=C

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
LABDIR=$(cd "$(dirname "$0")/.." && pwd -P)
TESTDIR="$LABDIR/tests"
FIXDIR="$LABDIR/fixtures"

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)
trap 'rm -rf "$TMP"' EXIT

CASES="$TMP/cases"
TRACERUN="$TMP/tracerun"

TIMEOUT=60          # per case; the longest case takes about a second
VG_TIMEOUT=300      # valgrind is roughly 30x slower

pass=0; fail=0; skip=0
declare -a RESULTS
LAST_RESULT=FAIL
record() { # <name> <PASS|FAIL|SKIP>
	RESULTS+=("$2|$1")
	LAST_RESULT=$2
	case $2 in
		PASS) pass=$((pass+1)) ;;
		SKIP) skip=$((skip+1)) ;;
		*)    fail=$((fail+1)) ;;
	esac
}

# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g'

echo "== building $WORKDIR =="

# 1. The graded build. This script's own compiler line, on mymalloc.c, into
#    this script's own temporary directory. Nothing in <workdir> influences it.
[ -f "$WORKDIR/mymalloc.c" ] || {
	echo "run.sh: no mymalloc.c in $WORKDIR." >&2
	echo "RESULT: build failure"; exit 1; }

LIB="$TMP/libmymalloc.a"
OBJ="$TMP/mymalloc.o"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -c -o "$OBJ" "$WORKDIR/mymalloc.c" \
     >"$TMP/build.log" 2>&1; then
	echo "build FAILED (the graded build is this script's own compiler line," >&2
	echo "not your Makefile: gcc $GRADE_CFLAGS -c mymalloc.c):" >&2
	cat "$TMP/build.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
if grep -q -F "warning:" "$TMP/build.log"; then
	echo "build produced warnings (the spec is -Werror clean):" >&2
	grep -F "warning:" "$TMP/build.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
if ! ar rcs "$LIB" "$OBJ" 2>"$TMP/ar.log"; then
	echo "run.sh: could not archive $OBJ:" >&2
	cat "$TMP/ar.log" >&2
	echo; echo "RESULT: build failure"; exit 1
fi
echo "build OK"

# 2. Your Makefile, as a separate gate. It does not produce anything graded,
#    but 'make' and 'make test' are the documented workflow and have to work.
if ! (make -C "$WORKDIR" clean >/dev/null 2>&1 &&
      make -C "$WORKDIR" all >"$TMP/make.log" 2>&1 &&
      [ -f "$WORKDIR/libmymalloc.a" ]); then
	echo "your Makefile did not build libmymalloc.a:" >&2
	cat "$TMP/make.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "Makefile OK"

for drv in cases tracerun; do
	if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -o "$TMP/$drv" \
	     "$TESTDIR/$drv.c" "$LIB" >"$TMP/$drv.log" 2>&1; then
		echo "run.sh: could not build the $drv driver against your library." >&2
		echo "That usually means mymalloc.h no longer matches the contract:" >&2
		cat "$TMP/$drv.log" >&2
		echo; echo "RESULT: build failure"
		exit 1
	fi
done
echo "drivers OK"
echo

# ---------------------------------------------------------------------------
# case runners
# ---------------------------------------------------------------------------

show() { # <file> <what>
	if [ -s "$1" ]; then
		echo "    --- $2 ---" >&2
		head -20 "$1" >&2
	fi
}

# A case that must pass: the driver exits 0.
ccase() { # <label> <case-name> [VAR=VALUE ...]
	local label=$1 name=$2; shift 2
	local rc
	env "$@" timeout -k 5 "$TIMEOUT" "$CASES" "$name" \
		>"$TMP/out" 2>"$TMP/err"
	rc=$?
	if [ "$rc" = 0 ]; then
		record "$label" PASS
		return
	fi
	record "$label" FAIL
	{
		if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
			echo "  [$label] TIMED OUT after ${TIMEOUT}s (case '$name')"
			echo "    An allocator that loses track of a block loops for ever"
			echo "    looking for it. Bound every list walk you write."
		elif [ "$rc" -gt 128 ]; then
			echo "  [$label] killed by signal $((rc-128)) (case '$name')"
			echo "    A crash inside the allocator: the usual cause is following"
			echo "    a free-list link out of a block that was already reused."
		else
			echo "  [$label] failed (case '$name')"
		fi
		show "$TMP/err" "what the case complained about"
	} >&2
}

# A case that must FAIL, for a stated reason. Used once: the coalescing
# control. A pattern that passes whether or not blocks merge would not be
# evidence that merging works.
ncase() { # <label> <case-name> <expected-text-in-stderr> [VAR=VALUE ...]
	local label=$1 name=$2 want=$3; shift 3
	local rc
	env "$@" timeout -k 5 "$TIMEOUT" "$CASES" "$name" \
		>"$TMP/out" 2>"$TMP/err"
	rc=$?
	if [ "$rc" = 1 ] && grep -q -F "$want" "$TMP/err"; then
		record "$label" PASS
		return
	fi
	record "$label" FAIL
	{
		echo "  [$label] (case '$name', exit $rc)"
		if [ "$rc" = 0 ]; then
			echo "    The case passed, and here it must not: with MYM_COALESCE=0"
			echo "    the four blocks must stay four blocks and the 4096-byte"
			echo "    request must fail. Either myfree() is ignoring the"
			echo "    environment variable, or something else is merging."
		else
			echo "    The case failed, but not with the expected complaint:"
			echo "    looked for: $want"
		fi
		show "$TMP/err" "stderr"
	} >&2
}

# ---------------------------------------------------------------------------
# Part 1 -- the arena, the header, the contract
# ---------------------------------------------------------------------------

ccase "Part 1: every returned pointer is 16-byte aligned"          p1_align
ccase "Part 1: distinct allocations do not overlap"                p1_no_overlap
ccase "Part 1: contents survive later allocations"                 p1_survives
ccase "Part 1: mymalloc(0) is unique and freeable"                 p1_zero
ccase "Part 1: myfree(NULL) does nothing, even before the arena"   p1_free_null
ccase "Part 1: the stats account for every byte of the arena"      p1_stats
ccase "Part 1: an impossible request returns NULL and is harmless" p1_too_big

# ---------------------------------------------------------------------------
# Part 2 -- free list, first fit, splitting
# ---------------------------------------------------------------------------

ccase "Part 2: a freed block is handed out again"                  p2_reuse
ccase "Part 2: 2000 rounds of churn through a 64 KiB arena"        p2_churn
ccase "Part 2: one big free block serves eight small requests"     p2_split
ccase "Part 2: a remainder too small to be a block is not made one" p2_tiny_remainder

# The free list lives in the free space. If the allocator calls the C
# library's allocator, it is not managing memory, it is subcontracting.
#
# $OBJ is the object file THIS script compiled, so a Makefile that builds a
# different source file cannot hide a libc call from this check either.
LIBC_HEAP='malloc calloc realloc reallocarray free aligned_alloc posix_memalign strdup strndup'
if ! nm -u "$OBJ" >"$TMP/undef" 2>"$TMP/nm.err"; then
	record "Part 2: the allocator uses no libc heap" FAIL
	{ echo "  [Part 2: the allocator uses no libc heap] could not read symbols"
	  show "$TMP/nm.err" "nm"; } >&2
else
	found=""
	for sym in $LIBC_HEAP; do
		if grep -q -w -F "$sym" "$TMP/undef"; then
			found="$found $sym"
		fi
	done
	if [ -n "$found" ]; then
		record "Part 2: the allocator uses no libc heap" FAIL
		{
		  echo "  [Part 2: the allocator uses no libc heap]"
		  echo "    mymalloc.c refers to:$found"
		  echo "    Every byte of bookkeeping has to live inside the arena."
		} >&2
	else
		record "Part 2: the allocator uses no libc heap" PASS
	fi
fi

# ---------------------------------------------------------------------------
# Part 3 -- coalescing
# ---------------------------------------------------------------------------

ccase "Part 3: four blocks freed low-to-high merge into one"       p3_merge_up
merge_up=$LAST_RESULT
ccase "Part 3: four blocks freed high-to-low merge into one"       p3_merge_down
ccase "Part 3: a block freed between two free ones merges both ways" p3_merge_both
ccase "Part 3: freeing everything leaves exactly one free block"   p3_collapse

# The control is only evidence about coalescing if the case it controls passes
# with coalescing on. p3_merge_up fails with "did not merge" for any allocator
# that cannot serve the 4096-byte request -- including one that never reuses
# anything at all -- so running it with MYM_COALESCE=0 against a broken
# allocator would award a mark for a mechanism that is not there.
if [ "$merge_up" = PASS ]; then
	ncase "Part 3: with MYM_COALESCE=0 the same pattern fails" \
	      p3_merge_up "did not merge" MYM_COALESCE=0
else
	record "Part 3: with MYM_COALESCE=0 the same pattern fails" SKIP
	{ echo "  [Part 3: with MYM_COALESCE=0 the same pattern fails] SKIPPED:"
	  echo "    the case it controls is itself failing with coalescing ON, so"
	  echo "    running it with coalescing OFF would say nothing. Get"
	  echo "    'four blocks freed low-to-high merge into one' passing first."
	} >&2
fi

# ---------------------------------------------------------------------------
# Part 4 -- fit policies
# ---------------------------------------------------------------------------

ccase "Part 4: first fit takes the lowest-address block that fits" p4_first
ccase "Part 4: best fit takes the smallest block that fits"        p4_best
ccase "Part 4: worst fit takes the largest block"                  p4_worst

# The traces have to replay cleanly before their numbers mean anything.
trace_ok=1
: > "$TMP/trace.log"
for tr in trace-small trace-mixed trace-grow; do
	for pol in first best worst; do
		if ! timeout -k 5 "$TIMEOUT" "$TRACERUN" "$FIXDIR/$tr.txt" "$pol" \
		     >>"$TMP/trace.log" 2>>"$TMP/trace.err"; then
			trace_ok=0
			echo "$tr/$pol: the driver did not finish" >> "$TMP/trace.err"
		fi
	done
done
if [ "$trace_ok" = 1 ] && ! grep -q -F "failed=0" "$TMP/trace.log"; then
	trace_ok=0
	echo "no run reported failed=0" >> "$TMP/trace.err"
fi
if [ "$trace_ok" = 1 ] && [ "$(grep -c -F "heap=ok" "$TMP/trace.log")" != 9 ]; then
	trace_ok=0
	echo "some run finished with mym_check_heap() reporting damage:" \
		>> "$TMP/trace.err"
	grep -v -F "heap=ok" "$TMP/trace.log" >> "$TMP/trace.err"
fi
if [ "$trace_ok" = 1 ] && [ "$(grep -c -F "failed=0" "$TMP/trace.log")" != 9 ]; then
	trace_ok=0
	echo "some run could not satisfy every allocation in its trace:" \
		>> "$TMP/trace.err"
	grep -v -F "failed=0" "$TMP/trace.log" >> "$TMP/trace.err"
fi
if [ "$trace_ok" = 1 ]; then
	record "Part 4: all three traces replay under all three policies" PASS
else
	record "Part 4: all three traces replay under all three policies" FAIL
	{ echo "  [Part 4: all three traces replay under all three policies]"
	  show "$TMP/trace.err" "trace driver"
	  show "$TMP/trace.log" "what did run"; } >&2
fi

# Three policies that produce one set of numbers are one policy with three
# names, and Part 4's whole exercise would be vacuous.
if [ "$trace_ok" = 1 ]; then
	sig=$(grep -F "trace=trace-mixed.txt" "$TMP/trace.log" |
	      sed 's/.*\(peak_frag_pct=[^ ]*\).*\(end_largest_free=[0-9]*\).*/\1 \2/' |
	      sort -u | wc -l)
	if [ "$sig" -ge 2 ]; then
		record "Part 4: the policies really do behave differently" PASS
	else
		record "Part 4: the policies really do behave differently" FAIL
		{ echo "  [Part 4: the policies really do behave differently]"
		  echo "    All three policies produced identical fragmentation and"
		  echo "    identical end-state numbers on trace-mixed. On a trace with"
		  echo "    three size classes that cannot happen unless two of the"
		  echo "    three are the same code path."
		  grep -F "trace=trace-mixed.txt" "$TMP/trace.log" | sed 's/^/    | /'
		} >&2
	fi
else
	record "Part 4: the policies really do behave differently" FAIL
	echo "  [Part 4: the policies really do behave differently] no numbers to compare" >&2
fi

# The report itself. Its numbers are marked by rubric, not here; what the
# harness can check is that all nine runs are actually in it.
FITS="$WORKDIR/FITS.md"
if [ ! -f "$FITS" ]; then
	record "Part 4: FITS.md covers every policy and every trace" FAIL
	{ echo "  [Part 4: FITS.md covers every policy and every trace]"
	  echo "    No FITS.md in $WORKDIR. Part 4's deliverable is the report."; } >&2
else
	missing=""
	for w in first best worst trace-small trace-mixed trace-grow; do
		grep -q -F "$w" "$FITS" || missing="$missing $w"
	done
	# Six substring hits can all come from one line. Nine runs of numbers
	# cannot: require at least nine lines that carry a digit, so a PASS here
	# means a table was actually written out.
	numbered=$(grep -c '[0-9]' "$FITS")
	if [ -n "$missing" ]; then
		record "Part 4: FITS.md covers every policy and every trace" FAIL
		{ echo "  [Part 4: FITS.md covers every policy and every trace]"
		  echo "    FITS.md never mentions:$missing"; } >&2
	elif [ "$numbered" -lt 9 ]; then
		record "Part 4: FITS.md covers every policy and every trace" FAIL
		{ echo "  [Part 4: FITS.md covers every policy and every trace]"
		  echo "    FITS.md has only $numbered line(s) with a number in them."
		  echo "    Part 4's deliverable is a table of nine runs; the harness"
		  echo "    cannot mark the numbers, but it can tell they are missing."
		} >&2
	else
		record "Part 4: FITS.md covers every policy and every trace" PASS
	fi
fi

# ---------------------------------------------------------------------------
# Part 5 -- correctness under abuse
# ---------------------------------------------------------------------------

ccase "Part 5: a double free is rejected and the heap survives"    p5_double_free
ccase "Part 5: freeing an interior pointer is rejected"            p5_interior
ccase "Part 5: freeing a pointer outside the arena is rejected"    p5_outside
ccase "Part 5: the walker catches an overflow into the next header" p5_overflow
ccase "Part 5: the walker catches a free list the physical walk contradicts" p5_walker_list
ccase "Part 5: the walker catches a free block that fell off the list" p5_walker_leak
ccase "Part 5: the walker catches two adjacent free blocks"        p5_walker_adjacent
ccase "Part 5: with MYM_COALESCE=0 the walker allows them"         p5_walker_coalesce_off MYM_COALESCE=0
ccase "Part 5: the walker returns on a heap whose walk is a cycle" p5_walker_bound
ccase "Part 5: 200000 random operations, contents verified throughout" p5_stress

if command -v valgrind >/dev/null 2>&1; then
	# A tenth of the operation count, because valgrind is roughly 30x
	# slower and this is a memory-safety check, not a second stress run.
	env MYM_STRESS_OPS=20000 timeout -k 5 "$VG_TIMEOUT" \
	   valgrind -q --error-exitcode=9 --track-origins=yes \
	   "$CASES" p5_stress >"$TMP/vg.out" 2>"$TMP/vg.err"
	vg_rc=$?
	# --error-exitcode=9 is what makes the two failures distinguishable:
	# 9 is valgrind's verdict, 1 is the case's own assertions failing.
	if [ "$vg_rc" = 0 ]; then
		record "Part 5: valgrind finds nothing during the stress run" PASS
	elif [ "$vg_rc" = 9 ]; then
		record "Part 5: valgrind finds nothing during the stress run" FAIL
		{ echo "  [Part 5: valgrind finds nothing during the stress run]"
		  echo "    An allocator can pass every functional test while reading"
		  echo "    uninitialised memory or walking off the end of the arena."
		  show "$TMP/vg.err" "valgrind"; } >&2
	else
		record "Part 5: valgrind finds nothing during the stress run" SKIP
		{ echo "  [Part 5: valgrind finds nothing during the stress run] SKIPPED:"
		  echo "    the stress case itself failed under valgrind (exit $vg_rc),"
		  echo "    so there is no verdict about memory safety to give. Fix"
		  echo "    'Part 5: 200000 random operations' first; this case will"
		  echo "    then have something to say."
		  show "$TMP/vg.err" "stderr"; } >&2
	fi
else
	record "Part 5: valgrind finds nothing during the stress run" SKIP
	echo "  [Part 5: valgrind] SKIPPED: valgrind is not installed." >&2
fi

# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------

echo
echo "== results =="
for r in "${RESULTS[@]}"; do
	printf '  %-6s %s\n' "${r%%|*}" "${r#*|}"
done
echo
if [ "$skip" -gt 0 ]; then
	echo "$pass passed, $fail failed, $skip skipped"
else
	echo "$pass passed, $fail failed"
fi
echo "(The numbers in FITS.md are marked against the rubric in solutions/README.md,"
echo " not by this script.)"
[ "$fail" -eq 0 ]
