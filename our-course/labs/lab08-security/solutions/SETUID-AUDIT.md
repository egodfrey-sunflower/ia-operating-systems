# SETUID-AUDIT.md — model audit (Part 4)

> Model answer on the author's machine, to show the rubric. Yours will differ —
> a setuid audit of your own machine has no fixed answer, which is the point.

## 1. Inventory

Read-only scan: `find / -perm -4000 -type f 2>/dev/null`. Forty setuid binaries
here; the interesting ones:

```
/usr/bin/sudo                                 run a command as another user
/usr/bin/passwd                               change your password
/usr/bin/su                                    become another user
/usr/bin/mount, /usr/bin/umount               mount/unmount filesystems
/usr/bin/chfn, /usr/bin/chsh                   edit your GECOS / login shell
/usr/bin/gpasswd, /usr/bin/newgrp             group membership / password
/usr/bin/at                                    schedule a job as yourself
/usr/bin/fusermount3                          mount a FUSE filesystem
/usr/lib/dbus-1.0/dbus-daemon-launch-helper   launch a system-bus service
/usr/lib/polkit-1/polkit-agent-helper-1       polkit authentication
/usr/lib/openssh/ssh-keysign                  host-based auth signing
```

All are setuid **root** (owner root, mode `u+s`). Each is a place where an
unprivileged user is deliberately allowed to invoke root code — so each is a
trust boundary, and the confused-deputy risk is exactly proportional to how much
each is trusted with.

## 2. Two analysed

### `/usr/bin/passwd`

- **Privilege it needs:** write access to `/etc/shadow` (mode `0640 root:shadow`),
  which an ordinary user cannot read or write. Changing your own password means
  editing that file.
- **Why the design gives it root:** the data a user is allowed to change (their
  own hash) lives in a file they must not be able to read wholesale (everyone
  else's hashes). There is no file-permission split that grants "edit only your
  own line" — the granularity is the whole file — so the operation is delegated
  to a trusted program that enforces the "own line only" rule itself.
- **Least-privilege judgement:** `passwd` is trusted with *all* password hashes in
  order to let you change *one*. That is more authority than the task needs, and
  it is the classic confused-deputy shape: the binary must police, in its own
  code, the gap between the privilege it holds (all of shadow) and the action the
  caller is entitled to (their row). A narrower design would grant write to a
  per-user shard, or move the check behind a privilege-separated daemon.

### `/usr/bin/mount`

- **Privilege it needs:** the `CAP_SYS_ADMIN`-level ability to attach a filesystem
  into the namespace — a root-only operation.
- **Why the design gives it root:** mounting touches kernel state that affects
  every process, so the kernel restricts it to root; `mount` is setuid so that
  the specific, whitelisted mounts marked `user` in `/etc/fstab` can be performed
  by ordinary users.
- **Least-privilege judgement:** `mount` is trusted with *arbitrary* mounts but is
  only meant to permit the `fstab`-blessed ones, so — like `passwd` — its safety
  is entirely in its own option-parsing and fstab-checking. Historically a rich
  source of privilege-escalation bugs precisely because so much authority sits
  behind so much argument parsing. The modern narrower answer is unprivileged mount
  namespaces, which give the capability without the setuid-root deputy.

## 3. Privilege-drop demonstration

`dropdemo.c` (in this directory) acquires a privilege, uses it, and drops it
**permanently**, then proves the drop stuck. The substance is the ordering and
the saved-set-UID:

```
setgroups(0, NULL);   /* drop supplementary groups -- needs privilege, so FIRST */
setgid(real_gid);     /* drop gid, saved-set-gid included */
setuid(real_uid);     /* drop uid, saved-set-uid included -- LAST */
/* then: seteuid(0) must fail with EPERM */
```

- **Order matters.** `setgroups` needs privilege, so it must run while we still
  have it — before `setuid` throws the privilege away. gid before uid for the
  same reason.
- **`setuid()` alone is not a drop.** A process has real, effective, *and saved*
  set-user-IDs. While the effective ID is still 0, `seteuid(0)` can restore
  privilege from the saved ID. Calling `setuid(ruid)` **as root** is what also
  clears the saved-set-uid, making the drop irreversible; `seteuid(ruid)` would
  not, and is the classic incomplete-drop bug.
- **The proof.** After the drop, the program calls `seteuid(0)` and requires it to
  fail with `EPERM`. Run setuid-root it prints `RESULT: dropped permanently;
  privilege cannot be regained`; run without root it prints the modelled sequence
  (`RESULT: modelled`) and the same reasoning, no root required.
