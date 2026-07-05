# ⚠️ SPOILERS — Lab 2 reference solution ⚠️

```
╔══════════════════════════════════════════════════════════════════════╗
║  STOP. This directory contains the complete reference solution for    ║
║  Lab 2. Do the lab yourself first. Reading this before you have       ║
║  struggled with the kernel boundary throws away the entire point of   ║
║  the exercise. You have been warned.                                  ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## What's here

- `solution.patch` — a unified diff (`diff -ruN`, `-p1`) against a tree
  created by `starter/setup.sh`. It touches only these six kernel files:
  `defs.h`, `kalloc.c`, `proc.c`, `proc.h`, `syscall.c`, `sysproc.c`.
- `apply.sh <workdir>` — dry-runs then applies `solution.patch` to a starter
  tree. Does not build; run `make` afterwards.
- `files/` — the full post-solution versions of the six changed kernel files,
  for reading without applying the patch.

## Apply and test

```
./starter/setup.sh ~/lab2-sol
solutions/apply.sh ~/lab2-sol
tests/run.sh ~/lab2-sol           # expect 5/5 PASS, incl. usertests -q
```

## Design notes

- **`trace`** — a per-process `int trace_mask` field on `struct proc`,
  initialised to 0 in `allocproc`, copied parent→child in `kfork`, set by
  `sys_trace`. It is left untouched by `kexec`, so it survives `exec` for
  free (MIT semantics: inherited by children, not cleared on exec). The
  trace line is printed at the bottom of `syscall()`, after the handler runs,
  gated on `p->trace_mask & (1 << num)`, using a `syscall_names[]` table that
  mirrors the `syscalls[]` dispatch table.

- **`sysinfo`** — `kfreemem()` (in `kalloc.c`) counts the free list under
  `kmem.lock` and multiplies by `PGSIZE`; `knproc()` (in `proc.c`) counts
  `state != UNUSED` under each `p->lock`. `sys_sysinfo` fills a kernel-local
  `struct sysinfo` and `copyout`s it to the user pointer, returning −1 if the
  copyout fails.

- **`getppid`** — `kgetppid()` (in `proc.c`, where `wait_lock` is visible)
  reads `p->parent->pid` under `wait_lock`, the same lock `kfork`,
  `reparent`, and `kwait` use to keep the parent pointer consistent. Because
  `kexit`→`reparent` reassigns orphans to `initproc` under that same lock, a
  process whose parent has exited correctly observes ppid 1 (init). init
  itself has no parent; the solution returns 0 for that case (it is never
  exercised by the tests, since init never exits).
