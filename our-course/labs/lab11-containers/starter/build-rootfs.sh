#!/bin/sh
# build-rootfs.sh -- assemble a minimal root filesystem from a static busybox.
#
# GIVEN AND COMPLETE. You do not have to write this; building a rootfs from
# nothing is an hour that teaches nothing about isolation. Read it once so you
# know what your container's "/" actually contains, then use it.
#
# Usage:  ./build-rootfs.sh [DIR]        (default DIR: ./rootfs)
#
# What it does: makes the directory skeleton a shell needs, copies the ONE
# static busybox binary in, and symlinks every applet the lab uses to it. That
# is the whole trick to a container image -- it is a directory, nothing more.
# The kernel is shared; only the filesystem view changes.
set -eu

ROOT="${1:-./rootfs}"
BB="$(command -v busybox || true)"
[ -n "$BB" ] || { echo "build-rootfs: busybox not found on PATH" >&2; exit 1; }

# busybox must be statically linked -- there is no dynamic loader or libc in
# the rootfs, so a dynamic binary would fail to start inside the container.
if ldd "$BB" >/dev/null 2>&1 && ldd "$BB" 2>/dev/null | grep -q '=>'; then
	echo "build-rootfs: $BB is dynamically linked; need a static busybox" >&2
	exit 1
fi

echo "build-rootfs: building rootfs at $ROOT from $BB"
rm -rf "$ROOT"
mkdir -p "$ROOT"/bin "$ROOT"/proc "$ROOT"/sys "$ROOT"/dev "$ROOT"/tmp \
         "$ROOT"/etc "$ROOT"/root "$ROOT"/oldroot "$ROOT"/share
# oldroot: pivot_root's put_old mountpoint. share: optional host bind mount
# for the Part 4 ownership demo. Both pre-created so the container never has
# to mkdir into (and thus mutate) the shared rootfs template.

cp "$BB" "$ROOT/bin/busybox"
chmod 0755 "$ROOT/bin/busybox"

# Symlink the applets. busybox dispatches on argv[0], so each name is just a
# link to the one binary. Add more here if your command needs them.
for applet in sh ls cat mount umount hostname id ps sleep echo mkdir rmdir \
              grep wc head tail true env ip ifconfig ln pwd whoami dmesg df; do
	ln -sf busybox "$ROOT/bin/$applet"
done

# If a static ./hog (Part 5 workload) has been built, copy it in.
if [ -x ./hog ]; then
	cp ./hog "$ROOT/bin/hog"
	echo "build-rootfs: included ./hog for the cgroup demo"
fi

# A couple of niceties so an interactive shell is not completely bare.
# Pre-make /dev mountpoint files so the container can bind the host's device
# nodes onto them (it cannot create device nodes itself, unprivileged).
for dev in null zero full random urandom tty; do
	: > "$ROOT/dev/$dev"
done

printf 'root:x:0:0:root:/root:/bin/sh\n' > "$ROOT/etc/passwd"
printf 'root:x:0:\n'                     > "$ROOT/etc/group"
printf 'container\n'                     > "$ROOT/etc/hostname"

# NOTE: no device nodes. Creating /dev/null et al. needs CAP_MKNOD in the
# initial user namespace, which the unprivileged tier does not have. busybox
# sh runs fine without them; if a command you run needs /dev/null, your
# container can bind-mount the host's in, or mount a devtmpfs when privileged.

echo "build-rootfs: done. $(find "$ROOT" -maxdepth 2 | wc -l) entries; / contains:"
ls "$ROOT"
