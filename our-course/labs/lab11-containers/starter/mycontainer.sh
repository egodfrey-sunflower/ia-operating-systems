#!/bin/sh
# mycontainer.sh -- SKELETON. This is where you work if you take the SHELL route.
#
# Target interface (assert.sh drives exactly this; the C skeleton takes the
# same arguments, on purpose):
#
#     ./mycontainer.sh [--mem BYTES] [--pids N] ROOTFS CMD [ARGS...]
#
# ============================================================================
# WHICH ROUTE TO TAKE
# ============================================================================
# Either is accepted, and they decompose the problem identically -- the same
# five primitives in the same order, with the same preconditions. The shell
# route gets you there in about half the time, which matters at 4 hours in the
# last teaching week. Take the C route (starter/mycontainer.c) if you want the
# syscall-level view: clone() flags, writing the maps into /proc yourself, and
# pivot_root() as a raw syscall. Take this one if you want the decomposition.
#
# Whichever you pick, Parts 1 and 2 are worth doing with bare `unshare` on the
# command line first -- one namespace at a time, watching what changes. That is
# literally what Part 1 asks for, and it is five minutes.
#
# ============================================================================
# THE MECHANICS YOU ARE GIVEN
# ============================================================================
# Undocumented-in-the-obvious-places, un-guessable, and not what is being
# tested. What each primitive RESTRICTS is not given: that is the lab.
#
#  1. unshare(1) creates the namespaces. --fork is not optional when you ask
#     for --pid: a PID namespace only takes effect for a CHILD, so without
#     --fork nothing is renumbered, and the first fork after that fails,
#     because the new namespace has no init. In util-linux, --kill-child
#     implies --fork, so a command line that has --kill-child already forks
#     whether or not you also wrote --fork; write it anyway, because the
#     requirement belongs to --pid and not to whatever else happens to be
#     on the line.
#
#  2. --map-root-user writes the uid and gid maps for you, in the required
#     order (setgroups denied, then gid_map). Do NOT also pass --setgroups
#     deny: the second write is rejected and unshare exits.
#
#  3. Being uid 0 inside is what gives you capabilities inside. execve() drops
#     capabilities for any non-zero uid, so a map to any inner uid other than 0
#     leaves you unable to mount anything (unless you also pass --keep-caps).
#
#  4. pivot_root(8) usually lives in /usr/sbin, which is often not on your
#     PATH: `command -v pivot_root || echo /usr/sbin/pivot_root`.
#
#  5. pivot_root has two preconditions:
#       - mount propagation must be private, or your later unmount of the old
#         root would propagate back to the host. `unshare --mount` already
#         makes propagation private by default, so `mount --make-rprivate /`
#         here is belt-and-braces on this route -- keep it, and understand why
#         it matters: on a raw CLONE_NEWNS container (the C route) propagation
#         is inherited and pivot_root really is refused without it;
#       - the new root must itself be a MOUNT POINT: `mount --bind $ROOTFS
#         $ROOTFS` -- bind the rootfs directory onto itself. This one pivot_root
#         does check and refuse without.
#     put_old must be a directory under the new root; build-rootfs.sh makes
#     /oldroot.
#
#  6. /proc is what ps, id and everything else read. Mount a fresh one INSIDE
#     (`mount -t proc proc /proc`), after pivot_root and before you detach the
#     old root. You cannot bind the host's /proc in from an unprivileged user
#     namespace, and you cannot mount a fresh proc at all without a pid
#     namespace of your own -- the kernel refuses both.
#
#  7. Detach the old root with `umount -l /oldroot` (lazy). Do not rmdir it:
#     the new root is a self-bind of a real directory, so you would delete it.
#
#  8. cgroup v2: memory.max and pids.max exist in your leaf only if the PARENT
#     cgroup lists those controllers in cgroup.subtree_control, and a cgroup
#     holding processes may not enable controllers of its own. So: mkdir a
#     leaf under a delegated parent, write the limits there, put the process
#     in the leaf. ./probe.sh prints the parent as LAB11_CGROUP_BASE.
#     Set memory.swap.max to 0 as well, or the hog swaps instead of dying.
#
#  9. Only the CONTAINER belongs in the cgroup. This shell and unshare(1) are
#     not the container; if they are in the leaf too, the limit is shared
#     between them and the experiment measures something else. The shell has
#     no handle on the container's pid -- think about who does, and about
#     which pid namespace `cgroup.procs` resolves the pid you write in.
#
# 10. `exec` matters here. Work out what a SIGTERM aimed at the pid you
#     launched actually hits if this script is still sitting in the middle,
#     and what happens to the container when this script dies.
set -eu

MEM=""; PIDS=""
while [ $# -gt 0 ]; do
	case "$1" in
		--mem)  MEM="$2";  shift 2 ;;
		--pids) PIDS="$2"; shift 2 ;;
		--) shift; break ;;
		-*) echo "unknown option $1" >&2; exit 2 ;;
		*) break ;;
	esac
done
[ $# -ge 2 ] || { echo "usage: $0 [--mem BYTES] [--pids N] ROOTFS CMD [ARGS...]" >&2; exit 2; }
ROOTFS="$1"; shift

PIVOT="$(command -v pivot_root || echo /usr/sbin/pivot_root)"
export PIVOT

# --- Part 5: the cgroup leaf (mechanic 8). ---
CG=""
MYUID=$(id -u)   # systemd's per-user slice is named after the uid, not "1000"
BASE="${LAB11_CGROUP_BASE:-/sys/fs/cgroup/user.slice/user-$MYUID.slice/user@$MYUID.service}"
if [ -n "$MEM" ] || [ -n "$PIDS" ]; then
	CG="$BASE/mycontainer.sh.$$"
	# TODO: mkdir the leaf; write memory.max (+ memory.swap.max=0) and pids.max.
	#       Do NOT put this shell in it -- mechanic 9.
	echo "mycontainer.sh: cgroup $CG" >&2
fi
export CG

# The inner script runs inside the new namespaces, on the host root at first,
# and installs the private root before exec'ing the command.
INNER='
	set -eu
	ROOTFS="$1"; shift

	# TODO Part 5: admit exactly this process to "$CG" (mechanic 9), before
	#              forking anything and while /sys is still reachable.

	# TODO Part 1: give the container its own hostname.

	# TODO Part 3: mechanic 5 (make-rprivate, self-bind), then pivot_root,
	#              then cd /.

	# TODO Part 2: mount a fresh /proc (mechanic 6).

	# TODO Part 3, finishing: detach the old root (mechanic 7).

	exec "$@"
'

# TODO Part 1/2/4: the namespace flags. --user --map-root-user gives you
#      Part 4; --uts --pid --mount --net give you Parts 1-3; --fork is
#      mechanic 1; --kill-child ties the container's lifetime to unshare's.
#      Do Part 1 one flag at a time before you write this line for real.
# TODO mechanic 10: how should this line start?
unshare --fork -- /bin/sh -c "$INNER" sh "$ROOTFS" "$@"
