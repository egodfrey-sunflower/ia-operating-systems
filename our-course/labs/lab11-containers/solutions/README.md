# Lab 11 — Reference runtimes, model write-ups and rubric

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference mycontainer.c and mycontainer.sh, the model        ║
║  PART1-TRANSCRIPT.md, TIERS.md and NOT-A-KERNEL-OBJECT.md, and    ║
║  the rubric that marks them.  NOT-A-KERNEL-OBJECT.md is the       ║
║  deliverable this lab exists for; reading the model before you    ║
║  have written your own turns the whole point of the lab into a    ║
║  fill-in form.                                                    ║
╚═══════════════════════════════════════════════════════════════════╝
```

```
mycontainer.c          the C route,     ~300 lines with comments
mycontainer.sh         the shell route, ~130 lines with comments
PART1-TRANSCRIPT.md    model Part 1 demonstrations, one namespace at a time
TIERS.md               model tier write-up
NOT-A-KERNEL-OBJECT.md model principal write-up
```

Both runtimes take the same command line and both score **9 pass, 0 fail**
on `assert.sh`. The starter skeletons score **0 pass, 9 fail**.

```sh
cc -Wall -Wextra -O2 -o mycontainer mycontainer.c
eval "$(./probe.sh | sed -n 's/^ *export /export /p')"
./assert.sh ./mycontainer    ./rootfs      # 9 pass, 0 fail
./assert.sh ./mycontainer.sh ./rootfs      # 9 pass, 0 fail
```

## Design notes (what the reference chose, and what else is acceptable)

- **The order of operations in the child is not free.** Maps before exec
  (or you exec as uid 65534); `make-rprivate` and the self-bind before
  `pivot_root` (or `EINVAL`); `/proc` after `pivot_root` (or you mount it
  where you are about to leave) and before detaching the old root; cgroup
  admission before the process forks anything (or the children are charged
  elsewhere). Any solution that gets a different order to work is almost
  certainly not doing one of these steps at all — check `assert.sh`.
- **Only the container goes in the cgroup.** The C route admits the
  `clone()`d child by pid from the parent. The shell route cannot — after
  `exec unshare` there is no parent left holding a handle — so the
  container admits *itself* as its first act inside the namespaces, using
  `$$`, which is 1 there, and relying on `cgroup.procs` resolving pids in
  the writer's pid namespace. Both are correct; a solution that admits the
  launcher instead is not, and the difference is measurable:
  with `--pids 20`, admitting the launcher leaves the workload 17–18 slots
  instead of 19: the wrapper shell and `unshare` have taken two of them if
  the shell waits, one if it `exec`s `unshare` (they are then one process).
- **`memory.swap.max = 0` alongside `memory.max`.** Without it the hog
  swaps rather than dying and Part 5 looks like it does not work. Accept
  either this or a machine with no swap; do not accept "it didn't get
  killed".
- **`/oldroot` is not removed.** The new root is a self-bind of a real
  directory on disk, so `rmdir /oldroot` after the detach would delete the
  rootfs template. An empty detached mountpoint is harmless.
- **No device nodes in the rootfs.** `mknod` needs `CAP_MKNOD` in the
  *initial* user namespace, which the unprivileged tier does not have;
  the reference bind-mounts the host's `/dev/null` and friends onto
  pre-made regular files. A root-run solution may legitimately `mknod` or
  mount a `devtmpfs` instead.
- **The shell route's two honest defects** (util-linux 2.39.3): `unshare
  --fork` blocks SIGINT/SIGTERM in the waiting parent, so only SIGKILL
  tears the container down promptly; and `unshare(1)` does not propagate a
  signal death, so an OOM-killed container exits 1, not 137. Do not mark
  either against a shell-route submission — but a submission that
  *notices* either of them has earned the TIERS.md point for comparing the
  routes.

## How `assert.sh` is kept honest

Nine checks, and every one was verified to **fail** on a container broken
in exactly the corresponding way. The summary is that
`pivot_root` removed fails P3, the fresh `/proc` removed fails P2/proc
alone, the cgroup ignored fails both P5 lines alone, a uid map to 1
instead of 0 fails P4-inside alone, and the more destructive mutations
(no `--fork`, no pid namespace, no map at all) fail everything because the
container does not survive its own setup.

Three properties keep it non-vacuous, and they are worth pointing at when
a student asks why the script is written oddly:

1. **A missing answer is a FAIL, never a skip.** No "could not determine".
2. **The `/proc` check demands `/proc/1` exists as well as a small process
   count.** Counting alone passes on a container with no `/proc` mounted,
   because the glob matches nothing — verified: that mutation reports
   `count=1` and would have passed a count-only check.
3. **The memory check runs the hog under `ulimit -v` four times above the
   cgroup limit it is testing.** If the cgroup is real the OOM kill always
   wins (rc 137); if it is not, the ulimit stops the hog with a plain
   `malloc` failure (rc 1) and the check fails *instead of the machine
   falling over*. The safety net cannot manufacture a pass because it
   produces a different exit status from the thing under test. The same
   reasoning covers the optional outer systemd scope: every outer bound
   sits strictly above the inner one being tested.

## Rubric

Total 100. Each part's weight splits between **it runs** — which
`assert.sh` reports, and which a marker can re-run — and **you can say
what it restricts**, which lives in the two write-ups. The second column
is where this lab's marks actually are.

No row is tied to a particular file: the evidence for any of them may sit
in a transcript, in `TIERS.md`, or in `NOT-A-KERNEL-OBJECT.md`, and the
marker reads all of them before awarding a row.

### Part 1 — namespaces one at a time (25)

| Points | For |
|---|---|
| 6 | Four namespaces demonstrated **singly**, one run each, with transcripts: UTS (host unaffected), PID, mount (invisible outside), net (`lo` only). A single combined run showing all four at once scores 2, not 6 — the independence is the exercise |
| 4 | `assert.sh` P1 uts + P1 net pass on the finished runtime |
| 8 | Each namespace correctly attributed in NOT-A-KERNEL-OBJECT.md: what set of *names* it restricts, stated per namespace and not as one general sentence. All of them covered by one general sentence — "they isolate the container" — rather than one attribution each: at most 3 of 8 |
| 4 | The independence argued, not just performed: any subset is a legal configuration, and nothing in the kernel says which subsets are containers |
| 3 | The PID-namespace-does-not-renumber-the-caller observation made from evidence (the un-forked run, or `NSpid`), not merely repeated from the handout |

### Part 2 — a private process tree (12)

| Points | For |
|---|---|
| 3 | `assert.sh` P2 pid + P2 /proc pass |
| 3 | The same process shown from both sides with both PIDs (`NSpid`, or host `ps` against inside `ps`) |
| 4 | The split diagnosed: the PID namespace renumbers, `/proc` is what tools *read*, and it takes the mount namespace to give the container its own. Full marks require naming which primitive is responsible for each half |
| 2 | "Neither PID is more real than the other" — or an equivalent statement that the mapping is a property of the namespace, not a disguise over a true id |

### Part 3 — a private root with `pivot_root` (25)

| Points | For |
|---|---|
| 5 | `assert.sh` P3 passes: a host absolute path does not resolve inside |
| 4 | Both preconditions met and *explained* (new root is a mount point; propagation made private, and what would otherwise propagate where) |
| 4 | The old root actually detached, and the demonstration that it is — not just "I ran umount" |
| 8 | The `chroot` note: the escape stated **mechanically** — cwd outside the new root, `..` walks past it, `chroot` back — not "chroot is insecure". Full marks need the specific reason `pivot_root` closes it: it moves the root *mount* and the old root ends up with no mount, not merely no path |
| 4 | "An image is a directory" understood: what the rootfs actually contains, and that the kernel is shared |

### Part 4 — user namespaces and apparent root (13)

| Points | For |
|---|---|
| 3 | `assert.sh` P4 uid inside + P4 uid outside pass |
| 3 | **Both halves** demonstrated: root-ish things work inside; a host file it lacks permission for is refused, and a file it creates comes out owned by the unprivileged uid |
| 5 | The attribution: capabilities are valid only against objects the namespace owns; uid 0 is a namespace-relative integer; privilege lives in the capability set. A write-up that says "it pretends to be root" scores 1 |
| 2 | Connected to the rest of the lab: this is what made the other four parts runnable without `sudo` — and, in TIERS.md, why distributions disable it |

### Part 5 — cgroups v2 limits (25)

| Points | For |
|---|---|
| 5 | `assert.sh` P5 memory.max + P5 pids.max pass |
| 4 | Accounting read back and **quoted**: `memory.events` `oom_kill`, `memory.peak`, `pids.events` `max`, `pids.peak`. "It died" scores 0 of these 4 |
| 4 | The correct process in the cgroup, and evidence the student checked (`cgroup.procs` or `/proc/<pid>/cgroup`) rather than assumed |
| 10 | **The visibility/consumption split**, which is what this part is for: cgroups limit consumption and hide nothing (the hog could still *see* the machine's full memory); namespaces restrict names and limit nothing. Full marks need the point made from the student's own evidence, and the consequence stated — namespaces alone leave a container able to starve the host; cgroups alone leave it able to see everything. The split asserted in the handout's own words, with no measurement of the student's own behind it: at most 3 of 10 |
| 2 | `memory.swap.max` understood as necessary, not incantation |

### The closing argument (folded into the parts above; mark it once)

Not separately weighted — it is the last 8 points of Part 1 and the last
10 of Part 5, plus Part 3's note — but read `NOT-A-KERNEL-OBJECT.md`'s
conclusion as a whole before awarding them. What earns the top of each
band:

- there is no kernel object: no create call, no id, no handle, nothing to
  close, and one perfectly ordinary `task_struct`;
- the configuration space is continuous and no point in it is privileged —
  `--net=host`, a bare systemd service, a snap, gVisor, all sit on the
  same axis and the kernel treats none of them specially;
- and the App. B comparison landed: a VMM *is* an object with a boundary
  the hardware enforces, which is why it costs what it costs; a container's
  boundary is assembled from independent pieces, which is where both its
  density and its fragility come from. Firecracker and gVisor as two
  answers to that same fact.

A write-up that reaches the third bullet has done the lab. One that
reaches only the first has described the machinery.

### TIERS.md (marked within the parts above; if it is missing or thin, cap the total at 95)

Expected: which tier the machine offered, evidenced by `probe.sh`; which
parts ran which way; what root would have changed *on the outside* (who
may do it, what a bug costs, `mknod`); and what a machine with
unprivileged user namespaces disabled would have cost, argued as a real
security trade-off rather than as a broken machine. A student whose
machine forced the privileged tier is not penalised — that write-up is
more interesting, not less, and should say what Part 4 could not show.
