#!/usr/bin/env bash
#
# Lab 7 autograder.  Usage: tests/run.sh <workdir>
#
#   <workdir> is a directory containing mycontainer.c + Makefile
#   (i.e. starter/ or solutions/).
#
# Two tiers:
#   * Unprivileged tier  (always): build, usage/error handling, and - IF
#     `unshare -Ur true` works on this host - a user-namespace smoke test.
#   * Privileged tier    (root only): PID-1, hostname, /proc isolation,
#     pivot_root escape, cgroup memory cap, cgroup CPU cap, and the veth ping
#     for the unweighted network stretch (reported PASS or SKIP, never FAIL).
#
# Every check is timeout-protected.  A cleanup trap kills stragglers,
# unmounts leftovers and removes any test cgroup even on failure.
# Exit status:
#   0  the graded (privileged) tier ran and every executed check passed
#   1  at least one check FAILed
#   2  usage error
#   3  the graded (privileged) tier did NOT run - NOT a submission pass.
#      Build/usage smoke checks alone never turn the gate green; rerun as
#      root for the graded set.

set -u

# --------------------------------------------------------------------- setup
WORKDIR="${1:-}"
if [ -z "$WORKDIR" ] || [ ! -d "$WORKDIR" ]; then
	echo "usage: $0 <workdir>  (dir with mycontainer.c + Makefile)" >&2
	exit 2
fi
WORKDIR="$(cd "$WORKDIR" && pwd)"
MC="$WORKDIR/mycontainer"

TMP="$(mktemp -d /tmp/lab7test.XXXXXX)"
ROOTFS="$TMP/rootfs"
IS_ROOT=0; [ "$(id -u)" = "0" ] && IS_ROOT=1
PRIV_RAN=0     # set once the graded (privileged) tier actually executes
VETH_TESTED=0  # set once OUR --net test has run (guards veth cleanup)

# result tables
declare -a R_NAME R_STAT R_NOTE

record() { R_NAME+=("$1"); R_STAT+=("$2"); R_NOTE+=("${3:-}"); }
pass()   { echo "  PASS  $1"; record "$1" PASS "${2:-}"; }
fail()   { echo "  FAIL  $1 ${2:+- $2}"; record "$1" FAIL "${2:-}"; }
skip()   { echo "  SKIP  $1 ${2:+- $2}"; record "$1" SKIP "${2:-}"; }

# --------------------------------------------------------------------- cleanup
cleanup() {
	# kill any mycontainer strays
	pkill -9 -f "$MC" 2>/dev/null
	# remove any test cgroups we (or the runtime) left behind
	for cg in /sys/fs/cgroup/mycontainer_* /sys/fs/cgroup/lab7test_*; do
		[ -d "$cg" ] && rmdir "$cg" 2>/dev/null
	done
	# delete a leftover veth (deleting one end removes the pair) - but ONLY
	# if our own --net test ran: "veth0" is a generic name and may belong
	# to the host, in which case deleting it would break host networking.
	[ "$VETH_TESTED" = 1 ] && ip link del veth0 2>/dev/null
	# lazily unmount anything still mounted under our tmpdir
	for m in $(awk -v t="$TMP" '$2 ~ t {print $2}' /proc/mounts 2>/dev/null \
		   | sort -r); do
		umount -l "$m" 2>/dev/null
	done
	rm -rf "$TMP" 2>/dev/null
}
trap cleanup EXIT INT TERM

# helper: run with a hard timeout, echo combined output
mc() { timeout 15 "$MC" "$@" 2>&1; }

