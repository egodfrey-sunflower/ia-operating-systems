#!/usr/bin/env bash
#
# hostsync.sh <hostsync_dir>
#
# Lab 5 Part C autograder: compile and stress-test the three host-Linux pthreads
# programs (bounded buffer, cyclic barrier, writer-preference rwlock).
#
# A row PASSES iff the program exits 0 AND prints "<name>: PASS". Each stress
# run is wrapped in `timeout` so a deadlocked or starving solution is reported
# as a FAIL instead of hanging the suite. bbuffer and rwlock are run twice to
# shake out concurrency flakes.
#
# Prints a per-row table and a summary; exits 0 iff every row passed.

set -uo pipefail   # NB: not -e; we want to run every test and tally results.

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <hostsync_dir>" >&2
  exit 2
fi

DIR="$1"
if [ ! -d "$DIR" ]; then
  echo "error: '$DIR' is not a directory" >&2
  exit 2
fi
DIR="$(cd "$DIR" && pwd)"

# name -> PASS/FAIL, in insertion order.
ROWS=()
record() { # <name> <PASS|FAIL> [detail]
  ROWS+=("$1|$2|${3:-}")
  printf '  [%s] %s%s\n' "$2" "$1" \
    "$( [ -n "${3:-}" ] && printf ' -- %s' "$3" )"
}

# Run <prog> under <timeout_s>; PASS iff exit 0 and stdout has "<prog>: PASS".
# Returns 0 on pass, 1 on fail; echoes a short detail to stderr via a global.
DETAIL=""
run_once() { # <prog> <timeout_s>
  local prog="$1" tmo="$2" out rc
  out="$(cd "$DIR" && timeout "$tmo" "./$prog" 2>&1)"
  rc=$?
  if [ "$rc" -eq 124 ]; then
    DETAIL="timed out after ${tmo}s"
    return 1
  fi
  if [ "$rc" -ne 0 ]; then
    DETAIL="exit $rc: $(printf '%s' "$out" | tail -n1)"
    return 1
  fi
  if printf '%s' "$out" | grep -q "^$prog: PASS"; then
    DETAIL="$(printf '%s' "$out" | grep "^$prog: PASS" | head -n1)"
    return 0
  fi
  DETAIL="no PASS line: $(printf '%s' "$out" | tail -n1)"
  return 1
}

# A row that runs a program N times, all of which must pass.
run_rows() { # <prog> <timeout_s> <repeats>
  local prog="$1" tmo="$2" reps="$3" i ok=1 last=""
  for i in $(seq 1 "$reps"); do
    if run_once "$prog" "$tmo"; then
      last="$DETAIL"
    else
      record "$prog (run $i/$reps)" FAIL "$DETAIL"
      ok=0
      break
    fi
  done
  if [ "$ok" -eq 1 ]; then
    record "$prog (${reps}x)" PASS "$last"
  fi
}

echo "== Part C: building $DIR =="
if (cd "$DIR" && make clean >/dev/null 2>&1 && make 2>&1); then
  record "compile" PASS
else
  record "compile" FAIL "make failed (see output above)"
  # No binaries to run; emit table and bail.
  echo
  echo "Part C: 0/1 passed (compile failed)"
  exit 1
fi

echo "== Part C: stress tests =="
run_rows bbuffer 10 2
run_rows barrier 15 1
run_rows rwlock  10 2

# ---- summary --------------------------------------------------------------
echo
echo "=================================================="
echo "  Lab 5 Part C (host pthreads) results"
echo "=================================================="
npass=0
for row in "${ROWS[@]}"; do
  IFS='|' read -r name status detail <<<"$row"
  printf '  %-22s %s\n' "$name" "$status"
  [ "$status" = PASS ] && npass=$((npass + 1))
done
echo "=================================================="
echo "  Part C: $npass/${#ROWS[@]} passed"
echo

[ "$npass" -eq "${#ROWS[@]}" ]
