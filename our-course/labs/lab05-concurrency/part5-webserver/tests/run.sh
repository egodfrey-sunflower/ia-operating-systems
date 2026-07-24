#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 5 Part 5, the two web servers.
#
# Compiles <workdir>/wsthread.c and <workdir>/wsevent.c HERE, with this
# script's own gcc command lines, into its own temporary directory. Your
# Makefile is never the graded build. It is still required to work, and to
# produce BOTH servers, because "twice" is the assignment.
#
# Nothing here reads your source. Every case is a real HTTP client on a real
# socket, and every check is on what came back and how long it took.
#
# THE SERVERS ARE STARTED ON PORT 0 -- any free port -- and found by reading
# the "listening on <port>" line they print. A test suite that hardcodes a
# port fails mysteriously whenever anything else on the machine happens to be
# using it, and the failure looks like a bug in the code under test.
#
# The document root is built by tests/mkwww.sh into a temporary directory, so
# the fixtures the server is asked for are the harness's, not whatever is in
# your copy of www/. A file called secret.txt is placed OUTSIDE it, which is
# what the path-traversal case goes looking for.
#
# Two cases are architecture-specific, and they are the point of the part:
#
#   'a stalled client blocks the pool'  runs against webserver-threaded ONLY
#       and requires head-of-line blocking to happen. With more stalled
#       clients than the pool has workers, there is nobody left to serve
#       anyone with, and a submission where this does not happen is not a
#       bounded thread pool.
#
#   'a stalled client blocks nobody'    runs against webserver-event ONLY and
#       requires the opposite. A connection that is not finished arriving is
#       a slot in a table, and it costs the others nothing.
#
# Between them they make it impossible to submit one server twice.
#
# Exits 0 only if every case passes. No root needed. No fixed ports.

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

SRV_PID=
stop_server() {
	local i
	[ -n "$SRV_PID" ] || return 0
	kill "$SRV_PID" 2>/dev/null
	for i in 1 2 3 4 5 6 7 8 9 10; do
		kill -0 "$SRV_PID" 2>/dev/null || break
		sleep 0.1
	done
	kill -9 "$SRV_PID" 2>/dev/null
	wait "$SRV_PID" 2>/dev/null
	SRV_PID=
}
cleanup() { stop_server; rm -rf "$TMP"; }
trap cleanup EXIT INT TERM

CASES="$TMP/cases"
WWW="$TMP/www"
FDLIMIT=64          # the descriptor limit the servers run under
CASE_TIMEOUT=60     # per case
VG_TIMEOUT=300

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

for f in wsthread.c wsevent.c http.c http.h pcbuffer.c pcbuffer.h; do
	[ -f "$WORKDIR/$f" ] || {
		echo "run.sh: no $f in $WORKDIR." >&2
		echo "RESULT: build failure"; exit 1; }
done

build_one() { # <output> <sources...>
	local out=$1; shift
	if ! gcc $GRADE_CFLAGS -I"$WORKDIR" -o "$out" "$@" \
	     >"$TMP/build.log" 2>&1; then
		echo "build FAILED (the graded build is this script's own gcc" >&2
		echo "line, not your Makefile):" >&2
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
}

build_one "$TMP/webserver-threaded" "$WORKDIR/wsthread.c" "$WORKDIR/http.c" \
	"$WORKDIR/pcbuffer.c"
build_one "$TMP/webserver-event" "$WORKDIR/wsevent.c" "$WORKDIR/http.c"
echo "build OK"

if ! (make -C "$WORKDIR" clean >/dev/null 2>&1 &&
      make -C "$WORKDIR" all >"$TMP/make.log" 2>&1 &&
      [ -x "$WORKDIR/webserver-threaded" ] &&
      [ -x "$WORKDIR/webserver-event" ]); then
	echo "your Makefile did not build both webserver-threaded and" >&2
	echo "webserver-event:" >&2
	cat "$TMP/make.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "Makefile OK"

if ! gcc $GRADE_CFLAGS -o "$CASES" "$TESTDIR/cases.c" >"$TMP/cases.log" 2>&1; then
	echo "run.sh: could not build its own case driver:" >&2
	cat "$TMP/cases.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
