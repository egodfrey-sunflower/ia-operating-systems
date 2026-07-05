# Lab 7 — Containers from scratch

**Weeks:** 15–16 · **Budget:** 8–10 hours · **Track:** userspace · **Weight:** 5%

**Language:** C (gnu11).

> "A container is a process with a good imagination." You are going to build
> the imagination. By the end you will have `mycontainer`, a few hundred lines
> of C that starts a process which believes it is PID 1 on its own host,
> inside its own root filesystem, with a hard cap on the memory and CPU it may
> use — and, if you take the stretch, its own network interface. This is what
> Docker/`runc` do underneath — minus the parts we call out at the end.

---

## Background

A "container" is not a kernel object. It is an ordinary process that the
kernel has been asked to lie to, using three independent mechanisms:

- **Namespaces** (`clone(2)`/`unshare(2)`) virtualise a *global resource* so
  the process sees its own instance: PID, mount table, hostname (UTS),
  network stack, IPC objects, user/group IDs. A process in a new PID
  namespace is PID 1 there; it cannot see or signal processes outside it.
- **`pivot_root(2)`** swaps the root of the mount namespace, so `/` becomes an
  image you supply rather than the host's filesystem.
- **cgroups v2** account for and *limit* resource consumption (memory, CPU,
  …) of a set of processes.

None of these is a security boundary on its own — that is what capabilities,
seccomp and LSMs (AppArmor/SELinux) add, and which we deliberately skip. See
the reading list (`../../reading-list.md`) for the namespaces(7), cgroups(7),
`pivot_root(2)`, `user_namespaces(7)` and `clone(2)` man pages, which are the
primary sources for this lab.

> ### Security note — read this
> `mycontainer` is a **teaching tool, not a security boundary.** It drops no
> capabilities, applies no seccomp filter, and most tasks require **root**.
> A process inside it that gains code execution can do real damage to your
> host. **Never run an untrusted image**, and prefer a throwaway VM.

---

## Setup

You need a Linux machine (Ubuntu 24.04 assumed) with `gcc`, `make`, cgroup v2
(the default), and — only if you take the network stretch — iproute2 (`ip`).
Check:

```sh
stat -fc %T /sys/fs/cgroup      # want: cgroup2fs
which ip gcc make
```

### Get a root filesystem

From `starter/`:

```sh
make rootfs            # downloads a pinned Alpine minirootfs, verifies sha256
```

This fetches `alpine-minirootfs-3.21.3-x86_64.tar.gz` from
`dl-cdn.alpinelinux.org`, checks its SHA-256, and extracts it into `./rootfs/`.

**Offline / no-network fallback.** If the download is blocked, build a minimal
tree around the statically-linked BusyBox instead (needs the `busybox-static`
package, which provides `/bin/busybox` or `/usr/bin/busybox`):

```sh
sudo apt-get install -y busybox-static     # if not already present
make rootfs-busybox
```

Either target leaves you a usable `./rootfs/` with a shell at `/bin/sh`.

### On unprivileged user namespaces (Ubuntu 24.04)

Ubuntu 24.04 ships an AppArmor policy that **restricts unprivileged user
namespaces**. Check it:

```sh
cat /proc/sys/kernel/apparmor_restrict_unprivileged_userns   # 1 = restricted
unshare -Ur true && echo "userns works" || echo "userns blocked"
```

If it prints `1` / `blocked`, you have three options, in order of preference:

1. **Run the whole lab as root** (`sudo ./mycontainer run …`). Simplest, and
   required for Task 3 anyway.
2. Temporarily relax the restriction (root):
   `sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0`
   (revert by setting it back to `1`).
3. Allow this specific binary via an AppArmor `userns` profile (advanced; see
   the man page `apparmor.d(5)`).

Task 4 discusses this in depth.

Build the runtime:

```sh
cd starter && make            # produces ./mycontainer
```

---

## Tasks

Implement the four graded `TODO`-marked functions in `starter/mycontainer.c`
(a fifth, `setup_net()`, belongs to the unweighted stretch at the end). The
argument parser, the `clone(2)` stack, the parent↔child synchronisation pipe,
and a raw `pivot_root(2)` wrapper are provided. The CLI is:

```
mycontainer run [options] <rootfs> <cmd> [args...]
  -m, --mem SIZE    memory cap (e.g. 100M)
  -c, --cpus N      cpu cap as fraction of one CPU (e.g. 0.5)
  -u, --userns      new user namespace (map you -> root)
  -n, --net         new network namespace with a veth pair
```

### Task 1 — Namespaces (15%)

Make `mycontainer run <rootfs> <cmd>` create the child with `clone(2)` (the
skeleton already passes `CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS |
CLONE_NEWIPC`; namespaces(7) is the overview and clone(2) documents each
flag), then in `setup_namespaces()` set the hostname to `container`
(`sethostname(2)`). Demonstrate that the child observes itself as **PID 1**:

```sh
sudo ./mycontainer run ./rootfs /bin/sh -c 'echo $$; hostname'
# expect: 1
#         container
```

*Why PID 1?* Because `CLONE_NEWPID` gives the child a fresh PID namespace in
which it is the first process. Note it must be a *child* of the cloning
process that enters the namespace — the calling process itself stays in the
old namespace (the new one only takes effect for children); the skeleton
already does this for you.

### Task 2 — Filesystem: pivot_root (35%)

Implement `setup_rootfs()`. The shape of it: make the mount table **private
first**, so that nothing you do propagates back to the host — skipping this
can unmount your *host's* filesystems (`mount_namespaces(7)`, "Shared and
slave subtrees") — then bind-mount the new root, pivot into it, detach the old
root, and mount a fresh `/proc`. The NOTES section of `man 2 pivot_root`
walks through this exact sequence; work the details out from there, including
why `new_root` must be a mount point and where `put_old` has to live.

Verify isolation:

```sh
sudo ./mycontainer run ./rootfs /bin/sh -c 'ps -e | wc -l; ls /'
# ps shows only a couple of processes; ls shows the image, not your host /
```

**In your write-up, answer:** *why `pivot_root` and not `chroot`?* (compare what
each call does to the mount table; `man 2 pivot_root`, NOTES section)

### Task 3 — cgroups v2 (25%)

Implement `setup_cgroup()`. cgroup v2 is a single unified hierarchy at
`/sys/fs/cgroup`, and its interface is plain files; the "Cgroups version 2"
sections of cgroups(7) document every file you need. Requirements:

- Create a cgroup directory named **exactly** `mycontainer_<pid>` (the
  child's PID) directly under `/sys/fs/cgroup`. This name is the grader's
  contract, not a suggestion — the autograder looks for it. The kernel
  populates the new directory with control files.
- Apply **both** caps: the `--mem` memory limit and the `--cpus` CPU
  fraction (the CPU controller thinks in terms of a quota per period;
  cgroups(7) explains how a fraction of one CPU is expressed).
- Move the child into the cgroup **before** it execs.

Two pitfalls. A memory cap alone may not kill the hog — if its memory can
swap out, it just slows down instead of dying; one of the sibling memory
control files closes that escape. And a freshly-made sub-cgroup may have
**no controllers enabled** in it — cgroups(7)'s "Enabling and disabling
controllers" section explains the mechanism.

Demonstrate both caps:

```sh
# memory: a hog is OOM-killed at the cap instead of eating all RAM
sudo ./mycontainer run --mem 100M ./rootfs /bin/sh -c \
  'while :; do :; done'      # replace with a memory hog; expect it to die

# cpu: a spin loop pegs at ~50% of one core
sudo ./mycontainer run --cpus 0.5 ./rootfs /bin/sh -c 'while :; do :; done' &
# in another shell, read the cgroup's cpu.stat usage_usec over a 2 s window,
# or watch `top` — the process should sit near 50%.
```

### Task 4 — User namespaces (25%)

Add `CLONE_NEWUSER` (the skeleton does this when `--userns` is passed) and
implement `setup_userns()`, which the **parent** calls to write the child's
ID maps under `/proc/<pid>/`. The "User and group ID mappings: uid_map and
gid_map" section of user_namespaces(7) defines the map-file format and the
rules for writing it. Requirements:

- Map the container's root (uid/gid 0) to your own uid/gid, so that an
  unprivileged you becomes root *inside* the namespace.
- **Before** writing the gid map you must deny `setgroups` for the child
  (user_namespaces(7) says where and how).
- **If invoked as real root, map the full ID range instead** of a single
  ID. See the pitfall below — a single-uid map written by root is a trap.

**In your write-up, explain the setgroups rule:** why does the kernel refuse to
let you write `gid_map` until `setgroups` has been denied, and what escalation
does that prevent?

With this, an *unprivileged* user becomes root **inside** the container:

```sh
./mycontainer run --userns ./rootfs /bin/sh -c 'id'    # uid=0(root) inside
```

**The unmapped-inode pitfall.** If container-root gets `EPERM` on `mkdir`
inside its own image only when run as real root, ask *whose* uids are mapped.
Expect a question on this in your writeup: *whose* uid must be mapped for
`mkdir(put_old)` to succeed, and which kernel rule enforces it?

**The proc-mount ordering pitfall.** If `/proc` mounting fails with `EPERM`
only under `--userns`, look hard at *when* your Task 2 code mounts it relative
to the pivot.

**Ubuntu 24.04 caveat.** If `apparmor_restrict_unprivileged_userns=1` (see
Setup), the clone may succeed but your process gets **no capabilities** in the
new namespace, so `sethostname`/mounts fail with `EPERM`. Detect it, and
either run as root or relax the sysctl as described in Setup. Your write-up
should show the check and explain the failure mode.

### Stretch (unweighted) — Network namespace

This course does not teach networking — veth pairs, addressing, routing and
NAT get a proper treatment in a networking course — so this task is **optional
and unweighted**, and it is deliberately a **cookbook**: type it, watch it
work, then explain in a short paragraph what each `ip` line did.

Add `CLONE_NEWNET` (skeleton does this on `--net`) and implement
`setup_net()`, called by the parent with the child PID. A brand-new network
namespace has only a (down) loopback. Give it connectivity with a **veth
pair** — a virtual Ethernet cable with an end in each namespace.

Shelling out to `ip` via the provided `run_cmd()` helper is **acceptable and
expected** (a production runtime would use rtnetlink directly):

```
ip link add veth0 type veth peer name veth1     # create the pair (host)
ip link set veth1 netns <child-pid>             # push one end into the netns
ip addr add 10.0.7.1/24 dev veth0               # host side
ip link set veth0 up
```

Then, from **inside** the child (there is a marked spot in `child_fn` that
still runs with the host's `ip` before the pivot), bring up the container end:

```
ip link set lo up
ip addr add 10.0.7.2/24 dev veth1
ip link set veth1 up
ip route add default via 10.0.7.1
```

Demonstrate:

```sh
sudo ./mycontainer run --net ./rootfs /bin/sh -c 'ping -c1 10.0.7.1'
```

**Going further: NAT to the internet.** Enable forwarding
(`sysctl -w net.ipv4.ip_forward=1`) and add an nftables masquerade rule on the
host's uplink, e.g.:

```sh
nft add table ip nat
nft 'add chain ip nat postrouting { type nat hook postrouting priority 100 ; }'
nft add rule ip nat postrouting ip saddr 10.0.7.0/24 oif "<uplink>" masquerade
```

Then a container with a DNS server in `/etc/resolv.conf` can reach the outside.

---

## Hints

- `clone(2)` needs `_GNU_SOURCE` and a **stack pointer to the top** of the
  stack region (it grows down) — the skeleton handles this.
- glibc has no `pivot_root` wrapper; the skeleton provides one via
  `syscall(2)`.
- The parent must write `uid_map`/`gid_map` and set up the cgroup/veth
  **while the child waits** — that is what the pipe in the skeleton is for.
  The child blocks on `read()` until the parent closes its end.
- Watch the file-descriptor trap: `clone` without `CLONE_FILES` gives the
  child a *copy* of your fd table, so it also holds the pipe's write end.
  It must close that copy or the pipe never reaches EOF. (The skeleton does
  this — understand why.)
- Order matters: do cgroup/uid_map (and, for the stretch, veth) setup in the
  parent, then release the child, which then sets hostname, configures its
  veth end if it has one, and *finally* pivots (after which the host `ip`
  binary is gone).
- Debug mounts with `findmnt` and `cat /proc/self/mountinfo`; debug cgroups by
  `cat`-ing the control files.

---

## Deliverables

1. `mycontainer.c` implementing the four graded functions (plus `setup_net()`
   if you took the stretch).
2. A short **demo transcript** (`transcript.txt`) showing, with `sudo` where
   needed: PID 1 + hostname (T1), `ps`/`ls` isolation (T2), a memory hog being
   OOM-killed and a spin loop capped at ~50% (T3), and `id` as root via
   `--userns` (T4). If you took the network stretch, add the cross-namespace
   `ping`.
3. A **design note** (≤1 page) answering the two write-up questions
   (Task 2: `pivot_root` vs `chroot`; Task 4: the setgroups/gid_map rule) and
   the comparison below.

### Comparison — what does Docker add that this doesn't?

Address briefly in your design note:

- **Layered images (overlayfs):** Docker stacks read-only image layers under a
  writable layer via OverlayFS; we just extract a flat tarball.
- **Capability dropping:** real runtimes drop most of root's capabilities
  (`CAP_SYS_ADMIN`, …) — we keep them all.
- **seccomp:** a syscall-filter allowlist blocks dangerous calls; we have none.
- **Image distribution:** registries, content-addressed layers, signing — we
  `curl` one tarball.
- Others worth a sentence: pid1/reaping (`--init`), rootless mode done
  properly, resource accounting beyond mem/cpu, networking via CNI plugins.

---

## Rubric (100%)

| Task | Weight | PASS criteria |
|------|--------|---------------|
| T1 Namespaces      | 15% | child is PID 1; hostname is `container` |
| T2 pivot_root      | 35% | `/proc` isolated (few procs); host `/` not reachable; write-up explains pivot_root-vs-chroot + put_old |
| T3 cgroups         | 25% | memory hog OOM-killed at `--mem`; spin loop within ±20% of the `--cpus` cap |
| T4 user namespace  | 25% | unprivileged → uid 0 inside via uid/gid maps; setgroups-deny explained; AppArmor caveat handled |

The network-namespace stretch is unweighted: the autograder reports it as
PASS when it works and SKIP otherwise, and it never costs marks. The design
note (pivot_root/chroot, setgroups, Docker comparison) is graded as part of
T2/T4. Run `tests/run.sh <your-dir>` (as root for the full set) to
self-check.
