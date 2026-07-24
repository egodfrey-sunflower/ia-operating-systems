#!/bin/sh
# assert.sh -- advisory checks on the observable properties of your container.
# GIVEN AND COMPLETE.
#
#   ./assert.sh [--no-scope] RUNTIME ROOTFS
#
#   RUNTIME   your container runtime: ./mycontainer or ./mycontainer.sh
#   ROOTFS    a root filesystem built by ./build-rootfs.sh, with a static
#             ./hog copied to ROOTFS/bin/hog
#
# THIS IS NOT AN AUTOGRADER. It is a fast mirror: nine observable properties,
# one PASS/FAIL line each, checked from outside your container by running it.
# It knows nothing about how you built the thing -- only whether the process
# that came out has the properties a container is supposed to have. Your marks
# are in TIERS.md and NOT-A-KERNEL-OBJECT.md; this only tells you the machinery
# under them works.
#
# THE ONE INTERFACE CONTRACT. Your runtime must accept:
#
#     RUNTIME [--mem BYTES] [--pids N] ROOTFS CMD [ARGS...]
#
# and run CMD inside the container. Both reference solutions do. If yours takes
# its arguments differently, either adapt it or edit the run() function below --
# the checks care about the container, not the command line.
#
# WHAT MAKES THESE CHECKS REAL. A check that cannot fail is worse than no check
# at all, so every line below was verified to FAIL against a deliberately broken
# container (no pivot_root; no fork in the pid namespace; no cgroup limit; no
# uid map). Note the limit of that claim: the root check below is written
# against what is observable from outside, and a chroot-only container passes
# it too -- see the comment at the ROOT check for why, and what is left for
# your write-up to argue. Three properties of the design keep them honest:
#
#   * A missing answer is a FAIL, never a skip. If a container dies, hangs, or
#     prints nothing, its checks fail. There is no "could not determine" verdict.
#   * The /proc check demands that /proc/1 exist AND that the process count be
#     small. Counting alone would pass on a container with no /proc mounted at
#     all, where the glob matches nothing.
#   * The memory check runs the hog under `ulimit -v` a factor of four ABOVE
#     the cgroup limit it is testing. If the cgroup is enforcing, the OOM kill
#     (rc 137) always happens first; if it is not, the ulimit stops the hog
#     with a plain malloc failure (rc 1) and the check fails instead of the
#     machine falling over. The safety net cannot manufacture a pass, because
#     it produces a different exit status from the thing being tested.
#
# The same reasoning covers the optional outer systemd scope (default on when
# available, --no-scope to disable): its MemoryMax is 512M against a 256M
# ulimit against a 64M cgroup, and its TasksMax is 200 against a fork bomb
# bounded at 60. Both outer bounds sit strictly above the inner ones, so the
# scope can only ever catch a runaway -- it can never be the thing that fires.
set -u

