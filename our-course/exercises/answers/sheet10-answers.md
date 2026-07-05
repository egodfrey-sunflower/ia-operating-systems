# ⚠️ SPOILER — Examples Sheet 10 model answers ⚠️

> **STOP.** Full worked solutions. Do the sheet closed-book first. The LFS
> write-cost numbers (F) were verified with Python; checks are noted inline.

---

## A. Warm-ups

**A1. False.** `fsync(fd)` forces *this file's* data and inode to disk, but the
new **directory entry** that names it lives in the parent directory's blocks —
you must also `fsync` the parent directory's fd. Without that, a crash can leave
the file's blocks on disk but no name pointing at them (or vice-versa).

**A2. False.** Metadata-only ("ordered") journaling writes the **data block once**
(in place) and journals only metadata; it *orders* the data write before the
metadata commit but does not duplicate the data. It is **data journaling**
(`data=journal`) that writes data twice (once to the journal, once home).

**A3. Mostly true, with a twist.** `fork` copies the parent's signal
dispositions (handlers) into the child. But a successful `exec` **resets
handled signals to their default** disposition (the handler code no longer
exists in the new image); signals set to *ignore* stay ignored. So "handlers
survive exec unchanged" is **false** — they are reset.

**A4. False.** A pipe write is atomic only up to **`PIPE_BUF`** bytes (≥ 512,
4096 on Linux). Larger writes may interleave with other writers' data. Below
`PIPE_BUF` the write is guaranteed not to be split.

---

## B. Bookwork

Marking notes (full answer: OSTEP ch. 5; Kerrisk ch. 24–27; xv6 book ch. 1):
the shell loop is *read command → `fork` → in child: set up fds (`open`/`dup2`/
`close`), `exec` the program → in parent: `wait` (unless backgrounded)*. Key
points a good answer makes: `fork` returns twice (0 in child, pid in parent);
`exec` **replaces** the image but keeps the fd table; `wait` reaps the zombie
and yields the exit status; redirection/pipes are set up in the child *between*
`fork` and `exec`. C makes this exact.

---

## C. Shell syscall sequences (Lab 1)

**(a) `sort < in.txt | uniq > out.txt`.**

