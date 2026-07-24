> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 17 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** `unlink("f")` removes the *link* — the (name → inode) entry
in the directory — and decrements the inode's link count. Blocks are freed
only when that count reaches **zero** *and* no process still holds the file
open. If another hard link exists, or a process has it open, the data
survives. This is precisely why the call is named `unlink`, not `delete`.

**A2. FALSE.** `lseek` moves nothing on the disk; it sets an integer (the
current offset) in the open-file-table entry. No I/O occurs, no head moves.
A disk seek may happen *later*, when a read/write at that offset is issued —
but `lseek` itself is a pure in-memory operation. (This is the confusion
ch. 39 explicitly warns against.)

**A3. TRUE.** `fork()` makes the child share the parent's open-file-table
*entry* — including the offset field. A read (or `lseek`) by either process
advances the single shared offset both see. This is what lets cooperating
processes append to one output file without coordinating.

**A4. FALSE.** Two separate `open()` calls create two *distinct*
open-file-table entries, each with its own offset (even though both point at
the same inode). A read by one leaves the other's offset untouched. The
contrast with A3 is the whole point of the open-file-table design.

**A5. FALSE.** `write()` typically just buffers the data in memory; the file
system flushes it later (seconds). A crash in the gap loses it. Durability
requires `fsync(fd)` — and, for a newly created file, sometimes an `fsync`
of the containing *directory* too (A-grade answers mention this; see C1).

**A6. TRUE.** Hard links to directories are disallowed to prevent cycles in
the tree (and the ambiguity they create for `..` and tree-walkers).
Symbolic links to directories are permitted precisely because a symlink is
just a name holding a path — following it is explicit, and cycles are
detected at resolution time rather than corrupting the structure.

**A7. TRUE.** A hard link is a peer name for the same inode; removing "the
original" just drops one of several equal links, and the file persists via
the others (the very notion of "original" is a fiction — all hard links are
equal). A symbolic link stores a *pathname*; delete what that path names and
the link **dangles** — following it fails with "No such file".

**A8. TRUE.** Reading `f` needs `r` on `f` itself, and **execute (search)**
permission on every directory along the path — `/`, `/a`, `/a/b` — because
execute-on-a-directory is what grants the right to traverse *through* it to
resolve a name. Read-on-a-directory (by contrast) only lets you *list* it.
Missing execute on any component blocks resolution regardless of `f`'s own
bits.

---

## B. The interface, traced

**B1.**

| Step | Returns | OFT-a (fd, fd2) | OFT-b (fd3) |
|---|---|---|---|
| (1) open | fd = 3 | 0 | — |
| (2) dup | fd2 = 4 | 0 (fd2 **shares** OFT-a) | — |
| (3) read fd 100 | 100 | 100 | — |
| (4) lseek fd2 500 | 500 | **500** (shared!) | — |
| (5) read fd 100 | 100 | 600 | — |
| (6) open | fd3 = 5 | 600 | 0 (new entry) |
| (7) read fd3 100 | 100 | 600 | 100 |
| (8) fork | — | 600 (**shared** parent/child) | 100 (shared parent/child) |
| (9) child lseek END | 1000 | 1000 | 100 |
| (10) parent lseek CUR | **o1 = 1000** | 1000 | 100 |
| (11) parent lseek CUR | **o2 = 100** | 1000 | 100 |

`o1 = 1000`, `o2 = 100`. The differences in kind: at **(2)** `dup` makes fd2
share fd's *open-file-table entry* (same offset — that is why (4) moved fd's
offset to 500); at **(8)** `fork` makes the child share *all* the parent's
entries, so the child's `SEEK_END` at (9) is visible to the parent at (10) —
hence `o1 = 1000`; at **(6)** a fresh `open` created a *separate* entry with
its own offset, untouched by any of it — hence `o2 = 100`. Same inode
throughout; three different offsets because three different sharing rules.

**B2.**
**(a)** Link counts after each step (inode of `a`/`b`/`f` is one shared
inode; call it I; directories track `.`/`..`):