SCOPE=yes
[ "${1:-}" = "--no-scope" ] && { SCOPE=no; shift; }
[ $# -eq 2 ] || { echo "usage: $0 [--no-scope] RUNTIME ROOTFS" >&2; exit 2; }
RT="$1"; ROOTFS="$2"

# --- setup errors abort; they are not check results ------------------------
[ -x "$RT" ]              || { echo "assert: $RT is not executable" >&2; exit 2; }
[ -d "$ROOTFS" ]          || { echo "assert: $ROOTFS is not a directory" >&2; exit 2; }
[ -x "$ROOTFS/bin/sh" ]   || { echo "assert: $ROOTFS/bin/sh missing -- run ./build-rootfs.sh" >&2; exit 2; }
[ -x "$ROOTFS/bin/hog" ]  || { echo "assert: $ROOTFS/bin/hog missing -- cc -static -O2 -o hog hog.c && cp hog $ROOTFS/bin/" >&2; exit 2; }
command -v timeout >/dev/null || { echo "assert: need timeout(1)" >&2; exit 2; }

# Re-run ourselves inside a systemd scope with limits well above everything we
# are testing. The scope and any cgroup your runtime makes under
# LAB11_CGROUP_BASE are SIBLINGS, not ancestor and descendant, so the scope's
# limits do not apply to a container that placed itself in such a leaf. What
# the scope buys is the other case: a runtime that places the container in NO
# cgroup at all still runs inside this scope, and so cannot take the machine
# down. The `ulimit -v` in run B is what bounds the delegated-leaf case. See
# the header for why neither can fake a PASS.
if [ $SCOPE = yes ]; then
	# Pre-flight with the IDENTICAL property set we are about to exec with:
	# `-p` can be refused where a bare scope succeeds, and after the exec
	# there is no recovery -- we would die having run no checks at all.
	if [ "${LAB11_IN_SCOPE:-}" != 1 ] \
	   && systemd-run --user --scope --quiet --collect \
	        -p MemoryMax=512M -p MemorySwapMax=0 -p TasksMax=200 -- true >/dev/null 2>&1; then
		# Resolve our own path first. $0 is a bare name when we were started
		# as `sh assert.sh`, and systemd-run resolves its command through
		# PATH -- a bare name is not found there, however plainly it sits in
		# the current directory. Going through /bin/sh also means this works
		# whether or not the execute bit survived however you got the file.
		SELF=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")
		echo "assert: (re-running inside a systemd scope as a safety net; --no-scope to skip)"
		LAB11_IN_SCOPE=1 exec systemd-run --user --scope --quiet --collect \
			-p MemoryMax=512M -p MemorySwapMax=0 -p TasksMax=200 \
			-- /bin/sh "$SELF" --no-scope "$RT" "$ROOTFS"
	fi
	# Only reached when we did NOT re-exec: either LAB11_IN_SCOPE was already
	# set in the environment, or systemd-run is missing or refused these
	# properties. Say so rather than skipping the guardrail silently.
	echo "assert: no systemd scope available -- running WITHOUT the safety net" >&2
fi

ROOTFS_ABS=$(cd "$ROOTFS" && pwd)
HOST_UID=$(id -u)
HOST_HOSTNAME=$(hostname)
MARKER="$(pwd)/.assert-host-marker.$$"     # absolute path, host side only
CANARY=".assert-canary.$$"
echo "host-only, must not be visible inside the container" > "$MARKER"
# If that write failed the marker does not exist, and the ROOT check below
# would report PASS for absolutely any container. Abort instead.
[ -e "$MARKER" ] || { echo "assert: cannot create $MARKER -- run from a writable directory" >&2; exit 2; }
rm -f "$ROOTFS_ABS/tmp/$CANARY"

PASS=0; FAIL=0
ok() { printf 'PASS  %-22s %s\n' "$1" "$2"; PASS=$((PASS+1)); }
no() { printf 'FAIL  %-22s %s\n' "$1" "$2"; FAIL=$((FAIL+1)); }

fact() { printf '%s\n' "$OUT" | sed -n "s/^$1=//p" | head -1; }

# The canary is removed from both places it can land: inside the rootfs when
# the runtime really changed root, and in the HOST /tmp when it did not --
# which is every skeleton's first run.
cleanup() { rm -f "$MARKER" "$ROOTFS_ABS/tmp/$CANARY" "/tmp/$CANARY"; }
trap cleanup EXIT INT TERM

echo "=== lab11 assertions: $RT on $ROOTFS ==="

# ---------------------------------------------------------------- run A ----
# One container, no limits, reporting everything Parts 1-4 can be seen by.
PROBE='
echo A_PID=$$
echo A_UID=$(id -u)
echo A_UTS=$(hostname)
[ -d /proc/1 ] && echo A_PROC1=yes || echo A_PROC1=no
set -- /proc/[0-9]*; echo A_NPROC=$#
[ -e "'"$MARKER"'" ] && echo A_MARKER=visible || echo A_MARKER=hidden
echo A_NETIF=$(( $(cat /proc/net/dev 2>/dev/null | wc -l) - 2 ))
touch /tmp/'"$CANARY"' 2>/dev/null && echo A_CANARY=made || echo A_CANARY=failed
'
OUT=$(timeout 30 "$RT" "$ROOTFS" /bin/sh -c "$PROBE" 2>&1)
HOST_HOSTNAME_AFTER=$(hostname)

# 1. UTS -- the container renamed itself and the host did not notice.
c_uts=$(fact A_UTS)
if [ -n "$c_uts" ] && [ "$c_uts" != "$HOST_HOSTNAME" ] && [ "$HOST_HOSTNAME_AFTER" = "$HOST_HOSTNAME" ]; then
	ok "P1 uts ns" "inside '$c_uts', host still '$HOST_HOSTNAME'"
else
	no "P1 uts ns" "inside '${c_uts:-<no answer>}', host '$HOST_HOSTNAME' -> '$HOST_HOSTNAME_AFTER'"
fi

# 2. NET -- a fresh network namespace has loopback and nothing else. Counted
#    from /proc/net/dev (two header lines) rather than `ip`, which is a
#    busybox applet that need not be compiled in; /proc is required anyway.
c_net=$(fact A_NETIF)
if [ "$c_net" = 1 ]; then ok "P1 net ns" "1 interface inside (lo only)"
else no "P1 net ns" "${c_net:-<no answer>} interfaces inside; a fresh net ns has exactly 1"; fi

# 3. PID -- the container's first process is pid 1.
c_pid=$(fact A_PID)
if [ "$c_pid" = 1 ]; then ok "P2 pid ns" "the command runs as pid 1"
else no "P2 pid ns" "the command is pid '${c_pid:-<no answer>}' -- a pid ns renumbers the CHILD only"; fi

# 4. PROC -- /proc reflects THIS pid namespace, not the host's.
c_p1=$(fact A_PROC1); c_n=$(fact A_NPROC)
if [ "$c_p1" = yes ] && [ -n "$c_n" ] && [ "$c_n" -ge 1 ] && [ "$c_n" -le 5 ]; then
	ok "P2 /proc" "$c_n process(es) in /proc, including /proc/1"
else
	no "P2 /proc" "/proc/1=${c_p1:-<none>} count=${c_n:-<none>} (host /proc would show hundreds; no /proc at all shows no /proc/1)"
fi

# 5. ROOT -- a host absolute path does not resolve inside the container.
#    That is all this observes. It does NOT distinguish pivot_root from
#    chroot: a chroot-only container passes this line too, because the
#    difference between them is escapability, and escapability is not
#    observable from outside a container that does not try to escape.
#    Arguing that difference is what NOT-A-KERNEL-OBJECT.md is for.
c_mk=$(fact A_MARKER)
if [ "$c_mk" = hidden ]; then ok "P3 host root" "host marker unreachable inside"
else no "P3 host root" "host marker is ${c_mk:-<no answer>} at $MARKER"; fi

# 6. UID -- root inside.
c_uid=$(fact A_UID)
if [ "$c_uid" = 0 ]; then
	ok "P4 uid inside" "id -u reports 0"
elif [ "$c_uid" = 65534 ]; then
	no "P4 uid inside" "id -u reports 65534 -- the overflow uid: a user namespace with no map written"
else
	no "P4 uid inside" "id -u reports '${c_uid:-<no answer>}' -- inside a mapped user namespace this is 0"
fi

# 7. OWNER -- and nobody outside. The file the container made as "root" is
#    owned on the host by the unprivileged user who launched it.
c_can=$(fact A_CANARY)
if [ "$c_can" = made ] && [ -e "$ROOTFS_ABS/tmp/$CANARY" ]; then
	owner=$(stat -c %u "$ROOTFS_ABS/tmp/$CANARY")
	if [ "$owner" = "$HOST_UID" ] && [ "$owner" != 0 ]; then
		ok "P4 uid outside" "file made as root inside is owned by uid $owner on the host"
	elif [ "$HOST_UID" = 0 ]; then
		no "P4 uid outside" "Part 4 cannot be demonstrated as root -- see probe.sh's verdict and TIERS.md"
	else
		no "P4 uid outside" "file owner on the host is uid $owner, expected $HOST_UID (unprivileged)"
	fi
elif [ "$c_can" = made ]; then
	no "P4 uid outside" "the container made /tmp/$CANARY, but it is not in $ROOTFS/tmp -- the container's / is not your rootfs"
else
	no "P4 uid outside" "the container could not create /tmp/$CANARY (${c_can:-<no answer>})"
fi

# ---------------------------------------------------------------- run B ----
# 8. MEMORY -- past memory.max, the kernel kills it. The ulimit is the safety
#    net described in the header: 256 MiB of address space against a 64 MiB
#    cgroup, so the OOM kill always wins if the cgroup is real.
MEM=67108864
B=$(timeout 60 "$RT" --mem $MEM "$ROOTFS" /bin/sh -c \
      'ulimit -v 262144; /bin/hog mem >/dev/null 2>&1; echo B_RC=$?' 2>&1)
b_rc=$(printf '%s\n' "$B" | sed -n 's/^B_RC=//p' | head -1)
case "$b_rc" in
	137) ok "P5 memory.max" "the hog was SIGKILLed past $((MEM>>20)) MiB (rc 137)" ;;
	1)   no "P5 memory.max" "the hog died of a failed malloc at the 256 MiB safety ulimit, not of the cgroup -- nothing enforced $((MEM>>20)) MiB" ;;
	"")  no "P5 memory.max" "no answer: the container did not survive to report (check ./probe.sh, and that --mem is accepted)" ;;
	*)   no "P5 memory.max" "the hog exited $b_rc; a cgroup OOM kill is 137" ;;
