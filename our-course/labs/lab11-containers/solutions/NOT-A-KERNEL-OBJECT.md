# NOT-A-KERNEL-OBJECT.md — model answer

Model answer for the lab's principal write-up. Read the rubric in
`README.md` *after* you have written your own.

---

## Every primitive I used, and what it restricts

| Primitive | Created by | Restricts | Does **not** restrict |
|---|---|---|---|
| UTS namespace | `CLONE_NEWUTS` | which hostname/domain string the process can read and set | anything it can do with the machine |
| PID namespace | `CLONE_NEWPID` (+ a fork) | which process IDs *exist* as names; what `kill` can address; what the kernel will let it see in `/proc` | how many processes it may create |
| Mount namespace | `CLONE_NEWNS` | which mount table it sees, and therefore which filesystems have paths | which files it may open through the paths it does have |
| Network namespace | `CLONE_NEWNET` | which interfaces, routes, iptables rules and port numbers exist for it | how much bandwidth it uses |
| `pivot_root` | a syscall, inside the mount namespace | which directory is `/`, and — crucially — detaches the old root so it has no name at all | file permissions on what remains reachable |
| User namespace | `CLONE_NEWUSER` + a uid map | which uid/gid numbers mean what, and where its capabilities are valid | the actual kuid the kernel checks host files against |
| cgroup: `memory.max` | writing a file in cgroupfs | how much memory the process and its descendants may *consume* | what memory it can see |
| cgroup: `pids.max` | writing a file in cgroupfs | how many tasks it may *create* | which tasks it can see |

Six of these restrict **visibility**: they change the contents of a
namespace of names — hostnames, PIDs, paths, interface names, uid numbers.
Two restrict **consumption**: they change how much of a finite machine
resource the process may use. That is the line sheet 26 §B4(c) draws, and
it is not a stylistic distinction — the two are enforced by entirely
different kernel machinery, were added years apart, and fail in different
ways.

## Visibility versus consumption, made concrete

The measurable difference: a visibility limit changes what a *correct*
program observes; a consumption limit changes whether a *greedy* program
survives.

- `hog` knows nothing about cgroups, and needs to know nothing. Placed in
  a cgroup with `memory.max = 67108864` and `memory.swap.max = 0`, it was
  SIGKILLed:

  ```
  memory.max=67108864 memory.peak=67108864
  memory.events: low 0 high 0 max 22 oom 1 oom_kill 1 oom_group_kill 0
  ```

  Twenty-two times the kernel refused to grow the cgroup past its limit;
  once, it gave up and killed. Nothing was hidden from `hog` — it could
  still *see* a machine with 3.7 GB of RAM, `/proc/meminfo` said so, and
  `malloc` kept succeeding. It was simply not allowed to use it.

  `memory.swap.max = 0` is doing real work in that line and is not
  decoration: with swap available the cgroup's pressure is relieved by
  swapping instead of by killing, so the hog merely slows down, the OOM
  kill never happens, and the limit never demonstrates itself.

- The pids limit keeps the same kind of record. With `pids.max = 20` and
  `hog fork`:

  ```
  hog: fork failed after 18 children
  pids.max=20 pids.peak=20
  pids.events: max 1
  ```

  The shell, `hog` and eighteen children are twenty; the nineteenth
  `fork` returned `EAGAIN`. `pids.peak` sitting exactly on the cap and
  `max 1` — one refusal, ever — is the whole enforcement record, and it
  is worth reading precisely because "it stopped forking" is also what a
  bug in your own loop looks like.

- The mirror image: in the PID namespace, `ps` listed one process. Nothing
  was *taken away* from the container — the host's 300 other processes
  were still running, still consuming CPU, still on the same kernel. They
  merely had no names in that namespace.

So: visibility limits remove names; consumption limits remove resources. A
container that has only namespaces is perfectly isolated and completely
unlimited — it can starve the machine while seeing none of it. A container
that has only cgroups is perfectly limited and completely exposed. Neither
is a container, and each is useless without the other for a different
reason.

