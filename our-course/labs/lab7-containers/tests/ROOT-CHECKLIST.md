# Lab 7 — root validation checklist

The autograder changes of 2026-07 (cgroup-mem START/DONE/rc gating, cgroup-cpu
missing-cgroup FAIL, exit-code 3 for a skipped graded tier, scoped veth
cleanup) were validated statically and unprivileged only — this box has no
sudo. A human with root must confirm the privileged tier behaves as designed.
Run everything from `labs/lab7-containers/`; `busybox-static` must be
installed (the grader builds its own rootfs).

Use throwaway build dirs so mutations never touch the repo:

```sh
WORK=$(mktemp -d)
cp -r starter  "$WORK/starter"
cp -r solutions "$WORK/solutions"
```

## 1. Unmodified starter must now FAIL cgroup-mem (was: false PASS)

The old grader passed the starter as root because "no DONE + nonzero rc"
matched a runtime that dies at startup. The memhog now prints `START` after
its first allocation, and the check requires START present.

```sh
sudo tests/run.sh "$WORK/starter"
```

Expect: `FAIL  cgroup-mem: hog never started` (plus the other expected
starter FAILs: pid-1, hostname, proc-isolation, pivot, cgroup-cpu). Overall
exit code 1.

## 2. Solutions pass the full set

```sh
sudo tests/run.sh "$WORK/solutions"
```

Expect: every check PASS (veth stretch PASS or SKIP is fine), exit code 0,
and NO "NOT A SUBMISSION PASS" banner. In particular
`cgroup-mem: hog killed at cap (rc=137)` — rc 137 = 128+SIGKILL from the OOM
killer.

## 3. No-op setup_cgroup mutation must FAIL cgroup-cpu (was: SKIP)

```sh
cp -r solutions "$WORK/nocg"
# make setup_cgroup succeed without creating anything:
#   in $WORK/nocg/mycontainer.c, replace the body of setup_cgroup() with
#     (void)child; (void)mem_bytes; (void)cpus; return 0;
#   (keep cg_path untouched so cleanup_cgroup stays a no-op)
sudo tests/run.sh "$WORK/nocg"
```

Expect: `FAIL  cgroup-cpu: no /sys/fs/cgroup/mycontainer_<pid> cgroup`
(not SKIP), and `FAIL  cgroup-mem: hog reached DONE` since no cap exists.
Exit code 1.

## 4. Swap-leak mutation must FAIL with the swap diagnostic (was: PASS via rc=124)

```sh
cp -r solutions "$WORK/swapleak"
# in $WORK/swapleak/mycontainer.c setup_cgroup(), delete the two lines that
# write memory.swap.max ("Disable swap so the hog is actually OOM-killed").
sudo tests/run.sh "$WORK/swapleak"
```

Expect (on a host with swap enabled): `FAIL  cgroup-mem: hog timed out
instead of being killed` with the "memory cap did not bind ... re-read the
Task 3 pitfalls" hint. On a swapless host the hog is OOM-killed anyway and
this mutation is not observable — check `swapon --show` first.

## 5. veth cleanup is scoped (spot check)

Before a root run, `sudo ip link add veth0 type veth peer name vethX_probe`,
then run the grader against a tree WITHOUT --net support reaching the veth
test (e.g. kill the run early or use the starter). Confirm `veth0` still
exists afterwards (`ip link show veth0`), then delete it manually. The
grader now deletes `veth0` only when its own `--net` test ran.

## 6. Unprivileged userns tier PASSes with the userns restriction relaxed

On stock Ubuntu 24.04 the unprivileged tier SKIPs (AppArmor restricts
unprivileged user namespaces), so the userns checks — including the
`id -u` == 0 check — never actually run. Relax the restriction temporarily,
run the grader unprivileged (no `sudo`), then revert:

```sh
old=$(sysctl -n kernel.apparmor_restrict_unprivileged_userns)
sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
tests/run.sh "$WORK/solutions"
sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns="$old"
```

Expect the userns block to run (not SKIP) and every userns check to PASS,
including `userns: uid 0 inside (uid/gid maps)`. A `setup_userns()` that omits
the uid/gid maps must FAIL this check.

## Sign-off

- [ ] starter FAILs cgroup-mem with "hog never started"
- [ ] solutions: full PASS, exit 0, no banner
- [ ] no-op cgroup mutation FAILs cgroup-cpu
- [ ] swap-leak mutation FAILs with the "memory cap did not bind" hint (host has swap)
- [ ] pre-existing veth0 survives a grader run whose --net test did not run
- [ ] with the userns restriction relaxed, the unprivileged tier PASSes the
      `id -u` == 0 userns check (SKIPs on stock 24.04)
