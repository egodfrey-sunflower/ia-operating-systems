# Lab 11 — Containers from primitives

**Week 26 · 4.0 hours · OSTEP App. B (virtual machine monitors) + Kerrisk's *Namespaces in operation* 1–3, Firecracker, gVisor, eBPF**

Linux, userspace, from scratch. You will need `unshare`, `pivot_root`, a
static `busybox`, a C compiler if you take the C route, and a kernel with
unprivileged user namespaces (`./probe.sh` decides). No xv6, no QEMU, no
second machine — and, on most modern desktop Linux, **no root**.

You will build a container runtime in miniature: a process in its own
namespaces, with a private root filesystem installed by `pivot_root`,
memory and process-count limits enforced by cgroups v2, and a user
namespace that makes it root inside and nobody outside. One primitive at
a time, in that order, each demonstrated before the next is added.

The reason for building it primitive by primitive rather than calling a
runtime is the conclusion: **there is no container object in the kernel.**
There is a process, with several independent restrictions applied to it,
each of which exists on its own and does one thing. Kerrisk's series makes
that argument; assembling the pieces yourself is what makes it stick. The
deliverable that carries the most marks in this lab is the note in which
you say which restriction came from which primitive.

App. B is the substrate this sits on. A virtual machine monitor
deprivileges a whole guest kernel and pays for it in trapped instructions
and shadow page tables; containers ask the cheaper question — *what if we
shared the kernel and just restricted what each process can see and use?*
Firecracker and gVisor are the two answers to what that costs you, and
this week you read both.

> **This lab cannot move.** It is the only week that covers containers, and
> it starts and ends in week 26. If time is short, take the hours from the
> project, not from here.

> **Do this on a machine you can break**, ideally a VM. `pivot_root` and
> cgroup writes are exactly the operations that make a system unusable when
> they go wrong.

## Layout

```
lab11-containers/
  README.md            this handout
  starter/probe.sh     RUN THIS FIRST: what your machine can do   <- given, complete
  starter/assert.sh    the advisory checks                        <- given, complete
  starter/build-rootfs.sh   builds a busybox rootfs               <- given, complete
  starter/hog.c        the Part 5 workload                        <- given, complete
  starter/mycontainer.c     C skeleton, flags enumerated          <- work here (route A)
  starter/mycontainer.sh    shell skeleton, unshare-based         <- work here (route B)
  solutions/           SPOILERS. Reference runtimes, model write-ups, rubric.
```

Copy `starter/` somewhere of your own and work there:

```sh
cp -r starter ~/lab11 && cd ~/lab11
./probe.sh                            # first, always
eval "$(./probe.sh | sed -n 's/^ *export /export /p')"   # sets LAB11_CGROUP_BASE
cc -static -O2 -o hog hog.c
cc -Wall -Wextra -O2 -o mycontainer mycontainer.c    # C route only
./build-rootfs.sh ./rootfs            # copies ./hog in if it is there
./assert.sh ./mycontainer.sh ./rootfs   # or ./mycontainer -- 0 pass, 9 fail on the skeleton
```

`solutions/` is deliberately left behind.

## Two routes; pick one

| | C route | Shell route |
|---|---|---|
| File | `starter/mycontainer.c` (~250 lines finished) | `starter/mycontainer.sh` (~120 lines finished) |
| You call | `clone()`, `mount()`, `pivot_root()`, `/proc/<pid>/uid_map` | `unshare(1)`, `mount(8)`, `pivot_root(8)` |
| Time | the full four hours | closer to two and a half |
| Take it if | you want the syscall-level view | you want the decomposition |

**Both are accepted and both are marked the same.** They decompose the
problem identically — the same five primitives in the same order, with the
same preconditions; only the amount of syscall you touch by hand differs.
The C route is *recommended only* if you want to see the system calls
themselves, which is a real reason. Both skeletons take the same command
line, so `assert.sh` drives either.

