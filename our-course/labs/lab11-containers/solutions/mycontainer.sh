#!/bin/sh
# mycontainer.sh -- the same container, assembled from unshare(1). REFERENCE.
#
#   ./mycontainer.sh [--mem BYTES] [--pids N] ROOTFS CMD [ARGS...]
#
# unshare does in one call what mycontainer.c spells out by hand: it creates
# the namespaces, and with --map-root-user it denies setgroups and writes the
# uid/gid maps in the correct order for you (do NOT also pass --setgroups deny;
# --map-root-user already handles it, and the double write is rejected). The
# mount choreography --
# make-rprivate, self-bind, pivot_root, fresh /proc, detach the old root --
# is the same sequence, and the same gotchas, as the C version. That is the
# point of offering both: the decomposition is identical; only the amount of
# syscall you touch by hand differs.
#
# Two things this script does that look fussy and are not; both are places the
# obvious shell version is silently wrong, and both are called out again in the
# README's stuck-list:
#
#  1. ONLY THE CONTAINER GOES IN THE CGROUP. The naive version writes $$ (this
#     shell) into cgroup.procs before unsharing, so the limit covers this
#     shell, unshare, and the container together. A 64 MiB budget then pays for
#     three processes, and which of them the OOM killer picks is luck: you get
#     exit 1 from a failed fork or a dead wrapper as often as you get the
#     container killed. mycontainer.c admits the clone()d child by pid from the
#     parent; the shell has no handle on that pid, so the container admits
#     itself as its first act inside the namespaces (its $$ is 1 there, and a
#     write to cgroup.procs resolves pids in the writer's pid namespace).
#     Everything it forks afterwards is charged to the same cgroup.
#
#  2. exec unshare, NOT unshare. Without exec, this shell stays alive as the
#     parent, and a SIGTERM aimed at the pid you started goes to the SHELL --
#     unshare never sees a signal, --kill-child never fires, and the container
#     is orphaned to init and keeps running. exec replaces this shell with
#     unshare, so the pid you started IS unshare: signals reach it, --kill-child
#     tears the container down, and there is no wrapper left to leak.
#     The price is that nothing of ours survives the container to rmdir the
#     cgroup leaf -- which is exactly why real runtimes keep a supervisor
#     process around. We sweep stale empty leaves on the next run instead.
#
# Two places where the shell route is honestly worse than the C route, both
# because unshare(1) owns the parent process and we do not (measured on
# util-linux 2.39.3):
#
#   * unshare --fork BLOCKS SIGINT and SIGTERM in the waiting parent
#     (/proc/<pid>/status SigBlk: 0000000000004002). A SIGTERM aimed at the
#     container is therefore deferred until the container exits by itself;
#     SIGKILL works, and --kill-child then takes the container down with it.
#     mycontainer.c catches both and tears down immediately.
#   * unshare(1) does not propagate a child that died from a signal: when the
#     OOM killer takes the container, unshare fails with "sigprocmask unblock
#     failed" and exits 1 instead of 128+9. So do not read Part 5's OOM kill
#     off this script's exit status -- run the hog as a CHILD of a shell
#     inside the container and read the shell's $? (assert.sh does exactly
#     that, which is why its memory check works for both routes).
#     mycontainer.c returns 137.
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

# --- Part 5: optional cgroup v2 leaf. ---
# The leaf holds processes, so it enables no controllers of its own -- cgroup
# v2's "no internal processes" rule. memory/pids are usable here because the
# PARENT lists them in its cgroup.subtree_control; ./probe.sh checks that.
CG=""
MYUID=$(id -u)   # systemd's per-user slice is named after the uid, not "1000"
BASE="${LAB11_CGROUP_BASE:-/sys/fs/cgroup/user.slice/user-$MYUID.slice/user@$MYUID.service}"
if [ -n "$MEM" ] || [ -n "$PIDS" ]; then
	# Sweep leaves left by earlier runs. rmdir on a cgroup with live processes
	# in it fails, so this can only ever remove genuinely dead ones.
	for old in "$BASE"/mycontainer.sh.*; do
		[ -d "$old" ] && rmdir "$old" 2>/dev/null || true
	done

	CG="$BASE/mycontainer.sh.$$"
	mkdir -p "$CG"
	[ -n "$MEM" ]  && { echo "$MEM" > "$CG/memory.max"; echo 0 > "$CG/memory.swap.max"; }
	[ -n "$PIDS" ] && echo "$PIDS" > "$CG/pids.max"
	echo "mycontainer.sh: cgroup $CG" >&2
fi
export CG

# The inner script runs inside the new namespaces (still on the host root at
# first) and installs the private root before exec'ing the command.
INNER='
	set -eu
	ROOTFS="$1"; shift

	# Admit ONLY this process -- see note 1 in the header. $$ is 1 in the new
	# pid namespace, and that is what cgroup.procs wants: it resolves the pid
	# it is given in the pid namespace of whoever writes it. Must happen before
	# we fork anything, so that everything below is charged here too. Done
	# while the host /sys is still reachable, i.e. before pivot_root.
	[ -n "${CG:-}" ] && echo $$ > "$CG/cgroup.procs"

	hostname container
	mount --make-rprivate /
	mount --bind "$ROOTFS" "$ROOTFS"
	for d in null zero full random urandom tty; do
		mount --bind "/dev/$d" "$ROOTFS/dev/$d" 2>/dev/null || true
	done
	[ -n "${LAB11_SHARE:-}" ] && mount --bind "$LAB11_SHARE" "$ROOTFS/share" || true
	cd "$ROOTFS"
	"$PIVOT" . oldroot          # pivot_root(new_root=".", put_old="oldroot")
	cd /
	mount -t proc proc /proc     # fresh /proc reflects THIS pid namespace
	umount -l /oldroot           # detach the host tree; now unreachable
	exec "$@"
'

exec unshare --user --map-root-user \
             --uts --pid --mount --net --fork --kill-child \
             -- /bin/sh -c "$INNER" sh "$ROOTFS" "$@"