## Why `pivot_root` and not `chroot`

Both give a process a different `/`. `chroot(2)` sets the process's root
directory to a subtree of the *existing* mount tree — the rest of that
tree is still mounted, still there, merely not reachable by any path
starting at the new `/`. That is the escape: a process that retains
`CAP_SYS_CHROOT` (and root inside a user namespace has it) can
`chroot("subdir")`, then `chdir("../../../..")` — since its cwd is now
outside its new root, `..` keeps walking up past the root it was given —
and `chroot(".")` back onto the real root. Around ten lines of C, no
kernel bug involved. `chroot` was never a security boundary; it is a
build-system convenience that people mistook for one.

`pivot_root(new_root, put_old)` does something different in kind: it moves
the root **mount** of the current mount namespace. The old root is not
merely unreachable by path — after `umount2("/oldroot", MNT_DETACH)` it is
not in the mount table at all. There is no `..` chain to walk, because
there is no mounted object at the other end of it. That is the difference:
`chroot` hides a name, `pivot_root` removes the thing.

Which also explains its two preconditions, both of which are about the
mount tree rather than the directory:

- the new root must itself be a **mount point** — you are moving mounts,
  and a plain directory is not one, hence the bind of the rootfs onto
  itself;
- propagation must be private (`mount --make-rprivate /`) — otherwise the
  later unmount of the old root propagates back to the host's mount
  namespace and takes the host's root with it. The kernel refuses rather
  than let you do that.

Verified: with `pivot_root` in place, an absolute host path that exists
(`/home/edward/.../HOST_ONLY_MARKER`) does not resolve inside. With the
`pivot_root` step removed and everything else identical, it does.

That check only shows the pivot happened; it would pass just as well with
`/oldroot` still mounted underneath. What shows the old root actually
*detached* is the mount table, read from inside the container:

```
/ # cat /proc/mounts
/dev/sda1 / ext4 rw,relatime 0 0
udev /dev/null devtmpfs rw,nosuid,relatime,size=1937408k,...,inode64 0 0
    [five more udev binds: /dev/zero /dev/full /dev/random /dev/urandom /dev/tty]
proc /proc proc rw,relatime 0 0
/ # grep oldroot /proc/mounts; echo "rc=$?"
rc=1
/ # grep -c oldroot /proc/self/mountinfo
0
/ # ls -a /oldroot
.
..
```

Eight mounts, and the host's several dozen are not among them — not the
host root, not `/home`, not `/boot`, and no `/oldroot` in either
`/proc/mounts` or the fuller `/proc/self/mountinfo`. `/oldroot` is still
a *directory*, because it is part of the rootfs template on disk; it is
empty because there is no longer a mount at it. That is exactly the
distinction this section is about, visible in one listing: `chroot` would
have left the old root mounted and merely unnamed, and `..` would still
have had somewhere to go.

## What "root inside" turned out to mean

Inside, `id -u` reports 0 and the process can `sethostname`, `mount`,
`pivot_root` and `umount`. On the host, the very same task is:

```
Name:   sh
Uid:    1000    1000    1000    1000
NSpid:  2229744 1
```

uid 1000, unprivileged, and a file it creates as "root" is owned by uid
1000 on the host.

The other half is what that "root" is refused. With the host's `/etc`
bind-mounted at `/share`:

```
/ # echo "id -u inside: $(id -u)"
id -u inside: 0
/ # ls -l /share/shadow
-rw-r-----    1 65534    65534          986 Feb  8  2025 /share/shadow
/ # cat /share/shadow > /dev/null; echo "rc=$?"
cat: can't open '/share/shadow': Permission denied
rc=1
```

`/etc/shadow` is mode 0640 `root:shadow` on the host; the process reports
uid 0; the open is denied anyway, because the kernel checks host files
against the kuid and the kuid is 1000. And note the owner column: 65534,
not 0. Host uid 0 has no entry in this namespace's map, so it has no name
here at all — the file's owner is not "root seen from another angle", it
is a uid this namespace cannot express.

