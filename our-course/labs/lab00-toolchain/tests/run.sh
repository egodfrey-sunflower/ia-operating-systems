#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 0, Part 3.
#
# Builds mycat/myls/mygrep in <workdir>, generates the fixture tree, then for
# every case runs YOUR tool and the SYSTEM tool with identical arguments and
# identical stdin, and diffs all three observable outputs:
#
#     stdout   byte for byte
#     stderr   byte for byte, after rewriting the leading program name
#              ("mycat: " -> "cat: ") so only the wording is compared
#     exit status
#
# References: cat for mycat, `ls -1` for myls, `grep -F` for mygrep.
#
# Exits 0 only if every case passes; on failure it names the case and shows
# the difference. No root, no QEMU, no cross-compiler needed.

set -u
export LC_ALL=C          # byte-value sort order and C-locale messages
unset -f grep ls cat sort sed 2>/dev/null || true   # no shell-function shims
unset GREP_OPTIONS GREP_COLORS 2>/dev/null || true

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd)
LABDIR=$(cd "$(dirname "$0")/.." && pwd)

MYCAT="$WORKDIR/mycat"
MYLS="$WORKDIR/myls"
MYGREP="$WORKDIR/mygrep"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
TO="timeout 10"

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
for b in "$MYCAT" "$MYLS" "$MYGREP"; do
	[ -x "$b" ] || { echo "missing binary: $b" >&2; echo "RESULT: build failure"; exit 1; }
done
echo

# ---------------------------------------------------------------------------
# no-stdio gate
# ---------------------------------------------------------------------------
#
# The handout bans stdio. Check each binary's *undefined* symbols. Names are
# matched exactly (never as substrings), after stripping glibc's "@GLIBC_x"
# version suffix and its fortified/ISO-C99 alias spellings, so that a legit
# open/read never trips a rule about fopen/fread.
#
# opendir/readdir/closedir are deliberately NOT banned -- they are the one
# stated exception, since there is no portable lower-level directory read.

STDIO_BANNED="printf fprintf sprintf snprintf vprintf vfprintf vsprintf \
vsnprintf dprintf vdprintf puts fputs putc putchar fputc putw getc getchar \
fgetc fgets getline getdelim ungetc getw scanf fscanf sscanf vscanf vfscanf \
vsscanf fopen fopen64 fdopen freopen freopen64 fclose fread fwrite fflush \
fseek fseeko ftell ftello rewind fgetpos fsetpos setbuf setbuffer setlinebuf \
setvbuf perror tmpfile tmpfile64 popen pclose fmemopen open_memstream \
clearerr feof ferror fileno \
asprintf vasprintf err verr errx verrx warn vwarn warnx vwarnx error"