Parts 1 and 2 are worth doing with bare `unshare` on the command line
before you write any file at all. That is what Part 1 asks for, and it
takes five minutes.

---

# ⚠ Read `./probe.sh` before you plan your afternoon

This is the one lab in the course where the **machine**, not your code,
decides which exercises are possible. Unprivileged user namespaces are a
distribution policy switch; cgroup v2 delegation is a systemd
configuration. The commonest way to lose an hour here is to spend it
debugging a part your kernel has switched off.

| Tier | Needs | Covers |
|---|---|---|
| **Unprivileged** | a kernel with unprivileged user namespaces enabled, and cgroup v2 delegated to your user | everything: Parts 1–5 |
| **Privileged** | `root` or `CAP_SYS_ADMIN` | Parts 1–3 and 5 without a user namespace; Part 4 cannot be demonstrated without unprivileged user namespaces, because it *is* them |

`probe.sh` tries each capability rather than reading a sysctl, because a
permissive sysctl is not evidence: on Ubuntu 24.04,
`kernel.unprivileged_userns_clone=1` together with an AppArmor profile
that refuses the clone anyway is a perfectly ordinary combination, and
only the attempt tells them apart. If a check fails, the probe prints the
relevant sysctls as *diagnosis*, and tells you what it costs you.

**If your machine cannot do the unprivileged tier, that is a finding, not
a failure.** Record it in `TIERS.md`; a distribution that disables
unprivileged user namespaces is making a defensible security argument, and
saying what it is worth part of the marks.

## Prerequisites

- Lab 1 (process creation, `exec`) and Lab 5 (comfort with process-level
  plumbing).
- App. B, read in week 25: trap-and-emulate deprivileging, shadow page
  tables, the semantic gap. Containers are the answer to "what if we
  shared the kernel instead?", and you need the question first.
- Kerrisk's *Namespaces in operation* parts 1–3, plus Firecracker and
  gVisor, all read this week.
- Ch. 53's least privilege from week 19 — Part 4 is that principle applied
  as a mechanism rather than a slogan.
- **Sheet 26 §B4 first.** It specifies a container from parts *on paper*:
  naming the primitive per requirement, ordering the setup calls, sorting
  visibility from consumption. This lab builds what that question
  specifies. Do the sheet, then build its answer.

## What you hand in

| Deliverable | Part | Weight | Marked |
|---|---|---|---|
| Each namespace demonstrated singly, with a transcript | 1 | 25% | `assert.sh` + rubric |
| PID + mount namespaces with a fresh `/proc` | 2 | 12% | `assert.sh` + rubric |
| `pivot_root`, old root detached, and the `chroot` note | 3 | 25% | `assert.sh` + rubric |
| User namespace with a UID map, both halves shown | 4 | 13% | `assert.sh` + rubric |
| cgroup v2 `memory.max` and `pids.max` enforced, accounting read back | 5 | 25% | `assert.sh` + rubric |
| `TIERS.md` and `NOT-A-KERNEL-OBJECT.md` | — | *how each part above is marked* | rubric |

Every part's weight splits between **it runs** (`assert.sh` agrees) and
**you can say what it restricts** (the write-ups). The split, part by
part, is in `solutions/README.md` — read that rubric *after* you have your
own results. A green `assert.sh` with a thin `NOT-A-KERNEL-OBJECT.md` is
not a finished lab; it is the easy 60% of one.

---

# Part 1 — Namespaces, one at a time (~1.0 h, 25%)

Create **one namespace at a time** and demonstrate the effect of each:

- **UTS** — change the hostname inside; show the host's is unchanged.
- **PID** — your process is 1; `ps` shows nothing else.
- **Mount** — make a mount that is invisible from outside.
- **Network** — no interfaces but loopback.

One short demonstration per namespace, kept as a transcript. Four of
them, in four separate runs. Do not build the combined runtime yet.

The point of doing them singly is that they are **independent bits**, not
a bundle: each is a separate flag, each restricts a different kind of
visibility, and any subset of them is a legal configuration. You cannot
see that from a runtime that switches all of them on at once — and
"which of these five things is the container?" is a question you should
find yourself unable to answer.

