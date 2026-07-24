#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 5 Part 1, the user-level thread
# package.
#
# Compiles <workdir>/uthread.c and <workdir>/swtch.S HERE, with this script's
# own gcc command line, into this script's own temporary directory. Your
# Makefile is never the graded build, so a Makefile that quietly drops
# -Werror, adds -w, or builds some other source file cannot change the grade.
# Your Makefile is still required to work: it runs as a separate gate,
# because `make` is the documented workflow.
#
# Nothing here inspects your internals. Every case goes through uthread.h.
# One thread table or sixty-four, a linked queue or an array scan, malloc'd
# stacks or mmap'd ones -- the harness cannot tell and does not care. What it
# can tell is the ORDER your threads run in, because the header fixes it:
# create appends to the tail, yield sends the caller to the tail, the head
# runs next.
#
# Six cases are decided by diffing the program's output against a file in
# ../fixtures. The rest decide their own verdict and report on stderr.
#
# Every case runs in its own process under its own timeout. Part 1 is
# cooperative and single-threaded at the kernel level, so a case that hangs
# is a scheduler that lost a thread, not a race; the timeout says so.
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

TIMEOUT=10          # per case; the longest case takes well under a second
VG_TIMEOUT=180      # valgrind is roughly 30x slower

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

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g -O2'

echo "== building $WORKDIR =="

for f in uthread.c swtch.S uthread.h; do
	[ -f "$WORKDIR/$f" ] || {
		echo "run.sh: no $f in $WORKDIR." >&2
		echo "RESULT: build failure"; exit 1; }
done

LIB="$TMP/libuthread.a"

: > "$TMP/build.log"
for src in uthread.c swtch.S; do
	if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -c -o "$TMP/${src%.*}.o" \
	     "$WORKDIR/$src" >>"$TMP/build.log" 2>&1; then
		echo "build FAILED (the graded build is this script's own compiler" >&2
		echo "line, not your Makefile: gcc $GRADE_CFLAGS -c $src):" >&2
		cat "$TMP/build.log" >&2
		echo; echo "RESULT: build failure"
		exit 1
	fi
done
if grep -q -F "warning:" "$TMP/build.log"; then
	echo "build produced warnings (the spec is -Werror clean):" >&2
	grep -F "warning:" "$TMP/build.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
if ! ar rcs "$LIB" "$TMP/uthread.o" "$TMP/swtch.o" 2>"$TMP/ar.log"; then
	echo "run.sh: could not archive the objects:" >&2
	cat "$TMP/ar.log" >&2
	echo; echo "RESULT: build failure"; exit 1
fi
echo "build OK"

# Your Makefile, as a separate gate. It produces nothing graded, but `make`
# is the documented workflow and has to work.
if ! (make -C "$WORKDIR" clean >/dev/null 2>&1 &&
      make -C "$WORKDIR" all >"$TMP/make.log" 2>&1 &&
      [ -f "$WORKDIR/libuthread.a" ]); then
	echo "your Makefile did not build libuthread.a:" >&2
	cat "$TMP/make.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "Makefile OK"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -o "$CASES" \
     "$TESTDIR/cases.c" "$TESTDIR/regprobe.S" "$LIB" \
     >"$TMP/cases.log" 2>&1; then
	echo "run.sh: could not build the case driver against your library." >&2
	echo "That usually means uthread.h no longer matches the contract:" >&2
	cat "$TMP/cases.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "driver OK"
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

hung() { # <label> <case-name>
	echo "  [$1] TIMED OUT after ${TIMEOUT}s (case '$2')"
	echo "    Nothing in Part 1 blocks, so a hang is a queue that has lost"
	echo "    track of a thread: a scheduler loop that never dequeues, a"
	echo "    thread requeued while it is still running, or a stale tail"
	echo "    pointer that makes the queue circular."
}

# A case that decides its own verdict: the driver exits 0.
acase() { # <label> <case-name>
	local label=$1 name=$2 rc
	timeout -k 5 "$TIMEOUT" "$CASES" "$name" >"$TMP/out" 2>"$TMP/err"
	rc=$?
	if [ "$rc" = 0 ]; then
		record "$label" PASS
		return
	fi
	record "$label" FAIL
	{
		if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
			hung "$label" "$name"
		elif [ "$rc" -gt 128 ]; then
			echo "  [$label] killed by signal $((rc-128)) (case '$name')"
			echo "    A crash inside the thread package. The usual causes are"
			echo "    a stack pointer that is not 16-byte aligned when a"
			echo "    thread starts, an initial frame whose top word is not"
			echo "    the entry function's address, and running on a stack"
			echo "    that has already been freed."
		else
			echo "  [$label] failed (case '$name')"
		fi
		show "$TMP/err" "what the case complained about"
	} >&2
}

