# Part 1 — one namespace at a time (model transcript)

Machine: Linux 6.8.0-60-generic, Ubuntu 24.04, uid 1000, unprivileged
throughout — every command below was run as an ordinary user. `./probe.sh`
first:

```
PASS  unprivileged userns        created, uid 0 inside (map written)
PASS  uts+pid+mount+net ns       all five unshared, pid 1 inside
PASS  cgroup memory.max          writable in a leaf under the base
PASS  cgroup pids.max            writable in a leaf under the base
  TIER: unprivileged. Parts 1-4 run as you, no root needed.
```

Each demonstration below adds **exactly one** namespace to `--user`. The
`--user` is not part of the demonstration; it is what makes the rest
possible without root, and Part 4 is where it becomes the subject.

---

## UTS — the hostname

```
$ hostname
Main
$ unshare --user --map-root-user --uts -- sh -c 'hostname demo-uts; echo "inside: $(hostname)"'
inside: demo-uts
$ hostname
Main
```

Set from inside, and the host never sees it. The UTS namespace holds
exactly two strings — the hostname and the NIS domain name — and nothing
else. It is the smallest namespace Linux has, and it is a good one to
start with precisely because it makes the shape of the mechanism obvious:
a copy of one piece of global kernel state, per namespace.

## PID — process identifiers

The first run is the mistake, kept deliberately:

```
$ unshare --user --map-root-user --pid -- sh -c 'echo "inside, no fork: pid=$$"'
inside, no fork: pid=2229659
```

Not 1. `unshare(2)` with `CLONE_NEWPID` creates the namespace but does not
move the calling process into it — a process's PID is fixed for its
lifetime, so only *children* created afterwards can be numbered in the new
namespace. Add the fork:

```
$ unshare --user --map-root-user --pid --fork -- sh -c 'echo "inside, forked: pid=$$"'
inside, forked: pid=1
```

The same process, seen from the host while it runs:

```
$ grep -E '^(Name|NSpid|Uid)' /proc/2229744/status
Name:   sh
Uid:    1000    1000    1000    1000
NSpid:  2229744 1
```

`NSpid` is the whole idea in one line: **one process, two PIDs, in two
namespaces, neither more real than the other**. The host number is not a
"true" PID that the container number disguises; they are two names for the
same task, valid in two different scopes. What the namespace changes is
which names exist, not which task is running.

## Mount — the mount table

```
$ mkdir mnt-demo
$ unshare --user --map-root-user --mount -- \
      sh -c 'mount -t tmpfs none ./mnt-demo && echo hello > ./mnt-demo/file &&
             echo "inside: $(ls mnt-demo)" && grep -c mnt-demo /proc/mounts'
inside: file
1
$ ls mnt-demo ; grep -c mnt-demo /proc/mounts
                       # empty
0
```

A filesystem mounted, written to, and read back inside; from outside the
directory is empty and `/proc/mounts` has never heard of it. The namespace
copies the *mount table*, not the filesystems: the tmpfs really exists
while the namespace does, and disappears with it.

## Network — interfaces

```
$ ip -o link | wc -l
3
$ unshare --user --map-root-user --net -- ip -o link
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN mode DEFAULT group default qlen 1000
```

A fresh network namespace gets its own loopback and nothing else — not the
host's interfaces made invisible, but a genuinely separate network stack,
with its own routing table, its own iptables rules and its own port
number space. Two containers can both bind port 80 and neither will hear
of the other. Note that `lo` comes up `DOWN`: even loopback has to be
brought up, because it is a new one.

---

## What doing them singly shows

Four flags, four independent effects, in four separate runs — and at no
point did any of them require the others. A process can have a private
mount table and the host's PIDs, or a private network and the host's
filesystem. Every subset is a legal configuration, and nothing in the
kernel says which subsets count as "a container".

That is why the question "which of these is the container?" has no answer,
and it is worth noticing now rather than at the end: what Docker calls a
container is a particular *habitual* subset — all of these, plus a
`pivot_root`, plus a cgroup, plus a seccomp filter — chosen by convention,
not by the kernel.

One more thing the single runs make visible, which the combined runtime
hides: after `--pid --fork`, `ps` inside still listed 135 processes. The
PID namespace was working perfectly; `ps` was reading the host's `/proc`,
because a mount namespace is a separate primitive and nothing had mounted
anything yet. That is Part 2's entire subject, and meeting it here — as a
puzzle rather than as a step in a recipe — is the argument for doing Part 1
one flag at a time.
