# TIERS.md — model answer

Which parts ran privileged, which unprivileged, and what differed.

## The machine

Linux 6.8.0-60-generic, Ubuntu 24.04, ordinary user (uid 1000). Relevant
policy. The two sysctl values were read straight out of `/proc/sys`:
`probe.sh` prints them only in the FAIL branch of its user-namespace check,
as diagnosis, and this box passes that check, so it prints none of them.
The cgroup lines below are `probe.sh`'s own output.

```
$ cat /proc/sys/kernel/unprivileged_userns_clone             -> 1
$ cat /proc/sys/kernel/apparmor_restrict_unprivileged_userns -> 0

$ ./probe.sh
PASS  cgroup v2                  mounted at /sys/fs/cgroup (unified)
      delegated base             /sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service
        subtree_control          cpu memory pids
```

**Every part of this lab ran unprivileged. `sudo` was never used.** That
is the interesting result, and it needs explaining rather than
celebrating.

| Part | Ran as | What made it possible |
|---|---|---|
| 1 namespaces singly | uid 1000 | user namespace first; every other `unshare` flag then needs only `CAP_SYS_ADMIN` *in that namespace* |
| 2 pid + mount + `/proc` | uid 1000 | same; mounting a fresh procfs needs a pid namespace you own, which we have |
| 3 `pivot_root` | uid 1000 | `CAP_SYS_ADMIN` in the new user namespace covers `mount` and `pivot_root` on mounts that namespace owns |
| 4 user namespace | uid 1000 | this is the part that *is* the mechanism; it cannot be demonstrated any other way |
| 5 cgroups v2 | uid 1000 | systemd delegates `cpu memory pids` on `user@1000.service`, so the leaf directories are ours to create and write |

## What "privileged" would have changed

`mycontainer --no-userns` exists for this comparison: it builds the same
container without a user namespace, and needs real root to do it. **I did
not run it** — this machine is not one I have root on, and the lab's own
advice is not to do `pivot_root` experiments as root on a machine you
cannot break. So the comparison below is argued from the mechanism, not
measured, and it is labelled as such. (If you *do* have a VM, run it both
ways: `assert.sh` should report the same nine lines either way, because it
looks at the container, and the container is the same.)

Everything that differs is on the *outside*:

- **Who may do it.** Without the user namespace, every `mount`,
  `pivot_root` and `sethostname` needs `CAP_SYS_ADMIN` in the initial user
  namespace — i.e. real root, i.e. a process that can also reboot the
  machine, load kernel modules, and read every file on it. With it, the
  same operations are done by uid 1000 with capabilities that are only
  valid against objects its own namespace owns.
- **What a bug costs.** A defect in a root-run runtime is a host
  compromise. The same defect in the unprivileged one leaves the attacker
  as uid 1000, because that is what the kernel checks host objects
  against — `NSpid: 2229744 1` with `Uid: 1000` is the whole story.
- **What the container may do to the host filesystem.** Root-run, a
  container that writes into a bind-mounted host directory writes as uid
  0. Unprivileged, the identical container's files come out owned by uid
  1000 — verified: the canary file created by a process reporting `id -u`
  = 0 inside is `-rw-rw-r-- 1000 1000` on the host.
- **Device nodes.** `mknod` needs `CAP_MKNOD` in the *initial* user
  namespace, which the unprivileged tier does not have at all. So
  `build-rootfs.sh` creates empty regular files under `/dev` and the
  runtime bind-mounts the host's `/dev/null` etc. onto them. A root-run
  runtime would just `mknod` or mount a `devtmpfs`. This is the one place
  where the unprivileged tier is genuinely poorer, not merely safer, and
  it is the reason rootless Podman ships the same workaround.

## What a machine with the switch off would have cost

If `kernel.apparmor_restrict_unprivileged_userns` had been 1 — the Ubuntu
23.10+ default, which this machine has had turned off — `unshare --user`
fails and **Parts 1, 2, 3 and 5 all require root, while Part 4 cannot be
demonstrated at all**, because Part 4 *is* unprivileged user namespaces.
`probe.sh` reports that case explicitly rather than letting you discover
it forty minutes in.

That is a real deployment consideration and not a broken machine. The
argument for the switch is that user namespaces let unprivileged code
reach a large amount of kernel that historically assumed a privileged
caller, and a substantial share of Linux local-privilege-escalation CVEs
have entered through exactly that door. The argument against is that
turning them off makes every rootless container runtime — the entire
mitigation for "containers used to require a root daemon" — unavailable,
so you trade a class of kernel-attack-surface bugs for the certainty that
container runtimes run as root. Distributions genuinely disagree about
this; Ubuntu restricts it by AppArmor profile, Debian and Fedora do not.

## Two things the shell route cannot do that the C route can

Both routes produce identical containers (`assert.sh`: 9/9 for each), but
`unshare(1)` owns the parent process, and that costs the shell route two
things — measured on util-linux 2.39.3:

1. **Signal handling.** `unshare --fork` blocks SIGINT and SIGTERM in the
   waiting parent (`/proc/<pid>/status` → `SigBlk: 0000000000004002`), so
   a SIGTERM aimed at the container is deferred until the container exits
   on its own. SIGKILL works, and `--kill-child`'s `PR_SET_PDEATHSIG` then
   tears the container down. `mycontainer.c` installs its own handler and
   tears down on either.
2. **Exit status.** `unshare(1)` does not propagate a child that died from
   a signal: when the OOM killer takes the container it exits 1 with
   `sigprocmask unblock failed`, not 137. `mycontainer.c` returns
   `128 + WTERMSIG` and reports 137. So Part 5's OOM kill has to be
   observed from inside the container on the shell route — run the hog as
   a child of a shell and read `$?` — which is what `assert.sh` does, so
   that its memory check works for both routes.

Neither is a difference in the *container*. Both are differences in the
supervisor, which is the part a real runtime spends most of its code on.
