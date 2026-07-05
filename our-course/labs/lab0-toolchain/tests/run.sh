#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 0 Unix warm-up.
#
# Builds the tools in <workdir> with `make`, statically inspects the built
# binaries for banned library calls (the handout's "no stdio; no readdir(3)
# in myls" rule, enforced via nm on undefined symbols), constructs a
# deterministic fixture tree in a temp dir, runs mycat/myls/mygrep, and
# compares their output and exit status against reference behaviour:
#   - mycat  vs coreutils `cat`
#   - mygrep vs coreutils `grep -F`
#   - myls   vs an expected listing constructed here per the handout format
#
# Prints a PASS/FAIL table and exits nonzero if any test fails. Every tool
# invocation is wrapped in `timeout 10`. No network access, no QEMU, no
# cross-compiler needed.

set -u
export LC_ALL=C   # deterministic byte-order sorting and messages

# ---------------------------------------------------------------------------
# args / setup
# ---------------------------------------------------------------------------

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi

WORKDIR=$1
if [ ! -d "$WORKDIR" ]; then
	echo "run.sh: workdir '$WORKDIR' is not a directory" >&2
	exit 2
fi
# absolutize
WORKDIR=$(cd "$WORKDIR" && pwd)

MYCAT="$WORKDIR/mycat"
MYLS="$WORKDIR/myls"
MYGREP="$WORKDIR/mygrep"

TMP=$(mktemp -d)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

pass=0
fail=0
declare -a RESULTS

record() { # name status
	RESULTS+=("$2|$1")
	if [ "$2" = "PASS" ]; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
	fi
}

TO="timeout 10"

# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

echo "== building $WORKDIR =="
if make -C "$WORKDIR" clean >/dev/null 2>&1 && make -C "$WORKDIR" all >"$TMP/build.log" 2>&1; then
	echo "build OK"
else
	echo "build FAILED:" >&2
	cat "$TMP/build.log" >&2
	echo "BUILD|build" >&2
	echo
	echo "RESULT: build failure"
	exit 1
fi
echo

# ---------------------------------------------------------------------------
# static symbol gate (rubric: stdio or readdir(3) => automatic 0 for the tool)
# ---------------------------------------------------------------------------
#
# Inspect each binary's *undefined* symbols with nm and fail on any banned
# libc function. Symbols are matched exactly by name (never by substring, so
# e.g. legit `open`/`read` never trip the `opendir`/`readdir` rules), after
# stripping the `@GLIBC_x` version suffix and normalizing glibc's fortified
# (`__printf_chk`) and ISO-C99 (`__isoc99_scanf`) alias forms.

STDIO_BANNED="printf fprintf sprintf snprintf vprintf vfprintf vsprintf \
vsnprintf dprintf vdprintf puts fputs putc putchar fputc putw getc getchar \
fgetc fgets getline getdelim ungetc getw scanf fscanf sscanf vscanf vfscanf \
vsscanf fopen fopen64 fdopen freopen freopen64 fclose fread fwrite fflush \
fseek fseeko ftell ftello rewind fgetpos fsetpos setbuf setbuffer setlinebuf \
setvbuf perror tmpfile tmpfile64 popen pclose fmemopen open_memstream \
clearerr feof ferror fileno"
DIR_BANNED="opendir fdopendir readdir readdir64 readdir_r readdir64_r \
closedir rewinddir seekdir telldir scandir scandir64 scandirat dirfd"

