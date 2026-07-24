#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 5 Part 3, the bounded buffer.
#
# Compiles <workdir>/pcbuffer.c HERE, with this script's own gcc command
# line, into its own temporary directory. Your Makefile is never the graded
# build. It is still required to work: it runs as a separate gate, because
# `make` is the documented workflow.
#
# Nothing here inspects the buffer's internals. Every case goes through
# pcbuffer.h.
#
# The two failure modes are kept apart, because they come from different
# mistakes:
#
#   WRONG ANSWER -- an item came out twice, or never, or a slot was read
#       before it was written. That is a wait guarded by `if` instead of
#       `while`: the thread proceeds on a predicate that was true when it was
#       signalled and false by the time it ran.
#
#   DEADLOCK -- nothing came out at all and the case had to be killed. That is
#       a signal delivered to the wrong queue: one condition variable instead
#       of two, or a put that signals the variable producers wait on. Everyone
#       is asleep and nobody is left to wake them.
#
# This is also the script you point at a deliberately broken copy for Part 3's
# two demonstrations. Copy your working directory, break one rule, run this
# over the copy, and paste what it says into BREAKAGE.md.
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

TIMEOUT=30          # per case; the longest case takes about four seconds
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

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread'

echo "== building $WORKDIR =="

for f in pcbuffer.c pcbuffer.h; do
	[ -f "$WORKDIR/$f" ] || {
		echo "run.sh: no $f in $WORKDIR." >&2
		echo "RESULT: build failure"; exit 1; }
done

LIB="$TMP/libpcbuffer.a"
OBJ="$TMP/pcbuffer.o"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -c -o "$OBJ" "$WORKDIR/pcbuffer.c" \
     >"$TMP/build.log" 2>&1; then
	echo "build FAILED (the graded build is this script's own compiler line," >&2
	echo "not your Makefile: gcc $GRADE_CFLAGS -c pcbuffer.c):" >&2
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
      [ -f "$WORKDIR/libpcbuffer.a" ]); then
	echo "your Makefile did not build libpcbuffer.a:" >&2
	cat "$TMP/make.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "Makefile OK"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -o "$CASES" "$TESTDIR/cases.c" "$LIB" \
     -pthread >"$TMP/cases.log" 2>&1; then
	echo "run.sh: could not build the case driver against your library." >&2
	echo "That usually means pcbuffer.h no longer matches the contract:" >&2
	cat "$TMP/cases.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "driver OK"
echo

# ---------------------------------------------------------------------------
# case runner
# ---------------------------------------------------------------------------

show() { # <file> <what>
	if [ -s "$1" ]; then
		echo "    --- $2 ---" >&2
		head -20 "$1" >&2
	fi
}

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
			echo "    Not a wrong answer -- no answer. Every thread is"
			echo "    waiting and nobody is left to signal. The causes, in"
			echo "    the order they are worth checking:"
			echo "      * one condition variable doing both jobs, so a"
			echo "        wakeup meant for a producer lands on a consumer,"
			echo "        which goes straight back to sleep;"
			echo "      * pcb_put signalling the variable producers wait on"
			echo "        (or pcb_get signalling the consumers'), which is"
			echo "        the same bug with two variables present;"
			echo "      * a path out of pcb_put or pcb_get that does not"
			echo "        unlock the mutex."
		elif [ "$rc" -gt 128 ]; then
			echo "  [$label] killed by signal $((rc-128)) (case '$name')"
			echo "    A crash rather than a wrong answer. Reading slots[head]"
			echo "    when count is 0, or indexing past capacity because head"
			echo "    or tail was not wrapped, will do this."
		else
			echo "  [$label] WRONG ANSWER (case '$name')"
		fi
		show "$TMP/err" "what the case complained about"
	} >&2
}

# ---------------------------------------------------------------------------
# the cases
# ---------------------------------------------------------------------------
#
# Small and deterministic first, so that a failure in the cheap cases is not
# buried under a four-second soak.

ccase "Part 3: one producer, one consumer, every item exactly once" p3_spsc
ccase "Part 3: items come out in the order they went in"            p3_order
ccase "Part 3: a full buffer blocks the producer"                   p3_bounded
ccase "Part 3: an empty buffer blocks the consumer"                 p3_blocking_get
ccase "Part 3: four producers and four consumers, every item exactly once" p3_mpmc
mpmc=$LAST_RESULT
ccase "Part 3: capacity 1 with eight of each, every item exactly once" p3_cap1
ccase "Part 3: every wakeup path is used and none is lost"          p3_wakeups
ccase "Part 3: a million items pass through without stalling"       p3_soak

# ---------------------------------------------------------------------------
# helgrind
# ---------------------------------------------------------------------------
#
# This is the check that earns its runtime in Part 3. helgrind understands
# pthread mutexes and condition variables completely -- it intercepts every
# call -- so unlike Part 2 there is nothing here it has to take your word
# for. It sees state touched without the mutex, a wait on a mutex the thread
# does not hold, and a condition variable used with two different mutexes,
# none of which the outcome cases can see at all.

