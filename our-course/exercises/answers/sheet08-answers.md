# ⚠️ SPOILER — Examples Sheet 8 model answers ⚠️

> **STOP.** Full worked solutions. Do the sheet closed-book first. Numeric
> answers were verified with Python; checks are noted inline.

---

## A. Warm-ups

**A1. False.** Both names are ordinary directory entries pointing at the *same
inode*; neither is primary. The inode carries a link count (= number of such
entries); the file's data persists until that count reaches 0. There is no
"original" and "copy".

**A2. False.** Permissions (and owner, timestamps, size, block map) live in the
**inode**, which both hard links share. Changing the mode via either name
changes the single inode, so both names always show identical permissions. (A
*symbolic* link is different — it is a separate inode.)

**A3. False.** `unlink` removes a directory entry and decrements the inode's
link count. The inode and its data blocks are freed only when the link count
reaches 0 **and** no process still has the file open (in-core reference count
0). An open file with 0 links survives until the last `close`.

**A4. True.** A directory is a file of a special type whose contents the kernel
interprets as (name, inode-number) records. The name→inode mapping is *in the
directory's data blocks*; the inode itself holds no name. (This is why hard
links — extra directory entries — are cheap.)

---

## B. Bookwork

Marking notes (full answers in OSTEP 39–40, xv6 book ch. 8):

* **B1(c) True** — a hard link is a directory entry holding an inode *number*,
  and inode numbers are only meaningful within one filesystem; a link in
  filesystem X cannot name an inode in filesystem Y, so hard links cannot cross
  mount points. **B1(d) False** — redirection connects an fd to a *file* (or
  device/pipe); "the buffer cache" is an internal kernel cache, not a nameable
  redirection target. The shell can no more redirect "to the buffer cache" than
  to the page cache.
* **B2** — directory service: (1) map human names to files (the name space), (2)
  provide the hierarchy/organisation. Metadata: type, size, owner/group,
  permissions, timestamps, link count, and the block map — **not** the name.
  Hard link: extra directory entry → same inode; forces metadata into the inode
  (shared), can't cross filesystems. Soft link: a separate inode whose data is a
  *path string*; can dangle, can cross filesystems, adds an extra resolution
  step.
* **B3** — V7 layout: boot block, superblock (sizes + free lists), inode array,
  data blocks; free inodes via a cached list in the superblock, free blocks via
  a chained free list. Inode: mode/links/owner/size/timestamps + 13 block
  addresses (10 direct + single + double + triple in true V7; xv6 uses 12+1).
  Largest file: computed in C. Enhancements: reliability → journaling /
  duplicated superblock; performance → larger blocks, cylinder groups /
  locality (FFS, paper 21), read-ahead, buffer cache.
* **B4** — name space via directories + inodes; allocation via the block map
  (direct/indirect); metadata in inode + superblock; pipes as in-kernel bounded
  buffers with two fds. Snapshot scenario: hard-linked "copies" share inodes, so
  they protect against *unlinking/renaming* a name (each snapshot keeps its own
  entry) but **not** against editing file contents in place (all links see the
  edit — same inode/data), and give **no** protection against a disk head crash
  (one physical copy). Cheap in space; false sense of backup.

---

## C. Inode arithmetic

`BSIZE = 1024`, 4-byte block numbers ⇒ `NINDIRECT = 256`. *All verified in
Python.*

**(a) Stock xv6 (12 direct + 1 single):**
`12 + 256 = 268` blocks = `268 × 1024 = 274 432` bytes (≈ 268 KiB).

**(b) Lab 6 (11 direct + single + double):**
`11 + 256 + 256×256 = 11 + 256 + 65 536 = 65 803` blocks
= `65 803 × 1024 = 67 382 272` bytes = **64.26 MiB**.
The slot count is unchanged — stock uses 12 direct + 1 pointer = 13 slots; the
rebalance uses 11 direct + 1 single-pointer + 1 double-pointer = 13 slots — so
`sizeof(struct dinode)` and hence inodes-per-block (`IPB`) are identical; you
are *reinterpreting* two slots, not enlarging the inode.

**(c) Byte offset 40 000 000.** Block number = `40 000 000 / 1024 = 39 062`
(offset within it `40 000 000 − 39 062×1024 = 512`). Which arm?
* direct covers blocks 0–10 (11 blocks);
* singly-indirect covers 11 … 11+256−1 = 11–266;
* doubly-indirect covers 267 … 267+65 536−1 = 267–65 802.

39 062 ≫ 266, so it is in the **doubly-indirect** arm. Index within that arm =
`39 062 − 267 = 38 795`. Then `mid = 38 795 / 256 = 151`, `leaf = 38 795 % 256 =
139`. Reads to reach the data block: **the double-indirect top block (1) → the
singly-indirect mid block #151 (1) → the data block (1) = 3 block reads** (the
inode itself is assumed already in core).