check_no_stdio() { # <name> <binary>
	local name=$1 bin=$2 bad="" s base b
	for s in $( { nm -u "$bin" 2>/dev/null; nm -Du "$bin" 2>/dev/null; } \
			| awk '{print $NF}' | sed 's/@.*$//' | sort -u); do
		base=${s#__}; base=${base%_chk}; base=${base#isoc99_}
		for b in $STDIO_BANNED; do
			if [ "$s" = "$b" ] || [ "$base" = "$b" ]; then bad="$bad $s"; break; fi
		done
	done
	if [ -z "$bad" ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		echo "  [$name] stdio symbol(s) referenced:$bad" >&2
	fi
}

check_no_stdio "no-stdio gate: mycat"  "$MYCAT"
check_no_stdio "no-stdio gate: myls"   "$MYLS"
check_no_stdio "no-stdio gate: mygrep" "$MYGREP"

# ---------------------------------------------------------------------------
# fixtures
# ---------------------------------------------------------------------------

FIX="$TMP/fix"
if ! "$LABDIR/fixtures/genfixtures.sh" "$FIX" >/dev/null 2>&1; then
	echo "run.sh: could not generate fixtures" >&2
	exit 2
fi

# ---------------------------------------------------------------------------
# comparison engine
# ---------------------------------------------------------------------------
#
# RUN_STDIN    -- file redirected onto stdin (regular file: read() returns the
#                 full buffer every time until the last one)
# RUN_PIPE     -- file piped onto stdin
# RUN_TRICKLE  -- file piped onto stdin in two bursts with a pause between, so
#                 the FIRST read() comes back short with data still to come.
#                 This is the case that kills "n = read(...); done" code.
# All empty => stdin is /dev/null.

RUN_STDIN=""; RUN_PIPE=""; RUN_TRICKLE=""
STDERR_MODE=exact      # exact | nonempty   (nonempty: only usage text differs)

reset_run() { RUN_STDIN=""; RUN_PIPE=""; RUN_TRICKLE=""; STDERR_MODE=exact; }

trickle() { # <file> -- write it to stdout in two bursts
	head -c 100000 "$1"; sleep 0.3; tail -c +100001 "$1"
}

# compare3 <case-name> <ref-argv0> <ref-extra-args> <mybinary> [args...]
compare3() {
	local name=$1 refprog=$2 refargs=$3 mybin=$4; shift 4
	local rrc mrc mybase refbase
	mybase=$(basename "$mybin"); refbase=$refprog

	# $refargs are the flags that make the SYSTEM tool behave as specified
	# (`ls -1`, `grep -F`); your tool is never given them.
	if [ -n "$RUN_TRICKLE" ]; then
		trickle "$RUN_TRICKLE" | $TO "$refprog" $refargs "$@" >"$TMP/r.out" 2>"$TMP/r.err"; rrc=$?
		trickle "$RUN_TRICKLE" | $TO "$mybin"            "$@" >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
	elif [ -n "$RUN_PIPE" ]; then
		cat "$RUN_PIPE" | $TO "$refprog" $refargs "$@" >"$TMP/r.out" 2>"$TMP/r.err"; rrc=$?
		cat "$RUN_PIPE" | $TO "$mybin"            "$@" >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
	elif [ -n "$RUN_STDIN" ]; then
		$TO "$refprog" $refargs "$@" >"$TMP/r.out" 2>"$TMP/r.err" <"$RUN_STDIN"; rrc=$?
		$TO "$mybin"            "$@" >"$TMP/m.out" 2>"$TMP/m.err" <"$RUN_STDIN"; mrc=$?
	else
		$TO "$refprog" $refargs "$@" >"$TMP/r.out" 2>"$TMP/r.err" </dev/null; rrc=$?
		$TO "$mybin"            "$@" >"$TMP/m.out" 2>"$TMP/m.err" </dev/null; mrc=$?
	fi

	# rewrite "myls: " -> "ls: " at the start of each diagnostic line
	sed "s|^$mybase: |$refbase: |" "$TMP/m.err" > "$TMP/m.err.n"

	local ok=1 why=""
	cmp -s "$TMP/r.out" "$TMP/m.out" || { ok=0; why="$why stdout"; }
	if [ "$STDERR_MODE" = exact ]; then
		cmp -s "$TMP/r.err" "$TMP/m.err.n" || { ok=0; why="$why stderr"; }
	else
		[ -s "$TMP/r.err" ] && [ ! -s "$TMP/m.err" ] && { ok=0; why="$why stderr(empty)"; }
	fi
	[ "$rrc" = "$mrc" ] || { ok=0; why="$why status($refbase=$rrc, $mybase=$mrc)"; }

	if [ "$ok" = 1 ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		{
			echo "  [$name] mismatch:$why"
			echo "    ran: $mybase $*"
			if ! cmp -s "$TMP/r.out" "$TMP/m.out"; then
				echo "    --- stdout: expected (first 10 lines) ---"
				head -10 "$TMP/r.out" | sed 's/^/    | /'
				echo "    --- stdout: yours (first 10 lines) ---"
				head -10 "$TMP/m.out" | sed 's/^/    | /'
			fi
			if [ "$STDERR_MODE" = exact ] && ! cmp -s "$TMP/r.err" "$TMP/m.err.n"; then
				echo "    --- stderr: expected ---"; sed 's/^/    | /' "$TMP/r.err"
				echo "    --- stderr: yours (name-normalised) ---"; sed 's/^/    | /' "$TMP/m.err.n"
			fi
		} >&2
	fi
	reset_run
}

mycat()  { compare3 "$1" cat  ""    "$MYCAT"  "${@:2}"; }
myls()   { compare3 "$1" ls   "-1"  "$MYLS"   "${@:2}"; }
mygrep() { compare3 "$1" grep "-F"  "$MYGREP" "${@:2}"; }

# ---------------------------------------------------------------------------
# mycat
# ---------------------------------------------------------------------------

mycat "mycat: ordinary file"            "$FIX/hello.txt"
mycat "mycat: empty file"               "$FIX/empty.txt"
mycat "mycat: no trailing newline"      "$FIX/nonewline.txt"
mycat "mycat: binary safe (NUL bytes)"  "$FIX/binary.bin"
mycat "mycat: large file, many reads"   "$FIX/large.txt"
mycat "mycat: several files at once"    "$FIX/hello.txt" "$FIX/empty.txt" \
                                        "$FIX/nonewline.txt" "$FIX/colours.txt"

RUN_STDIN="$FIX/hello.txt"; mycat "mycat: no arguments reads stdin"
RUN_PIPE="$FIX/large.txt";  mycat "mycat: stdin from a pipe"
RUN_TRICKLE="$FIX/large.txt"; mycat "mycat: short read mid-stream"
RUN_STDIN="$FIX/colours.txt"; mycat "mycat: '-' operand means stdin" \
                                        "$FIX/hello.txt" -

mycat "mycat: missing file"             "$FIX/nope.txt"
mycat "mycat: missing then good file"   "$FIX/nope.txt" "$FIX/hello.txt"
# opens successfully, then read() fails with EISDIR -- the case that hangs a
# `while (read(...) != 0)` loop forever.
mycat "mycat: a directory fails at read, not open" "$FIX/tree"

# ---------------------------------------------------------------------------
# myls
# ---------------------------------------------------------------------------

myls "myls: directory, sorted, dotfile hidden" "$FIX/tree"
myls "myls: nested subdirectory"               "$FIX/tree/sub"
myls "myls: directory that lists as empty"     "$FIX/tree/sub/deeper"
myls "myls: empty directory"                   "$FIX/emptydir"
myls "myls: fixture root"                      "$FIX"
myls "myls: plain file operand"                "$FIX/hello.txt"
myls "myls: missing path"                      "$FIX/nope.txt"

# no arguments => list "."
( cd "$FIX/tree" && $TO ls -1 ) >"$TMP/r.out" 2>/dev/null
( cd "$FIX/tree" && $TO "$MYLS" ) >"$TMP/m.out" 2>/dev/null; mrc=$?
if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$mrc" = 0 ]; then
	record "myls: no arguments lists ." PASS
else
	record "myls: no arguments lists ." FAIL
	echo "  [myls: no arguments lists .] output or status differs from 'ls -1'" >&2
fi

# ---------------------------------------------------------------------------
# mygrep
# ---------------------------------------------------------------------------

mygrep "mygrep: substring match"          green "$FIX/colours.txt"
mygrep "mygrep: no match, status 1"       purple "$FIX/colours.txt"
mygrep "mygrep: empty file"               green "$FIX/empty.txt"
mygrep "mygrep: line without newline"     newline "$FIX/nonewline.txt"
# byte 65534 of large.txt -- straddles the 64 KiB boundary, so the pattern is
# split across two read() calls for any buffer of 64 KiB or less.
mygrep "mygrep: pattern spanning a read"  "line 001057" "$FIX/large.txt"
mygrep "mygrep: two files get FILE: prefix" e "$FIX/hello.txt" "$FIX/colours.txt"
mygrep "mygrep: mid-word substring"       reen "$FIX/colours.txt"

RUN_STDIN="$FIX/colours.txt"; mygrep "mygrep: reads stdin when no file" green
RUN_STDIN="$FIX/colours.txt"; mygrep "mygrep: '-' operand means stdin" green -
mygrep "mygrep: empty pattern matches every line" "" "$FIX/colours.txt"
# byte 105400 -- past the trickle harness's 100000-byte split, so a mygrep that
# treats a short read as end of input will miss it entirely.
RUN_TRICKLE="$FIX/large.txt"; mygrep "mygrep: short read mid-stream" \
                                     "line 001700"

mygrep "mygrep: missing file"             green "$FIX/nope.txt"
mygrep "mygrep: missing plus good file"   green "$FIX/nope.txt" "$FIX/colours.txt"

# no arguments at all: both must fail with status 2 and say something on
# stderr, but the exact usage wording is each program's own.
STDERR_MODE=nonempty; mygrep "mygrep: no arguments is an error"

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
[ "$fail" -eq 0 ]
