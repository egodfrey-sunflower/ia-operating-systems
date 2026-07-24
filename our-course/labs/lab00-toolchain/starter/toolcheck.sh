#!/usr/bin/env bash
#
# toolcheck.sh [XV6DIR] -- Part 1 smoke test.
#
# Two questions, and only two:
#   1. Does each required tool exist and report a version we can live with?
#   2. Does `make qemu` in XV6DIR reach an xv6 shell prompt inside a timeout?
#
# Version floors below are MAJOR NUMBERS ONLY, deliberately. Anything newer
# is assumed fine. Pinning exact versions here would rot this script within a
# year, and a version mismatch is not what actually breaks a student's build.
#
# XV6DIR defaults to $XV6DIR, then ./xv6-riscv, then ../xv6-riscv. If no xv6
# tree is found, the tool checks still run and the boot check is SKIPPED --
# the script only fails on things it actually tested.
#
# No root required. Nothing is installed; nothing is modified.

set -u
export LC_ALL=C

BOOT_TIMEOUT=${BOOT_TIMEOUT:-90}	# seconds to wait for a shell prompt

fails=0
skips=0

ok()   { printf '  %-8s %s\n' "OK"   "$1"; }
bad()  { printf '  %-8s %s\n' "FAIL" "$1"; fails=$((fails + 1)); }
skip() { printf '  %-8s %s\n' "SKIP" "$1"; skips=$((skips + 1)); }

# major <version-string> -> leading integer, or 0
major() { local v=${1%%.*}; case $v in ''|*[!0-9]*) echo 0;; *) echo "$v";; esac; }

# ---------------------------------------------------------------------------
# 1. tools
# ---------------------------------------------------------------------------

echo "== tools =="

# check_tool <command> <min-major> <awk-field-holding-the-version> <what-for>
check_tool() {
	local cmd=$1 min=$2 field=$3 what=$4 line ver maj
	if ! command -v "$cmd" >/dev/null 2>&1; then
		bad "$cmd -- not on \$PATH ($what)"
		return 1
	fi
	line=$("$cmd" --version 2>/dev/null | head -1)
	ver=$(printf '%s\n' "$line" | awk -v f="$field" '{print (f=="NF") ? $NF : $f}')
	maj=$(major "$ver")
	if [ "$maj" -eq 0 ]; then
		bad "$cmd -- present but printed no version I could parse: $line"
		return 1
	fi
	if [ "$maj" -lt "$min" ]; then
		bad "$cmd -- version $ver, need major >= $min ($what)"
		return 1
	fi
	ok "$cmd $ver ($what)"
	return 0
}

check_tool gcc    9 NF "host C compiler, for Part 3"
check_tool make   4 NF "build tool"
check_tool strace 4 NF "syscall tracer, for Part 4"

# The RISC-V cross compiler goes by several names depending on the distro.
CROSSCC=""
for c in riscv64-unknown-elf-gcc riscv64-elf-gcc riscv64-linux-gnu-gcc; do
	if command -v "$c" >/dev/null 2>&1; then CROSSCC=$c; break; fi
done
if [ -z "$CROSSCC" ]; then
	bad "riscv64 cross compiler -- none of riscv64-{unknown-elf,elf,linux-gnu}-gcc found"
else
	check_tool "$CROSSCC" 10 NF "RISC-V cross compiler"
fi

# QEMU prints "QEMU emulator version 8.2.2 (...)": the version is field 4.
check_tool qemu-system-riscv64 5 4 "RISC-V system emulator"

# A debugger that can speak riscv:rv64. gdb-multiarch on Debian/Ubuntu;
# riscv64-*-gdb elsewhere; plain gdb only if it was built multi-target.
GDBCMD=""
for g in gdb-multiarch riscv64-unknown-elf-gdb riscv64-elf-gdb gdb; do
	if command -v "$g" >/dev/null 2>&1; then GDBCMD=$g; break; fi