**(d) Classic V7 (12 direct + single + double + triple, 256 addr, 4 KiB):**
`12 + 256 + 256² + 256³ = 12 + 256 + 65 536 + 16 777 216 = 16 843 020` blocks
= `16 843 020 × 4096 = 68 989 009 920` bytes = **≈ 64.25 GiB** (this is the
`y2021p2q3` answer). Read/write asymmetry: reading block *N* traverses the
indirect blocks and issues `bread`s for blocks that already exist. The **first
write** of block *N* must additionally **allocate** each missing level —
`balloc` scans the free-block bitmap, marks a block used, zeroes it, and logs
the bitmap + indirect-block updates — for the data block *and* for any not-yet-
existing indirect blocks on the path. So a first write can turn one logical
write into several allocations + bitmap + indirect-block writes, whereas the
read just follows existing pointers.

---

## D. Directory-lookup cost

Assume root inode cached; each other inode and each directory block is one disk
read.

**(a) Read `/usr/ann/notes.txt` (3 data blocks):**

1. root inode — cached (0)
2. read root's data block, find `usr` → **1**
3. read `usr` inode → **1**
4. read `usr` data block, find `ann` → **1**
5. read `ann` inode → **1**
6. read `ann` data block, find `notes.txt` → **1**
7. read `notes.txt` inode → **1**
8. read its 3 data blocks → **3**

Total = **9 disk accesses**.

**(b) Then read `/usr/ann/todo.txt` (2 blocks):** the inodes and directory
blocks for `usr` and `ann` (and root) are now in the buffer cache, so the whole
path prefix is free. Only `todo.txt`'s inode (**1**) and its 2 data blocks
(**2**) miss → **3 disk accesses**. The saved reads are the entire directory
walk (`usr`, `ann` inodes and directory blocks), because a warm buffer cache
serves repeated metadata reads — the point of `y2021p2q3(b)(iii)`.

**(c)** (1) **Hashed directories** — FFS/ext2 with dir indexing, and ext3/4's
**htree**, store entries in an on-disk hash tree keyed by name, giving ~O(1)
(or O(log n)) lookup instead of a linear scan; (2) **B-tree / B+-tree
directories** — XFS, Btrfs, NTFS index directory entries in a balanced tree
keyed by name (or name hash), so a lookup is O(log n) and large directories stay
fast. (Either way the inode number is still found by name; the improvement is
the search structure over the entries.)

**(d)** A deep path costs a directory-block read + inode read **per component**
the first time, because each level must be resolved before the next can be found
(you cannot read `ann`'s block until you have `ann`'s inode number from `usr`).
The **dentry cache** memoises resolved (parent, name) → inode results, so
re-walking the same path hits entirely in memory — no disk accesses — turning an
O(depth) disk walk into O(depth) hash-table lookups.

**(e)** A **mount point** is a directory in one file system that the kernel
designates as the attachment site for the root of another: `mount /dev/sdb1
/usr` records in an in-memory **mount table** that the directory `/usr` is
*covered by* the file system on `sdb1`. The graft is purely a name-space
operation — no bytes change on either disk; each file system keeps its own
inode space (inode numbers are only meaningful within one file system, which
is why hard links cannot cross a mount point — B1(c)). The lookup code must
special-case both directions of the boundary. **Downward:** when a resolved
component is a mount point, the walker must consult the mount table and
substitute the mounted file system's **root inode** for the covered
directory's inode before continuing — the covered directory's own contents
become invisible while the mount is in place. **Upward:** when the walk
follows `..` in a mounted file system's *root*, it must hop back to the mount
*point's parent* in the underlying file system (a mounted root's own `..`
refers to itself), otherwise paths like `cd /usr; cd ..` would strand the walk
inside the mounted volume. Linux implements exactly this by checking each
dentry against the mount table (the dentry/vfsmount pair) at every component.

---

## E. The fd / open-file / inode tables

**(a)**

```
 process A                 system-wide             in-core
 fd table (ofile[])        open-file table         inode table
 +-----+                   +------------------+     +-----------+
 | 0 --|------------------>| offset, flags,   |---->| inode of  |
 | 1   |                   | inode ptr        |     | "f": mode,|
 | 2   |                   +------------------+     | size,     |
 | 3=f-|--> (points to the same open-file entry)    | blockmap  |
 +-----+                                            +-----------+
```

The **offset** lives in the open-file-table entry (`struct file.off`), *not* in
the fd table or the inode — this is the crux.

**(b) After `fork()`:**
* **fd table** — *duplicated*: the child gets its own copy of `ofile[]`, but each
  entry **points at the same open-file-table entry** (the reference count on that
  entry is bumped).
* **open-file table** — *shared*: parent and child fds refer to the same entry,
  so they **share the file offset and flags**.
* **inode table** — *shared*: one in-core inode, ref-counted.

Therefore if the parent reads 100 bytes, the *shared* offset advances to 100,
and the child's next read on the inherited fd starts at **offset 100**, not 0.
(This is why a shell and a subshell writing to the same redirected fd append in
order rather than overwriting.)

**(c)** Two independent `open()`s of the same file create **two separate
open-file-table entries** → two independent offsets (both pointing at the one
in-core inode). `dup(fd)` (like the fork case) creates a new fd pointing at the
**same** open-file-table entry → **shared** offset. So: shared offset for
`fork`/`dup`/`dup2`; independent offset for a second `open`. Shell redirection
uses `dup2` precisely so the child's fd 1 shares the *opened file's* offset, and
`>>` (append mode) plus a shared offset make concurrent appends land in order.

