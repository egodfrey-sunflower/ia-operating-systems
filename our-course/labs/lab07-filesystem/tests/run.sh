#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 7, Part 1 (the ch. 39 tools).
#
# Four tools -- mystat, myls, mytail, myfind -- diffed against the system
# equivalents on a generated fixture tree, exactly as Lab 0 did:
#
#     mystat PATH...     vs   stat --printf='name: %n\ntype: %F\n...' PATH...
#     myls -l DIR        vs   `stat --printf='%A %h %s %n'` per sorted entry
#     mytail -n N FILE   vs   tail -n N FILE
#     myfind START NAME  vs   find START -name NAME        (both sorted)
#
# For each tool the harness DOES THE GRADED BUILD ITSELF -- its own gcc line,
# its own -Wall -Wextra -Werror -- so a Makefile that quietly drops -Werror
# cannot change the grade.
#
# mytail additionally has a read-budget case: it runs mytail under strace on a
# 4 MiB file and checks that the bytes it reads (the whole read family) or
# maps (mmap of the file) are a small fraction of the file, so a tool that
# prints the right lines by streaming the whole file fails even though its
# output is correct. That case SKIPs (it does not pass, and it does not fail)
# if strace cannot trace on this machine.
#
# Every verdict is a property of the tool's output (and, for the budget case,
# its syscalls) under a fixed input. Exits 0 only if every case passes.
# No root, no QEMU, no cross-compiler.

set -u
export LC_ALL=C
unset -f stat ls tail find sort diff cmp 2>/dev/null || true

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
LABDIR=$(cd "$(dirname "$0")/.." && pwd -P)

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)
trap 'rm -rf "$TMP"' EXIT
TO="timeout 20"

pass=0; fail=0; skip=0
declare -a RESULTS
record() { # <name> <PASS|FAIL|SKIP>
	RESULTS+=("$2|$1")
	case $2 in
		PASS) pass=$((pass+1)) ;;
		SKIP) skip=$((skip+1)) ;;
		*)    fail=$((fail+1)) ;;
	esac
}

# ---------------------------------------------------------------------------
# graded build -- each tool compiled by the harness, never by make
# ---------------------------------------------------------------------------

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g'
echo "== building $WORKDIR =="
for t in mystat myls mytail myfind; do
	src="$WORKDIR/$t.c"
	[ -f "$src" ] || { echo "missing source: $t.c" >&2; echo "RESULT: build failure"; exit 1; }
	if ! gcc $GRADE_CFLAGS -o "$TMP/$t" "$src" >"$TMP/$t.build" 2>&1; then
		echo "graded build FAILED ($t.c):" >&2
		cat "$TMP/$t.build" >&2
		echo; echo "RESULT: build failure"; exit 1
	fi
	if grep -q -F "warning:" "$TMP/$t.build"; then
		echo "$t.c produced warnings (the spec is -Werror clean):" >&2
		grep -F "warning:" "$TMP/$t.build" >&2
		echo; echo "RESULT: build failure"; exit 1
	fi
done
MYSTAT="$TMP/mystat"; MYLS="$TMP/myls"; MYTAIL="$TMP/mytail"; MYFIND="$TMP/myfind"
echo "build OK"; echo

# ---------------------------------------------------------------------------
# fixtures
# ---------------------------------------------------------------------------

FIX="$TMP/fix"
if ! "$LABDIR/fixtures/genfixtures.sh" "$FIX" >/dev/null 2>&1; then
	echo "run.sh: could not generate fixtures" >&2
	exit 2
fi
META="$FIX/meta"; TAIL="$FIX/tail"; FIND="$FIX/find"

show() { # dump expected vs actual for a failing case
	echo "    --- expected (first 8 lines) ---"; head -8 "$TMP/r.out" | sed 's/^/    | /'
	echo "    --- yours (first 8 lines) ---";     head -8 "$TMP/m.out" | sed 's/^/    | /'
}

