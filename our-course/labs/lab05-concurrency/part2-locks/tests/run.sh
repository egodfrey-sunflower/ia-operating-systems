#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 5 Part 2, the three locks.
#
# Compiles <workdir>/mylock.c HERE, with this script's own gcc command line,
# into this script's own temporary directory. Your Makefile is never the
# graded build, so a Makefile that quietly drops -Werror or adds -w cannot
# change the grade. Your Makefile is still required to work: it runs as a
# separate gate, because `make` is the documented workflow.
#
# Nothing here inspects a lock's internals. Every case goes through mylock.h.
#
# ---------------------------------------------------------------------------
# WHAT A GREEN RUN IS AND IS NOT EVIDENCE OF
#
# Every case here is probabilistic. A lock built from plain loads and stores
# can pass all of them on a quiet machine; a correct lock can fail one on a
# machine that is thrashing. The cases are built to make the wrong answer
# likely rather than possible -- many iterations, several repeats, a critical
# section short enough for two threads to collide inside it -- and the first
# case run is a NEGATIVE CONTROL that checks the machine can still lose an
# update at all. If the control fails, nothing after it means anything, and
# it says so.
#
# helgrind is required and is not a formality, but be clear about what it
# buys. It cannot audit a hand-built lock: the annotations in hbnotate.h tell
# it what your lock claims to do and it believes them, so a lock made of
# plain loads and stores passes helgrind cleanly. What it finds -- and what
# nothing else in Part 2 finds -- is shared state you left outside a critical
# section altogether. Both halves of that sentence are in the handout too.
# ---------------------------------------------------------------------------
#
# Every case runs in its own process under its own timeout, and a timeout is
# reported as a DEADLOCK, distinctly from a wrong answer, because the two
# have completely different causes.
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

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)
trap 'rm -rf "$TMP"' EXIT

CASES="$TMP/cases"

TIMEOUT=20          # per case; the longest case takes about half a second
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
#
# -O2 is part of the specification for this part, not a preference. An
# unbarriered spin loop is a loop the compiler is entitled to hoist the load
# out of, and at -O0 you would never find out.

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread'

# helgrind's annotation macros need <valgrind/helgrind.h>. Without it,
# hbnotate.h compiles them away and the helgrind cases skip -- annotation-free
# helgrind over a hand-built lock reports thousands of false races and no
# true ones.
HAVE_HG=1
if ! echo '#include <valgrind/helgrind.h>' | gcc -E -x c - >/dev/null 2>&1; then
	HAVE_HG=0
	GRADE_CFLAGS="$GRADE_CFLAGS -DMYLOCK_NO_VALGRIND"
fi

echo "== building $WORKDIR =="

for f in mylock.c mylock.h hbnotate.h; do
	[ -f "$WORKDIR/$f" ] || {
		echo "run.sh: no $f in $WORKDIR." >&2
		echo "RESULT: build failure"; exit 1; }
done

LIB="$TMP/libmylock.a"
OBJ="$TMP/mylock.o"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -c -o "$OBJ" "$WORKDIR/mylock.c" \
     >"$TMP/build.log" 2>&1; then
	echo "build FAILED (the graded build is this script's own compiler line," >&2
	echo "not your Makefile: gcc $GRADE_CFLAGS -c mylock.c):" >&2
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

if ! (make -C "$WORKDIR" clean >/dev/null 2>&1 &&
      make -C "$WORKDIR" all >"$TMP/make.log" 2>&1 &&
      [ -f "$WORKDIR/libmylock.a" ]); then
	echo "your Makefile did not build libmylock.a:" >&2
	cat "$TMP/make.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "Makefile OK"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -o "$CASES" "$TESTDIR/cases.c" "$LIB" \
     -pthread >"$TMP/cases.log" 2>&1; then
	echo "run.sh: could not build the case driver against your library." >&2
	echo "That usually means mylock.h no longer matches the contract:" >&2
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