| Step | I (a/b/f) | c (symlink) | d0 | sub | deeper |
|---|---|---|---|---|---|
| 1 touch a | 1 | — | 2 | — | — |
| 2 ln a b | 2 | — | 2 | — | — |
| 3 ln -s a c | 2 | 1 | 2 | — | — |
| 4 mkdir sub | 2 | 1 | **3** | 2 | — |
| 5 ln b sub/f | **3** | 1 | 3 | 2 | — |
| 6 rm a | **2** | 1 | 3 | 2 | — |
| 7 cat c | 2 | 1 | 3 | 2 | — |
| 8 cat sub/f | 2 | 1 | 3 | 2 | — |
| 9 mkdir sub/deeper | 2 | 1 | 3 | **3** | 2 |

(`d0` becomes 3 at step 4 because `sub/..` points back to it;
`sub` becomes 3 at step 9 for the same reason via `deeper/..`.)

**(b)** Step 7 **`cat c` fails.** `c` is a symlink whose data is the
pathname `a`; `a` was unlinked at step 6, so resolving `c` dangles →
"No such file". Step 8 **`cat sub/f` succeeds** and prints the content:
`sub/f` is a *hard* link to inode I, whose count is still 2 (`b` and
`sub/f`) — removing the name `a` didn't touch the data, only the count.
The contrast is the A7 lesson in the concrete.

**(c)** A directory's link count = 2 + (number of sub-directories). The 2
is its own name in its parent plus its own `.` entry; each child
sub-directory adds one via its `..` entry pointing back. (So an empty
directory has count 2; `d0` with one subdir has 3.)

**(d)** The **inode number** (`st_ino`), with `st_dev`. Files sharing an
inode number on the same device are the same file; a de-duplicating tool
records (dev, ino) pairs it has already counted. (`st_nlink > 1` is the hint
that sharing *might* exist; the inode number confirms *which* files share.)

**B3.**
**(a)**

| | index.html (rw-rw-r--, alice/web) | notes.txt (rw-r-----, bob/staff) | deploy.sh (rwxr-x---, alice/web) |
|---|---|---|---|
| alice | owner: r,w | group staff: r only | owner: r,w,x |
| bob | other: r only | owner: r,w | other: — (bob ∉ web): none |
| carol | group web: r,w | other: none | group web: r,x (no w) |

Reasons in brief: alice owns index.html and deploy.sh (owner bits); alice is
**in staff**, so on notes.txt (`rw-r-----`, group staff) she gets the group
bits — **r only**, no write. bob owns notes.txt; for the alice-owned files
bob is neither owner nor in web → "other" (r for index.html, nothing for
deploy.sh). carol is in web → group bits on the alice/web files, "other" on
notes.txt (none).

**(b)** `/home/bob` = `drwx------`: only bob has search permission on it.
**Yes — one verdict changes.** In (a) alice could read `notes.txt` through
her staff-group membership; but she cannot **traverse** `/home/bob` to reach
it, so that read is now denied — the file's own `r--` group bit is
irrelevant once the path is closed. (carol never had access to notes.txt,
so nothing changes for her; bob still reaches it, holding `x` on his own
directory.) The rule: **a directory's execute bit gates traversal; losing it
denies access to everything beneath, overriding the files' own
permissions** (A8: execute on *every* path component is required).