# ---------------------------------------------------------------------------
# mystat  (diff stdout + exit status against `stat --printf`; stderr wording,
# which drifts between coreutils versions, is not compared)
# ---------------------------------------------------------------------------

SFMT='name: %n\ntype: %F\nsize: %s\nlinks: %h\nmode: %a %A\n'

mystat_case() { # <name> <path...>
	local name=$1; shift
	stat --printf="$SFMT" "$@" >"$TMP/r.out" 2>/dev/null; local rrc=$?
	$TO "$MYSTAT" "$@"         >"$TMP/m.out" 2>/dev/null; local mrc=$?
	if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$rrc" = "$mrc" ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		{ echo "  [$name] differs from stat (status: stat=$rrc, yours=$mrc)"; show; } >&2
	fi
}

mystat_case "mystat: regular file, mode 644"        "$META/reg644"
mystat_case "mystat: executable, mode 755"          "$META/exec755"
mystat_case "mystat: empty file (regular empty)"    "$META/empty"
mystat_case "mystat: directory (type + nlink 2)"    "$META/subdir"
mystat_case "mystat: symlink reports the link"      "$META/link_to_reg"
mystat_case "mystat: dangling symlink still stats"  "$META/dangling"
mystat_case "mystat: set-user-ID bit in mode"       "$META/setuid"
mystat_case "mystat: set-group-ID bit in mode"      "$META/setgid"
mystat_case "mystat: sticky bit in mode"            "$META/sticky"
mystat_case "mystat: hard-linked file (nlink 2)"    "$META/hard_a"
mystat_case "mystat: character device"              "/dev/null"
mystat_case "mystat: several paths at once"         "$META/reg644" "$META/subdir" "$META/link_to_reg"
mystat_case "mystat: good then missing, exit 1"     "$META/reg644" "$META/no_such"
mystat_case "mystat: missing only, exit 1"          "$META/no_such"

# no operands at all: usage error, exit 2 (stat's own no-operand status differs,
# so this is a direct check, not a diff)
$TO "$MYSTAT" >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
if [ "$mrc" = 2 ]; then
	record "mystat: no operands is exit 2" PASS
else
	record "mystat: no operands is exit 2" FAIL
	echo "  [mystat: no operands is exit 2] wanted exit 2, got status=$mrc" >&2
fi

# ---------------------------------------------------------------------------
# myls -l  (diff against a stat-built oracle: one `stat` line per sorted,
# non-dot entry)
# ---------------------------------------------------------------------------

oracle_ls() { # <dir> -> stdout
	( cd "$1" && LC_ALL=C ls -1 | while IFS= read -r e; do
		stat --printf='%A %h %s %n\n' "$e"
	done )
}

myls_case() { # <name> <dir>
	local name=$1 dir=$2
	oracle_ls "$dir"       >"$TMP/r.out" 2>/dev/null
	$TO "$MYLS" -l "$dir"  >"$TMP/m.out" 2>/dev/null; local mrc=$?
	if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$mrc" = 0 ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		{ echo "  [$name] differs from stat oracle (status yours=$mrc)"; show; } >&2
	fi
}

myls_case "myls: mixed directory, sorted, dotfile hidden" "$META"
myls_case "myls: empty directory prints nothing"          "$META/subdir"
myls_case "myls: find tree top level"                     "$FIND"

# no DIR argument means "."
( cd "$META" && oracle_ls . ) >"$TMP/r.out" 2>/dev/null
( cd "$META" && $TO "$MYLS" -l ) >"$TMP/m.out" 2>/dev/null; mrc=$?
if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$mrc" = 0 ]; then
	record "myls: no DIR argument lists ." PASS
else
	record "myls: no DIR argument lists ." FAIL
	{ echo "  [myls: no DIR argument lists .] differs (status=$mrc)"; show; } >&2
fi

# a directory that cannot be opened: exit 2, something on stderr
$TO "$MYLS" -l "$META/no_such_dir" >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
if [ "$mrc" = 2 ] && [ -s "$TMP/m.err" ] && [ ! -s "$TMP/m.out" ]; then
	record "myls: missing directory is exit 2" PASS