done
if [ -z "$GDBCMD" ]; then
	bad "gdb -- no gdb-multiarch / riscv64-*-gdb / gdb on \$PATH"
else
	check_tool "$GDBCMD" 10 NF "debugger (must support riscv:rv64)"
	if [ "$GDBCMD" = gdb ]; then
		if "$GDBCMD" -batch -ex 'set architecture riscv:rv64' >/dev/null 2>&1; then
			ok "gdb understands 'set architecture riscv:rv64'"
		else
			bad "gdb cannot 'set architecture riscv:rv64' -- install gdb-multiarch"
		fi
	fi
fi

# ---------------------------------------------------------------------------
# 2. does xv6 boot?
# ---------------------------------------------------------------------------

echo
echo "== xv6 boot =="

XV6=${1:-${XV6DIR:-}}
if [ -z "$XV6" ]; then
	for d in ./xv6-riscv ../xv6-riscv "$HOME/xv6-riscv"; do
		if [ -f "$d/Makefile" ] && [ -d "$d/kernel" ]; then XV6=$d; break; fi
	done
fi

if [ -z "$XV6" ] || [ ! -f "$XV6/Makefile" ]; then
	skip "no xv6 tree found -- pass its path: ./toolcheck.sh /path/to/xv6-riscv"
elif [ "$fails" -ne 0 ]; then
	skip "boot test skipped: fix the missing tools above first"
else
	XV6=$(cd "$XV6" && pwd)
	TMP=$(mktemp -d)
	trap 'rm -rf "$TMP"' EXIT
	LOG="$TMP/boot.log"
	IN="$TMP/console-in"
	mkfifo "$IN"

	echo "  booting $XV6 (up to ${BOOT_TIMEOUT}s) ..."
	# Open the fifo read-WRITE: a plain `exec 9>fifo` blocks until some
	# reader shows up, and our reader is the very process we start next.
	exec 9<>"$IN"
	( cd "$XV6" && exec make qemu ) <"$IN" >"$LOG" 2>&1 &
	MAKEPID=$!

	booted=0
	i=0
	while [ "$i" -lt "$BOOT_TIMEOUT" ]; do
		# "init: starting sh" is the last thing xv6 prints before the prompt.
		if grep -q -F 'init: starting sh' "$LOG" 2>/dev/null; then
			booted=1
			break
		fi
		kill -0 "$MAKEPID" 2>/dev/null || break	# make died early
		sleep 1
		i=$((i + 1))
	done

	printf '\001x' >&9 2>/dev/null		# Ctrl-A x: tell QEMU to quit
	exec 9>&-
	wait "$MAKEPID" 2>/dev/null
	pkill -f "qemu-system-riscv64.*$XV6" 2>/dev/null	# belt and braces

	if [ "$booted" -eq 1 ]; then
		ok "make qemu reached a shell prompt in under ${BOOT_TIMEOUT}s"
		if grep -q -F 'xv6 kernel is booting' "$LOG"; then
			ok "kernel banner seen on the console"
		else
			bad "shell started but the kernel banner was missing -- odd; check $LOG"
		fi
	else
		bad "make qemu did not reach a shell prompt in ${BOOT_TIMEOUT}s"
		echo "  --- last 20 lines of the boot log ---"
		tail -20 "$LOG" | sed 's/^/  | /'
		echo "  (if a qemu is still running, quit it with Ctrl-A then x)"
	fi
fi

# ---------------------------------------------------------------------------

echo
if [ "$fails" -ne 0 ]; then
	echo "$fails check(s) failed, $skips skipped."
	echo
	echo "On Debian/Ubuntu everything above comes from:"
	echo "    sudo apt update"
	echo "    sudo apt install build-essential git strace \\"
	echo "        gcc-riscv64-unknown-elf gdb-multiarch qemu-system-misc"
	echo "(That apt line is the only root you need in this lab, and it is"
	echo " setup, not the lab itself.)"
	exit 1
fi
echo "All checks passed ($skips skipped)."
exit 0