Two of the four have a complication you will meet immediately, and both
are stated in the skeleton's mechanics list rather than left for you to
find: the PID namespace does not renumber the process that asks for it,
and `ps` reads `/proc` rather than the kernel.

# Part 2 — A private process tree (~0.5 h, 12%)

Combine the PID and mount namespaces, and mount a fresh `/proc` inside, so
that `ps` inside the container reports only container processes. Then show
the *same* processes from the host, under different PIDs.

That last step is the part worth doing carefully: the process has two
PIDs, simultaneously, and neither is more real than the other. Write down
which number each side sees and what that implies about where the mapping
lives.

The mount namespace is doing something specific here, and it is not the
same thing the PID namespace is doing. Be able to say which of the two is
responsible for `ps` being wrong before you mount `/proc`, and which is
responsible for it being right afterwards.

# Part 3 — A private root with `pivot_root` (~1.0 h, 25%)

Build a minimal root filesystem with `./build-rootfs.sh` — it copies one
static busybox in and symlinks the applets, which is the whole trick to a
container image: **an image is a directory.** Read it once so you know
what your container's `/` contains, then use it; assembling a rootfs by
hand is an hour that teaches nothing about isolation.

Then use `pivot_root` to install it and unmount the old root, so the
container cannot reach the host tree. Prove the last part: put a file
somewhere on the host, and show that its absolute path does not resolve
inside.

**Then write the note the part is really about:** why `pivot_root` rather
than `chroot`? Both give a process a different `/`. One of them is
routinely used as a *convenience* and is not considered a security
boundary; the other is what every real runtime uses. Find out what the
escape from the first one actually is — it is short, well documented, and
does not need root — and say precisely what `pivot_root` does that closes
it. This note is part of `NOT-A-KERNEL-OBJECT.md`, and the rubric asks for
the mechanism, not the slogan.

# Part 4 — User namespaces and apparent root (~0.5 h, 13%)

Add a user namespace with a UID map, so the process is UID 0 inside and an
unprivileged UID outside. Demonstrate **both halves**:

- inside, it can do root-ish things — `id` reports 0, and it can mount, set
  the hostname, and own files in its own filesystem;
- outside, it cannot touch a host file it lacks permission for, and a file
  it creates as "root" is owned on the host by your unprivileged UID.

The second half is the one that matters, and it is the primitive that
makes the whole unprivileged tier of this lab possible: everything else
you have built so far needed `CAP_SYS_ADMIN`, and you have not been root
once.

Then ask what "root" turned out to name. It is not a uid; the uid is
just an integer that means different things in different namespaces.

# Part 5 — cgroups v2 limits (~1.0 h, 25%)

Create a cgroup v2, set `memory.max` and `pids.max`, put **the container
process** in it, and demonstrate enforcement with `hog`:

```sh
# Until your cgroup placement works, these are an unbounded allocator and a
# fork bomb running on your own machine, so put an outer bound on them. The
# scope's limits sit well above the ones you are testing, so they cannot be
# what fires if your cgroup is real; and `fork 200` stops after 200 children
# even when nothing at all caps it.
SCOPE='systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=0 -p TasksMax=200 --'

$SCOPE ./mycontainer --mem 67108864 ./rootfs /bin/hog mem       # killed at 64 MiB
$SCOPE ./mycontainer --pids 20      ./rootfs /bin/hog fork 200  # stops at the cap
```

Read the accounting back out of the cgroup files while it happens or just
after: `memory.events` (`oom_kill`), `memory.peak`, `pids.events`
(`max`), `pids.peak`. Quote the numbers in your write-up — "it died" is an
observation, "`oom_kill 1` at `memory.max 67108864` with `memory.peak`
just under it" is a measurement.