else
	record "myls: missing directory is exit 2" FAIL
	echo "  [myls: missing directory is exit 2] wanted exit 2 + stderr, got status=$mrc" >&2
fi

# missing -l flag: usage error, exit 2
$TO "$MYLS" "$META" >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
if [ "$mrc" = 2 ]; then
	record "myls: missing -l is a usage error" PASS
else
	record "myls: missing -l is a usage error" FAIL
	echo "  [myls: missing -l is a usage error] wanted exit 2, got status=$mrc" >&2
fi

# ---------------------------------------------------------------------------
# mytail  (diff stdout + exit status against `tail -n N FILE`)
# ---------------------------------------------------------------------------

mytail_case() { # <name> <n> <file>
	local name=$1 n=$2 file=$3
	tail -n "$n" "$file"          >"$TMP/r.out" 2>/dev/null; local rrc=$?
	$TO "$MYTAIL" -n "$n" "$file" >"$TMP/m.out" 2>/dev/null; local mrc=$?
	if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$rrc" = "$mrc" ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		{ echo "  [$name] differs from tail -n $n (status: tail=$rrc, yours=$mrc)"; show; } >&2
	fi
}

mytail_case "mytail: last 1 of 20 lines"              1  "$TAIL/lines20"
mytail_case "mytail: last 3 of 20 lines"              3  "$TAIL/lines20"
mytail_case "mytail: last 5 of 20 lines"              5  "$TAIL/lines20"
mytail_case "mytail: n = 0 prints nothing"            0  "$TAIL/lines20"
mytail_case "mytail: n larger than file prints all"   25 "$TAIL/lines20"
mytail_case "mytail: last line has no newline"        1  "$TAIL/nonl"
mytail_case "mytail: last 2, no trailing newline"     2  "$TAIL/nonl"
mytail_case "mytail: single line, no newline"         1  "$TAIL/oneline_nonl"
mytail_case "mytail: empty file prints nothing"       1  "$TAIL/empty"
mytail_case "mytail: last 10 of a 4 MiB file"         10 "$TAIL/big"

# missing file: exit 1, something on stderr
$TO "$MYTAIL" -n 1 "$TAIL/no_such" >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
if [ "$mrc" = 1 ] && [ -s "$TMP/m.err" ]; then
	record "mytail: missing file is exit 1" PASS
else
	record "mytail: missing file is exit 1" FAIL
	echo "  [mytail: missing file is exit 1] wanted exit 1 + stderr, got status=$mrc" >&2
fi

# ---- the read-budget case: seek from the end, do not stream the file --------
#
# Correct OUTPUT alone cannot tell a seek from a whole-file read, so this case
# runs mytail under strace and charges it for the file bytes it pulls in:
#
#   - bytes returned by the whole read family -- read, pread64, readv, preadv,
#     preadv2 -- on ANY descriptor (so staging the data through a scratch file
#     and reading that back is charged too), plus
#   - the length of every mmap of the data file itself. Mapped bytes arrive by
#     page fault, invisible to read-syscall accounting, so the mapping is
#     charged up front; strace -y names the file behind each descriptor, which
#     is how the map is tied to the data file.
#
# The 4 MiB file's last 10 lines are a few hundred bytes; a seeking tool is
# charged a few KiB, a whole-file streamer -- via read, pread, readv or mmap --
# at least 4 MiB. Not charged: kernel-side copies that never surface the bytes
# to the process (sendfile, splice, copy_file_range, io_uring); but to choose
# the right lines a tool must get the bytes back somehow, and reading a staged
# copy is charged like any other read. The case also re-checks the output, so
# a tool that reads nothing (and prints nothing) fails here too.

