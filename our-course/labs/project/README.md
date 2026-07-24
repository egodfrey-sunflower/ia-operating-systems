# Final project

**Weeks 24–27 · 8+ h (wk 24: 2.0 h · wk 25: 4.0 h · wk 26: 2.0 h · wk 27: write-up)**
**OSTEP:** none specific — the project draws on the whole course; App. G and H are the spirit of it.
**Platform:** your choice, constrained by the option you pick.

## What you'll build

One substantial system of your own choosing, from a menu grounded across the
course: a proposal, a design, a working implementation, and — the part that
distinguishes a project from an exercise — an **evaluation** that measures whether
it does what you claimed. The menu exists so you can pick something that suits
your interests and your machine; the milestones exist so a four-week open-ended
build does not quietly become a four-week open-ended stall.

Unlike every lab before it, this has no specified parts. You write the spec. That
is the point: the course has taught you enough to now decide what is worth
building and how you would know it worked.

**This is evaluation-first, and that is deliberate.** The week-24 proposal must
name *how you will evaluate the system* before a single line of code exists.
Naming the evaluation up front — not the features, the evaluation — is the one
thing that separates a project from a large exercise. A system nobody measured is
an assertion; a system with one reproducible number behind it is a result. Every
option below names a concrete thing to measure; your proposal turns that into a
plan you commit to.

**Observable success:** someone else can clone your repository, follow your
README, build it, run it, and reproduce **one** of your evaluation numbers. That
sentence is the bar. Aim the whole project at making it true.

## Prerequisites

- Everything through week 24. Options differ in what they additionally require —
  the menu notes this per option.
- **You commit to an option in week 24.** Weeks 25–26 assume the decision is
  made; a student still choosing in week 25 will not finish. Read the whole menu
  once, including the risk notes, before you choose — the commonest way this
  project goes wrong is picking on interest alone and meeting the schedule problem
  in week 26, when it is too late to switch.

## How to read the menu

Every option carries the same three-line starter note under its description:

- **Where to look** — the specific files from a lab you have already done that
  this option extends. These are real files you have already read or written;
  none of this option is a blank page.
- **Traps** — the two or three places this specific option is known to go wrong.
  These are not the general hazards of the topic; they are the ones that cost a
  build its week.
- **The minimum that counts as done** — one sentence naming the smallest core
  that earns the "working implementation" marks. Every option here can absorb
  unlimited time. Aim at the minimum first, get it evaluated, and *then* extend.
  An evaluated minimum scores far better than an unevaluated maximum.

**Three options carry the least risk — C, D and F.** Each extends a lab whose
harness already exists and whose code *you wrote*, so week 25's skeleton is
mostly a matter of finding your own tree again. **Option G carries the most,**
because the lab it builds on (Lab 11) does not unlock until week 26 — read its
note carefully before choosing it. If you want a project that is very likely to
finish, start with C, D or F.

---

## The option menu

Each option names its grounding, what it additionally needs, and its main risk.
All are scoped to 8–10 hours; ambition beyond that is welcome but should be
structured so the core is finishable.

### A — A FUSE file system

Build a userspace file system: an in-memory one, or a view over an existing tree
with a twist (versioning, transparent compression, a tag-based namespace).
*Grounds in weeks 17–18 (ch. 39–41).* **Needs:** libfuse; on some systems, root
or a `user_allow_other` setting. **Risk:** the FUSE API is large — pick a narrow
feature set. **Evaluation:** correctness against a filesystem test suite, plus
throughput against the native FS.

