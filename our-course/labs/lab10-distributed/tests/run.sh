#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 10, Parts 1-5.
#
# The lab is five command-line tools -- udpecho, reliable, rpcdemo,
# fileserver, fileclient -- driven entirely through argv/stdin/stdout, run
# as separate client and server processes over loopback UDP.
#
# The harness does the GRADED BUILD ITSELF: it copies the student's five
# tool sources plus rpc.c into its own temporary directory next to its OWN
# copies of the given files (net.c, msg.c, rpc.h, fsproto.h -- from
# tests/given/), and compiles there with its own -Wall -Wextra -Werror
# line.  A submission cannot soften the loss simulator, the wire contract
# headers, or the compiler flags.
#
# Networking discipline: every server binds an EPHEMERAL port (port=0) and
# prints it; the harness reads it back.  Nothing here uses a fixed port.
# Every loss experiment is driven by the seeded simulator in the harness's
# net.c, so runs are reproducible; where the harness claims a run was
# lossy it verifies that from the simulator's own stderr counters
# (NET_STATS=1), not from the submission's say-so.  Every case runs under
# a timeout, and every server the harness starts is killed by PID before
# it exits.
#
# Prints a PASS/FAIL table and 'N passed, M failed'; exits non-zero if
# anything failed.

set -u
export LC_ALL=C

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
HERE=$(cd "$(dirname "$0")" && pwd -P)
GIVEN="$HERE/given"

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)