# A case that must pass. The two failure modes are kept apart on purpose:
#
#   exit 1  -- the answer was wrong. Threads got in each other's way; the
#              lock does not exclude.
#   timeout -- nobody finished. Threads are waiting for each other or for a
#              wakeup that never came; the lock does not release, or a
#              sleeper was never woken.
#
# They look the same on a results line and they are not the same bug, so the
# diagnostic says which.
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
			echo "  [$label] DEADLOCK: no progress in ${TIMEOUT}s (case '$name')"
			echo "    This is not a wrong answer, it is no answer: some"
			echo "    thread is waiting for something that will not happen."
			echo "    In this part the causes are a release path that does"
			echo "    not actually free the lock, an acquire that spins on a"
			echo "    value the compiler hoisted out of the loop, and a"
			echo "    sleeping lock that parks a waiter without leaving a"
			echo "    mark for the releaser to see, so the wake never comes."
		elif [ "$rc" -gt 128 ]; then
			echo "  [$label] killed by signal $((rc-128)) (case '$name')"
		else
			echo "  [$label] wrong answer (case '$name')"
		fi
		show "$TMP/err" "what the case complained about"
	} >&2
}

# ---------------------------------------------------------------------------
# the control
# ---------------------------------------------------------------------------
#
# Run first, because if this fails everything after it is theatre.

ccase "Part 2: an unsynchronised counter really does lose updates" p2_race_control
control=$LAST_RESULT
if [ "$control" != PASS ]; then
	echo "  ^^ the control failed. Every case below can still be read as a" >&2
	echo "     failure -- a lock that hangs still hangs -- but a PASS below" >&2
	echo "     this line is not evidence that anything works." >&2
fi

# ---------------------------------------------------------------------------
# outcome
# ---------------------------------------------------------------------------

ccase "Part 2: the spin lock leaves the counter at exactly N x M"      p2_spin_counter
spin_ok=$LAST_RESULT
ccase "Part 2: the ticket lock leaves the counter at exactly N x M"    p2_ticket_counter
ticket_ok=$LAST_RESULT
ccase "Part 2: the sleeping lock leaves the counter at exactly N x M"  p2_sleep_counter
sleep_ok=$LAST_RESULT

ccase "Part 2: no two threads are inside the spin lock at once"        p2_spin_excl
ccase "Part 2: no two threads are inside the ticket lock at once"      p2_ticket_excl
ccase "Part 2: no two threads are inside the sleeping lock at once"    p2_sleep_excl

ccase "Part 2: two spin locks are two locks"                           p2_spin_two
ccase "Part 2: two ticket locks are two locks"                         p2_ticket_two
ccase "Part 2: two sleeping locks are two locks"                       p2_sleep_two

# ---------------------------------------------------------------------------
# the properties that tell the three locks apart
# ---------------------------------------------------------------------------
#
# Without these, one lock submitted three times scores full marks: everything
# above this line asks only for correct mutual exclusion, and all three locks
# provide that. One case per lock, each checking the property that makes that
# lock the lock it is -- arrival order for the ticket lock, and where the
# waiters spend the wait for the other two.

ccase "Part 2: the ticket lock serves threads in arrival order"        p2_ticket_fifo
ccase "Part 2: the sleeping lock's waiters burn no processor time"     p2_sleep_parks
ccase "Part 2: the spin lock's waiters spin"                           p2_spin_burns

# ---------------------------------------------------------------------------
# helgrind
# ---------------------------------------------------------------------------

hgcase() { # <label> <case-name> <gate>
	local label=$1 name=$2 gate=$3 rc
	if [ "$HAVE_HG" = 0 ]; then
		record "$label" SKIP
		return
	fi
	if [ "$gate" != PASS ]; then
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the plain run of '$name' is failing, so"
		  echo "    an instrumented copy of it would report the consequences"
		  echo "    of that and nothing useful. Fix the plain run first."; } >&2
		return
	fi
	# Far less work than the plain run: helgrind is about 30x slower, and a
	# race it can see it sees in the first hundred iterations.
	env MYLOCK_ITERS=300 MYLOCK_REPS=1 MYLOCK_THREADS=4 \
		timeout -k 5 "$VG_TIMEOUT" valgrind --tool=helgrind \
		--error-exitcode=9 -q "$CASES" "$name" \
		>"$TMP/vg.out" 2>"$TMP/vg.err"
	rc=$?
	if [ "$rc" = 0 ]; then
		record "$label" PASS
	elif [ "$rc" = 9 ]; then
		record "$label" FAIL
		{ echo "  [$label] helgrind found a data race (case '$name')"
		  echo "    Either something shared is being touched outside the"
		  echo "    critical section, or the annotations in hbnotate.h are"
		  echo "    not where hbnotate.h says to put them: HG_ACQUIRED where"
		  echo "    acquire is about to return, HG_RELEASING immediately"
		  echo "    BEFORE the store that frees the lock, HG_INIT in init."
		  show "$TMP/vg.err" "helgrind"; } >&2
	else
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the case did not complete under helgrind"
		  echo "    (exit $rc), so there is no verdict about races to give."
		  show "$TMP/vg.err" "stderr"; } >&2
	fi
}