if ! command -v valgrind >/dev/null 2>&1; then
	record "Part 3: helgrind finds no race through the buffer" SKIP
	echo "  [Part 3: helgrind] SKIPPED: valgrind is not installed." >&2
	echo "    apt install valgrind. There is no substitute for this check." >&2
elif [ "$mpmc" != PASS ]; then
	record "Part 3: helgrind finds no race through the buffer" SKIP
	{ echo "  [Part 3: helgrind finds no race through the buffer] SKIPPED:"
	  echo "    the plain run of 'four producers and four consumers' is"
	  echo "    failing, so an instrumented copy would report the"
	  echo "    consequences of that and nothing useful. Fix it first."; } >&2
else
	# A fraction of the item count: helgrind is roughly 30x slower, and a
	# race it can see it sees within the first few hundred items.
	env PCB_ITEMS=400 PCB_THREADS=4 timeout -k 5 "$VG_TIMEOUT" \
		valgrind --tool=helgrind --error-exitcode=9 -q \
		--suppressions="$TESTDIR/helgrind.supp" \
		"$CASES" p3_mpmc >"$TMP/vg.out" 2>"$TMP/vg.err"
	vg_rc=$?
	if [ "$vg_rc" = 0 ]; then
		record "Part 3: helgrind finds no race through the buffer" PASS
	elif [ "$vg_rc" = 9 ]; then
		record "Part 3: helgrind finds no race through the buffer" FAIL
		{ echo "  [Part 3: helgrind finds no race through the buffer]"
		  echo "    A buffer can pass every case above and still be touching"
		  echo "    shared state outside the mutex. helgrind sees that; the"
		  echo "    cases above do not. The other thing it reports here is"
		  echo "    pthread_cond_signal called after the mutex has been"
		  echo "    released -- move the signal back inside, before the"
		  echo "    unlock, as pcbuffer.h requires."
		  show "$TMP/vg.err" "helgrind"; } >&2
	else
		record "Part 3: helgrind finds no race through the buffer" SKIP
		{ echo "  [Part 3: helgrind finds no race through the buffer] SKIPPED:"
		  echo "    the case did not complete under helgrind (exit $vg_rc), so"
		  echo "    there is no verdict about races to give."
		  show "$TMP/vg.err" "stderr"; } >&2
	fi
fi

# ---------------------------------------------------------------------------
# the breakage demonstrations
# ---------------------------------------------------------------------------
#
# "Show me the bug" cannot be auto-graded. What can be checked is that the
# report covers both demonstrations and contains something that looks like
# captured output rather than a paragraph of intent. The reasoning is marked
# against the rubric in solutions/README.md.

BRK="$WORKDIR/BREAKAGE.md"
if [ ! -f "$BRK" ]; then
	record "Part 3: BREAKAGE.md covers both demonstrations" FAIL
	{ echo "  [Part 3: BREAKAGE.md covers both demonstrations]"
	  echo "    No BREAKAGE.md in $WORKDIR. The two deliberate breakages are"
	  echo "    a fifth of Part 3: copy your working directory, break one"
	  echo "    rule in the copy, run this script over it, and write down"
	  echo "    what happened and why."; } >&2
else
	missing=""
	for w in while if signal wait; do
		grep -q -F "$w" "$BRK" || missing="$missing $w"
	done
	# The two demonstrations fail differently, and the report has to show
	# both outcomes, not describe them.
	got_wrong=0; got_dead=0
	grep -q -F "WRONG ANSWER" "$BRK" && got_wrong=1
	grep -q -F "DEADLOCK" "$BRK" && got_dead=1
	lines=$(grep -c '[0-9]' "$BRK")
	if [ -n "$missing" ]; then
		record "Part 3: BREAKAGE.md covers both demonstrations" FAIL
		{ echo "  [Part 3: BREAKAGE.md covers both demonstrations]"
		  echo "    BREAKAGE.md never mentions:$missing"; } >&2
	elif [ "$got_wrong" = 0 ] || [ "$got_dead" = 0 ]; then
		record "Part 3: BREAKAGE.md covers both demonstrations" FAIL
		{ echo "  [Part 3: BREAKAGE.md covers both demonstrations]"
		  echo "    The two breakages fail in different ways and the report"
		  echo "    has to show both: this script prints 'WRONG ANSWER' for"
		  echo "    the one and 'DEADLOCK' for the other, and neither string"
		  echo "    is in BREAKAGE.md$([ $got_wrong = 1 ] && echo ' (DEADLOCK is missing)')$([ $got_dead = 1 ] && echo ' (WRONG ANSWER is missing)')."
		  echo "    Paste the transcripts, do not paraphrase them."; } >&2
	elif [ "$lines" -lt 8 ]; then
		record "Part 3: BREAKAGE.md covers both demonstrations" FAIL
		{ echo "  [Part 3: BREAKAGE.md covers both demonstrations]"
		  echo "    BREAKAGE.md has only $lines line(s) with a number in them,"
		  echo "    which is not a pair of captured transcripts."; } >&2
	else
		record "Part 3: BREAKAGE.md covers both demonstrations" PASS
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
echo " believe it. The argument in BREAKAGE.md is marked against the rubric"
echo " in solutions/README.md, not by this script.)"
[ "$fail" -eq 0 ]