# --------------------------------------------------------- build a busybox fs
build_rootfs() {
	local bb=""
	for p in /bin/busybox /usr/bin/busybox; do
		[ -x "$p" ] && bb="$p" && break
	done
	[ -z "$bb" ] && return 1
	mkdir -p "$ROOTFS"/{bin,proc,dev,etc,tmp}
	cp "$bb" "$ROOTFS/bin/busybox"
	local ap
	for ap in sh ls cat ps echo sleep hostname id ip ping mount mkdir; do
		ln -sf busybox "$ROOTFS/bin/$ap"
	done
	# a static memory hog for the cgroup test.  START after the first
	# allocation proves the runtime actually launched it (a runtime that
	# dies at startup must not pass the OOM check); DONE means the cap
	# never bit.
	cat > "$TMP/memhog.c" <<'EOF'
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
	size_t chunk = 8UL << 20, total = 0;
	for (;;) {
		char *p = malloc(chunk);
		if (!p) { write(1, "MALLOCFAIL\n", 11); return 1; }
		memset(p, 1, chunk);
		total += chunk;
		if (total == chunk) write(1, "START\n", 6);
		if (total > (512UL << 20)) { write(1, "DONE\n", 5); return 0; }
	}
}
EOF
	if command -v gcc >/dev/null 2>&1 &&
	   gcc -static -O0 -o "$ROOTFS/bin/memhog" "$TMP/memhog.c" 2>/dev/null; then
		:
	fi
	return 0
}

echo "== Lab 7 autograder =="
echo "workdir: $WORKDIR"
echo "root:    $([ $IS_ROOT = 1 ] && echo yes || echo no)"
echo

# ============================================================ UNPRIVILEGED TIER
echo "-- Unprivileged tier --"

# 1. build
if make -C "$WORKDIR" > "$TMP/build.log" 2>&1; then
	pass "build"
else
	fail "build" "see $TMP/build.log"
	echo "build failed - remaining checks are guarded and will be skipped"
fi

if [ -x "$MC" ]; then
	# 2. usage / error handling (works for both starter and solution)
	mc >/dev/null 2>&1; [ $? -eq 2 ] && pass "usage: no-args exits 2" \
		|| fail "usage: no-args exits 2"

	out="$(mc run /nonexistent-xyz /bin/sh)"; rc=$?
	[ $rc -eq 1 ] && pass "usage: bad rootfs exits 1" \
		|| fail "usage: bad rootfs exits 1" "rc=$rc"

	mc run --help >/dev/null 2>&1; [ $? -eq 0 ] && pass "usage: --help exits 0" \
		|| fail "usage: --help exits 0"
else
	skip "usage checks" "no binary"
fi

# 3. unprivileged namespace probe
USERNS_OK=0
if unshare -Ur true >/dev/null 2>&1; then
	USERNS_OK=1
fi

if [ -x "$MC" ] && [ $USERNS_OK -eq 1 ] && build_rootfs; then
	# hostname via a user+uts namespace, no root needed
	out="$(mc run --userns "$ROOTFS" /bin/sh -c 'hostname')"
	if echo "$out" | grep -qx "container"; then
		pass "userns: hostname is 'container'"
	else
		fail "userns: hostname is 'container'" "got: $(echo "$out"|tr '\n' ' ')"
	fi
	out="$(mc run --userns "$ROOTFS" /bin/sh -c 'echo PID=$$')"
	if echo "$out" | grep -qx "PID=1"; then
		pass "userns: child sees itself as PID 1"
	else
		fail "userns: child sees itself as PID 1" "got: $(echo "$out"|tr '\n' ' ')"
	fi
	# uid/gid maps: unprivileged outside, root inside.  This is the T4 rubric
	# property; hostname/PID=1 above do not require the maps.
	out="$(mc run --userns "$ROOTFS" /bin/sh -c 'id -u')"
	if echo "$out" | grep -qx "0"; then
		pass "userns: uid 0 inside (uid/gid maps)"
	else
		fail "userns: uid 0 inside (uid/gid maps)" "got: $(echo "$out"|tr '\n' ' ')"
	fi
else
	if [ $USERNS_OK -eq 0 ]; then
		skip "userns namespace tests" \
		     "unshare -Ur failed (Ubuntu 24.04 AppArmor restricts unprivileged userns; run privileged tier as root instead)"
	else
		skip "userns namespace tests" "no busybox for rootfs"
	fi