> **Where to look.** Lab 7 is the whole grounding, from two directions. Part 1's
> tools — `starter/mystat.c`, `starter/myls.c`, `starter/myfind.c` — are the
> *client* side of the interface your FUSE callbacks must implement: `mystat`
> shows exactly the `struct stat` fields a `getattr` callback has to fill,
> `myls`'s `opendir`/`readdir` loop is what a `readdir` callback serves, and
> `myfind`'s walk is what recursion over your tree looks like from outside.
> Part 4's `starter/xfsck/xv6fs.h` and `xfsck.c` are the *server* side: they name
> the invariants a real on-disk filesystem maintains (every block reachable from
> exactly one inode, link counts matching directory entries, `.`/`..` correct) —
> the same invariants your in-memory structures must keep, minus the disk. libfuse
> itself is **not** shipped by any lab; install `libfuse3-dev` (or your distro's
> equivalent) and build against its `example/hello.c`, which is the smallest
> mount that works.
>
> **Traps.** (1) The API is enormous and most of it is optional — implement the
> handful of callbacks your feature needs (`getattr`, `readdir`, `read`, and for a
> writable FS `write`/`create`/`mkdir`/`unlink`) and let the rest default; a
> half-filled `getattr` makes `ls` fail in ways that look like kernel bugs.
> (2) FUSE operations are **path-based, not descriptor-based** — every call hands
> you a path string and you resolve it yourself each time; there is no open-file
> table unless you build one. (3) libfuse is multithreaded by default; mount with
> `-s` (single-threaded) until your data structures are correct, or you will debug
> a race that is really a missing lock.
>
> **The minimum that counts as done:** an in-memory filesystem that mounts via
> libfuse and implements `getattr`, `readdir`, `read`, `write`, `create` and
> `mkdir` well enough that `ls`, `cat`, `echo > f` and `mkdir` all work inside the
> mountpoint. The twist (versioning, compression, tags) is the extension on top.

### B — Kernel threads in xv6

Add a `clone()`-style system call sharing an address space between processes, plus
a user-level thread library over it — and the locking to make it safe.
*Grounds in weeks 11–14 (concurrency) and 4–6 (xv6 syscalls).* **Needs:** xv6.
**Risk:** the sharing interacts with `exit` and `wait` in ways that are easy to
get subtly wrong. **Evaluation:** correctness under a concurrent stress test;
context-switch cost against processes.

> **Where to look.** Two labs together. Lab 2 is the syscall-plumbing template:
> `kfork` in `kernel/proc.c` (the core of the `fork` syscall; `sys_fork` in
> `kernel/sysproc.c` wraps it) is the function `clone` is a variant of — copy it
> and change what it shares — and the layers a new syscall threads through
> (`user/usys.pl`, `kernel/syscall.h`, `kernel/sysproc.c`, `kernel/proc.h`) are
> the ones you traced in Lab 2 Part 1. Lab 4 is where the address-space mechanics
> live: `uvmcopy` in `kernel/vm.c` is exactly the copy your `clone` must *not*
> do — instead of duplicating the page table, the child shares the parent's
> `p->pagetable` and gets a caller-supplied user stack. The locking pattern (take
> `p->lock`, one at a time, never two at once) is stated in Lab 2's spec.
>
> **Traps.** (1) `exit` frees the address space (`proc_freepagetable`): a thread
> exiting must **not** tear down a page table another thread is still running on —
> reference-count the shared address space, or free only when the last sharer
> goes. (2) `wait` reaps a *child*; a thread is not quite a child, and deciding
> what `wait`/`join` returns for a thread versus a process is where the semantics
> get subtly wrong. (3) The user stack is yours to allocate and pass in — `clone`
> does not grow it lazily the way `fork` inherits the parent's, so a wrong stack
> pointer segfaults the new thread the instant it runs.
>
> **The minimum that counts as done:** a `clone(fn, stack)`-style syscall that
> creates a new schedulable context sharing the caller's page table, plus a
> `join`, demonstrated by two threads incrementing a shared counter under a
> spinlock and the total coming out exactly right. The user-level thread library
> is the extension.

### C — A modern scheduler in xv6

Implement CFS-style weighted fair scheduling with virtual runtime, or an
EEVDF-style policy, and compare it against the lottery and MLFQ schedulers from
Lab 2. *Grounds in weeks 4–5 (ch. 8–9) and Lab 2.* **Needs:** xv6, Lab 2 complete.
**Risk:** the least risky option — Lab 2's harness is directly reusable.
**Evaluation:** fairness and latency across mixed CPU-bound and interactive
workloads.