SERVERS=()
cleanup() {
	local pid
	for pid in "${SERVERS[@]:-}"; do
		[ -n "$pid" ] && kill -9 "$pid" 2>/dev/null
	done
	wait 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

TIMEOUT=60      # per client run / per wait; whole cases finish in seconds

pass=0; fail=0
declare -a RESULTS
record() { # <name> <PASS|FAIL>
	RESULTS+=("$2|$1")
	if [ "$2" = PASS ]; then pass=$((pass+1)); else fail=$((fail+1)); fi
}
fail_with() { # <label> <message> [file-to-excerpt]
	record "$1" FAIL
	{ echo "  [$1] $2"
	  if [ $# -ge 3 ] && [ -s "$3" ]; then
		echo "    --- output ---"; head -15 "$3"
	  fi
	} >&2
}

# ---------------------------------------------------------------------------
# the graded build
# ---------------------------------------------------------------------------

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g'
BUILD="$TMP/build"
mkdir -p "$BUILD"

echo "== graded build (harness's own gcc line, harness's own given files) =="
build_ok=1
for f in udpecho.c reliable.c rpc.c rpcdemo.c fileserver.c fileclient.c; do
	if [ ! -f "$WORKDIR/$f" ]; then
		echo "  missing $f in $WORKDIR" >&2
		build_ok=0
	else
		cp "$WORKDIR/$f" "$BUILD/"
	fi
done
cp "$GIVEN/net.c" "$GIVEN/net.h" "$GIVEN/msg.c" "$GIVEN/msg.h" \
   "$GIVEN/rpc.h" "$GIVEN/fsproto.h" "$BUILD/"

grade_cc() { # <out> <srcs...>
	local out=$1; shift
	if ! (cd "$BUILD" && gcc $GRADE_CFLAGS -o "$out" "$@") \
	     >"$TMP/$out.build" 2>&1; then
		echo "  $out: graded build FAILED (gcc $GRADE_CFLAGS $*):" >&2
		cat "$TMP/$out.build" >&2
		build_ok=0
	fi
}
if [ "$build_ok" = 1 ]; then
	grade_cc udpecho    udpecho.c net.c msg.c
	grade_cc reliable   reliable.c net.c msg.c
	grade_cc rpcdemo    rpcdemo.c rpc.c net.c msg.c
	grade_cc fileserver fileserver.c rpc.c net.c msg.c
	grade_cc fileclient fileclient.c rpc.c net.c msg.c
fi
if [ "$build_ok" != 1 ]; then
	echo "RESULT: build failure" >&2
	exit 1
fi
UDPECHO="$BUILD/udpecho"; RELIABLE="$BUILD/reliable"; RPCDEMO="$BUILD/rpcdemo"
FILESERVER="$BUILD/fileserver"; FILECLIENT="$BUILD/fileclient"
echo "tools OK"

# The student Makefile, as a separate smoke gate only.
if [ -f "$WORKDIR/Makefile" ]; then
	if ! (make -C "$WORKDIR" clean >/dev/null 2>&1 && \
	      make -C "$WORKDIR" all >"$TMP/make.log" 2>&1); then
		echo "note: 'make' did not build cleanly (a smoke gate, not the" >&2
		echo "graded build, but 'make' is the documented workflow):" >&2
		tail -5 "$TMP/make.log" >&2
	fi
fi
echo

# ---------------------------------------------------------------------------
# process helpers
# ---------------------------------------------------------------------------

# start_server <outfile> <errfile> <env...> -- <cmd...>
# Starts the server in the background, waits for its "port=N" line, and
# sets SRV_PID and SRV_PORT.  Returns 1 on failure.
start_server() {
	local out=$1 err=$2; shift 2
	local envs=()
	while [ "$1" != "--" ]; do envs+=("$1"); shift; done
	shift
	env "${envs[@]:-_=_}" "$@" >"$out" 2>"$err" &
	SRV_PID=$!
	disown "$SRV_PID" 2>/dev/null
	SERVERS+=("$SRV_PID")
	local i
	for i in $(seq 100); do
		if grep -q '^port=[0-9]' "$out" 2>/dev/null; then
			SRV_PORT=$(sed -n 's/^port=\([0-9]*\)$/\1/p' "$out" | head -1)
			return 0
		fi
		kill -0 "$SRV_PID" 2>/dev/null || break
		sleep 0.05
	done
	SRV_PORT=""
	return 1
}

stop_server() { # <pid>
	kill -9 "$1" 2>/dev/null
	wait "$1" 2>/dev/null
}

# wait_for_line <file> <fixed-string> [tries]  -- poll until grep -F hits
wait_for_line() {
	local f=$1 s=$2 tries=${3:-200} i
	for i in $(seq "$tries"); do
		grep -q -F "$s" "$f" 2>/dev/null && return 0
		sleep 0.05
	done
	return 1
}

# wait_exit <pid> [tries] -- wait (polling) for a background pid to exit
wait_exit() {
	local pid=$1 tries=${2:-200} i
	for i in $(seq "$tries"); do
		kill -0 "$pid" 2>/dev/null || return 0
		sleep 0.05
	done
	return 1
}

run_client() { # <outfile> <errfile> <env...> -- <cmd...>; returns exit code
	local out=$1 err=$2; shift 2
	local envs=()
	while [ "$1" != "--" ]; do envs+=("$1"); shift; done
	shift
	env "${envs[@]:-_=_}" timeout -k 5 "$TIMEOUT" "$@" >"$out" 2>"$err"
}

# field <file> <key> -- extract N from the first "key=N" occurrence
field() {
	sed -n "s/.*\\b$2=\\(-\\{0,1\\}[0-9][0-9]*\\).*/\\1/p" "$1" | head -1
}

# stats_dropped <errfile> -- the harness net.c's own dropped counter
stats_dropped() {
	sed -n 's/^net: sent=[0-9]* dropped=\([0-9]*\) recvd=[0-9]*$/\1/p' "$1" | head -1
}

# ===========================================================================
# Part 1 -- UDP client and server
# ===========================================================================
echo "== Part 1: udpecho =="

L="Part 1: a clean exchange echoes every payload"
if start_server "$TMP/p1s.out" "$TMP/p1s.err" -- "$UDPECHO" server; then
	run_client "$TMP/p1c.out" "$TMP/p1c.err" -- \
		"$UDPECHO" client "$SRV_PORT" n=8
	if grep -q -F "echo done n=8 ok=8 bad=0 mismatch=0 lost=0" "$TMP/p1c.out"; then
		record "$L" PASS
	else
		fail_with "$L" "expected echo done n=8 ok=8 bad=0 mismatch=0 lost=0" "$TMP/p1c.out"
	fi

	L="Part 1: injected corruption is detected, not echoed"
	run_client "$TMP/p1c2.out" "$TMP/p1c2.err" -- \
		"$UDPECHO" client "$SRV_PORT" n=8 corrupt=3
	if grep -q -F "echo done n=8 ok=7 bad=1 mismatch=0 lost=0" "$TMP/p1c2.out"; then
		record "$L" PASS
	else
		fail_with "$L" "expected ok=7 bad=1 mismatch=0 lost=0 (message 3 was corrupted in flight)" "$TMP/p1c2.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p1s.out"
	record "Part 1: injected corruption is detected, not echoed" FAIL
fi

L="Part 1: a lossy link loses datagrams and the bare client survives it"
if start_server "$TMP/p1ls.out" "$TMP/p1ls.err" \
	LOSS_RATE=25 LOSS_SEED=6 NET_STATS=1 -- "$UDPECHO" server; then
	run_client "$TMP/p1lc.out" "$TMP/p1lc.err" \
		LOSS_RATE=25 LOSS_SEED=5 NET_STATS=1 -- \
		"$UDPECHO" client "$SRV_PORT" n=20 timeout=150
	rc=$?
	ok=$(field "$TMP/p1lc.out" ok); lost=$(field "$TMP/p1lc.out" lost)
	cdrop=$(stats_dropped "$TMP/p1lc.err")
	if [ "$rc" = 0 ] && grep -q -F "echo done n=20 " "$TMP/p1lc.out" && \
	   grep -q -F "bad=0 mismatch=0" "$TMP/p1lc.out" && \
	   [ -n "$ok" ] && [ -n "$lost" ] && [ "$ok" -gt 0 ] && \
	   [ "$lost" -gt 0 ] && [ $((ok + lost)) -eq 20 ] && \
	   [ -n "$cdrop" ] && [ "$cdrop" -gt 0 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want exit 0, ok>0, lost>0, ok+lost=20, bad=0 mismatch=0 (rc=$rc ok=$ok lost=$lost simdrops=$cdrop)" "$TMP/p1lc.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p1ls.out"
fi

# ===========================================================================
# Part 2 -- the reliability layer
# ===========================================================================
echo "== Part 2: reliable =="

# The expected application-layer delivery, derivable from the handout:
# msg-0000 .. msg-0049, each exactly once, in order.
for i in $(seq 0 49); do printf 'deliver payload=msg-%04d\n' "$i"; done \
	> "$TMP/expect.deliver"

p2_case() { # <label> <rate> <sseed> <cseed> <lossy: 0|1>
	local L=$1 rate=$2 sseed=$3 cseed=$4 lossy=$5
	local so="$TMP/p2s-$rate-$sseed.out" se="$TMP/p2s-$rate-$sseed.err"
	local co="$TMP/p2c-$rate-$cseed.out" ce="$TMP/p2c-$rate-$cseed.err"
	if ! start_server "$so" "$se" \
		LOSS_RATE="$rate" LOSS_SEED="$sseed" NET_STATS=1 -- \
		"$RELIABLE" server; then
		fail_with "$L" "server did not print port=N" "$so"
		return
	fi
	run_client "$co" "$ce" LOSS_RATE="$rate" LOSS_SEED="$cseed" NET_STATS=1 -- \
		"$RELIABLE" client "$SRV_PORT" n=50 timeout=50 retries=64
	local rc=$?
	# the server must terminate on its own once the stream is closed
	if ! wait_exit "$SRV_PID" 200; then
		stop_server "$SRV_PID"
		fail_with "$L" "server still running after the client finished (the close never completed)" "$so"
		return
	fi
	grep '^deliver ' "$so" > "$TMP/got.deliver"
	if [ "$rc" != 0 ]; then
		fail_with "$L" "client exit=$rc (want 0: everything acknowledged)" "$co"
		return
	fi
	if ! cmp -s "$TMP/got.deliver" "$TMP/expect.deliver"; then
		fail_with "$L" "the application-layer delivery is not msg-0000..msg-0049 exactly once each, in order ($(grep -c '^deliver ' "$so") deliver lines)" "$co"
		diff "$TMP/expect.deliver" "$TMP/got.deliver" | head -8 >&2
		return
	fi
	if ! grep -q -F "client done sent=50 acked=50 " "$co" || \
	   ! grep -q -F " giveups=0 " "$co"; then
		fail_with "$L" "want client done sent=50 acked=50 ... giveups=0" "$co"
		return
	fi
	local sdrop cdrop retrans
	sdrop=$(stats_dropped "$se"); cdrop=$(stats_dropped "$ce")
	retrans=$(field "$co" retrans)
	if [ "$lossy" = 0 ]; then
		# nothing was dropped, so nothing may be resent or duplicated
		if [ "$retrans" = 0 ] && \
		   grep -q -F "server done delivered=50 dups=0" "$so"; then
			record "$L" PASS
		else
			fail_with "$L" "at 0% loss: want retrans=0 and server done delivered=50 dups=0 (retrans=$retrans)" "$co"
		fi
	else
		# the harness's own simulator must have dropped in BOTH
		# directions, and the layer must have resent; the exact
		# deliver diff above already proves every loss was healed
		# to exactly-once.  (The server's own dups counter is NOT
		# asserted here: how many duplicates the receiver absorbs
		# depends on the design -- it may legitimately be 0.)
		if [ -n "$sdrop" ] && [ -n "$cdrop" ] && [ "$sdrop" -gt 0 ] && \
		   [ "$cdrop" -gt 0 ] && [ -n "$retrans" ] && [ "$retrans" -gt 0 ] && \
		   grep -q -F "server done delivered=50 " "$so"; then
			record "$L" PASS
		else
			fail_with "$L" "want drops both ways (client=$cdrop server=$sdrop), retrans>0 ($retrans), delivered=50" "$co"
		fi
	fi
}

p2_case "Part 2: 0% loss -- in-order, exactly-once, no retransmissions" 0 101 1 0
p2_case "Part 2: 10% loss, seed 11 -- every message exactly once"      10 111 11 1
p2_case "Part 2: 10% loss, seed 12 -- every message exactly once"      10 112 12 1
p2_case "Part 2: 30% loss, seed 31 -- every message exactly once"      30 131 31 1
p2_case "Part 2: 30% loss, seed 32 -- every message exactly once"      30 132 32 1
p2_case "Part 2: 30% loss, seed 33 -- every message exactly once"      30 133 33 1

# ===========================================================================
# Part 3 -- the RPC library
# ===========================================================================
echo "== Part 3: rpcdemo =="

# --- lossless smoke: marshalling and the counter ---
L="Part 3: ping marshals there and back"
L2="Part 3: inc and get agree on a lossless wire"
if start_server "$TMP/p3s.out" "$TMP/p3s.err" NET_STATS=1 -- "$RPCDEMO" server; then
	run_client "$TMP/p3p.out" "$TMP/p3p.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=1 ping n=5
	if [ $? = 0 ] && grep -q -F "ping done calls=5 ok=5 " "$TMP/p3p.out"; then
		record "$L" PASS
	else
		fail_with "$L" "expected ping done calls=5 ok=5" "$TMP/p3p.out"
	fi
	run_client "$TMP/p3i.out" "$TMP/p3i.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=11 inc n=7
	run_client "$TMP/p3g.out" "$TMP/p3g.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=12 get
	run_client "$TMP/p3x.out" "$TMP/p3x.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=13 shutdown
	wait_exit "$SRV_PID" 100
	if grep -q -F "inc done calls=7 ok=7 value=7 " "$TMP/p3i.out" && \
	   grep -q -F "get value=7" "$TMP/p3g.out" && \
	   grep -q -F "executed_inc=7 " "$TMP/p3s.out"; then
		record "$L2" PASS
	else
		fail_with "$L2" "want inc value=7, get value=7, server executed_inc=7" "$TMP/p3i.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p3s.out"
	record "$L2" FAIL
fi

# --- THE CENTREPIECE: duplicate suppression under reply loss ---
# The server's replies are dropped 35% of the time (harness simulator, its
# own stderr proves it); every request arrives.  Each retransmitted request
# is therefore a duplicate the server has already executed.  25 calls to a
# non-idempotent inc must advance the counter by exactly 25.
L="Part 3: CENTREPIECE -- non-idempotent inc with replies dropped executes exactly once per call"
if start_server "$TMP/p3ds.out" "$TMP/p3ds.err" \
	LOSS_RATE=35 LOSS_SEED=7 NET_STATS=1 -- "$RPCDEMO" server; then
	run_client "$TMP/p3di.out" "$TMP/p3di.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=1 timeout=50 retries=64 inc n=25
	rci=$?
	run_client "$TMP/p3dg.out" "$TMP/p3dg.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=2 timeout=50 retries=64 get
	run_client "$TMP/p3dx.out" "$TMP/p3dx.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=3 timeout=50 retries=64 shutdown
	wait_exit "$SRV_PID" 100
	retrans=$(field "$TMP/p3di.out" retrans)
	sdrop=$(stats_dropped "$TMP/p3ds.err")
	if [ "$rci" = 0 ] && \
	   grep -q -F "inc done calls=25 ok=25 value=25 " "$TMP/p3di.out" && \
	   grep -q -F "get value=25" "$TMP/p3dg.out" && \
	   grep -q -F "executed_inc=25 " "$TMP/p3ds.out" && \
	   [ -n "$retrans" ] && [ "$retrans" -gt 0 ] && \
	   [ -n "$sdrop" ] && [ "$sdrop" -gt 0 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want value=25 / get value=25 / executed_inc=25 with retrans>0 ($retrans) and real reply drops ($sdrop)" "$TMP/p3di.out"
		grep -F "executed_inc" "$TMP/p3ds.out" >&2
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p3ds.out"
fi

# --- two clients: their calls must never be confused ---
L="Part 3: two clients' calls are never confused (20 incs -> 20)"
if start_server "$TMP/p32s.out" "$TMP/p32s.err" \
	LOSS_RATE=30 LOSS_SEED=9 NET_STATS=1 -- "$RPCDEMO" server; then
	run_client "$TMP/p32a.out" "$TMP/p32a.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=1 timeout=50 retries=64 inc n=10
	run_client "$TMP/p32b.out" "$TMP/p32b.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=2 timeout=50 retries=64 inc n=10
	run_client "$TMP/p32g.out" "$TMP/p32g.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=3 timeout=50 retries=64 get
	run_client "$TMP/p32x.out" "$TMP/p32x.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=4 timeout=50 retries=64 shutdown
	wait_exit "$SRV_PID" 100
	if grep -q -F "inc done calls=10 ok=10 value=10 " "$TMP/p32a.out" && \
	   grep -q -F "inc done calls=10 ok=10 value=20 " "$TMP/p32b.out" && \
	   grep -q -F "get value=20" "$TMP/p32g.out" && \
	   grep -q -F "executed_inc=20 " "$TMP/p32s.out"; then
		record "$L" PASS
	else
		fail_with "$L" "want A: value=10, B: value=20, get value=20, executed_inc=20" "$TMP/p32b.out"
		grep -F "executed_inc" "$TMP/p32s.out" >&2
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p32s.out"
fi

# --- two clients INTERLEAVED under reply loss ---
# Both clients run concurrently against a server whose replies are being
# dropped (harness simulator, proven from its stderr), so each client's
# retry traffic lands interleaved with the other's live calls.  Every
# non-idempotent inc must still execute exactly once: a server that
# cannot tell "a retry of A's call" from "B's call" either re-executes
# or starves one of them, and this case sees it directly.
L="Part 3: two clients interleaved under loss -- exactly once each (20 incs -> 20)"
if start_server "$TMP/p3is.out" "$TMP/p3is.err" \
	LOSS_RATE=35 LOSS_SEED=57 NET_STATS=1 -- "$RPCDEMO" server; then
	( run_client "$TMP/p3ia.out" "$TMP/p3ia.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=1 timeout=50 retries=64 inc n=10
	  echo $? > "$TMP/p3ia.rc" ) &
	IAPID=$!
	( run_client "$TMP/p3ib.out" "$TMP/p3ib.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=2 timeout=50 retries=64 inc n=10
	  echo $? > "$TMP/p3ib.rc" ) &
	IBPID=$!
	wait "$IAPID" "$IBPID" 2>/dev/null
	run_client "$TMP/p3ig.out" "$TMP/p3ig.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=3 timeout=50 retries=64 get
	run_client "$TMP/p3ix.out" "$TMP/p3ix.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=4 timeout=50 retries=64 shutdown
	wait_exit "$SRV_PID" 100
	rca=$(cat "$TMP/p3ia.rc" 2>/dev/null); rcb=$(cat "$TMP/p3ib.rc" 2>/dev/null)
	ra=$(field "$TMP/p3ia.out" retrans); rb=$(field "$TMP/p3ib.out" retrans)
	sdrop=$(stats_dropped "$TMP/p3is.err")
	# each client's own final value is interleaving-dependent; the graded
	# facts are exactly-once execution and both clients getting answers
	if [ "$rca" = 0 ] && [ "$rcb" = 0 ] && \
	   grep -q -F "inc done calls=10 ok=10 " "$TMP/p3ia.out" && \
	   grep -q -F "inc done calls=10 ok=10 " "$TMP/p3ib.out" && \
	   grep -q -F "get value=20" "$TMP/p3ig.out" && \
	   grep -q -F "executed_inc=20 " "$TMP/p3is.out" && \
	   [ -n "$ra" ] && [ -n "$rb" ] && [ $((ra + rb)) -gt 0 ] && \
	   [ -n "$sdrop" ] && [ "$sdrop" -gt 0 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want both clients ok=10 (rcA=$rca rcB=$rcb), get value=20, executed_inc=20, retrans>0 (A=$ra B=$rb), real reply drops ($sdrop)" "$TMP/p3ib.out"
		grep -F "executed_inc" "$TMP/p3is.out" >&2
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p3is.out"
fi

# --- lost requests, and loss in both directions ---
L="Part 3: lost requests are retried to exactly-once (15 incs -> 15)"
if start_server "$TMP/p3rs.out" "$TMP/p3rs.err" NET_STATS=1 -- "$RPCDEMO" server; then
	run_client "$TMP/p3ri.out" "$TMP/p3ri.err" \
		LOSS_RATE=30 LOSS_SEED=21 NET_STATS=1 -- \
		"$RPCDEMO" client "$SRV_PORT" id=1 timeout=50 retries=64 inc n=15
	run_client "$TMP/p3rg.out" "$TMP/p3rg.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=2 get
	run_client "$TMP/p3rx.out" "$TMP/p3rx.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=3 shutdown
	wait_exit "$SRV_PID" 100
	cdrop=$(stats_dropped "$TMP/p3ri.err")
	if grep -q -F "inc done calls=15 ok=15 value=15 " "$TMP/p3ri.out" && \
	   grep -q -F "get value=15" "$TMP/p3rg.out" && \
	   grep -q -F "executed_inc=15 " "$TMP/p3rs.out" && \
	   [ -n "$cdrop" ] && [ "$cdrop" -gt 0 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want value=15 everywhere with real request drops ($cdrop)" "$TMP/p3ri.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p3rs.out"
fi

L="Part 3: loss in both directions still gives exactly-once (20 incs -> 20)"
if start_server "$TMP/p3bs.out" "$TMP/p3bs.err" \
	LOSS_RATE=20 LOSS_SEED=41 NET_STATS=1 -- "$RPCDEMO" server; then
	run_client "$TMP/p3bi.out" "$TMP/p3bi.err" \
		LOSS_RATE=20 LOSS_SEED=42 NET_STATS=1 -- \
		"$RPCDEMO" client "$SRV_PORT" id=1 timeout=50 retries=64 inc n=20
	run_client "$TMP/p3bg.out" "$TMP/p3bg.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=2 timeout=50 retries=64 get
	run_client "$TMP/p3bx.out" "$TMP/p3bx.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=3 timeout=50 retries=64 shutdown
	wait_exit "$SRV_PID" 100
	if grep -q -F "inc done calls=20 ok=20 value=20 " "$TMP/p3bi.out" && \
	   grep -q -F "get value=20" "$TMP/p3bg.out" && \
	   grep -q -F "executed_inc=20 " "$TMP/p3bs.out"; then
		record "$L" PASS
	else
		fail_with "$L" "want value=20 / get value=20 / executed_inc=20" "$TMP/p3bi.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p3bs.out"
fi

# --- late duplicate replies must not answer the wrong call ---
# slow(120ms) with a 50ms timeout: the client retries while the server
# sleeps; the queued retries are answered from the reply cache, so several
# copies of the slow reply arrive.  The pings that follow only succeed if
# rpc_call refuses to take a stale reply as the answer to a new call.
# No loss simulation involved: this case is fully deterministic.
# (Timing note: retrans>=1 below assumes the client is scheduled at least
# once during the server's 120 ms sleep -- a 70 ms margin over the 50 ms
# timeout.  On a pathologically loaded box this is the first case to
# suspect if a spurious retrans=0 FAIL ever appears.)
L="Part 3: a late duplicate reply is not taken as the answer to the next call"
if start_server "$TMP/p3ss.out" "$TMP/p3ss.err" -- "$RPCDEMO" server; then
	run_client "$TMP/p3sl.out" "$TMP/p3sl.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=4 timeout=50 retries=10 slowping ms=120 n=3
	rcs=$?
	run_client "$TMP/p3sx.out" "$TMP/p3sx.err" -- \
		"$RPCDEMO" client "$SRV_PORT" id=5 shutdown
	wait_exit "$SRV_PID" 100
	retrans=$(field "$TMP/p3sl.out" retrans)
	if [ "$rcs" = 0 ] && \
	   grep -q -F "slowping done slow_ok=1 pings_ok=3 " "$TMP/p3sl.out" && \
	   [ -n "$retrans" ] && [ "$retrans" -ge 1 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want slow_ok=1 pings_ok=3 with retrans>=1 (the timeout must have fired; got retrans=$retrans)" "$TMP/p3sl.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p3ss.out"
fi

# ===========================================================================
# Part 4 -- the stateless file service
# ===========================================================================
echo "== Part 4: fileserver + fileclient =="

# expected workload file contents, derivable from the handout
mkexpect() { # <n> <outfile>
	local i
	for i in $(seq 0 $(($1 - 1))); do
		printf 'rec%05d........................' "$i"
	done > "$2"
}

L="Part 4: lookup/read/write/getattr over the wire"
EXP1="$TMP/exp1"; mkdir -p "$EXP1"
if start_server "$TMP/p4s.out" "$TMP/p4s.err" -- "$FILESERVER" "$EXP1"; then
	P4PORT=$SRV_PORT; P4PID=$SRV_PID
	printf 'open f\nwrite 0 hello-world\nread 0 11\ngetattr\nquit\n' | \
		run_client "$TMP/p4c.out" "$TMP/p4c.err" -- \
		"$FILECLIENT" "$P4PORT" cmd id=7
	if grep -q -F 'open name=f fh=' "$TMP/p4c.out" && \
	   grep -q -F 'write off=0 len=11 ok' "$TMP/p4c.out" && \
	   grep -q -F 'read off=0 len=11 data="hello-world"' "$TMP/p4c.out" && \
	   grep -q -F 'getattr size=11' "$TMP/p4c.out" && \
	   [ "$(cat "$EXP1/f" 2>/dev/null)" = "hello-world" ]; then
		record "$L" PASS
	else
		fail_with "$L" "basic cmd-mode ops wrong, or the server-side file does not hold the bytes" "$TMP/p4c.out"
	fi

	L="Part 4: the workload runs clean on a clean wire (2K+2 RPCs)"
	run_client "$TMP/p4w.out" "$TMP/p4w.err" -- \
		"$FILECLIENT" "$P4PORT" workload name=w n=200 id=8
	rcw=$?
	mkexpect 200 "$TMP/want-w"
	if [ "$rcw" = 0 ] && \
	   grep -q -F "workload done n=200 wrote=200 verified=200 size=6400 rpcs=402 retrans=0" "$TMP/p4w.out" && \
	   cmp -s "$EXP1/w" "$TMP/want-w"; then
		record "$L" PASS
	else
		fail_with "$L" "want the exact done line (rpcs=402 retrans=0) and byte-identical server file" "$TMP/p4w.out"
	fi
	stop_server "$P4PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p4s.out"
	record "Part 4: the workload runs clean on a clean wire (2K+2 RPCs)" FAIL
fi

L="Part 4: the workload completes correctly at 30% loss both ways"
EXP2="$TMP/exp2"; mkdir -p "$EXP2"
if start_server "$TMP/p4ls.out" "$TMP/p4ls.err" \
	LOSS_RATE=30 LOSS_SEED=52 NET_STATS=1 -- "$FILESERVER" "$EXP2"; then
	run_client "$TMP/p4lw.out" "$TMP/p4lw.err" \
		LOSS_RATE=30 LOSS_SEED=51 NET_STATS=1 -- \
		"$FILECLIENT" "$SRV_PORT" workload name=w n=120 id=9 timeout=50 retries=64
	rcw=$?
	retrans=$(field "$TMP/p4lw.out" retrans)
	cdrop=$(stats_dropped "$TMP/p4lw.err")
	mkexpect 120 "$TMP/want-lw"
	if [ "$rcw" = 0 ] && \
	   grep -q -F "workload done n=120 wrote=120 verified=120 size=3840 " "$TMP/p4lw.out" && \
	   cmp -s "$EXP2/w" "$TMP/want-lw" && \
	   [ -n "$retrans" ] && [ "$retrans" -gt 0 ] && \
	   [ -n "$cdrop" ] && [ "$cdrop" -gt 0 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want a verified=120 run, byte-identical file, retrans>0 ($retrans), real drops ($cdrop)" "$TMP/p4lw.out"
	fi
	stop_server "$SRV_PID"
else
	fail_with "$L" "server did not print port=N" "$TMP/p4ls.out"
fi

# --- the restart tests: kill -9 mid-workload, restart on the same port ---
restart_case() { # <label> <progress-line-to-kill-at>
	local L=$1 killat=$2
	local exp="$TMP/exp-restart-$RANDOM"; mkdir -p "$exp"
	if ! start_server "$TMP/prs.out" "$TMP/prs.err" -- "$FILESERVER" "$exp"; then
		fail_with "$L" "server did not print port=N" "$TMP/prs.out"
		return
	fi
	local port=$SRV_PORT pid1=$SRV_PID
	( env timeout -k 5 90 "$FILECLIENT" "$port" workload name=r n=200 \
	      id=11 timeout=100 retries=80 delay=5 \
	      >"$TMP/prc.out" 2>"$TMP/prc.err"
	  echo $? > "$TMP/prc.rc" ) &
	local cw=$!
	if ! wait_for_line "$TMP/prc.out" "$killat" 400; then
		stop_server "$pid1"; kill -9 "$cw" 2>/dev/null; wait "$cw" 2>/dev/null
		fail_with "$L" "workload never reached '$killat'" "$TMP/prc.out"
		return
	fi
	kill -9 "$pid1"
	# anti-vacuous: the workload must NOT already be finished
	if grep -q -F "workload done" "$TMP/prc.out"; then
		wait "$cw" 2>/dev/null
		fail_with "$L" "internal: workload finished before the kill (case is vacuous)" "$TMP/prc.out"
		return
	fi
	sleep 0.4       # the outage the client has to ride out
	if ! start_server "$TMP/prs2.out" "$TMP/prs2.err" -- \
		"$FILESERVER" "$exp" port="$port"; then
		wait "$cw" 2>/dev/null
		fail_with "$L" "restarted server did not come back on port $port" "$TMP/prs2.out"
		return
	fi
	local pid2=$SRV_PID
	wait "$cw" 2>/dev/null
	local rc; rc=$(cat "$TMP/prc.rc" 2>/dev/null)
	local retrans; retrans=$(field "$TMP/prc.out" retrans)
	mkexpect 200 "$TMP/want-r"
	if [ "$rc" = 0 ] && \
	   grep -q -F "workload done n=200 wrote=200 verified=200 size=6400 " "$TMP/prc.out" && \
	   cmp -s "$exp/r" "$TMP/want-r" && \
	   [ -n "$retrans" ] && [ "$retrans" -gt 0 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want the client to finish (rc=$rc), all 200 records byte-identical on disk, retrans>0 ($retrans)" "$TMP/prc.out"
	fi
	stop_server "$pid2"
}

restart_case "Part 4: RESTART -- kill -9 mid-writes; the client must not notice" \
	"progress phase=write i=100"
restart_case "Part 4: RESTART -- kill -9 mid-reads; old handles must still work" \
	"progress phase=read i=100"

# ===========================================================================
# Part 5 -- client caching and consistency
# ===========================================================================
echo "== Part 5: caching =="

EXP5="$TMP/exp5"; mkdir -p "$EXP5"
if start_server "$TMP/p5s.out" "$TMP/p5s.err" -- "$FILESERVER" "$EXP5"; then
	P5PORT=$SRV_PORT; P5PID=$SRV_PID

	# --- request reduction: 40 reads of an unchanged file ---
	# Cost model from the handout: open = 1 lookup; the first read = one
	# getattr + one fetch; every read inside the ac window after that =
	# 0 messages.  With one write in between: <= 6 RPCs for 40 reads.
	L="Part 5: the cache turns 40 reads into a handful of RPCs"
	{ printf 'open f\nwrite 0 CACHE-ME-CACHE-1\n'
	  for i in $(seq 40); do printf 'read 0 16\n'; done
	  printf 'stats\nquit\n'; } > "$TMP/p5h.in"
	run_client "$TMP/p5h.out" "$TMP/p5h.err" -- \
		"$FILECLIENT" "$P5PORT" cmd id=20 ac=5000 < "$TMP/p5h.in"
	nread=$(grep -c '^read ' "$TMP/p5h.out")
	ngood=$(grep -c -F 'data="CACHE-ME-CACHE-1"' "$TMP/p5h.out")
	rpcs=$(field "$TMP/p5h.out" rpcs); hits=$(field "$TMP/p5h.out" hits)
	if [ "$nread" = 40 ] && [ "$ngood" = 40 ] && \
	   [ -n "$rpcs" ] && [ "$rpcs" -le 6 ] && \
	   [ -n "$hits" ] && [ "$hits" -ge 38 ]; then
		record "$L" PASS
	else
		fail_with "$L" "want 40 correct reads with rpcs<=6 and hits>=38 (reads=$nread correct=$ngood rpcs=$rpcs hits=$hits)" "$TMP/p5h.out"
	fi

	# --- the staleness window, driven step by step over a FIFO ---
	printf 'open g\nwrite 0 OLD-OLD-OLD-OLD.\nquit\n' | \
		run_client "$TMP/p5pre.out" "$TMP/p5pre.err" -- \
		"$FILECLIENT" "$P5PORT" cmd id=21
	mkfifo "$TMP/p5fifo"
	env timeout -k 5 "$TIMEOUT" "$FILECLIENT" "$P5PORT" cmd id=22 ac=2500 \
		< "$TMP/p5fifo" > "$TMP/p5a.out" 2>"$TMP/p5a.err" &
	APID=$!
	SERVERS+=("$APID")
	exec 9>"$TMP/p5fifo"
	stale_ok=1
	printf 'open g\nread 0 16\n' >&9
	wait_for_line "$TMP/p5a.out" 'read off=0 len=16' 200 || stale_ok=0
	# another client overwrites the same 16 bytes (same size!)
	printf 'open g\nwrite 0 NEW-NEW-NEW-NEW.\nquit\n' | \
		run_client "$TMP/p5b.out" "$TMP/p5b.err" -- \
		"$FILECLIENT" "$P5PORT" cmd id=23
	printf 'read 0 16\n' >&9
	for i in $(seq 200); do
		[ "$(grep -c '^read ' "$TMP/p5a.out")" -ge 2 ] && break
		sleep 0.05
	done
	L="Part 5: inside the window the cache serves the (stale) old data"
	if [ "$stale_ok" = 1 ] && \
	   [ "$(grep -c -F 'data="OLD-OLD-OLD-OLD."' "$TMP/p5a.out")" = 2 ]; then
		record "$L" PASS
	else
		fail_with "$L" "the second read, well inside ac=2500ms, must still show OLD (a cache that always refetches is not a cache)" "$TMP/p5a.out"
	fi
	sleep 3.0       # let the attribute timeout expire, with margin
	printf 'read 0 16\nstats\nquit\n' >&9
	exec 9>&-
	wait_exit "$APID" 200 || kill -9 "$APID" 2>/dev/null
	L="Part 5: past the window the staleness ends -- the new data appears"
	hits=$(field "$TMP/p5a.out" hits)
	if [ "$(grep -c '^read ' "$TMP/p5a.out")" = 3 ] && \
	   grep -q -F 'data="NEW-NEW-NEW-NEW."' "$TMP/p5a.out" && \
	   [ -n "$hits" ] && [ "$hits" -ge 1 ]; then
		record "$L" PASS
	else
		fail_with "$L" "the third read, past ac=2500ms, must show NEW (and the run must have had >=1 cache hit; hits=$hits)" "$TMP/p5a.out"
	fi

	# --- write-through: while the writing client is still running, its
	# completed write must already be visible to another client ---
	L="Part 5: a write goes through immediately (no write-back delay)"
	mkfifo "$TMP/p5wfifo"
	env timeout -k 5 "$TIMEOUT" "$FILECLIENT" "$P5PORT" cmd id=24 ac=5000 \
		< "$TMP/p5wfifo" > "$TMP/p5w.out" 2>"$TMP/p5w.err" &
	WPID=$!
	disown "$WPID" 2>/dev/null
	SERVERS+=("$WPID")
	exec 8>"$TMP/p5wfifo"
	printf 'open h\nwrite 0 THROUGH-123\n' >&8
	wait_for_line "$TMP/p5w.out" 'write off=0 len=11 ok' 200
	# A is still alive, cache warm, quit not sent -- B reads now
	printf 'open h\nread 0 11\nquit\n' | \
		run_client "$TMP/p5r.out" "$TMP/p5r.err" -- \
		"$FILECLIENT" "$P5PORT" cmd id=25
	printf 'quit\n' >&8
	exec 8>&-
	wait_exit "$WPID" 100 || kill -9 "$WPID" 2>/dev/null
	if grep -q -F 'write off=0 len=11 ok' "$TMP/p5w.out" && \
	   grep -q -F 'read off=0 len=11 data="THROUGH-123"' "$TMP/p5r.out"; then
		record "$L" PASS
	else
		fail_with "$L" "client B (no cache) must see A's write as soon as A's write command returned -- A was still running" "$TMP/p5r.out"
	fi

	stop_server "$P5PID"
else
	fail_with "Part 5: the cache turns 40 reads into a handful of RPCs" \
		"server did not print port=N" "$TMP/p5s.out"
	record "Part 5: inside the window the cache serves the (stale) old data" FAIL
	record "Part 5: past the window the staleness ends -- the new data appears" FAIL
	record "Part 5: a write goes through immediately (no write-back delay)" FAIL
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
echo "$pass passed, $fail failed"
echo "(RELIABILITY.md, RESTART.md and CACHE.md -- the measurements and the"
echo " consistency argument -- are marked against the rubric in"
echo " solutions/README.md, not by this script.)"
[ "$fail" -eq 0 ]
