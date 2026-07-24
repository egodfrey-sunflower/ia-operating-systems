#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 1, Parts 1-5.
#
# Builds msh in <workdir>, then for every case feeds the SAME script to your
# shell and to bash, each in its own private copy of the fixture tree, and
# compares:
#
#     stdout       byte for byte
#     exit status  exactly
#
# stderr is NOT compared. Diagnostic wording is your shell's own; the tests
# only require that a case which should complain does put *something* on
# stderr. Where that is required the case name says "(error)".
#
# Parts 1-4 are transcript diffs against bash. Part 5 cannot be: a background
# job's "[1] 12345" notice has no bash equivalent when bash is not interactive,
# and a pid is not reproducible. Those cases assert on elapsed time, on the
# text of `jobs`, and on the process table instead.
#
# Part 6 is not tested here at all. Process groups and Ctrl-C are terminal
# behaviour and cannot be driven down a pipe; Part 6 is a manual checklist in
# the handout.
#
# EVERY case runs under `timeout`. Part 4's classic bug -- a pipe write end
# left open in the parent -- does not produce a wrong answer, it produces no
# answer at all, and an ungraded grader would hang here rather than fail.
#
# Exits 0 only if every case passes. No root needed.

set -u
export LC_ALL=C
unset -f cat sort uniq head wc echo ls sleep true false 2>/dev/null || true

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
LABDIR=$(cd "$(dirname "$0")/.." && pwd -P)
MSH="$WORKDIR/msh"

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)

