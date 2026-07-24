#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 5 Part 4, the synchronisation toolkit.
#
# Compiles <workdir>/toolkit.c HERE, with this script's own gcc command line,
# into its own temporary directory. Your Makefile is never the graded build.
# It is still required to work: it runs as a separate gate, because `make` is
# the documented workflow.
#
# Nothing here inspects the internals of any of the five structures. Every
# case goes through toolkit.h.
#
# Three failure modes, kept apart because they come from different mistakes:
#
#   WRONG ANSWER -- a count is off, two threads were inside a semaphore that
#       admits one, a barrier let a thread out early, a fork was in two
#       hands. The usual cause is a msem_wait that proceeds on a value it
#       read before it was woken: `while`, not `if`.
#
#   DEADLOCK -- the case had to be killed. Every thread is waiting for
#       something no running thread will do: five philosophers each holding
#       one fork, a rendezvous that waits before it posts, a barrier round
#       whose gate nobody opened, a turnstile nobody left.
#
#   A STARVED WRITER -- neither of those. The writer never runs, the readers
#       run for ever, and the process is not deadlocked and never will be.
#       That case measures how long the writer waited and says so.
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

for f in toolkit.c toolkit.h; do
	[ -f "$WORKDIR/$f" ] || {
		echo "run.sh: no $f in $WORKDIR." >&2
		echo "RESULT: build failure"; exit 1; }
done

LIB="$TMP/libtoolkit.a"
OBJ="$TMP/toolkit.o"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -c -o "$OBJ" "$WORKDIR/toolkit.c" \
     >"$TMP/build.log" 2>&1; then
	echo "build FAILED (the graded build is this script's own compiler line," >&2
	echo "not your Makefile: gcc $GRADE_CFLAGS -c toolkit.c):" >&2
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
      [ -f "$WORKDIR/libtoolkit.a" ]); then
	echo "your Makefile did not build libtoolkit.a:" >&2
	cat "$TMP/make.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "Makefile OK"

if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -o "$CASES" "$TESTDIR/cases.c" "$LIB" \
     -pthread >"$TMP/cases.log" 2>&1; then
	echo "run.sh: could not build the case driver against your library." >&2
	echo "That usually means toolkit.h no longer matches the contract:" >&2
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
			echo "    waiting and nobody is left to wake them. In this"
			echo "    part, in the order they are worth checking:"
			echo "      * philosophers: five hands each holding one"
			echo "        fork and waiting for the next, which is the"
			echo "        circular wait the solution has to break;"
			echo "      * rendezvous: waiting for the other side before"
			echo "        announcing your own arrival;"
			echo "      * barrier: a gate that nobody opens, or a"
			echo "        turnstile a thread waits on without posting"
			echo "        it again on the way through;"
			echo "      * reader-writer: a path that takes the guard"
			echo "        semaphore and returns without posting it."
		elif [ "$rc" -gt 128 ]; then
			echo "  [$label] killed by signal $((rc-128)) (case '$name')"
			echo "    A crash rather than a wrong answer. Indexing"
			echo "    fork[] past n, or using a semaphore that was"
			echo "    never initialised, will do this."
		else
			echo "  [$label] WRONG ANSWER (case '$name')"
		fi
		show "$TMP/err" "what the case complained about"
	} >&2
}

# ---------------------------------------------------------------------------
# the control
# ---------------------------------------------------------------------------
#
# Run first, because if this fails everything after it is theatre. Part 2's
# control, with a semaphore in place of a lock.

ccase "Part 4: an unsynchronised counter really does lose updates"     p4_race_control
if [ "$LAST_RESULT" != PASS ]; then
	echo "  ^^ the control failed. Every case below can still be read as a" >&2
	echo "     failure -- a toolkit that hangs still hangs -- but a PASS" >&2
	echo "     below this line is not evidence that anything works." >&2
fi

# ---------------------------------------------------------------------------
# the semaphore
# ---------------------------------------------------------------------------
#
# First, and on its own, because everything below it is built on it. A
# failure here makes every case after it unreadable.

ccase "Part 4: a semaphore at 1 leaves the counter at exactly N x M"   p4_sem_mutex
sem_ok=$LAST_RESULT
ccase "Part 4: a semaphore at 3 admits three and no more"              p4_sem_room
room_ok=$LAST_RESULT
ccase "Part 4: a wait on a zero semaphore blocks"                      p4_sem_blocks
ccase "Part 4: a thread waiting on a semaphore burns no processor time" p4_sem_parks
parks_ok=$LAST_RESULT

