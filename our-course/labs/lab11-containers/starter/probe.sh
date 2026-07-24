#!/bin/sh
# probe.sh -- what can this machine actually do? RUN THIS FIRST. GIVEN AND COMPLETE.
#
#   ./probe.sh
#
# This lab is the one place in the course where the *machine*, not your code,
# decides which exercises are possible. Unprivileged user namespaces are a
# distribution policy switch; cgroup v2 delegation is a systemd configuration;
# a static busybox may or may not be installed. The commonest way to lose an
# hour here is to spend it debugging a part your kernel has switched off.
#
# So: everything below is TRIED, not read off a sysctl. A permissive-looking
# sysctl is not evidence -- on Ubuntu 24.04, kernel.unprivileged_userns_clone=1
# and an AppArmor profile that refuses the clone anyway are a perfectly normal
# combination, and only the attempt tells them apart. Where a check fails, the
# sysctl values are printed as diagnosis, never as the verdict.
#
# Exit status: 0 if the unprivileged tier is fully open, 1 otherwise (which is
# not a failure of yours -- read the summary for what it costs you).

PASS=0; FAIL=0
ok()   { printf 'PASS  %-26s %s\n' "$1" "$2"; PASS=$((PASS+1)); }
no()   { printf 'FAIL  %-26s %s\n' "$1" "$2"; FAIL=$((FAIL+1)); }
info() { printf '      %-26s %s\n' "$1" "$2"; }

echo "=== lab11 capability probe ==="
info "kernel" "$(uname -sr)"
info "uid" "$(id -u) ($(id -un))"
[ "$(id -u)" = 0 ] && info "note" "running as root: the privileged tier is open by definition"
echo

# ---------------------------------------------------------------- tools ----
have() { command -v "$1" >/dev/null 2>&1; }

for t in unshare nsenter; do
	if have $t; then ok "tool: $t" "$(command -v $t)"
	else no "tool: $t" "missing -- install util-linux"; fi
done

# pivot_root(8) is normally in /usr/sbin, which is often not on a user's PATH.
PIVOT=$(command -v pivot_root 2>/dev/null || true)
[ -z "$PIVOT" ] && [ -x /usr/sbin/pivot_root ] && PIVOT=/usr/sbin/pivot_root
if [ -n "$PIVOT" ]; then ok "tool: pivot_root" "$PIVOT"
else no "tool: pivot_root" "missing -- needed only by the shell route (C calls the syscall)"; fi

if have cc || have gcc; then ok "tool: C compiler" "$(command -v cc || command -v gcc)"
else no "tool: C compiler" "missing -- the C route needs one; the shell route does not"; fi

# ------------------------------------------------------------- busybox ----
BB=$(command -v busybox 2>/dev/null || true)
if [ -z "$BB" ]; then
	no "busybox" "missing -- build-rootfs.sh needs it (apt install busybox-static)"
elif ldd "$BB" 2>/dev/null | grep -q '=>'; then
	no "busybox" "$BB is DYNAMIC -- the rootfs has no libc, so it will not start inside"
else
	ok "busybox" "$BB (static) -- $($BB 2>&1 | head -1 | cut -d' ' -f1-2)"
fi

echo
# ------------------------------------------------- unprivileged userns ----
# The real test: create a user namespace and get a uid map written into it.
# unshare --map-root-user does the map write from the parent, which is the
# part that fails when policy says no. We check the resulting uid, not just
# the exit status, because a userns with no map leaves you as the overflow
# uid (65534) rather than 0 -- which looks like success from a distance.
USERNS=no
if have unshare; then
	got=$(unshare --user --map-root-user -- id -u 2>/dev/null || echo "-")
	if [ "$got" = 0 ]; then
		ok "unprivileged userns" "created, uid 0 inside (map written)"
		USERNS=yes
	else
		no "unprivileged userns" "denied (got uid '${got}')"
		# Read /proc/sys directly: sysctl(8) lives in /usr/sbin, which is
		# often off a user's PATH, and this diagnosis has to print.
		for s in kernel.unprivileged_userns_clone \
		         kernel.apparmor_restrict_unprivileged_userns \
		         user.max_user_namespaces; do
			f=/proc/sys/$(printf '%s' "$s" | tr . /)
			[ -r "$f" ] && info "  $s" "$(cat "$f")"
		done
		info "  diagnosis" "a permissive sysctl above with a FAIL here means a"
		info "  " "security module (AppArmor/SELinux) is the one refusing"
	fi
fi

# All five namespaces at once, with a fork so the pid namespace takes effect.
NSALL=no
if [ "$USERNS" = yes ]; then
	got=$(unshare --user --map-root-user --uts --pid --mount --net --fork -- \
	      sh -c 'echo $$' 2>/dev/null || echo "-")
	if [ "$got" = 1 ]; then ok "uts+pid+mount+net ns" "all five unshared, pid 1 inside"; NSALL=yes
	else no "uts+pid+mount+net ns" "unshare succeeded but the child is pid '$got', not 1"; fi
elif [ "$(id -u)" = 0 ]; then
	got=$(unshare --uts --pid --mount --net --fork -- sh -c 'echo $$' 2>/dev/null || echo "-")
	if [ "$got" = 1 ]; then ok "uts+pid+mount+net ns" "as root, without a user namespace"; NSALL=yes
	else no "uts+pid+mount+net ns" "child is pid '$got', not 1"; fi
