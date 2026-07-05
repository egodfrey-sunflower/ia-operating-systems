# ⚠️ SPOILERS — Lab 7 reference solution ⚠️

> **STOP.** This directory contains a complete working `mycontainer`. If you
> are a student, do not read it until you have your own implementation (or have
> genuinely given up on a task and want to compare). Everything here is graded
> in the handout as work *you* produce.

---

## What's here

- `mycontainer.c` — full implementation of the four graded tasks plus the
  network stretch (~500 lines).
- `Makefile` — identical to the starter's (`-Wall -Wextra -Werror -std=gnu11
  -g`), plus the `rootfs` / `rootfs-busybox` targets.

## Build & run

```sh
make                 # builds ./mycontainer under -Werror
make rootfs          # or: make rootfs-busybox   (offline)
sudo ./mycontainer run ./rootfs /bin/sh
```

## Assumptions / environment

The solution is written to work **run as root on stock Ubuntu 24.04**:

- **cgroup v2** unified hierarchy mounted at `/sys/fs/cgroup`
  (`stat -fc %T /sys/fs/cgroup` → `cgroup2fs`). The code writes `+memory +cpu`
  to `cgroup.subtree_control` best-effort; on a stock system the controllers
  are already available.
- **iproute2** (`ip`) present on `PATH` (used for the veth pair; the network
  stretch).
- **`busybox-static`** installed *only* if you use `make rootfs-busybox`
  offline; otherwise `make rootfs` needs network access to
  `dl-cdn.alpinelinux.org`.
- Running as **root** (real uid 0). Without `--userns`, `clone` of the pid/
  mount/net namespaces needs `CAP_SYS_ADMIN`. With `--userns` it also works
  for an unprivileged user **iff** unprivileged user namespaces are permitted
  (see below).

### The Ubuntu 24.04 unprivileged-userns restriction

`kernel.apparmor_restrict_unprivileged_userns=1` (the Ubuntu default) means an
unprivileged `--userns` run may `clone` successfully yet come up with **no
capabilities**, so `sethostname`/mount fail with `EPERM`. The fix is to run as
root, or `sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0`
(revert to `1` after). This is expected and is discussed in the handout (Task
4).

## Design notes (the boxed handout questions)

**`pivot_root` vs `chroot`.** `chroot` only changes the directory against
which absolute paths resolve; the process keeps a reference to the old root
(an open dir fd, a `..` traversal from a fd opened before the `chroot`, or a
second `chroot`) and can climb back out — it was never meant as a security
mechanism. `pivot_root` instead moves the mount that *is* the root of the
mount namespace to `put_old` and installs `new_root` in its place; we then
`umount2(put_old, MNT_DETACH)`, so the host root is no longer mounted anywhere
the container can reach. `put_old` must live under `new_root` because
`pivot_root` needs a place *inside the new tree* to park the old root during
the swap; we create `oldroot`, pivot into `.`, then detach and `rmdir` it.

**setgroups/gid_map.** Writing a `gid_map` for a user namespace is refused
until `/proc/<pid>/setgroups` contains `deny`, unless the writer has
`CAP_SETGID` over the parent namespace. The reason: if an unprivileged user
mapped to gid 0 could still call `setgroups(2)`, they could *drop* a
supplementary group and gain access to a file whose permissions relied on that
group being present (files are sometimes made inaccessible to a group). Denying
`setgroups` closes that escalation before any mapping exists.

## Implementation shape

- `main` parses args, builds the `clone` flags, `clone`s `child_fn` on a 1 MiB
  stack, then (as parent) does `setup_userns` → `setup_cgroup` → `setup_net`
  and releases the child by closing the sync pipe.
- `child_fn` closes its inherited copy of the pipe write end (or the pipe
  never EOFs — a real deadlock we hit while writing this), waits for release,
  sets the hostname, brings up its veth end (while the host `ip` is still
  reachable), then `setup_rootfs` pivots and it `execvp`s the command as PID 1.
- Error paths kill+reap the still-blocked child so it never execs into a
  half-configured container.

## Self-check

`tests/run.sh` validates this solution. As root it exercises the full set
(pid-1, hostname, /proc isolation, pivot escape, cgroup mem OOM, cgroup cpu
cap, and the veth ping for the unweighted network stretch); unprivileged it
runs build + usage checks and, where the host
allows unprivileged userns, a hostname/PID-1 smoke test.

```sh
sudo ../tests/run.sh .
```

## Post-review fix: the uid_map range

The first version wrote `0 <uid> 1` unconditionally. Run as **root** against
a rootfs extracted by a normal user, that leaves the image's file owner (e.g.
uid 1000) unmapped in the namespace, and the kernel voids capability checks
against unmapped inodes (`capable_wrt_inode_uidgid`) — so container-root got
`EPERM` on `mkdir(put_old)` inside its own image. Found by the privileged
autograder tier. Now: unprivileged → `0 <uid> 1` (all we're allowed);
root → `0 0 4294967295` (full identity map, what real runtimes do).
`setup_rootfs` also now reports the exact failing step.

Second userns bug, same root-run test tier: the original code mounted the
fresh `/proc` *after* detaching the old root. In a user namespace, a new proc
mount is only allowed while a fully visible proc instance still exists in the
mount namespace (`fs_fully_visible`), so the userns path failed with EPERM at
that step. Fix: mount proc at `<rootfs>/proc` before `pivot_root` and let it
ride into the new root — bubblewrap's ordering. Plain-root runs never notice,
which is exactly why this tier of testing exists.