> **Lowest-risk option.** Everything you need already exists in your Lab 2 tree.
>
> **Where to look.** Lab 2 is not just the grounding, it is most of the
> scaffolding. `scheduler()` in `kernel/proc.c` already has three policies behind
> the `#if SCHED_POLICY` switch — your CFS/EEVDF policy is a fourth branch beside
> them, so all four coexist and the comparison is a `make POLICY=…` away.
> `kernel/sched.h` is where the policy constants live (add yours next to
> `MAX_TICKETS`, `NPRIO`, `MLFQ_ALLOTMENT`). `kernel/rand.c` you can ignore — CFS
> is deterministic. The measuring instruments are already written and must not be
> edited: `user/schedtest.c` and `user/mlfqtest.c` report per-process selection
> counts through `getpinfo` and `struct pinfo` (`kernel/pinfo.h`), and
> `tests/run.sh` builds and boots each policy for you.
>
> **Traps.** (1) Virtual runtime is bookkeeping charged *per tick*, and the place
> to charge it is where the MLFQ allotment is charged — in `yield()`, reached only
> from the timer interrupt in `kernel/trap.c`, so one call is one whole tick of
> CPU. Charge it where a process is *chosen* and an interactive process that
> blocks early is charged wrongly. (2) CFS selection replaces the lottery's
> two-pass ticket walk with "pick the runnable process of lowest vruntime" — a
> linear scan of `proc[]` is fine at xv6 scale; you do not need a red-black tree,
> and building one is a week you do not have. (3) To *see* fairness you must
> observe vruntime — add a field to `struct pinfo` and have `getpinfo` report it,
> exactly as the ticket count is reported, or your evaluation has nothing to plot.
>
> **The minimum that counts as done:** a `SCHED_CFS` (or EEVDF) branch in
> `scheduler()` that selects the runnable process with the lowest weighted virtual
> runtime, advances vruntime per tick in `yield()`, boots, passes `usertests`, and
> produces a `schedtest` fairness number you can put beside the lottery and MLFQ
> numbers. Weighting by a `nice`-like parameter is the natural extension.

### D — File-backed `mmap` in xv6

Implement `mmap`/`munmap` for file-backed mappings with demand paging and
write-back on unmap. *Grounds in weeks 8–11 (ch. 18–23) and Lab 4.* **Needs:**
xv6, Lab 4 complete. **Risk:** the interaction between the page cache and the file
system is where the real difficulty lives. **Evaluation:** correctness against a
read/write oracle; performance against `read`/`write` for random access.