# check_symbols <testname> <binary> <banned-name-list>
check_symbols() {
	local name=$1 bin=$2 banned=$3
	local bad="" s base b
	# Union of both symbol tables so a stripped .symtab cannot hide imports.
	for s in $( { nm -u "$bin" 2>/dev/null; nm -Du "$bin" 2>/dev/null; } \
			| awk '{print $NF}' | sed 's/@.*$//' | sort -u); do
		base=${s#__}
		base=${base%_chk}
		base=${base#isoc99_}
		for b in $banned; do
			if [ "$s" = "$b" ] || [ "$base" = "$b" ]; then
				bad="$bad $s"
				break
			fi
		done
	done
	if [ -z "$bad" ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		echo "  [$name] banned symbol(s) referenced:$bad" >&2
	fi
}

check_symbols "no-stdio gate: mycat"  "$MYCAT"  "$STDIO_BANNED"
check_symbols "no-stdio gate: myls"   "$MYLS"   "$STDIO_BANNED"
check_symbols "no-stdio gate: mygrep" "$MYGREP" "$STDIO_BANNED"
check_symbols "no-readdir gate: myls" "$MYLS"   "$DIR_BANNED"

# ---------------------------------------------------------------------------
# fixtures
# ---------------------------------------------------------------------------

FIX="$TMP/fix"
mkdir -p "$FIX"

printf 'hello\nworld\n'            > "$FIX/fileA.txt"          # normal, 2 lines
printf 'green\ngreener\ngreenest\n'> "$FIX/fileB.txt"          # substring hits
: > "$FIX/empty.txt"                                           # empty file
printf 'no newline here'           > "$FIX/nonl.txt"           # no trailing \n
printf 'a\x00b\x01c\nzz\n'         > "$FIX/binary.bin"         # NULs + text

TREE="$FIX/tree"
mkdir -p "$TREE/sub"
printf 'x\n'   > "$TREE/alpha.txt"
printf 'yy\n'  > "$TREE/beta.txt"
printf 'zzz\n' > "$TREE/zeta.bin"
printf 'h\n'   > "$TREE/.hidden"      # dotfile: listed (only . and .. dropped)
printf 'n\n'   > "$TREE/sub/nested.txt"

LNK="$FIX/linkdir"                    # symlink fixture (lstat semantics)
mkdir -p "$LNK"
printf 'target contents\n' > "$LNK/target.txt"
ln -s target.txt "$LNK/sym"

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

# run a command, capture stdout + exit code into files
# run_capture <outfile> <rcfile> -- reads argv from "$@"
run_capture() {
	local outf=$1 rcf=$2
	shift 2
	local rc
	"$@" > "$outf" 2>/dev/null
	rc=$?
	echo "$rc" > "$rcf"
	return 0
}

# compare_files <name> <expected> <actual>
compare_files() {
	local name=$1 exp=$2 act=$3
	if cmp -s "$exp" "$act"; then
		record "$name" PASS
	else
		record "$name" FAIL
		{
			echo "  [$name] output mismatch"
			echo "  --- expected ---"; sed 's/^/  | /' "$exp"
			echo "  --- actual   ---"; sed 's/^/  | /' "$act"
		} >&2
	fi
}

# compare_rc <name> <expected_rc> <actual_rc_file>
compare_rc() {
	local name=$1 exp=$2 actf=$3 act
	act=$(cat "$actf")
	if [ "$act" = "$exp" ]; then
		record "$name" PASS
	else
		record "$name" FAIL
		echo "  [$name] exit code: expected $exp, got $act" >&2
	fi
}

# Build the expected myls line for a path: "MODE SIZE NAME"
# uses coreutils stat; %A already renders type+rwx like ls -l.
myls_line() { # <statpath> <displayname>
	local sp=$1 nm=$2
	printf '%s %s %s\n' "$(stat -c '%A' "$sp")" "$(stat -c '%s' "$sp")" "$nm"
}

# ---------------------------------------------------------------------------
# mycat tests
# ---------------------------------------------------------------------------

# 1. single file
cat "$FIX/fileA.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYCAT" "$FIX/fileA.txt"
compare_files "mycat: single file" "$TMP/exp" "$TMP/act"
compare_rc    "mycat: single file rc" 0 "$TMP/rc"

# 2. multiple files incl. empty and no-trailing-newline
cat "$FIX/fileA.txt" "$FIX/empty.txt" "$FIX/nonl.txt" "$FIX/fileB.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYCAT" \
	"$FIX/fileA.txt" "$FIX/empty.txt" "$FIX/nonl.txt" "$FIX/fileB.txt"
compare_files "mycat: multi file" "$TMP/exp" "$TMP/act"

# 3. binary-safe passthrough
cat "$FIX/binary.bin" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYCAT" "$FIX/binary.bin"
compare_files "mycat: binary safe" "$TMP/exp" "$TMP/act"

# 4. stdin (no args)
cat < "$FIX/fileA.txt" > "$TMP/exp"
$TO "$MYCAT" < "$FIX/fileA.txt" > "$TMP/act" 2>/dev/null; echo $? > "$TMP/rc"
compare_files "mycat: stdin" "$TMP/exp" "$TMP/act"
compare_rc    "mycat: stdin rc" 0 "$TMP/rc"

# 5. missing file -> exit 1
run_capture "$TMP/act" "$TMP/rc" $TO "$MYCAT" "$FIX/does-not-exist"
compare_rc "mycat: missing file rc" 1 "$TMP/rc"

# 6. "-" operand reads stdin, in sequence with real files
cat "$FIX/fileA.txt" "$FIX/fileB.txt" > "$TMP/exp"
$TO "$MYCAT" "$FIX/fileA.txt" - < "$FIX/fileB.txt" > "$TMP/act" 2>/dev/null
echo $? > "$TMP/rc"
compare_files "mycat: dash reads stdin" "$TMP/exp" "$TMP/act"
compare_rc    "mycat: dash rc" 0 "$TMP/rc"

# 7. continue-after-error contract: first operand missing -> error to fd 2,
#    still emits the second operand's content, exits nonzero (1)
cat "$FIX/fileB.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYCAT" "$FIX/does-not-exist" "$FIX/fileB.txt"
compare_files "mycat: continue after error" "$TMP/exp" "$TMP/act"
compare_rc    "mycat: continue after error rc" 1 "$TMP/rc"

# ---------------------------------------------------------------------------
# mygrep tests
# ---------------------------------------------------------------------------

# 1. single file, some matches
grep -F green "$FIX/fileB.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYGREP" green "$FIX/fileB.txt"
compare_files "mygrep: single file" "$TMP/exp" "$TMP/act"
compare_rc    "mygrep: match rc" 0 "$TMP/rc"

# 2. no match -> empty output, exit 1
grep -F zzznope "$FIX/fileB.txt" > "$TMP/exp"   # empty
run_capture "$TMP/act" "$TMP/rc" $TO "$MYGREP" zzznope "$FIX/fileB.txt"
compare_files "mygrep: no-match output" "$TMP/exp" "$TMP/act"
compare_rc    "mygrep: no-match rc" 1 "$TMP/rc"

# 3. match on line without trailing newline (grep still appends \n)
grep -F newline "$FIX/nonl.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYGREP" newline "$FIX/nonl.txt"
compare_files "mygrep: no-trailing-newline" "$TMP/exp" "$TMP/act"

# 4. multi-file: grep -F prefixes "FILE:"
grep -F green "$FIX/fileA.txt" "$FIX/fileB.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYGREP" green "$FIX/fileA.txt" "$FIX/fileB.txt"
compare_files "mygrep: multi-file prefix" "$TMP/exp" "$TMP/act"

# 5. stdin
grep -F world < "$FIX/fileA.txt" > "$TMP/exp"
$TO "$MYGREP" world < "$FIX/fileA.txt" > "$TMP/act" 2>/dev/null; echo $? > "$TMP/rc"
compare_files "mygrep: stdin" "$TMP/exp" "$TMP/act"

# 6. missing file -> exit 2 (grep's error convention)
run_capture "$TMP/act" "$TMP/rc" $TO "$MYGREP" green "$FIX/does-not-exist"
compare_rc "mygrep: missing file rc" 2 "$TMP/rc"

# ---------------------------------------------------------------------------
# myls tests
# ---------------------------------------------------------------------------

# 1. single regular file -> one line, NAME is the argument verbatim
myls_line "$FIX/fileA.txt" "$FIX/fileA.txt" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYLS" "$FIX/fileA.txt"
compare_files "myls: single file" "$TMP/exp" "$TMP/act"
compare_rc    "myls: single file rc" 0 "$TMP/rc"

# 2. directory listing: entries minus . and .., sorted by byte value.
#    include dotfiles (only . and .. are dropped).
: > "$TMP/exp"
# gather entry names (incl. dotfiles), drop . and .., sort C-locale
names=$( { ls -1a "$TREE"; } | grep -vxE '\.|\.\.' | LC_ALL=C sort )
while IFS= read -r nm; do
	[ -z "$nm" ] && continue
	myls_line "$TREE/$nm" "$nm" >> "$TMP/exp"
done <<< "$names"
cp "$TMP/exp" "$TMP/exp_tree"   # reused by the no-args test below
run_capture "$TMP/act" "$TMP/rc" $TO "$MYLS" "$TREE"
compare_files "myls: directory listing" "$TMP/exp" "$TMP/act"

# 3. directory containing a subdirectory shows it with a 'd' type
#    (already covered by sub/ in tree; assert the sub line specifically)
if grep -qE '^d.{9} [0-9]+ sub$' "$TMP/act"; then
	record "myls: subdir type char" PASS
else
	record "myls: subdir type char" FAIL
	echo "  [myls: subdir type char] no 'd... sub' line in output" >&2
fi

# 4. missing path -> exit 1
run_capture "$TMP/act" "$TMP/rc" $TO "$MYLS" "$FIX/does-not-exist"
compare_rc "myls: missing path rc" 1 "$TMP/rc"

# 5. no args -> lists "." (same content as the $TREE listing, run from there)
( cd "$TREE" && $TO "$MYLS" ) > "$TMP/act" 2>/dev/null; echo $? > "$TMP/rc"
compare_files "myls: no args lists ." "$TMP/exp_tree" "$TMP/act"
compare_rc    "myls: no args rc" 0 "$TMP/rc"

# 6. symlink argument: lstat semantics -- report the link itself ('l' type,
#    st_size = length of the target string), never the target.
myls_line "$LNK/sym" "$LNK/sym" > "$TMP/exp"
run_capture "$TMP/act" "$TMP/rc" $TO "$MYLS" "$LNK/sym"
compare_files "myls: symlink not followed" "$TMP/exp" "$TMP/act"
if grep -q '^l' "$TMP/act"; then
	record "myls: symlink type char" PASS
else
	record "myls: symlink type char" FAIL
	echo "  [myls: symlink type char] no 'l'-type line for a symlink arg (stat instead of lstat?)" >&2
fi

# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------

echo "== results =="
for r in "${RESULTS[@]}"; do
	st=${r%%|*}
	nm=${r#*|}
	printf '  %-6s %s\n' "$st" "$nm"
done
echo
echo "$pass passed, $fail failed"

[ "$fail" -eq 0 ]