**(d)** `close(fd)` clears the fd-table slot and **decrements the reference count
on the open-file-table entry**; when that hits 0 the entry is freed and the
**in-core inode's** reference count is decremented. The on-disk inode (and its
data blocks) is freed only when **both** the on-disk link count is 0 (no names
left) **and** the in-core reference count is 0 (no one has it open). Hence a file
`unlink`ed while open keeps working — its inode has 0 links but a non-zero open
count — and is reclaimed at the final `close`. (In xv6: `fileclose` drops
`f->ref`, then `iput` drops the inode ref and calls `itrunc`/frees when
`nlink==0 && ref==0`.)

---

## F. The VFS / vnode dispatch layer

**(a)** The open-file entry does **not** point straight at ext4's code. Each
vnode carries a pointer to an **operations vector** — a table of function
pointers (BSD *vnode ops*: `VOP_OPEN`, `VOP_READ`, `VOP_WRITE`, …; Linux
`file_operations`/`inode_operations`). The generic `read` resolves fd →
open-file entry → vnode, then makes an **indirect call through that function
pointer**: roughly `vp->v_op->read(vp, …)`. A vnode created by ext4 was given
ext4's ops vector, one created by NFS was given NFS's, so the *same* call site
dispatches into different filesystem code. This is **dynamic dispatch — a
virtual method table (vtable)** written by hand in C: the vnode is the object,
the ops vector is its vtable, and the FS type is the "class".

**(b)** The open-file-table entry (`struct file`) points at a **vnode**, not
directly at an FS-specific inode. The vnode must carry (1) a pointer to its
**operations vector** (the per-type function table dispatched in (a)), and (2) a
pointer to the **filesystem-specific private object** those ops act on (the
in-core ext4 inode / NFS node / tmpfs node). So the §E chain gains a link:

```
 fd table → open-file entry → vnode (FS-independent) → FS-specific inode

 open-file table            vnode                        FS-specific object
 +------------------+       +--------------------+       +----------------+
 | offset, flags,   |------>| v_op  ----------------ops->| read/write/... | ext4
 | vnode ptr        |       | private-data ptr ------+   +----------------+ or
 +------------------+       +--------------------+   +-->| ext4/NFS inode | NFS
                                                         +----------------+
```

The **offset is unchanged from §E** — it still lives in the open-file entry
(`struct file.off`), not in the vnode. The vnode is the FS-*independent* handle;
the FS inode is its type-specific payload. (Two opens of the same file → two
open-file entries with independent offsets, both pointing at the *one* shared
vnode — the §E(c)/§E(d) sharing story is untouched; the vnode simply replaces
"inode" as the shared tail.)

**(c)** The interface is defined purely in terms of the **ops vector**, never in
terms of disk blocks. procfs and tmpfs supply their own ops vector whose `read`
**synthesises** the bytes — from live kernel data structures, or from RAM pages —
instead of issuing disk I/O, and they create vnodes exactly as a disk filesystem
does. Because the generic `read`/`write`/`open` only ever call *through* the ops
vector, they cannot tell — and need not care — whether the bytes came off a
platter, over the network, out of RAM, or from an `sprintf` of `/proc/meminfo`.
That indistinguishability **is** "everything is a file": one `read` syscall reads
a regular file, a pipe, a socket, and `/proc/self/status` identically. To add a
new filesystem type an author supplies **only**: an ops vector (the per-vnode
function table), the mount/superblock code that builds vnodes, and the private
in-core object each vnode points at. They **leave untouched** the entire generic
syscall layer, the fd/open-file tables, and the offset machinery — the payoff of
the indirection.

**(d)** A stacked filesystem installs vnodes whose ops vector is **its own**, but
whose private-data pointer references a **vnode of the underlying filesystem**.
When a stacked vnode's `read` (etc.) is called it does its layer-specific work —
overlayfs decides whether the name resolves in the writable *upper* layer or the
read-only *lower* one (and does copy-up on first write); a null/loopback layer
does nothing extra — and then **forwards the same operation down** by calling
`VOP_READ` (etc.) on the underlying vnode through *its* ops vector. Composition
is possible precisely because **every filesystem presents the identical vnode
interface**: the generic syscall layer *consumes* a vnode and the stacking layer
*wraps* a vnode using the very same ops-vector contract, so a filesystem can play
either role, and the upper layer can call the lower one without knowing whether
it is ext4, tmpfs, or yet another stack. (overlayfs uses exactly this to merge a
read-only lower tree with a writable upper tree into one namespace.)

---

*Python verification summary:* stock inode 268 blocks / 274 432 B; Lab 6 inode
65 803 blocks / 67 382 272 B (64.26 MiB); classic V7 (with triple indirect)
16 843 020 blocks / ≈ 64.25 GiB. Offset 40 000 000 → block 39 062, in the
double-indirect arm at mid 151, leaf 139 (3 block reads). Directory walk: 9
accesses cold, 3 warm.
