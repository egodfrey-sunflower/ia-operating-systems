#!/usr/bin/env bash
#
# Lab 1 autograder.  Usage: tests/run.sh <workdir>
#
# <workdir> must contain a Makefile that builds ./msh (the starter/ and
# solutions/ directories both qualify).  We build it, then drive the shell
# by piping scripts into its stdin and checking stdout, file side-effects,
# exit codes, and (for background/signal tests) live process state.
#
# The prompt is written to stderr, so stdout comparisons stay clean.
# Every case is wrapped in `timeout` so a hung shell fails instead of
# stalling the run.  Background/signal cases poll with deadlines rather
# than sleeping for a fixed synchronization interval.

set -u

WORKDIR="${1:?usage: run.sh <workdir>}"
WORKDIR="$(cd "$WORKDIR" && pwd)"
MSH="$WORKDIR/msh"

TMP="$(mktemp -d)"
OUT="$TMP/out"
ERR="$TMP/err"

SHIN=""
cleanup() {
    [ -n "$SHIN" ] && exec {SHIN}>&- 2>/dev/null
    # Only kill when a shell was actually started: kill "0" would signal
    # our whole process group and abort the rest of this cleanup.
    [ -n "${SHELL_PID:-}" ] && kill "$SHELL_PID" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT

# ------------------------------------------------------------------ #
# Scoring bookkeeping                                                 #
# ------------------------------------------------------------------ #
tasks=("Task 1: REPL & builtins" \
       "Task 2: Redirection" \
       "Task 3: Pipelines" \
       "Task 4: Background & wait" \
       "Task 5: Signals")
weights=(15 20 25 20 20)
declare -a tp tt          # per-task pass count / total count
pass=0
fail=0
T=0                       # current task index

set_task() { T="$1"; echo; echo "== ${tasks[$T]} =="; }

record() { # PASS|FAIL  description
    tt[$T]=$(( ${tt[$T]:-0} + 1 ))
    if [ "$1" = PASS ]; then
        pass=$((pass + 1)); tp[$T]=$(( ${tp[$T]:-0} + 1 ))
    else
        fail=$((fail + 1)); tp[$T]=$(( ${tp[$T]:-0} + 0 ))
    fi
    printf '  [%s] %s\n' "$1" "$2"
}

# ------------------------------------------------------------------ #
# Drivers                                                            #
# ------------------------------------------------------------------ #

# run_msh <input> : feed <input> to msh (cwd = $TMP), capture out/err,
# return msh's exit code.
run_msh() {
    ( cd "$TMP" && printf '%b' "$1" | timeout 10 "$MSH" ) > "$OUT" 2> "$ERR"
    return $?
}

# check_out <name> <input> <expected-stdout>  (trailing newlines ignored)
check_out() {
    run_msh "$2"
    local got; got="$(cat "$OUT")"
    if [ "$got" = "$3" ]; then
        record PASS "$1"
    else
        record FAIL "$1 (got [$got] want [$3])"
    fi
}

# check_file <name> <input> <relfile> <expected-content>
check_file() {
    run_msh "$2"
    local got; got="$(cat "$TMP/$3" 2>/dev/null)"
    if [ "$got" = "$4" ]; then
        record PASS "$1"
    else
        record FAIL "$1 (file $3 = [$got] want [$4])"
    fi
}

# ------------------------------------------------------------------ #
# Polling helpers (deadline-based, never a bare sleep for sync)      #
# ------------------------------------------------------------------ #
wait_for_grep() { # pattern file deadline_secs
    local d=$(( $(date +%s) + $3 ))
    while :; do
        grep -q "$1" "$2" 2>/dev/null && return 0
        [ "$(date +%s)" -ge "$d" ] && return 1
        sleep 0.05
    done
}
wait_for_child() { # name deadline_secs  (direct child of $SHELL_PID)
    local d=$(( $(date +%s) + $2 ))
    while :; do
        pgrep -P "$SHELL_PID" -x "$1" >/dev/null 2>&1 && return 0
        [ "$(date +%s)" -ge "$d" ] && return 1
        sleep 0.05
    done
}
wait_for_no_child() { # name deadline_secs
    local d=$(( $(date +%s) + $2 ))
    while :; do
        pgrep -P "$SHELL_PID" -x "$1" >/dev/null 2>&1 || return 0
        [ "$(date +%s)" -ge "$d" ] && return 1
        sleep 0.05
    done
}

# ------------------------------------------------------------------ #
# Long-lived shell driven over a FIFO (for background/signal tests). #
# The shell is started under setsid so it leads its own session, and #
# writes its own pid to a file (via exec, so the pid is msh's pid).  #
# ------------------------------------------------------------------ #
FIFO="$TMP/in.fifo"
PIDF="$TMP/shell.pid"
BGOUT="$TMP/bg.out"
BGERR="$TMP/bg.err"

start_bg_shell() {
    rm -f "$FIFO" "$PIDF"; mkfifo "$FIFO"
    : > "$BGOUT"; : > "$BGERR"
    # launcher records its pid, then exec's msh (keeping the same pid)
    ( cd "$TMP" && setsid sh "$TMP/launch.sh" "$PIDF" "$MSH" "$FIFO" ) \
        > "$BGOUT" 2> "$BGERR" &
    exec {SHIN}> "$FIFO"            # writer unblocks msh's read-open
    local d=$(( $(date +%s) + 5 ))
    while [ ! -s "$PIDF" ]; do
        [ "$(date +%s)" -ge "$d" ] && return 1
        sleep 0.02
    done
    SHELL_PID="$(cat "$PIDF")"
    return 0
}
send_shell() { printf '%s\n' "$1" >&"$SHIN"; }
stop_bg_shell() {
    [ -n "$SHIN" ] && exec {SHIN}>&-
    SHIN=""
    # Same guard as cleanup(): never kill "0" (the whole process group).
    [ -n "${SHELL_PID:-}" ] && kill "$SHELL_PID" 2>/dev/null
    SHELL_PID=""
    wait 2>/dev/null
}

# ------------------------------------------------------------------ #
# Build                                                              #
# ------------------------------------------------------------------ #
echo "== Building msh in $WORKDIR =="
if ! make -C "$WORKDIR" >/dev/null 2>&1; then
    echo "BUILD FAILED"; exit 1
fi
[ -x "$MSH" ] || { echo "no ./msh produced"; exit 1; }

# fixtures
printf 'line1\nline2\n'                 > "$TMP/in1"
printf 'hello2\n'                       > "$TMP/in2"
seq 1 5                                 > "$TMP/f_seq"
cat > "$TMP/launch.sh" <<'EOF'
#!/bin/sh
echo $$ > "$1"
exec "$2" < "$3"
EOF
cat > "$TMP/slow_write.sh" <<'EOF'
#!/bin/sh
sleep 0.2
echo bgdata > "$1"
EOF
chmod +x "$TMP/launch.sh" "$TMP/slow_write.sh"

# ================================================================== #
set_task 0   # REPL & builtins
# ================================================================== #
check_out "echo simple"              'echo hello\n'                     "hello"
check_out "echo multiple args"       'echo a b c\n'                     "a b c"
check_out "two commands in sequence" 'echo one\necho two\n'             $'one\ntwo'
check_out "PATH search (printf)"     'printf hi\n'                      "hi"
check_out "exit stops the shell"     'echo a\nexit\necho b\n'           "a"
check_out "cd + pwd"                 'cd /\npwd\n'                       "/"
check_out "cd error, shell survives" 'cd /no_such_dir_xyz\necho ok\n'   "ok"
check_out "unknown cmd, shell lives" 'no_such_cmd_xyz\necho after\n'    "after"

# ================================================================== #
set_task 1   # Redirection
# ================================================================== #
check_file "> creates file"          'echo hi > r1\n'                   r1  "hi"
check_out  "< reads from file"       'cat < in1\n'                      $'line1\nline2'
check_file ">> appends"              'echo a > r2\necho b >> r2\n'      r2  $'a\nb'
check_file "> truncates"             'echo longcontent > r3\necho x > r3\n' r3 "x"
check_file "redir order is flexible" '> r5 echo hi\n'                   r5  "hi"
check_file "< and > combined"        'cat < in2 > r6\n'                 r6  "hello2"

# 2>: stderr of a failing command is captured, stdout stays empty
run_msh 'ls /no_such_xyz 2> r4\n'
if [ -s "$TMP/r4" ] && [ ! -s "$OUT" ]; then
    record PASS "2> captures stderr, stdout clean"
else
    record FAIL "2> captures stderr (r4 size=$(wc -c <"$TMP/r4" 2>/dev/null), out=[$(cat "$OUT")])"
fi

# ================================================================== #
set_task 2   # Pipelines
# ================================================================== #
check_out "two-stage pipe"           'echo hello | cat\n'               "hello"
check_out "pipe into grep"           'seq 1 5 | grep 3\n'               "3"
check_out "first stage reads a file" 'cat f_seq | grep 4\n'            "4"

# long pipeline (4 stages); expected computed with the same tools
exp="$(seq 1 100 | grep 2 | grep 5 | wc -l)"
check_out "four-stage pipeline"      'seq 1 100 | grep 2 | grep 5 | wc -l\n' "$exp"
exp="$(seq 1 100 | grep -c 2)"
check_out "pipe count (grep -c)"     'seq 1 100 | grep -c 2\n'          "$exp"

check_file "pipeline + redirection"  'seq 1 5 | grep 3 > r7\n'          r7  "3"
# no fd leak: cat needs EOF from its predecessor or it hangs (timeout)
check_out "chained cats see EOF"     'echo done | cat | cat | cat\n'    "done"
# SIGPIPE path: head exits, yes dies; a leaked read end would hang
check_out "yes | head -1"            'yes | head -1\n'                  "y"

run_msh 'true | false\n';  [ $? -eq 1 ] && record PASS "pipeline status = last (fail)" || record FAIL "pipeline status = last (fail)"
run_msh 'false | true\n';  [ $? -eq 0 ] && record PASS "pipeline status = last (ok)"   || record FAIL "pipeline status = last (ok)"

# ================================================================== #
set_task 3   # Background & wait
# ================================================================== #
# wait blocks until the backgrounded job finishes (0.2s), so the file exists
check_out "wait waits for bg job" './slow_write.sh wout &\nwait\ncat wout\n' "bgdata"

# background prints [pid] to stderr, nothing to stdout
run_msh 'sleep 0.1 &\n'
if grep -Eq '\[[0-9]+\]' "$ERR" && [ ! -s "$OUT" ]; then
    record PASS "& prints [pid] to stderr"
else
    record FAIL "& prints [pid] to stderr (err=[$(cat "$ERR")])"
fi

# & does not block: a long bg job must not delay the next command
if start_bg_shell; then
    send_shell "sleep 5 &"
    send_shell "echo quick"
    if wait_for_grep "quick" "$BGOUT" 2; then
        record PASS "& does not block the shell"
    else
        record FAIL "& blocked the shell (quick not seen)"
    fi
    stop_bg_shell
else
    record FAIL "& does not block (shell failed to start)"
fi

# zombies from finished bg jobs get reaped.  The handout permits either
# a SIGCHLD handler OR an opportunistic waitpid(WNOHANG) loop run at the
# prompt, so the poll loop keeps sending trivial commands: each one gives
# a prompt-time reaper a turn.  (A SIGCHLD shell reaps regardless.)
# A sample only counts once the bg sleeps have actually finished: an
# early sample with the sleeps still alive would otherwise pass any
# shell before reaping was even exercised.
if start_bg_shell; then
    send_shell "sleep 0.1 &"
    send_shell "sleep 0.1 &"
    send_shell "sleep 0.1 &"
    send_shell "echo z_done"
    wait_for_grep "z_done" "$BGOUT" 3
    d=$(( $(date +%s) + 4 )); zres=FAIL
    while :; do
        # live = still-running sleep children; zc = zombie children
        live="$(ps -o stat=,comm= --ppid "$SHELL_PID" 2>/dev/null \
                | awk '$1 !~ /^Z/ && $2 == "sleep"' | wc -l)"
        zc="$(ps -o stat= --ppid "$SHELL_PID" 2>/dev/null | grep -c '^Z')"
        if [ "$live" -eq 0 ] && [ "$zc" -eq 0 ]; then zres=PASS; break; fi
        [ "$(date +%s)" -ge "$d" ] && break
        send_shell "echo poke"        # let a prompt-time WNOHANG reaper run
        sleep 0.05
    done
    [ "$zres" = PASS ] && record PASS "background zombies are reaped" \
                       || record FAIL "background zombies remain (live=$live z=$zc)"
    stop_bg_shell
else
    record FAIL "zombie reaping (shell failed to start)"
fi

# ================================================================== #
set_task 4   # Signals
# ================================================================== #
# SIGINT to the shell kills the foreground job, not the shell
if start_bg_shell; then
    send_shell "sleep 5"
    if wait_for_child "sleep" 3; then
        kill -INT "$SHELL_PID"
        s1=bad; s2=bad; s3=bad
        wait_for_no_child "sleep" 3 && s1=ok
        kill -0 "$SHELL_PID" 2>/dev/null && s2=ok
        send_shell "echo survived"
        wait_for_grep "survived" "$BGOUT" 3 && s3=ok
        if [ "$s1" = ok ] && [ "$s2" = ok ] && [ "$s3" = ok ]; then
            record PASS "SIGINT kills fg job, shell survives"
        else
            record FAIL "SIGINT handling (killed=$s1 alive=$s2 resumed=$s3)"
        fi
    else
        record FAIL "SIGINT: foreground sleep never started"
    fi
    stop_bg_shell
else
    record FAIL "SIGINT (shell failed to start)"
fi

# SIGINT must kill EVERY stage of a foreground pipeline, not just one
# child pid.  A shell that never calls setpgid and forwards the signal to
# a single pid passes the single-process test above but fails here: the
# other sleep survives.  Correct shells put the job in its own process
# group and kill(-pgid, ...).
if start_bg_shell; then
    send_shell "sleep 5 | sleep 5"
    d=$(( $(date +%s) + 3 )); nsleep=0
    while :; do
        nsleep="$(pgrep -P "$SHELL_PID" -x sleep 2>/dev/null | wc -l)"
        [ "$nsleep" -ge 2 ] && break
        [ "$(date +%s)" -ge "$d" ] && break
        sleep 0.05
    done
    if [ "$nsleep" -ge 2 ]; then
        kill -INT "$SHELL_PID"
        s1=bad; s2=bad; s3=bad
        wait_for_no_child "sleep" 3 && s1=ok
        kill -0 "$SHELL_PID" 2>/dev/null && s2=ok
        send_shell "echo pipe_survived"
        wait_for_grep "pipe_survived" "$BGOUT" 3 && s3=ok
        if [ "$s1" = ok ] && [ "$s2" = ok ] && [ "$s3" = ok ]; then
            record PASS "SIGINT kills whole fg pipeline, shell survives"
        else
            record FAIL "SIGINT pipeline (all stages dead=$s1 alive=$s2 resumed=$s3)"
        fi
    else
        record FAIL "SIGINT pipeline: both stages never started (saw $nsleep)"
    fi
    stop_bg_shell
else
    record FAIL "SIGINT pipeline (shell failed to start)"
fi

# SIGINT while idle must not kill the shell
if start_bg_shell; then
    kill -INT "$SHELL_PID"
    send_shell "echo alive"
    if wait_for_grep "alive" "$BGOUT" 3; then
        record PASS "SIGINT while idle is ignored"
    else
        record FAIL "SIGINT while idle killed the shell"
    fi
    stop_bg_shell
else
    record FAIL "idle SIGINT (shell failed to start)"
fi

# ================================================================== #
# Results                                                            #
# ================================================================== #
echo
echo "==================== RESULTS ===================="
score=0
for i in 0 1 2 3 4; do
    p=${tp[$i]:-0}; t=${tt[$i]:-0}
    if [ "$t" -gt 0 ]; then sc=$(( weights[i] * p / t )); else sc=0; fi
    score=$((score + sc))
    printf '  %-28s %2d/%-2d   weight %2d%%   -> %2d%%\n' \
           "${tasks[$i]}" "$p" "$t" "${weights[$i]}" "$sc"
done
echo "------------------------------------------------"
printf '  TOTAL: %d passed, %d failed        SCORE: %d%%\n' "$pass" "$fail" "$score"
echo "================================================"

[ "$fail" -eq 0 ]