# Kill anything still running inside our scratch tree.
#
# `timeout` kills msh and nothing else. From Part 6 onward each pipeline stage
# lives in its OWN process group, so when msh dies its stages are reparented to
# init and keep blocking on a pipe that will never deliver EOF -- forever. The
# Part 4 bug this lab predicts leaks a whole pipeline per timed-out case, and a
# student iterating on it would accumulate them by the dozen.
#
# Every process the harness starts runs with its cwd inside $TMP (a fresh
# mktemp -d), so that is the membership test: precise, and incapable of
# touching anything the harness did not create.
sweep_strays() {
	local d p target
	for d in /proc/[0-9]*; do
		p=${d#/proc/}
		target=$(readlink "$d/cwd" 2>/dev/null) || continue
		case "$target" in
			"$TMP"|"$TMP"/*) kill -9 "$p" 2>/dev/null ;;
		esac
	done
}
trap 'sweep_strays; rm -rf "$TMP"' EXIT

TIMEOUT=10                 # per diff case
REF=bash

pass=0; fail=0
declare -a RESULTS
record() { # <name> <PASS|FAIL>
	RESULTS+=("$2|$1")
	if [ "$2" = PASS ]; then pass=$((pass+1)); else fail=$((fail+1)); fi
}

# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

echo "== building $WORKDIR =="
# CFLAGS is forced on the command line, which overrides the Makefile: the
# -Werror rule is part of the spec, so weakening it in your own Makefile must
# not weaken the grade.
GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g'
if make -C "$WORKDIR" clean >/dev/null 2>&1 &&
   make -C "$WORKDIR" all CFLAGS="$GRADE_CFLAGS" >"$TMP/build.log" 2>&1; then
	if grep -q -F "warning:" "$TMP/build.log"; then
		echo "build produced warnings (the spec is -Werror clean):" >&2
		grep -F "warning:" "$TMP/build.log" >&2
		echo; echo "RESULT: build failure"
		exit 1
	fi
	echo "build OK"
else
	echo "build FAILED:" >&2
	cat "$TMP/build.log" >&2
	echo; echo "RESULT: build failure"
	exit 1
fi
[ -x "$MSH" ] || { echo "missing binary: $MSH" >&2; echo "RESULT: build failure"; exit 1; }
echo

# ---------------------------------------------------------------------------
# fixtures
# ---------------------------------------------------------------------------

FIX0="$TMP/fix0"
if ! "$LABDIR/fixtures/genfixtures.sh" "$FIX0" >/dev/null 2>&1; then
	echo "run.sh: could not generate fixtures" >&2
	exit 2
fi

# ---------------------------------------------------------------------------
# the comparison engine
# ---------------------------------------------------------------------------
#
# Each run gets a pristine private copy of the fixture tree as its working
# directory, and HOME points inside it. So the two shells never see each
# other's output files, `cd` with no argument has a fixed answer, and every
# path in a test script can be relative -- which is what lets the identical
# script text run in two different directories.

WANT_STDERR=0      # set to 1 before a case that must report an error

xcase() { # <name> <script text>
	local name=$1 script=$2
	local rrc mrc ok=1 why=""

	rm -rf "$TMP/ref" "$TMP/my"
	cp -a "$FIX0" "$TMP/ref"
	cp -a "$FIX0" "$TMP/my"
	printf '%s' "$script" > "$TMP/script"

	( cd "$TMP/ref" && HOME="$TMP/ref/home" exec timeout "$TIMEOUT" "$REF" ) \
		< "$TMP/script" > "$TMP/r.out" 2> "$TMP/r.err"; rrc=$?
	( cd "$TMP/my"  && HOME="$TMP/my/home"  exec timeout "$TIMEOUT" "$MSH" ) \
		< "$TMP/script" > "$TMP/m.out" 2> "$TMP/m.err"; mrc=$?

	if [ "$mrc" = 124 ]; then
		sweep_strays
		ok=0; why=" TIMED OUT after ${TIMEOUT}s"
	else
		cmp -s "$TMP/r.out" "$TMP/m.out" || { ok=0; why="$why stdout"; }
		[ "$rrc" = "$mrc" ] || { ok=0; why="$why status(bash=$rrc, msh=$mrc)"; }
		if [ "$WANT_STDERR" = 1 ] && [ ! -s "$TMP/m.err" ]; then
			ok=0; why="$why stderr(nothing reported)"
		fi
	fi

	if [ "$ok" = 1 ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		{
			echo "  [$name] mismatch:$why"
			echo "    --- script ---"
			sed 's/^/    $ /' "$TMP/script"
			if [ "$mrc" = 124 ]; then
				echo "    Your shell never exited. That is what an unclosed pipe"
				echo "    write end looks like: some stage is still waiting for an"
				echo "    EOF that cannot arrive while the parent holds a copy."
			else
				echo "    --- stdout: bash (first 12 lines) ---"
				head -12 "$TMP/r.out" | sed 's/^/    | /'
				echo "    --- stdout: msh (first 12 lines) ---"
				head -12 "$TMP/m.out" | sed 's/^/    | /'
				if [ -s "$TMP/m.err" ]; then
					echo "    --- stderr: msh (not compared, shown to help) ---"
					head -6 "$TMP/m.err" | sed 's/^/    | /'
				fi
			fi
		} >&2
	fi
	WANT_STDERR=0
}

# ---------------------------------------------------------------------------
# Part 1 -- REPL and builtins
# ---------------------------------------------------------------------------

xcase "Part 1: a command runs at all" \
'echo hello
'

xcase "Part 1: blank and whitespace-only lines are skipped" \
'

echo   spaced   out

'

xcase "Part 1: EOF ends the shell (last line has no newline)" \
'echo one
echo two'

xcase "Part 1: exit stops reading immediately" \
'echo one
exit
echo two
'

xcase "Part 1: exit N is the shell status" \
'echo one
exit 3
'

xcase "Part 1: cd changes the shell own directory" \
'cat marker.txt
cd sub
cat marker.txt
'

xcase "Part 1: cd .. goes back up" \
'cd sub
cd ../home
cat marker.txt
'

xcase "Part 1: cd with no argument goes to HOME" \
'cd
cat marker.txt
'

WANT_STDERR=1
xcase "Part 1: cd to a missing directory (error)" \
'cd no_such_dir
'

xcase "Part 1: a failed cd leaves the shell where it was" \
'cd no_such_dir
cat marker.txt
'

# ---------------------------------------------------------------------------
# Part 2 -- fork, exec, wait
# ---------------------------------------------------------------------------

xcase "Part 2: arguments reach the program" \
'/bin/echo one two three
'

xcase "Part 2: the shell waits for the child before the next command" \
'./slowprint.sh
echo B
'

xcase "Part 2: a non-zero exit status is kept" \
'false
'

xcase "Part 2: a later success clears an earlier failure" \
'false
/bin/echo recovered
'

WANT_STDERR=1
xcase "Part 2: command not found is status 127 (error)" \
'no_such_command_xyz
'

WANT_STDERR=1
xcase "Part 2: a non-executable file is status 126 (error)" \
'./noexec.txt
'

xcase "Part 2: the shell survives a failed exec and carries on" \
'no_such_command_xyz
echo still alive
'

xcase "Part 2: several commands in a row" \
'true
/bin/echo a
false
/bin/echo b
'

# ---------------------------------------------------------------------------
# Part 3 -- redirection
# ---------------------------------------------------------------------------

xcase "Part 3: > creates the file and fills it" \
'echo hello > out/f
cat out/f
'

xcase "Part 3: > does not leak into the next command" \
'echo one > out/f
echo two
cat out/f
'

xcase "Part 3: > truncates an existing file" \
'echo aaaaa > out/f
echo b > out/f
cat out/f
'

xcase "Part 3: < feeds the program stdin" \
'wc -l < lines.txt
'

xcase "Part 3: < does not leak into the next command" \
'wc -l < lines.txt
wc -l < words.txt
'

xcase "Part 3: < and > on the same command" \
'sort < words.txt > out/s
cat out/s
'

xcase "Part 3: the operator needs no surrounding space" \
'echo hi>out/f
cat out/f
'

WANT_STDERR=1
xcase "Part 3: < on a missing file (error)" \
'cat < no_such_file.txt
'

WANT_STDERR=1
xcase "Part 3: > into a directory that does not exist (error)" \
'echo x > no_such_dir/f
'

xcase "Part 3: a failed redirection does not run the command" \
'cat < no_such_file.txt
echo after
'

# ---------------------------------------------------------------------------
# Part 4 -- pipelines
# ---------------------------------------------------------------------------

xcase "Part 4: two stages" \
'cat words.txt | sort
'

xcase "Part 4: three stages" \
'cat words.txt | sort | uniq -c
'

xcase "Part 4: four stages" \
'cat words.txt | sort | uniq -c | head -3
'

xcase "Part 4: six stages terminate" \
'cat lines.txt | cat | cat | cat | cat | wc -l
'

xcase "Part 4: pipeline with < at the head and > at the tail" \
'sort < words.txt | uniq -c | head -2 > out/p
cat out/p
'

xcase "Part 4: pipeline status is the last stage status" \
'cat words.txt | false
'

xcase "Part 4: a failing first stage does not change the status" \
'false | /bin/echo ok
'

xcase "Part 4: a stage that exits early does not wedge the shell" \
'cat lines.txt | head -1
'

# A shell that waits only for the LAST stage returns to the prompt while the
# first stage is still asleep, so the marker does not exist yet and the `cat`
# fails. bash waits for every stage, so it prints LATE.
xcase "Part 4: the shell waits for every stage, not just the last" \
'./latewrite.sh | true
cat out/late.txt
'

xcase "Part 4: pipelines back to back" \
'cat lines.txt | wc -l
cat lines.txt | sort | wc -l
cat lines.txt | cat | cat | wc -l
echo done
'

# ls inherits every descriptor the shell had open. After four pipelines the
# listing must still be 0, 1, 2 and the descriptor ls opened for itself.
xcase "Part 4: no pipe descriptors leak into a later child" \
'cat words.txt | sort | uniq -c | wc -l
ls /proc/self/fd
'

WANT_STDERR=1
xcase "Part 4: a trailing | is a syntax error (error)" \
'echo a |
'

# ---------------------------------------------------------------------------
# Part 5 -- background jobs and reaping
#
# Not diffs. A "[1] 12345" notice has no non-interactive bash equivalent and
# the pid is not reproducible, so these assert on behaviour instead: the
# prompt comes back before the job finishes, `jobs` names it, job numbers are
# distinct, and nothing is left as a zombie.
# ---------------------------------------------------------------------------

# Run <script> under msh in a private fixture copy. Sets MRC, and leaves
# stdout in $TMP/b.out.
bgrun() { # <script> <timeout>
	rm -rf "$TMP/my"; cp -a "$FIX0" "$TMP/my"
	printf '%s' "$1" > "$TMP/script"
	( cd "$TMP/my" && HOME="$TMP/my/home" exec timeout "$2" "$MSH" ) \
		< "$TMP/script" > "$TMP/b.out" 2> "$TMP/b.err"
	MRC=$?
	[ "$MRC" = 124 ] && sweep_strays
	return 0
}

bgfail() { # <name> <why>
	record "$1" FAIL
	{
		echo "  [$1] $2"
		echo "    --- stdout: msh ---"
		head -12 "$TMP/b.out" | sed 's/^/    | /'
		if [ -s "$TMP/b.err" ]; then
			echo "    --- stderr: msh ---"
			head -6 "$TMP/b.err" | sed 's/^/    | /'
		fi
	} >&2
}

# --- 5a: & returns before the job finishes ---------------------------------
# `sleep 6 &` then two more commands. If the shell waits, the whole thing
# takes 6s; if it does not, it takes almost none. The threshold is 3s: half
# the sleep, and twenty times the work actually being done.
START=$(date +%s)
bgrun 'sleep 6 &
echo back at the prompt
' 15
ELAPSED=$(( $(date +%s) - START ))
if [ "$MRC" = 124 ]; then
	bgfail "Part 5: & returns before the job finishes" "timed out"
elif [ "$ELAPSED" -ge 3 ]; then
	bgfail "Part 5: & returns before the job finishes" \
	       "took ${ELAPSED}s; the shell waited for a background job"
elif ! grep -q -F "back at the prompt" "$TMP/b.out"; then
	bgfail "Part 5: & returns before the job finishes" \
	       "the command after the background job produced no output"
else
	record "Part 5: & returns before the job finishes" PASS
fi

# --- 5b: jobs lists the running job ----------------------------------------
bgrun 'sleep 6 &
jobs
' 15
if [ "$MRC" = 124 ]; then
	bgfail "Part 5: jobs lists a running background job" "timed out"
elif ! grep -q -F "sleep 6" "$TMP/b.out"; then
	bgfail "Part 5: jobs lists a running background job" \
	       "no line of output names the running command"
else
	record "Part 5: jobs lists a running background job" PASS
fi

# --- 5c: job numbers are distinct ------------------------------------------
bgrun 'sleep 6 &
sleep 7 &
jobs
' 15
if [ "$MRC" = 124 ]; then
	bgfail "Part 5: two background jobs get different job numbers" "timed out"
elif ! grep -q -F "[1]" "$TMP/b.out" || ! grep -q -F "[2]" "$TMP/b.out"; then
	bgfail "Part 5: two background jobs get different job numbers" \
	       "expected a [1] and a [2] in the output"
else
	record "Part 5: two background jobs get different job numbers" PASS
fi

# --- 5d: no zombie is left behind ------------------------------------------
#
# Timing here is deliberately loose. The shell is fed one line at a time down
# a fifo so that we control when it is idle: a background `sleep 0.2`, then
# 1.5s of nothing, then a foreground command -- which gives a "sweep at the
# prompt" implementation its chance to reap -- then another 1.5s before the
# process table is read. A SIGCHLD handler will have reaped long before.

zombie_children() { # <ppid> -- print the pid of every zombie child
	local ppid=$1 d line rest state parent
	for d in /proc/[0-9]*; do
		[ -r "$d/stat" ] || continue
		read -r line < "$d/stat" 2>/dev/null || continue
		rest=${line##*) }
		set -- $rest
		[ $# -ge 2 ] || continue
		state=$1; parent=$2
		if [ "$parent" = "$ppid" ] && [ "$state" = "Z" ]; then
			echo "${d#/proc/}"
		fi
	done
}

rm -rf "$TMP/my"; cp -a "$FIX0" "$TMP/my"
rm -f "$TMP/fifo"; mkfifo "$TMP/fifo"
( cd "$TMP/my" && HOME="$TMP/my/home" exec "$MSH" ) \
	< "$TMP/fifo" > "$TMP/b.out" 2> "$TMP/b.err" &
MSHPID=$!
exec 9> "$TMP/fifo"
printf './bgjob.sh &\n' >&9
sleep 1.5
printf '/bin/true\n' >&9
sleep 1.5
ZOMBIES=$(zombie_children "$MSHPID")
RAN=0; [ -f "$TMP/my/out/bg.done" ] && RAN=1
printf 'exit\n' >&9
exec 9>&-
wait "$MSHPID" 2>/dev/null
if [ "$RAN" != 1 ]; then
	bgfail "Part 5: a finished background job leaves no zombie" \
	       "the background job never ran (no out/bg.done after 3s)"
elif [ -n "$ZOMBIES" ]; then
	bgfail "Part 5: a finished background job leaves no zombie" \
	       "zombie child process(es) of the shell: $(echo $ZOMBIES)"
else
	record "Part 5: a finished background job leaves no zombie" PASS
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
echo "(Part 6 is a manual checklist in README.md and is not counted here.)"
[ "$fail" -eq 0 ]