fi

# ============================================================= PRIVILEGED TIER
echo
echo "-- Privileged tier --"

if [ $IS_ROOT -ne 1 ]; then
	skip "pid-1 / hostname / proc-isolation / pivot / cgroup-mem / cgroup-cpu / veth-stretch" \
	     "needs root: rerun as  sudo $0 $WORKDIR"
elif [ ! -x "$MC" ]; then
	skip "privileged checks" "no binary"
elif ! build_rootfs; then
	skip "privileged checks" "no busybox-static to build a rootfs"
else
	PRIV_RAN=1
	CG2=0; [ "$(stat -fc %T /sys/fs/cgroup 2>/dev/null)" = "cgroup2fs" ] && CG2=1

	# PID 1
	out="$(mc run "$ROOTFS" /bin/sh -c 'echo PID=$$')"
	echo "$out" | grep -qx "PID=1" \
		&& pass "pid-1: child is PID 1" \
		|| fail "pid-1: child is PID 1" "got: $(echo "$out"|tr '\n' ' ')"

	# hostname
	out="$(mc run "$ROOTFS" /bin/sh -c 'hostname')"
	echo "$out" | grep -qx "container" \
		&& pass "hostname: is 'container'" \
		|| fail "hostname: is 'container'" "got: $(echo "$out"|tr '\n' ' ')"

	# /proc isolation: very few processes visible
	out="$(mc run "$ROOTFS" /bin/sh -c 'ps -e 2>/dev/null | wc -l')"
	n="$(echo "$out" | tr -dc '0-9')"
	if [ -n "$n" ] && [ "$n" -gt 0 ] && [ "$n" -lt 10 ]; then
		pass "proc-isolation: $n lines from ps"
	else
		fail "proc-isolation: ps shows few procs" "lines=$n"
	fi

	# pivot_root escape: host-only dir /home must be invisible
	out="$(mc run "$ROOTFS" /bin/sh -c '[ -e /home ] && echo LEAK || echo OK; ls / | tr "\n" " "')"
	if echo "$out" | grep -q "LEAK"; then
		fail "pivot: host /home leaked into container" "$out"
	elif echo "$out" | grep -qw "bin"; then
		pass "pivot: only container root visible"
	else
		fail "pivot: unexpected root listing" "$out"
	fi

	# cgroup memory cap: memhog must START (runtime actually launched it),
	# must never reach DONE, and must die an OOM-kill-consistent death
	# (SIGKILL -> rc 137, or another nonzero exit).  A runtime that fails
	# at startup (no START) must NOT pass; a timeout (rc=124) means the
	# hog was swapping instead of dying, which is also a FAIL.
	if [ $CG2 -eq 1 ] && [ -x "$ROOTFS/bin/memhog" ]; then
		out="$(timeout 20 "$MC" run --mem 32M "$ROOTFS" /bin/memhog 2>&1)"; rc=$?
		if ! echo "$out" | grep -q "START"; then
			fail "cgroup-mem: hog never started" \
			     "runtime failed before the hog's first allocation (rc=$rc): $(echo "$out"|tr '\n' ' '|cut -c1-120)"
		elif echo "$out" | grep -q "DONE"; then
			fail "cgroup-mem: hog reached DONE (cap not enforced)"
		elif [ $rc -eq 124 ]; then
			fail "cgroup-mem: hog timed out instead of being killed" \
			     "the memory cap did not bind: is something letting the hog's memory escape the cap? (re-read the handout's Task 3 pitfalls)"
		elif [ $rc -ne 0 ]; then
			pass "cgroup-mem: hog killed at cap (rc=$rc)"
		else
			fail "cgroup-mem: unexpected" "rc=$rc out=$out"
		fi
	else
		skip "cgroup-mem" "no cgroup2 or no memhog"
	fi

	# cgroup CPU cap: spin loop should peg near 50% with --cpus 0.5
	if [ $CG2 -eq 1 ]; then
		timeout 8 "$MC" run --cpus 0.5 "$ROOTFS" \
			/bin/sh -c 'while :; do :; done' >/dev/null 2>&1 &
		bgpid=$!
		sleep 1
		# The handout's contract: a cgroup named exactly mycontainer_<pid>
		# directly under /sys/fs/cgroup.  Its absence is a FAIL, not a
		# SKIP - a setup_cgroup that silently does nothing must not pass.
		cg="$(find /sys/fs/cgroup -maxdepth 1 -type d \
			-regextype posix-extended \
			-regex '.*/mycontainer_[0-9]+' 2>/dev/null | head -1)"
		if [ -n "$cg" ] && [ -r "$cg/cpu.stat" ]; then
			u1="$(awk '/usage_usec/{print $2}' "$cg/cpu.stat")"
			sleep 2
			u2="$(awk '/usage_usec/{print $2}' "$cg/cpu.stat")"
			kill "$bgpid" 2>/dev/null; wait "$bgpid" 2>/dev/null
			pct=$(( (u2 - u1) / 20000 ))   # (delta_usec/2e6)*100
			if [ "$pct" -ge 30 ] && [ "$pct" -le 70 ]; then
				pass "cgroup-cpu: ~${pct}% of one CPU (cap 50%)"
			else
				fail "cgroup-cpu: ${pct}% outside 30-70%"
			fi
		else
			kill "$bgpid" 2>/dev/null; wait "$bgpid" 2>/dev/null
			fail "cgroup-cpu: no /sys/fs/cgroup/mycontainer_<pid> cgroup" \
			     "the handout requires exactly this name; setup_cgroup must create it (and cpu.stat must be readable)"
		fi
	else
		skip "cgroup-cpu" "no cgroup2"
	fi

	# veth ping across the namespace — the unweighted network stretch.
	# PASS when it works; otherwise SKIP (it never fails the run).
	if command -v ip >/dev/null 2>&1; then
		VETH_TESTED=1
		out="$(timeout 15 "$MC" run --net "$ROOTFS" \
			/bin/sh -c 'ping -c1 -W2 10.0.7.1' 2>&1)"; rc=$?
		if [ $rc -eq 0 ] && echo "$out" | grep -q "1 packets received\|1 received"; then
			pass "veth (stretch): container pinged host 10.0.7.1"
		else
			skip "veth (stretch)" "unweighted; not passing (rc=$rc)"
		fi
	else
		skip "veth (stretch)" "no ip binary"
	fi