if ! gcc $GRADE_CFLAGS -o "$TMP/loadgen" "$TESTDIR/loadgen.c" \
     >"$TMP/loadgen.log" 2>&1; then
	echo "run.sh: could not build its own load generator:" >&2
	cat "$TMP/loadgen.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
echo "driver OK"

if ! "$TESTDIR/mkwww.sh" "$WWW" >"$TMP/www.log" 2>&1; then
	echo "run.sh: could not build the fixture document root:" >&2
	cat "$TMP/www.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
printf 'SECRET-CONTENT: this file is outside the document root.\n' \
	> "$TMP/secret.txt"
echo "fixtures OK"
echo

# ---------------------------------------------------------------------------
# starting and stopping a server
# ---------------------------------------------------------------------------

PORT=
start_server() { # <binary> [valgrind args...]
	local bin=$1; shift
	local i
	: > "$TMP/srv.out"
	: > "$TMP/srv.err"
	PORT=
	if [ $# -gt 0 ]; then
		# Under valgrind, and without the descriptor limit: valgrind
		# wants descriptors of its own and the leak case does not run
		# here.
		"$@" "$bin" "$WWW" 0 >"$TMP/srv.out" 2>"$TMP/srv.err" &
	else
		( ulimit -n "$FDLIMIT"; exec "$bin" "$WWW" 0 ) \
			>"$TMP/srv.out" 2>"$TMP/srv.err" &
	fi
	SRV_PID=$!
	for i in $(seq 1 200); do
		PORT=$(awk '/^listening on /{print $3; exit}' "$TMP/srv.out")
		[ -n "$PORT" ] && return 0
		kill -0 "$SRV_PID" 2>/dev/null || break
		sleep 0.05
	done
	return 1
}

show() { # <file> <what>
	if [ -s "$1" ]; then
		echo "    --- $2 ---" >&2
		head -20 "$1" >&2
	fi
}

# A case against the server that is currently running.
ccase() { # <label> <case-name>
	local label=$1 name=$2 rc
	if [ -z "$PORT" ]; then
		record "$label" FAIL
		echo "  [$label] no server: it never printed its port" >&2
		return
	fi
	timeout -k 5 "$CASE_TIMEOUT" "$CASES" "$name" "$PORT" "$WWW" \
		>"$TMP/out" 2>"$TMP/err"
	rc=$?
	if [ "$rc" = 0 ]; then
		record "$label" PASS
		return
	fi
	record "$label" FAIL
	{
		if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
			echo "  [$label] TIMED OUT after ${CASE_TIMEOUT}s (case '$name')"
			echo "    The case did not finish. A server that accepts"
			echo "    a connection and never answers it looks like"
			echo "    this: the request went into the queue and no"
			echo "    worker took it out, or the event loop is"
			echo "    waiting to read from a connection it has"
			echo "    already read the whole request from."
		elif [ "$rc" -gt 128 ]; then
			echo "  [$label] the case was killed by signal $((rc-128))"
		else
			echo "  [$label] FAILED (case '$name')"
		fi
		show "$TMP/err" "what the case complained about"
		show "$TMP/srv.err" "the server's stderr"
	} >&2
}

# The whole common set, against whichever server is running.
#
# The two long load cases are gated on the plain fetch, for the same reason
# the valgrind runs are gated on the cases they instrument: against a server
# that cannot answer one request, a 200-request soak reports the consequences
# of that and nothing useful -- and it does it by waiting for hundreds of
# timeouts, which turns a broken submission's suite into a twenty-minute one.
FETCH_OK=FAIL
T_FETCH=FAIL
E_FETCH=FAIL
common_cases() { # <label-prefix>
	local p=$1
	ccase "Part 5 ($p): every fixture comes back byte for byte"   p5_fetch
	FETCH_OK=$LAST_RESULT
	if [ "$p" = threaded ]; then T_FETCH=$FETCH_OK; else E_FETCH=$FETCH_OK; fi
	ccase "Part 5 ($p): a slow-reading client still gets every byte" p5_drip
	ccase "Part 5 ($p): 404, 400 and 501 are what they should be" p5_status
	ccase "Part 5 ($p): a path with .. does not escape the root"  p5_traversal
	ccase_sampling "Part 5 ($p): a stalled client does not delay another" p5_stalled
	if [ "$FETCH_OK" != PASS ]; then
		for c in "200 concurrent requests all come back right" \
			 "150 connections under a limit of $FDLIMIT descriptors"; do
			record "Part 5 ($p): $c" SKIP
		done
		{ echo "  [Part 5 ($p)] SKIPPED the two long load cases: a"
		  echo "    single request is not being answered correctly, so"
		  echo "    350 more of them would report the consequences of"
		  echo "    that and nothing useful -- and would do it by"
		  echo "    waiting for hundreds of timeouts. Get one fixture"
		  echo "    back byte for byte first."; } >&2
		return
	fi
	ccase "Part 5 ($p): 200 concurrent requests all come back right" p5_soak
	ccase "Part 5 ($p): 150 connections under a limit of $FDLIMIT descriptors" p5_fdsoak
}

# A case, plus the largest thread count seen in /proc/<pid>/status while it
# runs. The thread count is the one structural check in the part -- "one
# thread" is not a style preference in an event loop, it is the claim -- and
# it is sampled during a case that is being run anyway rather than under a
# load generator of its own. That is not only cheaper: a run of the load
# generator opens several thousand connections, and a suite run repeatedly
# leaves tens of thousands of sockets in TIME_WAIT, at which point the
# HARNESS starts failing to connect and reports it as the server's fault.
# Measured: at several thousand connections per run, seven whole-suite runs
# in twenty fail that way.
THREADS_SEEN=0
ccase_sampling() { # <label> <case-name>
	local label=$1 name=$2 rc best=0 n cpid
	if [ -z "$PORT" ]; then
		record "$label" FAIL
		echo "  [$label] no server: it never printed its port" >&2
		return
	fi
	timeout -k 5 "$CASE_TIMEOUT" "$CASES" "$name" "$PORT" "$WWW" \
		>"$TMP/out" 2>"$TMP/err" &
	cpid=$!
	while kill -0 "$cpid" 2>/dev/null; do
		n=$(awk '/^Threads:/{print $2; exit}' \
			"/proc/$SRV_PID/status" 2>/dev/null)
		[ -n "$n" ] && [ "$n" -gt "$best" ] && best=$n
		sleep 0.1
	done
	wait "$cpid"
	rc=$?
	THREADS_SEEN=$best
	if [ "$rc" = 0 ]; then
		record "$label" PASS
		return
	fi
	record "$label" FAIL
	{ echo "  [$label] FAILED (case '$name')"
	  show "$TMP/err" "what the case complained about"
	  show "$TMP/srv.err" "the server's stderr"; } >&2
}

# ---------------------------------------------------------------------------
# the threaded server
# ---------------------------------------------------------------------------

echo "== webserver-threaded =="
if ! start_server "$TMP/webserver-threaded"; then
	echo "  the threaded server never printed 'listening on <port>'." >&2
	show "$TMP/srv.err" "its stderr"
	echo "  That line is the contract: http_announce(bound) after" >&2
	echo "  http_listen succeeds, and before serving anything." >&2
	for c in "every fixture comes back byte for byte" \
		 "a slow-reading client still gets every byte" \
		 "404, 400 and 501 are what they should be" \
		 "a path with .. does not escape the root" \
		 "a stalled client does not delay another" \
		 "200 concurrent requests all come back right" \
		 "150 connections under a limit of $FDLIMIT descriptors" \
		 "more than one thread is doing the serving" \
		 "a stalled client blocks the pool"; do
		record "Part 5 (threaded): $c" FAIL
	done
else
	common_cases threaded

	n=$THREADS_SEEN
	if [ "$n" -gt 1 ]; then
		record "Part 5 (threaded): more than one thread is doing the serving" PASS
	else
		record "Part 5 (threaded): more than one thread is doing the serving" FAIL
		{ echo "  [Part 5 (threaded): more than one thread is doing the serving]"
		  echo "    /proc/<pid>/status said Threads: $n while the server"
		  echo "    was serving the stalled-client case. A thread pool"
		  echo "    of WS_POOL_THREADS"
		  echo "    workers plus the accept loop is at least two."; } >&2
	fi

	# The architecture case. Read the comment at the top of this file
	# before deciding this one is unfair.
	ccase "Part 5 (threaded): a stalled client blocks the pool" p5_hol_blocks
fi
stop_server

# ---------------------------------------------------------------------------
# the event server
# ---------------------------------------------------------------------------

echo "== webserver-event =="
if ! start_server "$TMP/webserver-event"; then
	echo "  the event server never printed 'listening on <port>'." >&2
	show "$TMP/srv.err" "its stderr"
	for c in "every fixture comes back byte for byte" \
		 "a slow-reading client still gets every byte" \
		 "404, 400 and 501 are what they should be" \
		 "a path with .. does not escape the root" \
		 "a stalled client does not delay another" \
		 "200 concurrent requests all come back right" \
		 "150 connections under a limit of $FDLIMIT descriptors" \
		 "exactly one thread is doing the serving" \
		 "a stalled client blocks nobody"; do
		record "Part 5 (event): $c" FAIL
	done
else
	common_cases event

	n=$THREADS_SEEN
	if [ "$n" = 1 ]; then
		record "Part 5 (event): exactly one thread is doing the serving" PASS
	else
		record "Part 5 (event): exactly one thread is doing the serving" FAIL
		{ echo "  [Part 5 (event): exactly one thread is doing the serving]"
		  echo "    /proc/<pid>/status said Threads: $n while the"
		  echo "    server was serving. The event server is one"
		  echo "    thread and a select() loop; that is the whole of"
		  echo "    what is being compared. If this is your threaded"
		  echo "    server under another name, the comparison in"
		  echo "    PERF.md has nothing to compare."; } >&2
	fi

	ccase "Part 5 (event): a stalled client blocks nobody" p5_hol_free
fi
stop_server

# ---------------------------------------------------------------------------
# valgrind
# ---------------------------------------------------------------------------
#
# Two different tools, because the two servers have different failure modes
# and it is worth being clear about which tool can see what.
#
# helgrind on the THREADED server: several worker threads share a queue and,
# if anything has been left global that should be per-connection, a buffer.
# helgrind sees state touched outside the mutex; the outcome cases do not.
#
# memcheck on the EVENT server: one thread, so helgrind would have nothing to
# say and saying it would be dishonest. What that server does have is a state
# machine full of buffer arithmetic -- how much has arrived, how much has
# gone -- and an off-by-one there is exactly what memcheck is for.
#
# Both are killed rather than shut down cleanly, so the check is "did the
# tool print anything", not the exit code: valgrind reports each error as it
# finds it.

vg_case() { # <label> <binary> <gate> <tool args...>
	local label=$1 bin=$2 gate=$3; shift 3
	local rc
	if ! command -v valgrind >/dev/null 2>&1; then
		record "$label" SKIP
		echo "  [$label] SKIPPED: valgrind is not installed." >&2
		return
	fi
	if [ "$gate" != PASS ]; then
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: this server is not answering a"
		  echo "    plain request correctly, and an instrumented copy"
		  echo "    of it would take thirty times as long to say the"
		  echo "    same thing."; } >&2
		return
	fi
	if ! start_server "$bin" timeout -k 5 "$VG_TIMEOUT" valgrind "$@"; then
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the server did not start under"
		  echo "    valgrind, so there is no verdict to give."
		  show "$TMP/srv.err" "valgrind"; } >&2
		stop_server
		return
	fi
	timeout -k 5 "$CASE_TIMEOUT" "$CASES" p5_soak "$PORT" "$WWW" \
		>"$TMP/vgout" 2>"$TMP/vgerr"
	rc=$?
	stop_server
	# Two very different reasons the load can fail to finish, and they must
	# not share a verdict. A TIMEOUT is a legitimate SKIP: valgrind is about
	# thirty times slower, and a correct-but-slow server should not be
	# failed for the instrument's overhead. Anything else means the load ran
	# to completion and came back wrong, which is a result -- and it is
	# exactly the result a racy server produces, so recording it as "no
	# verdict" would make the suite greenest against the worst submissions.
	# The tool's own verdict is read either way.
	if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
		record "$label" SKIP
		{ echo "  [$label] SKIPPED: the load timed out under valgrind"
		  echo "    (about thirty times slower than a plain run), so"
		  echo "    there is no verdict to give. Re-run on a quieter"
		  echo "    machine before believing anything about this case."; } >&2
		return
	fi
	if [ "$rc" != 0 ]; then
		record "$label" FAIL
		{ echo "  [$label] the instrumented run did not come back clean"
		  echo "    (the load client exited $rc). Under an instrument"
		  echo "    everything is slower and every window is wider, so a"
		  echo "    race that is rare in a plain run shows up here as"
		  echo "    responses that are missing or wrong. This is a"
		  echo "    result, not a missing one."
		  show "$TMP/vgerr" "what the load reported"
		  show "$TMP/srv.err" "valgrind"; } >&2
		return
	fi
	if [ -s "$TMP/srv.err" ]; then
		record "$label" FAIL
		{ echo "  [$label] valgrind reported something."
		  show "$TMP/srv.err" "valgrind"; } >&2
	else
		record "$label" PASS
	fi
}