`hog` knows nothing about cgroups. That is the point: it is an ordinary
greedy process, and the limit is imposed entirely from outside, by the
cgroup it was placed in. Which raises the question the part exists to
ask: **what kind of restriction is this, compared to Parts 1–4?** Two of
the five primitives you have used are different in kind from the other
three. Sheet 26 §B4(c) drew that line on paper; here you can measure it.

One warning worth taking seriously, because it cost this course's build a
machine: a process that is not in the cgroup is not limited by it. If you
put the wrong process in — the launcher instead of the container, or
nothing at all because the write silently failed — the workload inherits
whatever cgroup it was started in, which is usually unlimited, and a
memory hog with no limit will take the box down. Check `cgroup.procs`
and `/proc/<pid>/cgroup` rather than assuming.

---

# What "done" looks like

- `mycontainer.c` **or** `mycontainer.sh`, taking a root-filesystem path
  and a command, and running it isolated.
- A demonstration transcript per namespace from Part 1.
- A container that cannot see host processes, cannot reach host files, is
  root inside and unprivileged outside, and dies at its memory limit.
- **`TIERS.md`** — which parts you ran privileged, which unprivileged, and
  what differed. If everything ran unprivileged, say what made that
  possible and what the privileged route would have needed instead.
- **`NOT-A-KERNEL-OBJECT.md`** — every primitive you used and what it
  restricts, closing on why "container" names a configuration rather than
  a thing. This is the real deliverable of the lab.

Observable success: `ps` inside shows only your process tree; the host
root is unreachable; `id` reports 0 inside while the host shows an
unprivileged owner; and a process allocating past `memory.max` is killed
while the host is unaffected.

# How it's checked

**A capability probe, an assertion script, and a rubric — and that is
deliberately the lightest grading in this course.**

There is **no autograder here, and there is not going to be one.** The
environment dependence is irreducible: kernel version, distribution policy
on unprivileged user namespaces, and cgroup delegation all differ per
machine, and a harness that fails on half of student machines is worse
than none. So:

- **`./probe.sh`** reports which tier is available and which parts are
  runnable here, before you start. It tries each capability rather than
  reading a sysctl.
- **`./assert.sh [--no-scope] RUNTIME ROOTFS`** runs your container and
  checks nine observable properties — one `PASS`/`FAIL` line each — by
  looking at what came out, not at how you built it. It is a fast mirror,
  not a grade. By default it re-runs itself inside a `systemd-run --user
  --scope` with limits set well above everything it is testing (512 MiB of
  memory, 200 tasks), so that a runtime which puts the container in no
  cgroup at all still cannot take your machine down; it says so on the
  first line when it does this. Pass `--no-scope` if your machine has no
  user systemd instance, or if you want to watch the run in your own
  cgroup — you then have no safety net.
- **The two write-ups are rubric-marked by hand** against
  `solutions/README.md`, and that is where the marks actually
  concentrate. `NOT-A-KERNEL-OBJECT.md` is rewarded for correctly
  attributing each restriction to its primitive, and for correctly
  separating visibility from consumption.

`assert.sh` requires one thing of you: your runtime must accept

```
RUNTIME [--mem BYTES] [--pids N] ROOTFS CMD [ARGS...]
```

Both skeletons already do. It also needs `hog` built static and copied to
`ROOTFS/bin/hog`, which `build-rootfs.sh` does if `./hog` exists when you
run it.

**What `assert.sh` is honest about.** A check that cannot fail is worse
than no check, so every line of it was verified to fail against
deliberately broken containers — one with `pivot_root` skipped, one whose
PID namespace has no fork, one with no `/proc` mounted, one that ignores
`--mem`/`--pids`, and two with the UID map wrong. The skeletons score
**0 pass, 9 fail**. A missing answer is always a `FAIL`, never a skip:
there is no "could not determine" verdict.

**What it does not check**: whether you used `pivot_root` or `chroot`
(the observable difference is escapability, which is what your write-up
must argue, not something a script can see from outside); whether you
understand any of it; and anything at all about the two notes. Those are
the rubric's business, and they are most of the mark.