**(c)** First, the bits *can* express execute-without-read, and here they are
even reachable with `chmod`/`chgrp` alone: bob is already in **staff**, so
`chgrp staff deploy.sh; chmod g=x,o= deploy.sh` gives group staff `--x` —
execute, no read — and bob picks it up through his staff membership. No new
group or `/etc/group` edit is needed. **But** `deploy.sh` is a **shell
script**: "execute" means the kernel opens it and hands it to an interpreter,
which must **read** it *with bob's credentials* — and bob has no read, so the
interpreter gets "Permission denied". The execute bit is expressible but
useless here. Verdict: **no** — you cannot give run-but-not-read on a *script*
with permission bits; the guarantee needs a compiled binary (where the
kernel's `exec` reads the file with the *kernel's* authority, not bob's) or a
setuid wrapper, which is the access-control chapter's territory (week 20).
And even a working execute bit would not stop a determined bob: with any
readable channel he could `cat` the contents — though here he has no read at
all. Credit for spotting the script-vs-binary distinction.

**(d)** Octal **0640**. `chmod 640 /srv/www/index.html`.

**B4.**
**(a)** Interleaving:

1. Daemon: `lstat("/var/mail/bob")` — sees a regular file owned by bob
   (bob, or the system, set it up legitimately). Checks pass.
2. **bob:** `unlink("/var/mail/bob")` then
   `symlink("/etc/passwd", "/var/mail/bob")` — or an atomic `rename` of a
   pre-made symlink over the name. Both permitted: bob owns `/var/mail/bob`
   and has write/execute on the directory, so he may replace the name.
3. Daemon: `open("/var/mail/bob", O_WRONLY|O_APPEND)` — **follows the
   symlink** to `/etc/passwd`, opening it with *root's* authority.
4. Daemon: `write(fd, msg, len)` — appends the attacker's message (a
   crafted `passwd` line) to `/etc/passwd`. Privilege escalation.

Every bob action is within bob's own rights over a name in a directory he
controls; the damage comes from the daemon performing the *use* with root's
authority on an object bob changed after the *check*.

**(b)** Shared mutable state: the **pathname binding** — what
`/var/mail/bob` resolves to. The **check** is line 1 (`lstat` on the path);
the **use** is line 3 (`open` on the same path); the gap between them is the
race window. A lock doesn't help because the "other thread" is a separate,
unprivileged, uncooperative process that shares no lock with the daemon —
and the resource it mutates (a directory entry) is the file system's, not
guarded by any lock the daemon can hold. You cannot mutex against an
adversary who never agreed to the mutex.

**(c)** `O_NOFOLLOW` makes the `open` fail if the final component is a
symlink, defeating the symlink swap; and — the general fix — doing the
checks via `fstat(fd)` binds them to the **inode the descriptor already
holds open**. Once `open` returns, the descriptor → open-file-table entry →
**inode** chain is fixed; bob's later `rename()`/`unlink()` change only the
*directory entry* (the name → inode mapping), which the descriptor no longer
consults. The check and the use now both refer to the same inode, not to a
name that can be re-pointed. Bob can rename the path all he likes; the
daemon is holding the object, not the name.

**(d)** A pathname is resolved *afresh* on each use, so check and use can
bind to different objects; a descriptor is a fixed handle to one already-
resolved object, so a check on it and a use of it necessarily refer to the
same thing.

---

## C. Diagnose the failure

**C1.**
**(a)** The file's *data* was made durable by the `fsync(fd)` calls — but a
newly created file's **directory entry** is a separate piece of metadata,
and `fsync` on the file descriptor does not guarantee the *directory* block
linking the name to the inode has reached disk. At the crash, the inode and
data blocks were on disk, but the entry `2026-07-20-14.log → inode` was
still only in the in-memory directory. On reboot there is no name pointing
at the inode, so the file "does not exist" (and its blocks are reclaimed as
unreferenced). Earlier hours survived because enough time passed for their
directory updates to flush. This is exactly the trap ch. 39 names: fsync the
file *and* the containing directory.

**(b)** After creating the file (or at least before relying on its
existence), `fsync()` the **containing directory** as well:
`dirfd = open("/logs", O_RDONLY); ... fsync(dirfd);`. Do it once the new
name must be durable — in practice right after the first `fsync` of the
file, or before the hour's first batch is acknowledged.