vg_case "Part 5 (threaded): helgrind finds no race between the workers" \
	"$TMP/webserver-threaded" "$T_FETCH" --tool=helgrind -q \
	--suppressions="$TESTDIR/helgrind.supp"
vg_case "Part 5 (event): memcheck finds no bad memory in the state machine" \
	"$TMP/webserver-event" "$E_FETCH" --tool=memcheck -q --leak-check=no

# ---------------------------------------------------------------------------
# PERF.md
# ---------------------------------------------------------------------------
#
# The measurements cannot be auto-graded and this does not pretend to. It
# checks that a report exists, that it covers both servers at more than one
# concurrency level, and that it contains numbers. Whether the explanation is
# any good is marked against the rubric in solutions/README.md, and it is
# most of the marks for this half.

PERF="$WORKDIR/PERF.md"
if [ ! -f "$PERF" ]; then
	record "Part 5: PERF.md reports both servers at several concurrencies" FAIL
	{ echo "  [Part 5: PERF.md reports both servers at several concurrencies]"
	  echo "    No PERF.md in $WORKDIR. Build it with tests/loadgen.c:"
	  echo "      ./loadgen <port> /hello.txt <concurrency> 5"
	  echo "    for both servers, at several concurrency levels, and"
	  echo "    write down what the shape of the numbers means."; } >&2
else
	missing=""
	for w in threaded event throughput latency concurrency; do
		grep -q -i -F "$w" "$PERF" || missing="$missing '$w'"
	done
	numbers=$(grep -c '[0-9]' "$PERF")
	if [ -n "$missing" ]; then
		record "Part 5: PERF.md reports both servers at several concurrencies" FAIL
		{ echo "  [Part 5: PERF.md reports both servers at several concurrencies]"
		  echo "    PERF.md never mentions:$missing"; } >&2
	elif [ "$numbers" -lt 12 ]; then
		record "Part 5: PERF.md reports both servers at several concurrencies" FAIL
		{ echo "  [Part 5: PERF.md reports both servers at several concurrencies]"
		  echo "    PERF.md has $numbers lines with a number in them,"
		  echo "    which is not two servers measured at several"
		  echo "    concurrency levels. Paste the loadgen output."; } >&2
	else
		record "Part 5: PERF.md reports both servers at several concurrencies" PASS
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
echo "(The measurements in PERF.md are marked against the rubric in"
echo " solutions/README.md, not by this script. Two of the cases above are"
echo " architecture-specific on purpose: the thread pool must block behind"
echo " stalled clients and the event loop must not.)"
[ "$fail" -eq 0 ]