if [ "$sem_ok" != PASS ]; then
	echo "  ^^ the semaphore itself is failing. The barrier, the" >&2
	echo "     rendezvous, the reader-writer lock and the philosophers are" >&2
	echo "     all built on it, so read their failures below as" >&2
	echo "     consequences until this one passes." >&2
fi

# ---------------------------------------------------------------------------
# the structures built on it
# ---------------------------------------------------------------------------

ccase "Part 4: nobody leaves the barrier until everybody has arrived"  p4_barrier_gate
ccase "Part 4: the barrier works two hundred rounds running"           p4_barrier_rounds
rounds_ok=$LAST_RESULT
ccase "Part 4: the barrier works at 1, 2, 3 and 32 threads"            p4_barrier_sizes
ccase "Part 4: neither side leaves the rendezvous alone"               p4_rendezvous

ccase "Part 4: readers hold the read lock together"                    p4_rw_shared
ccase "Part 4: a writer excludes readers and other writers"            p4_rw_excl
excl_ok=$LAST_RESULT
ccase "Part 4: a waiting writer is not overtaken for ever"             p4_rw_nostarve

ccase "Part 4: five philosophers eat without deadlocking"              p4_phil
ccase "Part 4: non-neighbouring philosophers eat at the same time"     p4_phil_parallel
ccase "Part 4: the table works at two philosophers and at seven"       p4_phil_sizes

# ---------------------------------------------------------------------------
# helgrind
# ---------------------------------------------------------------------------
#
# Worth its runtime here for the same reason as in Part 3 and not for the
# reason it is limited in Part 2: the semaphore is built on a pthreads mutex
# and condition variable, which helgrind intercepts and models exactly. It
# sees a post sent after the unlock, a wait on a mutex the thread does not
# hold, and state touched outside the critical section -- none of which the
# outcome cases above can see.
#
# Three runs, because they instrument different code: the semaphore on its
# own; the barrier, whose arrival count is the easiest field in the part to
# touch without its guard; and the reader-writer lock, which has the most
# state of its own outside the semaphore layer. An unguarded `arrived++`
# passes every outcome case in this suite and helgrind fails it in a second.

if command -v valgrind >/dev/null 2>&1; then HAVE_HG=1; else HAVE_HG=0; fi

hgcase() { # <label> <case-name> <gate> [VAR=VALUE ...]
	local label=$1 name=$2 gate=$3; shift 3
	local rc
	# A spinning msem_wait is functionally correct, so the plain cases
	# pass and the gate above would let helgrind run -- and instrumenting
	# a program that spins costs minutes per case rather than seconds.
	# Fix the spin first; the verdict about races is worth nothing until
	# then anyway.
	if [ "$parks_ok" != PASS ]; then
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: 'a thread waiting on a semaphore"
		  echo "    burns no processor time' is failing, so msem_wait"
		  echo "    is spinning. Under helgrind a spin costs minutes"
		  echo "    per case. Fix the wait first."; } >&2
		return
	fi
	if [ "$HAVE_HG" = 0 ]; then
		record "$label" SKIP
		echo "  [$label] SKIPPED: valgrind is not installed." >&2
		echo "    apt install valgrind. There is no substitute." >&2
		return
	fi
	if [ "$gate" != PASS ]; then
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the plain run of '$name' is failing,"
		  echo "    so an instrumented copy would report the"
		  echo "    consequences of that and nothing useful."; } >&2
		return
	fi
	# Far less work than the plain run: helgrind is about 30x slower, and
	# a race it can see it sees in the first few hundred iterations.
	env "$@" timeout -k 5 "$VG_TIMEOUT" valgrind --tool=helgrind \
		--error-exitcode=9 -q \
		--suppressions="$TESTDIR/helgrind.supp" \
		"$CASES" "$name" >"$TMP/vg.out" 2>"$TMP/vg.err"
	rc=$?
	if [ "$rc" = 0 ]; then
		record "$label" PASS
	elif [ "$rc" = 9 ]; then
		record "$label" FAIL
		{ echo "  [$label] helgrind found a race (case '$name')"
		  echo "    A toolkit can pass every case above and still touch"
		  echo "    shared state outside the mutex. The two things it"
		  echo "    reports here are a pthread_cond_signal sent after"
		  echo "    the unlock -- move it back inside, as toolkit.h"
		  echo "    requires -- and a field of one of the structures"
		  echo "    read or written outside the semaphore that guards"
		  echo "    it."
		  show "$TMP/vg.err" "helgrind"; } >&2
	elif [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
		# The only honest no-verdict: helgrind is about thirty times
		# slower and a correct-but-slow toolkit should not be failed for
		# the instrument's overhead.
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the case timed out under helgrind"
		  echo "    (about thirty times slower than a plain run), so"
		  echo "    there is no verdict to give. Re-run on a quieter"
		  echo "    machine before believing anything about this case."
		  show "$TMP/vg.err" "stderr"; } >&2
	else
		# Anything else -- the case failing its own checks (exit 1), an
		# assert, a crash -- means the instrumented run produced a
		# result and the result was bad. Recording that as "no verdict"
		# would make this case greenest against the worst toolkits,
		# which is the one thing a test must never do.
		record "$label" FAIL
		{ echo "  [$label] the instrumented run of '$name' failed"
		  echo "    (exit $rc). Under helgrind everything is slower and"
		  echo "    every window is wider, so a toolkit that passes the"
		  echo "    plain run and fails here is one whose correctness"
		  echo "    depended on the timing rather than on the"
		  echo "    synchronisation. This is a result, not a missing one."
		  show "$TMP/vg.out" "the case"
		  show "$TMP/vg.err" "stderr"; } >&2
	fi
}