if ! command -v valgrind >/dev/null 2>&1; then
	record "Part 2: helgrind is clean around the spin lock" SKIP
	record "Part 2: helgrind is clean around the ticket lock" SKIP
	record "Part 2: helgrind is clean around the sleeping lock" SKIP
	echo "  [Part 2: helgrind] SKIPPED: valgrind is not installed." >&2
	echo "    apt install valgrind. There is no substitute for this check." >&2
elif [ "$HAVE_HG" = 0 ]; then
	record "Part 2: helgrind is clean around the spin lock" SKIP
	record "Part 2: helgrind is clean around the ticket lock" SKIP
	record "Part 2: helgrind is clean around the sleeping lock" SKIP
	{ echo "  [Part 2: helgrind] SKIPPED: <valgrind/helgrind.h> is not"
	  echo "    installed, so hbnotate.h's annotations compile to nothing and"
	  echo "    helgrind would report a false race on every byte your locks"
	  echo "    protect. Install the valgrind development headers"
	  echo "    (apt install valgrind, which ships them)."; } >&2
else
	hgcase "Part 2: helgrind is clean around the spin lock"     p2_spin_counter   "$spin_ok"
	hgcase "Part 2: helgrind is clean around the ticket lock"   p2_ticket_counter "$ticket_ok"
	hgcase "Part 2: helgrind is clean around the sleeping lock" p2_sleep_counter  "$sleep_ok"
fi

# ---------------------------------------------------------------------------
# the report
# ---------------------------------------------------------------------------
#
# Its numbers are yours and are marked against the rubric in
# solutions/README.md. What this script can tell is whether the measurements
# were made at all.

LOCKS="$WORKDIR/LOCKS.md"
if [ ! -f "$LOCKS" ]; then
	record "Part 2: LOCKS.md reports all three locks under contention" FAIL
	{ echo "  [Part 2: LOCKS.md reports all three locks under contention]"
	  echo "    No LOCKS.md in $WORKDIR. Half of Part 2 is the measurement:"
	  echo "    run 'make bench', then write down what the shape means."; } >&2
else
	missing=""
	for w in spin ticket sleep; do
		grep -q -F "$w" "$LOCKS" || missing="$missing $w"
	done
	numbered=$(grep -c '[0-9]' "$LOCKS")
	if [ -n "$missing" ]; then
		record "Part 2: LOCKS.md reports all three locks under contention" FAIL
		{ echo "  [Part 2: LOCKS.md reports all three locks under contention]"
		  echo "    LOCKS.md never mentions:$missing"; } >&2
	elif [ "$numbered" -lt 12 ]; then
		record "Part 2: LOCKS.md reports all three locks under contention" FAIL
		{ echo "  [Part 2: LOCKS.md reports all three locks under contention]"
		  echo "    LOCKS.md has only $numbered line(s) with a number in them."
		  echo "    'make bench' produces twelve rows -- three locks at four"
		  echo "    thread counts. The harness cannot mark your numbers, but"
		  echo "    it can tell they are not there."; } >&2
	else
		record "Part 2: LOCKS.md reports all three locks under contention" PASS
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
echo "(These cases are probabilistic. Run the suite several times before you"
echo " believe it, and read what a green run does and does not prove in the"
echo " handout. The numbers in LOCKS.md are marked against the rubric in"
echo " solutions/README.md, not by this script.)"
[ "$fail" -eq 0 ]