BUDGET=1048576          # 1 MiB; file is 4 MiB, a correct tail reads ~KiB
TRACE_CALLS=read,pread64,readv,preadv,preadv2,mmap
charged_bytes() { # <log> <datafile>: read-family returns + mmap lengths of <datafile>
	awk -v f="$2" '
		/ mmap\(/ {
			if (index($0, "<" f ">")) {
				n = split($0, a, ", ")
				if (n >= 2 && a[2] ~ /^[0-9]+$/) s += a[2]
			}
			next
		}
		NF > 1 { v = $NF; if (v ~ /^[0-9]+$/) s += v }
		END { print s + 0 }
	' "$1"
}

strace_ok=0
if command -v strace >/dev/null 2>&1; then
	if strace -f -y -e trace="$TRACE_CALLS" -o "$TMP/probe.log" /bin/true >/dev/null 2>"$TMP/probe.err" \
	   && [ -s "$TMP/probe.log" ]; then
		strace_ok=1
	fi
fi

if [ "$strace_ok" = 1 ]; then
	tail -n 10 "$TAIL/big" >"$TMP/r.out" 2>/dev/null
	strace -f -y -e trace="$TRACE_CALLS" -o "$TMP/tail.log" \
		$TO "$MYTAIL" -n 10 "$TAIL/big" >"$TMP/m.out" 2>/dev/null
	bytes=$(charged_bytes "$TMP/tail.log" "$TAIL/big")
	if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$bytes" -lt "$BUDGET" ]; then
		record "mytail: seeks from end (read/mmap < 1 MiB of 4 MiB)" PASS
	else
		record "mytail: seeks from end (read/mmap < 1 MiB of 4 MiB)" FAIL
		echo "  [mytail: seeks from end] charged $bytes bytes (budget $BUDGET:" \
		     "read family on any fd + mmap of the file) and/or output differed" >&2
	fi
else
	record "mytail: seeks from end (read/mmap < 1 MiB of 4 MiB)" SKIP
	echo "  [mytail: seeks from end] SKIPPED: strace cannot trace here" \
	     "-- the seek requirement is UNCHECKED on this machine." >&2
fi

# ---------------------------------------------------------------------------
# myfind  (both outputs sorted, then diffed; exit status compared)
# ---------------------------------------------------------------------------

myfind_case() { # <name> <start> <name-to-find>
	local name=$1 start=$2 needle=$3
	find "$start" -name "$needle" 2>/dev/null | LC_ALL=C sort >"$TMP/r.out"; local rrc=${PIPESTATUS[0]}
	$TO "$MYFIND" "$start" "$needle" 2>/dev/null | LC_ALL=C sort >"$TMP/m.out"; local mrc=${PIPESTATUS[0]}
	if cmp -s "$TMP/r.out" "$TMP/m.out" && [ "$rrc" = "$mrc" ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		{ echo "  [$name] differs from find -name (status: find=$rrc, yours=$mrc)"; show; } >&2
	fi
}

myfind_case "myfind: whole-name match at three depths"  "$FIND" "foo"
myfind_case "myfind: case matters (FOO is not foo)"     "$FIND" "FOO"
myfind_case "myfind: deep match, four levels down"      "$FIND" "deep_target"
myfind_case "myfind: a directory name matches"          "$FIND" "needle_dir"
myfind_case "myfind: no matches, exit 0"                "$FIND" "nothing_here"
myfind_case "myfind: walk an empty subdirectory"        "$FIND/emptysub" "anything"
myfind_case "myfind: match the start directory itself"  "$FIND/a" "a"

# missing start: exit 1, something on stderr
$TO "$MYFIND" "$FIND/no_such_start" foo >"$TMP/m.out" 2>"$TMP/m.err"; mrc=$?
if [ "$mrc" = 1 ] && [ -s "$TMP/m.err" ]; then
	record "myfind: missing start is exit 1" PASS
else
	record "myfind: missing start is exit 1" FAIL
	echo "  [myfind: missing start is exit 1] wanted exit 1 + stderr, got status=$mrc" >&2
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
echo "$pass passed, $fail failed, $skip skipped"
[ "$fail" -eq 0 ]