esac

# ---------------------------------------------------------------- run C ----
# 9. PIDS -- fork() starts failing at pids.max. hog is bounded at 60 attempts
#    so that an UNLIMITED container is safe to observe: it reports 60 and
#    fails this check, rather than forking until the machine dies.
LIM=20
C=$(timeout 60 "$RT" --pids $LIM "$ROOTFS" /bin/hog fork 60 2>&1)
c_n=$(printf '%s\n' "$C" | sed -n 's/^hog: fork failed after \([0-9]*\) children/\1/p' | head -1)
c_all=$(printf '%s\n' "$C" | sed -n 's/^hog: forked \([0-9]*\) children/\1/p' | head -1)
if [ -n "$c_n" ] && [ "$c_n" -lt "$LIM" ]; then
	ok "P5 pids.max" "fork refused after $c_n children against pids.max=$LIM"
elif [ -n "$c_all" ]; then
	no "P5 pids.max" "forked all $c_all children -- nothing capped it at $LIM"
else
	no "P5 pids.max" "no answer: the container did not report (check ./probe.sh, and that --pids is accepted)"
fi

echo
echo "  $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ] || echo "  (advisory: fix these before writing up, then argue the WHY in NOT-A-KERNEL-OBJECT.md)"
[ $FAIL -eq 0 ]