**(c)** The temp-then-`rename` pattern *does* fix the missing-name problem:
`rename` is atomic w.r.t. crashes, so after the rename the log appears under
its final name with all-or-nothing content — and it also makes readers never
see a half-written log. What it quietly changes: the file is now published
**only at the end of the hour**, so a crash at 14:59 loses the *entire
hour's* records rather than just the last unflushed batch — strictly more
data than the current design, whose per-batch `fsync`s at least persisted
everything up to the last batch. (And rename still needs a directory fsync
to make the *rename itself* durable.) The right design usually combines
them: append-and-fsync during the hour for durability, and either fsync the
directory on create or accept the rename's end-of-hour publish — but you
cannot get incremental durability from rename alone.

**C2.**
**(a)** Hard links share one inode. If an editor modifies a file **in
place** — `open(path, O_WRONLY)` then `write()` into the existing inode —
then *every* hard link to that inode, in yesterday's snapshot and all
others, sees the change: they are the same bytes. `report.txt` was edited by
such an editor, so the "accidental edit" propagated backwards through every
snapshot at once. `config.yaml` was edited by a program using the
**write-temp-then-`rename`** pattern (ch. 39's atomic-update recipe): that
creates a *new* inode and renames it over the name, leaving the old inode —
still referenced by the snapshots' hard links — untouched. So its history
survived. The two classes are **in-place writers** and **atomic-rename
(replace-the-inode) writers**.

**(b)** The snapshotter needs every writer to **never modify a file's inode
in place** — always replace it (new inode via rename), so that existing
hard links keep pointing at the old content. The tool cannot enforce this:
it has no control over how arbitrary applications choose to write, and
in-place modification is a perfectly legal, common thing for a program to
do. (This is why real snapshot systems live *below* the file API, in the
file system or volume manager — weeks 21–22.)

**(c)** Within the tool: on snapshot day, instead of hard-linking, make a
**copy-on-write** or a genuine copy of any file whose content it must
preserve independently — or detect in-place-modifiable files and copy those.
Cost: space and time — the very thing the hard-link trick was avoiding; a
copy-on-write file system removes the trade-off but is out of the tool's
hands. (Credit for identifying that the honest fix abandons the space
saving, or pushes the problem below the file interface.)

**C3.**
**(a)** Unix checks permissions at **`open()`**, not at each `read()`. The
job opened the file while it still had access; it holds an open file
descriptor, and every subsequent `read()` goes through that descriptor
without re-consulting the file's permission bits. `chmod 000` changes the
bits that gate *future* `open()`s — it has no effect on descriptors already
handed out. The job keeps reading because it is exercising a right it was
granted at open time.

**(b)** Ch. 39 calls a file descriptor "a capability: an opaque handle that
gives you the power to perform certain operations". This incident is that
sentence in action: a capability, once granted, carries its authority with
it and is not re-checked against the object's current policy — so the
authority persists after the policy that granted it is withdrawn. The
property the administrator must live with is that **capabilities are hard to
revoke**: changing the bits doesn't reach out and invalidate handles already
in flight. To actually stop the job she must act on the *process*, not the
file — kill it (or the descriptor), so the capability is destroyed rather
than the (already-consulted) policy changed. *(The deeper duality between
this and permission-bit checking is developed later in the course; for now
the point is just: the fd is a held handle, and revoking the bits doesn't
recall it.)*

**(c)** Unix checks at open because (i) **cost** — re-validating on every
`read()` would put a full permission check (path? owner? group membership?)
on the hottest path in the system, for a policy that almost never changes
mid-file; and (ii) **semantics** — a long-running job that lost access
mid-read would fail in bizarre, unrecoverable places, so open-time checking
gives programs a stable contract ("if it opened, it stays readable"). The
judgement: check-at-open is the wrong default for systems where **prompt
revocation is a security requirement** — a multi-tenant server handling
sensitive data under changing authorisation, where "they kept reading for
six hours" is an unacceptable answer. There the design wants revocable
handles (leases/timeouts, or authority re-checked against a central
monitor), at the performance cost (i) names.

*Marking note: full credit ties the incident to the "capability" phrasing
and lands on "act on the process, not the file"; and the (c) judgement
should name a concrete class of system, not just "secure ones".*