> **Lowest-risk option.** The demand-paging machinery is the `vmfault` you already
> wrote.
>
> **Where to look.** Lab 4 is the direct parent. `vmfault()` in `kernel/vm.c` is
> the demand-paging handler from Lab 4 Part 3 — file-backed `mmap` is the same
> fault path with "allocate a zeroed page" replaced by "read the right file block
> into the page." `walk()` (with `alloc=1`), the PTE flags (`PTE_W` and friends
> in `kernel/riscv.h`; `PTE_COW` in `kernel/vm.h` is the course's example of a
> *software* PTE bit — the model for any bit you add yourself, since this xv6
> defines no hardware dirty bit), `uvmunmap`, and the `usertrap → vmfault` path in
> `kernel/trap.c` are all the
> lazy-allocation plumbing you already met. The file half is `readi`/`writei` on
> an inode (xv6's `kernel/fs.c`, format in `kernel/fs.h`), taken under `ilock`.
> `proc_freepagetable` in `kernel/proc.c` is where you hook teardown.
>
> **Traps.** (1) You must track the mapped regions yourself: `sbrk`'s `p->sz`
> describes one contiguous heap, but an `mmap` region can sit anywhere, so add a
> small per-process VMA array (start, length, permissions, file, offset) and have
> `vmfault` consult it to decide *which file block* a faulting address wants.
> (2) Write-back is the half people forget: on `munmap` (and on `exit`) a dirty
> `MAP_SHARED` page must be written back with `writei`, or the file on disk is
> stale — the read/write oracle catches exactly this. (3) `fork` must copy the VMA
> list and `exit` must unmap and write back, or a mapping outlives the process and
> panics in teardown, the same way a stray lazy page did in Lab 4.
>
> **The minimum that counts as done:** `mmap`/`munmap` syscalls supporting a
> file-backed `MAP_SHARED` mapping, demand-paged through `vmfault` reading file
> blocks, with dirty pages written back on `munmap`, verified by mapping a file,
> reading and writing through the mapping, unmapping, and confirming the on-disk
> bytes match what a `read`/`write` oracle produced. `MAP_PRIVATE` and lazy
> shared-page sharing are extensions.

### E — A log-structured store

Build an LFS-style key-value store: segment writes, an in-memory index, a
checkpoint, and a cleaner. Then measure cleaning cost against segment utilisation.
*Grounds in weeks 21–22 (ch. 43) and Lab 9.* **Needs:** nothing beyond a C or
Python toolchain. **Risk:** the cleaner is the hard part and the interesting part;
do not leave it until last. **Evaluation:** write amplification against
utilisation — the central contested question in the LFS literature.

> **Where to look.** Lab 9 gives you the append-and-recover machinery to build on.
> `starter/wal.py` is a log with a header, slots, sequence numbers and a
> `recover()` — your segments are that idea generalised from a fixed 7-slot region
> to an append-only sequence, and your checkpoint plays the role of `wal.py`'s
> committed header. `starter/blockdev.py` is a device with `installs` and
> `barriers` counters — that counting discipline is exactly how you will measure
> write amplification (bytes written to the log per byte of user data). The crash
> campaign (`sweep`, and the recovery-at-every-install idea) is the model for
> testing that your store recovers its index from the log after a crash. You may
> write the whole thing in Python (reusing this shape) or C.
>
> **Traps.** (1) The cleaner is the point and the hard part — start it in week 25,
> not week 26. It needs a *liveness* test: a record in an old segment is dead if
> the in-memory index now points its key somewhere newer, live otherwise, and the
> cleaner copies only the live records forward before freeing the segment.
> (2) Write amplification is only meaningful if you *count* it — instrument every
> byte written to a segment (as `blockdev.py` counts installs) and divide by user
> bytes; "it felt fast" is not the deliverable. (3) The checkpoint is what makes
> recovery cheap: without it, recovery must replay the entire log; decide what the
> checkpoint stores (the index, or a pointer into the log) and when it is written.
>
> **The minimum that counts as done:** a KV store that appends key/value records
> to fixed-size segments, keeps an in-memory hash index (key → newest location),
> writes a checkpoint so recovery rebuilds the index without a full replay, and
> has a cleaner that reclaims a segment by copying its live records forward — then
> one plot of write amplification against segment utilisation. Multiple cleaning
> policies (greedy vs cost-benefit) are the extension.

### F — A distributed key-value store

Extend Lab 10's RPC library into a replicated store with a primary, a backup, and
a failover policy. *Grounds in weeks 23–24 (ch. 48–50) and Lab 10.* **Needs:**
Lab 10 complete. **Risk:** consensus is out of scope — specify a simple failover
and stick to it, or this expands without limit. **Evaluation:** availability and
correctness under an injected-failure campaign, reusing Lab 10's loss simulator.

> **Lowest-risk option.** You are extending a library you wrote, with a failure
> simulator that already exists.
>
> **Where to look.** Lab 10 is the entire substrate. `rpc.c`/`rpc.h` is the
> exactly-once RPC library your store's `get`/`put` become calls on — the
> exactly-once property is what keeps a retried `put` from applying twice.
> `reliable.c` (ack/timeout/retry/sequence) sits under it. `net.c`/`net.h` is the
> UDP endpoint **and the loss simulator** — `LOSS_RATE`/`LOSS_SEED`/`NET_STATS` are
> your injected-failure campaign, reproducible from a seed. `fileserver.c` and
> `fileclient.c` are the template for a stateless server plus a client that rides
> out an outage on retries — your primary/backup pair is that pattern with a
> second server behind it. `msg.c`/`msg.h` marshals the wire format.
>
> **Traps.** (1) **Consensus is out of scope — say so in the proposal and hold the
> line.** Specify one simple failover (e.g. every `put` goes primary → backup and
> is acked only after the backup has it; the client fails over to the backup when
> the primary stops answering) and refuse to generalise it; the moment you reach
> for leader election you have signed up for a different, larger project.
> (2) Split-brain: if the primary is only *slow*, not dead, the client may promote
> the backup while the primary still lives — decide what your policy does about
> two primaries, even if the answer is "documented limitation, measured." (3) The
> RPC layer's exactly-once semantics interact with replication: a `put` the
> primary forwarded but crashed before acking may or may not be on the backup —
> pin down what "acknowledged" means before the failure campaign, because that is
> the invariant the campaign checks.
>
> **The minimum that counts as done:** a KV store exposed as `get`/`put` RPCs with
> a primary that forwards each `put` to a backup before acking, and a client that
> fails over to the backup when the primary stops responding — evaluated by
> killing the primary mid-workload under the loss simulator and confirming no
> acknowledged write is lost and reads still succeed. A cleaner failover protocol
> (heartbeats, a view number) is the extension.

### G — A container runtime with a real image format

Extend Lab 11 into something that pulls and unpacks an OCI image, layers a
filesystem with overlayfs, and runs it. *Grounds in week 26 and Lab 11.*
**Needs:** root; Lab 11 complete. **Risk:** the latest-unlocking option, and the
one that does not fit the standard milestone shape. **Evaluation:** startup
latency and memory overhead against a real runtime.

> **Highest-risk option — read this before choosing it.** Lab 11, and its reading,
> arrive in **week 26** — the same week the project's implementation milestone
> falls. So this option does **not** fit the normal milestone shape, and you must
> propose it split in two, or you will have nothing to show in week 25:
>
> - **Week 25 skeleton = the Lab-11-independent half.** Pull an OCI image over
>   HTTP from a registry, verify each blob against its sha256 digest, and unpack
>   the layers into a single directory — **with no isolation at all.** This half
>   needs nothing from Lab 11; it is pure HTTP, hashing and tar extraction, and it
>   is a complete, demonstrable, evaluable program on its own (an image is a
>   directory — Lab 11 Part 3 makes exactly that point).
> - **Week 26 = isolation, on top of Lab 11.** Once Lab 11 unlocks, take the rootfs
>   directory your week-25 code produced and run it under the runtime you built
>   there.
>
> Propose it that way in week 24, or pick another option.
>
> **Where to look.** Week 25's half stands alone; week 26's builds on
> `lab11-containers/starter/mycontainer.c` (or `mycontainer.sh`) — the
> namespace + `pivot_root` + cgroup runtime — and `build-rootfs.sh`, which already
> demonstrates that "an image is a directory" by assembling a busybox rootfs that
> `mycontainer` then runs a command inside. Your unpacked OCI layers replace that
> busybox rootfs as the thing `mycontainer` pivots into.
>
> **Traps.** (1) An OCI pull is a multi-step HTTP dance: fetch a registry auth
> token, GET the manifest, then the config and each layer blob by digest — each
> step is a different endpoint and content-type, and getting the token flow wrong
> is where week-25 hours vanish. (2) Digest verification is the security content of
> the whole option — every blob's sha256 must equal the digest the manifest named,
> and a layer that fails must abort the unpack, not warn. (3) Layers stack with
> whiteouts (a deleted file in an upper layer is marked, not absent); a naive
> "untar each layer over the last" gets deletions wrong — either handle whiteouts
> or use overlayfs and document the boundary.
>
> **The minimum that counts as done:** a program that pulls a named OCI image over
> HTTP, verifies every blob's sha256 digest, and unpacks its layers to a rootfs
> directory (week 25) — then, in week 26, runs a command in that rootfs under Lab
> 11's `mycontainer` with namespace and cgroup isolation. The overlayfs layering
> and the latency/memory comparison against a real runtime are the extension.
>
> **What the week-25 half measures.** Because the latency/memory comparison is the
> extension, the Lab-11-independent half has to carry its own numbers, or a G
> student who does exactly the minimum arrives at week 27 with nothing to put under
> the evaluation row. Two, both cheap, and both stated in the proposal: **digest
> verification, demonstrated against a deliberately corrupted blob** — flip one
> byte of a fetched layer and report that every good blob verified while the
> corrupted one aborted the unpack rather than warning past it, which is trap (2)
> turned into a measurement; and **unpack correctness, diffed against a known-good
> extraction** — `diff -r` your rootfs against the same image unpacked by a tool
> that already works (`skopeo copy` then `tar -x`, or `podman export`), reported as
> a count of differing paths with every remaining difference named and explained.
> Zero is achievable; a small number you can account for is a result too.

### H — Propose your own

Anything of comparable scope grounded in course material. Write the proposal
against the same headings and get it straight with yourself in week 24: what, why
it is hard, what working means, how you will measure it.

> **No starter note — the proposal is the work.** Because there is no lab to point
> at, the burden the other options carry in their starter notes falls entirely on
> your week-24 proposal, and it must answer the same four questions those notes
> answer for you elsewhere: **what** you will build (concretely, with a "minimum
> that counts as done" line of your own); **why it is hard** (name the two or three
> traps *before* you hit them); **what working means** (the observable behaviour);
> and **how you will measure it** (a number, a method, and a baseline to compare
> against). Ground it in a specific lab or chapter — an option with no grounding is
> a blank page, and a blank page is the one thing this project's structure exists
> to save you from. If you cannot write the four answers crisply in the week-24
> slot, pick a menu option instead.

---

## Milestones

These replace the fixed parts of a normal lab. Each is a checkpoint, not a
deliverable in itself.

### Week 24 — Choose and propose (~2.0 h)

Read the menu, pick one, and write a one-page proposal: what you will build, what
the hardest part will be, what "working" means concretely, and — stated up front —
**how you will evaluate it**. Naming the evaluation before writing code is what
stops it becoming an afterthought. The rubric weights below are given to you now,
in week 24, so you can aim at them.

### Week 25 — Design and skeleton (~4.0 h)

A design note (interfaces, data structures, the main control flow) and a running
skeleton: something that builds, runs, and does the smallest end-to-end slice of
the job, even if trivially. By week's end you should have a program that does
*something*. (Option G is the exception: its week-25 skeleton is the
Lab-11-independent OCI pull-and-unpack half — see its note.)

### Week 26 — Implementation (~2.0 h)

Functional implementation. **Feature-freeze at the end of this week regardless of
state** — an evaluated partial system scores far better than an unevaluated
complete one.

### Week 27 — Evaluate and write up

Run the evaluation you proposed in week 24, and write it up: what you built, what
you measured, what surprised you, and what you would do differently. **Do this
before the final exam, not after** — it is the part of the week with no exam
pressure attached.

> **The project is the course's relief valve.** If a week gets away from you, these
> are the hours to trim first — and the milestone structure is what makes that
> survivable. A student who loses a week still has a proposal, a skeleton and an
> evaluation plan, and can cut *implementation* scope rather than abandoning the
> project. Cut features, keep the evaluation.

## What "done" looks like

- **`PROPOSAL.md`** — one page, written in week 24, including the evaluation plan.
- **`DESIGN.md`** — interfaces, structures, control flow.
- **Working code**, feature-frozen at the end of week 26, that builds and runs
  from a documented command.
- **`EVALUATION.md`** — the measurements you proposed, presented with enough
  detail that someone could repeat them, plus an honest account of what did not
  work.

Observable success: someone else can clone your repository, follow your README,
build it, run it, and reproduce one of your evaluation numbers.

## How it's checked

**Rubric only — and deliberately so.** There is no autograder for an open-ended
project, and building one per option would cost more than the whole lab track.

The rubric weights, which are stated to you in week 24 so you can aim at them:

| | Weight | What earns it |
|---|---|---|
| **Working implementation** | 40% | It builds, runs, and does what the proposal said |
| **Evaluation** | 30% | Measured, not asserted; method reproducible; numbers interpreted |
| **Design write-up** | 20% | The decisions are explained, and the alternatives considered are named |
| **Honesty about limits** | 10% | What does not work, what was cut, what you would change |

Three of those rows have a floor as well as a ceiling, because a full-marks
description on its own tells a self-marker which way to deduct but not how far:

- **Working implementation (40%).** Your option's stated *minimum that counts as
  done*, met **and evaluated**, is worth at least 30 of the 40 — that line is the
  anchor this row is written against. Falling short of the minimum caps the row at
  20, unless the shortfall is itself measured and explained, in which case it has
  become part of the evaluation rather than a hole in it.
- **Evaluation (30%).** A strong evaluation compares against the baseline your
  option named — the native FS, the lottery and MLFQ numbers, the read/write
  oracle, a real runtime. A number with nothing beside it earns at most half of
  this row, however carefully it was measured.
- **Honesty about limits (10%).** After re-reading the proposal, list every item
  it promised that you did not deliver. Each one absent from your limitations
  section is evidence against this row; the list is the instrument, not a feeling
  about how candid the write-up sounds.

**That last row is worth real marks on purpose.** A project write-up claiming
everything worked is almost always a project that was not evaluated hard enough,
and the habit of saying plainly where a system falls short is the one this course
most wants to leave behind. The 10% is not a consolation prize for an incomplete
project — it is a skill being assessed directly, and a complete project with a
blank limitations section leaves it on the table.

## Self-assessment path

Since this is a self-study course, mark your own project against the rubric a week
after finishing it, in writing — and in this order. Mark **Evaluation**, **Design
write-up** and **Honesty about limits** first, from memory, and write down what you
remember promising. *Then* read the week-24 proposal, and score **Working
implementation** against its text, comparing what you remembered against what it
actually says. The gap between what you proposed in week 24 and what you built by
week 26 is the most useful thing the project has to tell you; marking the other
three rows before you re-read is what stops you quietly forgiving that gap instead
of measuring it.