Shell (parent):
1. `pipe(p)` → `p[0]` read end, `p[1]` write end.
2. `fork()` → **child 1 (sort)**:
   * `fd = open("in.txt", O_RDONLY)`; `dup2(fd, 0)`; `close(fd)`.
   * `dup2(p[1], 1)` (stdout → pipe write end).
   * `close(p[0])`; `close(p[1])` (the originals; the dup'd fd 1 remains).
   * `exec("sort", …)`.
3. `fork()` → **child 2 (uniq)**:
   * `dup2(p[0], 0)` (stdin ← pipe read end).
   * `fd = open("out.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644)`; `dup2(fd, 1)`;
     `close(fd)`.
   * `close(p[0])`; `close(p[1])`.
   * `exec("uniq", …)`.
4. Parent: `close(p[0])`; `close(p[1])` (**crucial**); then `wait()` twice.

**Why every `close`.** A pipe's read end returns EOF only when **all** write-end
fds are closed. `uniq` blocks on `read` waiting for EOF; if *any* process still
holds `p[1]` open, EOF never arrives and `uniq` hangs forever. So the parent and
**both** children must close the pipe fds they are not using: `sort` closes
`p[0]` (it never reads), the parent closes both, and after `sort` finishes and
its `dup2`'d fd 1 closes on exit, `uniq` finally sees EOF. Symmetrically, leaving
`p[0]` open in a writer wastes fds and can wedge flow control. The classic bug is
the parent forgetting `close(p[1])` — the pipeline then hangs.

**(b)** Redirection is done in the **child, after `fork`, before `exec`** because
`exec` **preserves the file-descriptor table** across the image replacement.
The child rearranges fds 0/1/2 to point at the files/pipe, then `exec` loads the
new program, which simply uses fds 0/1/2 without knowing they were redirected.
Doing it in the parent would redirect the *shell's* own fds. `fork` gives a
private fd table to mutate; `exec`'s fd-preservation carries the arrangement
into the new program.

**(c)** With a two-stage pipeline both children run concurrently; the finish
order does **not** matter for correctness because the pipe decouples them
(bounded-buffer flow control). The shell `wait`s for both and gets each child's
**exit status** (via `WEXITSTATUS`); by convention the pipeline's status (`$?`)
is that of the **last** stage (`uniq`). Each `wait` also reaps a zombie,
releasing its PCB. (A subtlety: the shell should keep `wait`ing until both
children are reaped, matching pids, so it does not attribute the wrong status.)

---

## D. Signals across `fork` and `exec`

**(a)**

| Attribute | across `fork` | across `exec` |
|-----------|---------------|---------------|
| dispositions (handlers) | **inherited** (child copies parent's) | **reset to default**, except signals set to *ignore* stay ignored |
| signal mask (blocked set) | **inherited** | **preserved** |
| pending signals | **cleared** in the child (child starts with none pending) | **preserved** (a pending signal stays pending) |

The special case: `exec` resets handled signals to `SIG_DFL` (the handler
function is gone), but leaves `SIG_IGN` dispositions in place.

**(b)** Before `exec`, the child is running with a **copy of the parent's
`SIGCHLD` handler** installed (it inherited all dispositions from `fork`). After
`exec`, that handler is **reset to default** — the new program image does not
contain the parent's handler code. A handler is a **function pointer into the
old address space**; `exec` discards that address space and loads a new one, so
the pointer is meaningless — hence dispositions must be reset (kept only as
default or ignore).

**(c)** A signal can interrupt the main program at **any instruction**,
including in the middle of a non-reentrant library call (e.g. `malloc` holding
the heap lock, or `printf` mid-way through the stdio buffer). If the handler then
calls the *same* function, it can deadlock on that lock or corrupt shared state —
because the interrupted call was not at a consistent point. Only
**async-signal-safe** functions (a specific POSIX list: `write`, `_exit`,
`signal`-safe syscalls, …) are guaranteed reentrant here. Concrete unsafe
example: main thread is inside `malloc` (heap lock held); `SIGCHLD` fires; the
handler calls `printf`, which calls `malloc`, which deadlocks on the held lock.

---

## E. Crash consistency

An append dirties the **inode**, the **bitmap**, and the new **data block**.

**(a) xv6 write-ahead log** (`kernel/log.c`). Order to disk:
1. copy the dirtied blocks (data, bitmap, inode) into the **log region**;
2. write the **log header** naming those blocks and their home destinations —
   **this single write is the atomicity/commit point**;
3. **install** (checkpoint) each logged block at its home location;
4. clear the header.

If power is lost **just before** the header write: the header still names 0 (or
the previous transaction), so `recover_from_log` installs **nothing** from this
transaction — the append is **lost but the filesystem is consistent** (never a
torn mix). If lost **just after** the header write: the next mount reads the
header, sees the named blocks, and **replays** them to their homes — the append
is **durable**. This is the atomicity-always / durability-once-committed
distinction the Lab 6 experiment demonstrates (`BEFORE_HEAD` → VERDICT old,
`AFTER_HEAD` → VERDICT new).

**(b) ext3 data journaling (`data=journal`):** the **data block, bitmap and
inode** are all written to the **journal** first, then a journal **commit
block**; only after the transaction commits are they checkpointed in place. Data
is written **twice**. **Metadata-only / ordered (`data=ordered`):** only the
**metadata** (inode, bitmap) is journaled; the **data block is written in place,
first**, and the metadata commit is not allowed to reach the journal until that
data write has completed. This ordering constraint (data → then metadata commit)
guarantees a metadata pointer never points at a block whose **new contents have
not yet landed** — so you never expose stale/garbage data through a freshly
committed pointer, even though the data itself is not journaled. (Paper 23
analyses exactly these orderings.)

**(c)**

| Scheme | data-block writes | guarantee |
|--------|------------------|-----------|
| xv6 WAL | 2 (log + home) | atomic + consistent; appended block never garbage |
| ext3 data journal | 2 (journal + home) | strongest: data + metadata atomic together |
| ext3 ordered | 1 (home only) | metadata consistent; appended data itself not atomic (a crash mid-data-write can leave a partially-written block, but no metadata points past valid data) |

Data journaling is worth its doubled write bandwidth when you need the *contents*
of each write to be atomic (databases without their own logging, or workloads
where a torn appended block is unacceptable); otherwise ordered mode is the
default because it halves data traffic while still keeping the *filesystem
structure* consistent.

**(d) `fsync(fd)` guarantees** that the file's data blocks **and** its inode
metadata are durable on the underlying device before it returns (`fdatasync`
omits non-essential metadata). It does **not** guarantee that the file's
**directory entry** is durable — for a newly created file you must also
`fsync` the **parent directory's** fd, or after a crash the data exists with no
name. This is a famous application-bug source: the "atomic rename" idiom
requires `fsync(newfile)` → `rename` → `fsync(dir)`; skipping the directory
`fsync` (or assuming `rename` alone is durable) loses files on power failure.

**(e) With no journal (raw V7), `fsck`** must **scan the entire filesystem** at
mount: walk all inodes, recompute the free-block and free-inode bitmaps, and
recompute link counts, then reconcile. It **can repair**: (1) a block marked
free but referenced by an inode (or vice-versa) — rebuild the bitmap; (2) a
wrong link count — recount references and fix, or move an unreferenced-but-
allocated inode to `lost+found`. It **cannot recover** the *lost update itself* —
if the crash happened between writing the data and the inode, `fsck` restores
*structural* consistency but the file's intended new contents/size are simply
gone (it cannot know what you meant to write). `fsck` is **slower** because it is
O(filesystem size), not O(recent operations), and **weaker** because it only
restores invariants, whereas a log **replays the exact intended updates** —
recovery is O(log) and preserves committed work.

---

## F. Log-structured file systems

**(a)** LFS buffers all new data and metadata in memory and writes them out as
one large **sequential** log, converting many small random writes into a few big
sequential ones — which is exactly what disks are fastest at (and what avoids
seek + rotation per write). The problem this creates: over time, overwrites and
deletes leave **dead blocks scattered** through old segments, so the log fills
with holes. **Segment cleaning** (garbage collection) must compact the live
blocks out of partly-dead segments to reclaim contiguous free segments for new
logging.

**(b)** *Verified in Python:* `write_cost = 2/(1−u)`.

| u | write cost |
|-----|-----------:|
| 0.5 | **4** |
| 0.75 | **8** |
| 0.9 | **20** |
| → 1 | **→ ∞** |

As `u → 1` (nearly-full segments) the cost diverges — cleaning a 95%-live
segment moves 19 live blocks to free 1, an appalling trade. So the cleaner
should strongly prefer **low-utilisation** (mostly-dead) segments, where each
unit of I/O frees a lot of space.

**(c)** LFS wants segments to be either **almost empty** (cheap to clean) or
**almost full** (leave them alone — cleaning them is ruinous, and full-of-live
means the data is cold and stable). Cleaning **cold** segments even at moderate
utilisation is worthwhile because their live blocks will *stay* live, so once
compacted they become long-lived full segments; **hot** segments are left to
**empty themselves** as their blocks are rapidly overwritten (utilisation falls
toward 0 on its own), and are then cheap to clean. This drives the utilisation
distribution to the two extremes (bimodal), avoiding the expensive middle. The
same logic governs an **SSD FTL**: it garbage-collects erase blocks by
relocating the few live pages out of mostly-invalid blocks and erasing them,
preferring low-live-count victims — LFS's segment cleaning is the direct
intellectual ancestor of flash GC and write amplification.

---

## G. TOCTOU file races

**(a)** A **check** (`access(path, W_OK)`) and the **use** it guards
(`open(path)`) are two **separate syscalls**, each crossing the user/kernel
boundary and returning to user space in between. Nothing holds the referent of
`path` fixed across that gap: the program checks a name, then re-uses the name,
and the two are not one atomic operation. That is a **race** in exactly the Sheet
5/9 sense — a shared resource (the directory entry / the file `path` names)
mutated concurrently with a computation that assumes it is stable — even though
the victim is single-threaded and touches no lock. The two operations that must
be atomic-with-respect-to-each-other but are not are **`access` (time of check)**
and **`open` (time of use)**. The role of "the other thread" is played by
**another process**: the unprivileged attacker running concurrently. The victim
never called `pthread_create`, but the OS is a multi-process concurrent system,
so the interleaving is just as real — the attacker *is* the second thread of the
computation.

**(b)** The window between the two syscalls is the attacker's. Let `path` be
`/tmp/attacker_dir/log`, a name the attacker controls.

```
  victim (euid=root)                     attacker (real user)
  ------------------------------------   ------------------------------------
  access("…/log", W_OK)                  … /log is a real file the user owns …
    → 0  (real uid MAY write it)
                                         unlink("…/log");
                                         symlink("/etc/passwd", "…/log");
  open("…/log", O_WRONLY|O_APPEND)
    → follows symlink, opens
      /etc/passwd with EUID root!
  write(fd, line, …)   ← corrupts /etc/passwd
```

The check passed against a benign file the real uid could write; the use landed
on `/etc/passwd`, which `open` reached with **root's** effective uid because the
symlink was swapped in the gap. The attacker **need not win on the first try**:
the helper presumably runs repeatedly (or can be invoked repeatedly), and each
run re-opens the window, so the attacker simply loops — flipping the symlink and
retrying until one interleaving lands inside the window. A low per-attempt
probability times unlimited attempts approaches certainty; this is why "the race
is narrow" is **not** a defence.

**(c)** `access` and `open` each take a **path string** and walk it from the
root/cwd afresh, resolving every component through the current directory
contents; the name is **late-bound**, and the binding is whatever the directory
says *at that instant*. Two resolutions of the same string can therefore reach
two different inodes.

An **open fd** is different. By Sheet 8 §E, `open` resolves the path **once** and
installs, in the process's fd table, a pointer to an **open-file description**
that points at the **inode** itself (via the in-core inode table) — not at the
name. From then on the fd **is** that inode; renaming, unlinking, or re-pointing
the *name* cannot change which inode the fd refers to (an unlinked-but-open file
even keeps its data blocks alive until the last fd closes). So the fix is to
**bind the name to an object once** and perform both the check and the use
against that fixed object: `open` first, then `fstat`/`fchmod`/`faccessat`
**on the fd** — there is no second path walk to subvert. `openat(dirfd, …)`
similarly pins the *directory* to an fd so intermediate components can't be
swapped. **`O_NOFOLLOW`** specifically defeats the **symlink** variant: it makes
`open` fail with `ELOOP` if the final component is a symbolic link, so the
attacker cannot redirect the final open through a link at all.

**(d)** The colleague is right that the **matrix** is correct — and that is
precisely why this is a **Sheet 1** failure, not a policy bug. The setuid helper
is a **confused deputy**: it holds root authority and is meant to exercise it
*only* on objects the caller is entitled to, but the caller (attacker) tricks it
into applying that authority to an object it should not. The access-control
policy is never violated on paper — `/etc/passwd` still says "real uid may not
write." TOCTOU just makes the deputy **misapply** the correct policy by acting on
a different object than the one it checked. This is the same shape as a **covert
channel**: the stated policy is sound, but a mechanism *around* the policy leaks
authority. So "the matrix is correct" is no defence — correctness of the policy
says nothing about whether the mechanism enforcing it is confusable.

Two mitigations remove the deputy's *power to be confused*:

1. **Drop privilege.** Before touching the file, set the effective uid to the
   **real** uid (`seteuid(getuid())`). Now the **kernel's own** permission check
   inside `open` runs as the real user, so if the real uid can't write the file,
   `open` fails — no separate `access` is needed, and the check and the use are
   the *same* uid on the *same* syscall. There is no window because there is no
   second syscall to race.
2. **Avoid the check entirely.** Don't check-then-act: **attempt** the operation
   and handle failure — `open(path, …)` and branch on `EACCES`. This is
   fundamentally race-free because the permission check and the object it applies
   to are decided **inside a single syscall**, atomically, by the kernel: the
   name is resolved and the access decision is made against *that* inode with no
   return to user space in between. There is no interval in which the referent
   can change, so there is nothing to swap. The general lesson (and the
   fds-as-capabilities theme of `y2022p2q4` below): **name the object once and
   act on the object, not the name.**

---

*Python verification summary:* LFS write cost `2/(1−u)` = 4.0 (u=0.5), 8.0
(u=0.75), 20.0 (u=0.9), → ∞ as u→1.