else
	# Neither route is open: not root, and no user namespace to be root in.
	# Report it rather than omitting the line -- a missing answer here is a
	# FAIL, exactly as in assert.sh.
	no "uts+pid+mount+net ns" "not attempted: needs an unprivileged user namespace (above) or root"
fi

echo
# ------------------------------------------------------- cgroup v2 ----
# Two separate questions, and they fail separately:
#   (a) is cgroup v2 mounted at all (not v1, not hybrid)?
#   (b) is there a directory we may mkdir in whose PARENT lists memory and
#       pids in cgroup.subtree_control? A controller that is not in the
#       parent's subtree_control simply has no files in the child, and
#       "no such file: memory.max" is what that looks like from inside
#       your runtime. This is the check the plan asks for by name.
CGBASE=""
if [ "$(stat -f -c %T /sys/fs/cgroup 2>/dev/null)" = cgroup2fs ]; then
	ok "cgroup v2" "mounted at /sys/fs/cgroup (unified)"

	MYCG=$(awk -F: '$1=="0"{print $3}' /proc/self/cgroup 2>/dev/null)
	info "our cgroup" "${MYCG:-unknown}"

	# Walk up from our own cgroup looking for the deepest ancestor that both
	# delegates memory+pids and lets us create a directory.
	d="$MYCG"
	while [ -n "$d" ]; do
		p="/sys/fs/cgroup$d"
		if [ -w "$p" ] && [ -r "$p/cgroup.subtree_control" ]; then
			sc=$(cat "$p/cgroup.subtree_control" 2>/dev/null)
			case " $sc " in *" memory "*) case " $sc " in *" pids "*)
				CGBASE="$p"; break ;; esac ;; esac
		fi
		[ "$d" = "/" ] && break
		d=$(dirname "$d")
	done
	[ -n "${LAB11_CGROUP_BASE:-}" ] && CGBASE="$LAB11_CGROUP_BASE"

	if [ -n "$CGBASE" ]; then
		info "delegated base" "$CGBASE"
		info "  subtree_control" "$(cat "$CGBASE/cgroup.subtree_control" 2>/dev/null)"
		# (b) prove it by doing it: create a leaf, set both limits, remove it.
		probe_cg="$CGBASE/lab11-probe.$$"
		if mkdir "$probe_cg" 2>/dev/null; then
			m=no; q=no
			echo 67108864 > "$probe_cg/memory.max" 2>/dev/null && m=yes
			echo 20        > "$probe_cg/pids.max"   2>/dev/null && q=yes
			rmdir "$probe_cg" 2>/dev/null
			[ $m = yes ] && ok "cgroup memory.max" "writable in a leaf under the base" \
			             || no "cgroup memory.max" "controller not delegated here"
			[ $q = yes ] && ok "cgroup pids.max"   "writable in a leaf under the base" \
			             || no "cgroup pids.max"   "controller not delegated here"
		else
			no "cgroup leaf creation" "cannot mkdir under $CGBASE"
		fi
	else
		no "cgroup delegation" "no ancestor of our cgroup delegates memory+pids to us"
		# Ask systemd for a scope whose OWN cgroup is delegated to us, and
		# work inside that. Putting the limits on the scope instead would cap
		# your runtime rather than the container, and Part 5 needs a leaf you
		# can create and write yourself.
		info "  fallback" "systemd-run --user --scope -p Delegate=yes -- \"\$SHELL\""
		info "  then inside" 'export LAB11_CGROUP_BASE=/sys/fs/cgroup$(cut -d: -f3 /proc/self/cgroup)'
		info "  or" "run Part 5 as root and set LAB11_CGROUP_BASE=/sys/fs/cgroup"
	fi
else
	no "cgroup v2" "not a unified hierarchy (v1 or hybrid) -- Part 5 needs v2"
	info "  fix" "boot with systemd.unified_cgroup_hierarchy=1"
fi

if systemd-run --user --scope --quiet -- true >/dev/null 2>&1; then
	ok "systemd-run --user" "available as the Part 5 fallback"
else
	info "systemd-run --user" "unavailable (only matters if the raw cgroup route failed)"
fi

echo
# ------------------------------------------------------------ summary ----
echo "=== verdict ==="
if [ "$NSALL" = yes ] && [ "$USERNS" = yes ]; then
	echo "  TIER: unprivileged. Parts 1-4 run as you, no root needed."
elif [ "$(id -u)" = 0 ]; then
	echo "  TIER: privileged only. Parts 1-3 and 5 run as root; Part 4 needs"
	echo "        unprivileged user namespaces and cannot be demonstrated here"
	echo "        (say so in TIERS.md -- a distribution that disables them is a"
	echo "        real deployment constraint, not a broken machine)."
else
	echo "  TIER: none available unprivileged. You need root, or a VM of your own."
fi
if [ -n "$CGBASE" ]; then
	echo "  PART 5: runnable. Export this before running your runtime:"
	echo "      export LAB11_CGROUP_BASE=$CGBASE"
else
	echo "  PART 5: not runnable through raw cgroupfs here -- use systemd-run, or root."
fi
echo
echo "  $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ]