Nothing was gained. What the user namespace did was
introduce a *second* uid number space, and grant capabilities that are
valid **only against objects owned by that namespace** — its own mounts,
its own PIDs, its own hostname. Capability checks against anything owned
by the initial user namespace still see uid 1000 and refuse.

So "root" is not a uid. `0` is an integer whose meaning is
namespace-relative; privilege lives in the capability set, and a
capability set is only meaningful relative to a user namespace. The
practical demonstration is this whole lab: every namespace operation above
requires `CAP_SYS_ADMIN`, and not one command in it was run with `sudo`.
The user namespace is the primitive that makes the other four affordable,
which is exactly ch. 53's least-privilege argument arriving as a mechanism
instead of a slogan.

It is also the primitive with the largest attack surface, which is why
distributions switch it off: it is the one place where an unprivileged
user can reach a great deal of kernel code that historically assumed a
privileged caller. `probe.sh` exists because of that trade-off.

## Two things that only became clear by getting them wrong

**A PID namespace without a fresh `/proc` looks completely broken.** After
`--pid --fork`, the process was pid 1 — and `ps` listed 135 processes,
because `ps` reads `/proc`, and `/proc` was still the host's. The PID
namespace was working exactly as specified; the tool was reading a
different primitive's output. Two independent restrictions have to line up
before the *observable* everyone associates with containers appears, and
neither of them is "the container".

**A process that is not in the cgroup is not limited by it.** With the
launcher in the cgroup instead of the container, a 20-process limit left
the workload only 17–18 slots: the wrapper shell and `unshare` had taken
the others — two of them when the shell stays around to wait, one when it
`exec`s `unshare` and the two become a single process. With nothing in the
cgroup at all, the memory hog ran until an external safety limit stopped
it. The cgroup does not restrict "the
container": it restricts *the set of tasks listed in its `cgroup.procs`*,
which is a set you have to construct, correctly, by hand. Nothing checks
that the set you built is the one you meant.

## Why "container" names a configuration, not a thing

At no point did I create a container. There is no `container_create()`,
no container id, no handle to pass to a syscall, nothing to `close()`.
What exists after all of the above is a perfectly ordinary process — one
`task_struct`, scheduled by the same scheduler as everything else, running
on the same kernel — that happens to hold references to five namespaces it
does not share, has had its root mount replaced, and appears in the
`cgroup.procs` of a cgroup with two limits set. Every one of those
properties is independently settable, independently removable, and
independently meaningful; each existed and was useful before anyone said
"container".

The proof that it is a configuration and not an object is that the
configuration space is continuous and no point in it is privileged. Drop
the network namespace and you have a container that shares the host's
network — which is what `docker run --net=host` is, and it is still called
a container. Drop the user namespace and you have the default Docker
configuration for most of that project's life. Keep only the cgroup and
you have a systemd service. Keep only the mount namespace and you have
`snap`. Add a seccomp filter and you have the Docker default profile;
replace the kernel interface entirely and you have gVisor. There is no
threshold anywhere along that path at which the kernel starts treating the
process differently, because the kernel has no opinion about which subsets
are containers.

Which is the real answer to App. B's comparison. A virtual machine
monitor *is* an object: it creates a guest with a boundary the hardware
enforces, and there is a definite thing to point at. A container is a
convention about how to configure a process — and its two selling points,
density and startup time, follow directly from there being no thing to
create. Its weakness follows just as directly: the isolation is only as
good as the completeness of the configuration, and the configuration is
a list of independent things that you, or your runtime, have to get right
one by one. Firecracker's argument is that this list is too easy to get
wrong and a hardware boundary is worth the microVM; gVisor's is that the
shared kernel interface is the problem and should be reimplemented in
userspace. Both are arguments about the same fact: the boundary here is
assembled, not given.