fi

# ------------------------------------------------------------------- summary
echo
echo "================ RESULTS ================"
nfail=0; nskip=0
for i in "${!R_NAME[@]}"; do
	printf "  %-6s %s%s\n" "${R_STAT[$i]}" "${R_NAME[$i]}" \
		"$( [ -n "${R_NOTE[$i]}" ] && echo "  (${R_NOTE[$i]})" )"
	[ "${R_STAT[$i]}" = FAIL ] && nfail=$((nfail+1))
	[ "${R_STAT[$i]}" = SKIP ] && nskip=$((nskip+1))
done
echo "========================================"
echo "FAIL: $nfail   SKIP: $nskip   total: ${#R_NAME[@]}"
if [ "$nskip" -gt 0 ]; then
	echo ">> $nskip check(s) SKIPPED - see notes above (often: run as root)."
fi
[ "$nfail" -gt 0 ] && exit 1

# Exit 0 only when the graded (privileged) tier actually executed.  On a
# stock unprivileged box every graded check SKIPs, and an exit-0 there would
# turn the submission gate green for zero tested work.
if [ "$PRIV_RAN" -ne 1 ]; then
	echo
	echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	echo "!!  NOT A SUBMISSION PASS - privileged tier did not run     !!"
	echo "!!  (rerun with sudo).  Only build/usage smoke checks ran;  !!"
	echo "!!  none of the graded rubric was tested.                   !!"
	echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	exit 3
fi
exit 0