# A case whose verdict is a byte-for-byte diff against ../fixtures.
tcase() { # <label> <case-name>
	local label=$1 name=$2 rc
	timeout -k 5 "$TIMEOUT" "$CASES" "$name" >"$TMP/out" 2>"$TMP/err"
	rc=$?
	if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
		record "$label" FAIL
		{ hung "$label" "$name"; show "$TMP/out" "what it managed to print"; } >&2
		return
	fi
	if [ "$rc" -gt 128 ]; then
		record "$label" FAIL
		{ echo "  [$label] killed by signal $((rc-128)) (case '$name')"
		  show "$TMP/out" "what it managed to print"
		  show "$TMP/err" "stderr"; } >&2
		return
	fi
	if [ "$rc" != 0 ]; then
		record "$label" FAIL
		{ echo "  [$label] the case reported a failure of its own (case '$name')"
		  show "$TMP/err" "what the case complained about"; } >&2
		return
	fi
	if diff -u "$FIXDIR/$name.expected" "$TMP/out" >"$TMP/diff" 2>&1; then
		record "$label" PASS
		return
	fi
	record "$label" FAIL
	{
		echo "  [$label] the run order is not the one uthread.h specifies"
		echo "    (case '$name'; '-' is expected, '+' is yours)"
		show "$TMP/diff" "diff against fixtures/$name.expected"
	} >&2
}

# ---------------------------------------------------------------------------
# the transcript cases
# ---------------------------------------------------------------------------

tcase "Part 1: one thread runs and the scheduler returns"          p1_smoke
tcase "Part 1: three threads take turns in round-robin order"      p1_roundrobin
smoke_rr=$LAST_RESULT
tcase "Part 1: threads with uneven lifetimes leave the queue cleanly" p1_uneven
tcase "Part 1: a thread that returns ends like one that calls uthread_exit" p1_exitpaths
tcase "Part 1: a running thread can create more threads"           p1_nested
tcase "Part 1: twelve threads over five rounds keep their order"   p1_order
order=$LAST_RESULT

# ---------------------------------------------------------------------------
# the assertion cases
# ---------------------------------------------------------------------------

acase "Part 1: uthread_self() matches the id create() returned"    p1_self
acase "Part 1: the callee-saved registers survive a switch"        p1_regs
acase "Part 1: each thread's stack survives every other thread"    p1_stacks
stacks=$LAST_RESULT
acase "Part 1: a finished thread's slot is reused"                 p1_slots
acase "Part 1: a thread id is never reused, even when a slot is"   p1_tids
acase "Part 1: creating past UTHREAD_MAX is refused, not fatal"    p1_limit
acase "Part 1: yielding from the main context is harmless"         p1_maincontext

# ---------------------------------------------------------------------------
# valgrind
# ---------------------------------------------------------------------------
#
# memcheck first. A thread package hands the compiler stacks it made up, so
# the ways to get memory wrong here are unusual: a stack too small for the
# frames put on it, an initial frame written past the end of the allocation,
# a stack freed while a thread is still standing on it. All three are
# invisible to the cases above on a good day and fatal on a bad one.
#
# helgrind second. Part 1 has one kernel thread and therefore no data races
# to find -- and that is the point of running it: it is the baseline. If
# helgrind reports something here it is about the harness or the C library,
# and it should report nothing.

vg() { # <label> <tool-args...> -- <case>
	local label=$1; shift
	local args=() name=""
	while [ $# -gt 0 ]; do
		if [ "$1" = "--" ]; then shift; name=$1; break; fi
		args+=("$1"); shift
	done
	local rc
	timeout -k 5 "$VG_TIMEOUT" valgrind "${args[@]}" --error-exitcode=9 -q \
		"$CASES" "$name" >"$TMP/vg.out" 2>"$TMP/vg.err"
	rc=$?
	if [ "$rc" = 0 ]; then
		record "$label" PASS
	elif [ "$rc" = 9 ]; then
		record "$label" FAIL
		{ echo "  [$label] valgrind found something (case '$name')"
		  show "$TMP/vg.err" "valgrind"; } >&2
	else
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the case itself did not pass under"
		  echo "    valgrind (exit $rc), so there is no verdict about memory"
		  echo "    to give. Get '$name' passing on its own first."
		  show "$TMP/vg.err" "stderr"; } >&2
	fi
}

if ! command -v valgrind >/dev/null 2>&1; then
	record "Part 1: memcheck finds nothing while eight stacks interleave" SKIP
	record "Part 1: helgrind is clean" SKIP
	echo "  [Part 1: valgrind] SKIPPED: valgrind is not installed." >&2
	echo "    Install it (apt install valgrind). Parts 2 and 3 need helgrind," >&2
	echo "    which comes with it, and there is no substitute." >&2
else
	# Each valgrind case is gated on the case it re-runs. Running an
	# instrumented copy of a case that is already failing produces a wall
	# of consequential errors and no information.
	if [ "$stacks" = PASS ]; then
		vg "Part 1: memcheck finds nothing while eight stacks interleave" \
		   --leak-check=no -- p1_stacks
	else
		record "Part 1: memcheck finds nothing while eight stacks interleave" SKIP
		{ echo "  [Part 1: memcheck ...] SKIPPED: the case it re-runs,"
		  echo "    'each thread's stack survives every other thread', is"
		  echo "    failing on its own. Fix that first."; } >&2
	fi
	if [ "$order" = PASS ]; then
		vg "Part 1: helgrind is clean" \
		   --tool=helgrind -- p1_order
	else
		record "Part 1: helgrind is clean" SKIP
		{ echo "  [Part 1: helgrind is clean] SKIPPED: the case it re-runs,"
		  echo "    'twelve threads over five rounds keep their order', is"
		  echo "    failing on its own. Fix that first."; } >&2
	fi
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
if [ "$smoke_rr" != PASS ]; then
	echo "(Start with 'three threads take turns in round-robin order'. Almost"
	echo " every other case depends on the same three things working.)"
fi
[ "$fail" -eq 0 ]