hgcase "Part 4: helgrind finds no race in the semaphore" \
	p4_sem_mutex "$sem_ok" TK_ITERS=2000 TK_THREADS=4
hgcase "Part 4: helgrind finds no race in the barrier" \
	p4_barrier_rounds "$rounds_ok" TK_ITERS=20
hgcase "Part 4: helgrind finds no race in the reader-writer lock" \
	p4_rw_excl "$excl_ok" TK_ITERS=50

# ---------------------------------------------------------------------------
# DEADLOCK.md
# ---------------------------------------------------------------------------
#
# "Which condition does your solution break" cannot be auto-graded: the
# answer is an argument. What can be checked is that the report names the
# four conditions and covers both structures it is asked about. The
# reasoning is marked against the rubric in solutions/README.md.

DL="$WORKDIR/DEADLOCK.md"
if [ ! -f "$DL" ]; then
	record "Part 4: DEADLOCK.md names the conditions each solution breaks" FAIL
	{ echo "  [Part 4: DEADLOCK.md names the conditions each solution breaks]"
	  echo "    No DEADLOCK.md in $WORKDIR. It is a fifth of Part 4: the"
	  echo "    four necessary conditions, which one your philosophers"
	  echo "    break and how, and the same question asked of your"
	  echo "    reader-writer lock."; } >&2
else
	missing=""
	for w in "mutual exclusion" "hold" "preempt" "circular" \
	         "philosoph" "reader"; do
		grep -q -i -F "$w" "$DL" || missing="$missing '$w'"
	done
	lines=$(grep -c . "$DL")
	if [ -n "$missing" ]; then
		record "Part 4: DEADLOCK.md names the conditions each solution breaks" FAIL
		{ echo "  [Part 4: DEADLOCK.md names the conditions each solution breaks]"
		  echo "    DEADLOCK.md never mentions:$missing"
		  echo "    The four conditions have names and the report has to"
		  echo "    use them, because 'it cannot deadlock because the"
		  echo "    footman stops it' is the claim, not the argument."; } >&2
	elif [ "$lines" -lt 20 ]; then
		record "Part 4: DEADLOCK.md names the conditions each solution breaks" FAIL
		{ echo "  [Part 4: DEADLOCK.md names the conditions each solution breaks]"
		  echo "    DEADLOCK.md is $lines non-blank lines, which is not"
		  echo "    an argument about four conditions and two"
		  echo "    structures."; } >&2
	else
		record "Part 4: DEADLOCK.md names the conditions each solution breaks" PASS
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
echo " believe it. The argument in DEADLOCK.md is marked against the rubric"
echo " in solutions/README.md, not by this script.)"
[ "$fail" -eq 0 ]