# Stretch goals

Unweighted. Do them if the five parts came easily.

- **A seccomp allowlist.** Add a seccomp-BPF filter restricting the
  container to a syscall allowlist, and find the smallest set your
  workload actually needs (`strace -c -f` is the honest way to start).
  This is sheet 26 §B4(d)'s hardening layer made real, and it connects
  straight to gVisor's argument: once you are enumerating the syscalls a
  workload may make, you are most of the way to arguing that the kernel
  interface itself is the attack surface. Note which syscalls surprised
  you.
- **A veth pair.** Give the container real connectivity — the piece Part 1
  deliberately leaves as "loopback only". Create a veth pair, move one end
  into the container's network namespace, address both ends, and ping
  across. Then note what you had to do *on the host* to make it work, and
  who has to be root to do it: this is where the unprivileged tier ends,
  and it is why rootless container runtimes ship a userspace network stack.

---

# If you get stuck

Every one of these is a real symptom with a specific cause. The question
after it is the one to answer; none of them is a trick.

- **`clone()` returns and then everything segfaults immediately** — what
  did you give it to run on? The skeleton's mechanics list item 1 says
  what, but not why: which direction do stacks grow?
- **`id -u` inside reports 65534** — that is the overflow uid, the kernel's
  answer for "a uid with no mapping". Which of the two possibilities is
  it: no map was written, or the process reached `execve` before the map
  was?
- **Writing `gid_map` fails with `EPERM` while `uid_map` worked** — one
  extra file has to be written first, and it is not a map. Why would the
  kernel insist on that before letting you name groups?
- **`pivot_root` fails with `EINVAL` and everything looks right** — it has
  two preconditions and checks both first. One is about what `new_root`
  *is*; the other is about what would happen to the host when you later
  unmount the old root. Which of the two have you not done?
- **`ps` inside shows every process on the machine** — where does `ps` get
  its information? It is not a system call.
- **`ps` inside shows nothing at all, not even your shell** — the opposite
  problem, and also about `/proc`. What is mounted there now?
- **You mounted a fresh `/proc` and the mount succeeded, but the host's
  `ps` output changed too** — mounts propagate. Which step were you told
  to do before `pivot_root`, and what does it do that you are now seeing
  the absence of?
- **`echo $$` says you are not pid 1, but you definitely passed the PID
  flag** — a PID namespace takes effect for one specific process and not
  another. Which one, and is the process you are testing in that one?
- **The container starts, and the very next `fork()` fails** — you have a
  PID namespace with nobody as its init. What is supposed to occupy pid 1,
  and did anything?
- **Your absolute host path still resolves inside the container** — you
  changed what `/` means but did not remove what it used to mean. What is
  still mounted, and where?
- **`memory.max`: no such file or directory** — the file exists only when
  a controller is switched on somewhere else. Where, and by whom? Run
  `./probe.sh`; it names the directory.
- **You set `memory.max` and the hog sails past it** — check two things:
  whether the process is actually in the cgroup (`/proc/<pid>/cgroup`),
  and what the machine did with the pages instead of failing to allocate
  them. There are two files, not one.
- **The hog is killed at roughly the right size but your launcher dies
  too, or the exit status is not 137** — who else is in that cgroup, and
  who is *supposed* to be? Read `cgroup.procs` while it runs.
- **`rmdir` on your cgroup fails with `EBUSY` after the container exits** —
  a cgroup directory can only be removed when it is empty. Empty of what,
  and did the pid namespace's death really take everything with it?
- **`--pids 20` and the fork bomb still runs away** — same question as
  the memory case: is the *forking* process in the cgroup, or only its
  parent? Which processes count against `pids.current`?
- **Killing your runtime leaves the container running with ppid 1** — the
  signal went somewhere. Which process did you actually signal, and was it
  the one waiting for the container?
- **Everything works as root and nothing works as you** — read
  `./probe.sh` output again, and then write the finding into `TIERS.md`.
  This is the most interesting failure in the lab.
