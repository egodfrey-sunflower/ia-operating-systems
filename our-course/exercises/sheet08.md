# Examples Sheet 8 — File management

**Attempt after Week 11.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet08-answers.md` (spoilers — attempt first).

*Covers: the file abstraction and metadata; directory service,
hierarchy, hard and soft links; the Unix V7 on-disk layout and inodes; inode
arithmetic; directory-lookup cost; mount points; the fd / open-file / inode
table structure.*

Reading: OSTEP ch. 39–40 (files & directories, FS implementation) and ch. 41
(locality and the Fast File System — read alongside paper 21); the xv6 book
ch. 8; reading-list paper 21 (McKusick et al., FFS). This sheet is the theory
counterpart to **Lab 6 (file systems)** — the inode arithmetic and
hard-vs-soft link questions are exactly what you *will* make concrete there
(this sheet is due in week 11, before that lab).

---

## A. Warm-ups (true / false, with a one-line justification)

**A1.** Given a file with two hard links, one of the two names is the "real"
file and the other is a copy.

**A2.** Two hard links to the same file can carry different access permissions.

**A3.** `unlink`-ing a file always frees its inode and data blocks immediately.

**A4.** A directory is just a file whose contents are name→inode-number
mappings.

---

## B. Bookwork from the IA sheet (do by citation)

**B1.** Do **IA Examples Sheet 3, Q1(c) and Q1(d)**
(`../../cambridge-course/examples_sheets/examples_sheet3.pdf`) — true/false: "in Unix, hard links
cannot span mount points" and "the Unix shell supports redirection to the buffer
cache". *Note:* (c) hinges on inode numbers being per-filesystem; (d) is about
what a redirection target actually is (a file/fd, not a kernel cache).

**B2.** Do **IA Examples Sheet 3, Q2** — the two functions of the directory
service, directory hierarchy, file metadata, and hard vs soft links (and the
constraints each places on where metadata lives). *Note:* your answer to the
hard/soft-link parts should line up with the Lab 6 Task 2 contrast; C(d) below
extends it.

**B3.** Do **IA Examples Sheet 3, Q5** — the Unix V7 on-disk layout
(superblock, free inode/block management), the V7 inode, an estimate of the
largest file, and reliability/performance enhancements. *Note:* C makes the
"largest file" estimate exact for the xv6/Lab 6 numbers; the enhancements
anticipate FFS (paper 21) and journaling (Sheet 10).

**B4.** Do **IA Examples Sheet 3, Q6** — how Unix implements the name space,
on-disk allocation, metadata and pipes; and the "versioned filesystem via nightly
hard-linked snapshots" scenario. *Note:* the snapshot scenario is really a
question about what hard links can and cannot capture (metadata sharing, the
inability to snapshot the inode itself, deletion semantics) — connect it to A1–A3.

---

## C. Inode arithmetic (xv6 / V7, the Lab 6 layout)

xv6 uses V7-style inodes with block size **BSIZE = 1024 bytes** and **4-byte**
block numbers, so an indirect block holds `NINDIRECT = 1024/4 = 256` block
numbers.

**(a)** The **stock** xv6 inode has **12 direct** slots and **1 singly-indirect**
block. Compute the maximum file size in blocks and in bytes.

**(b)** In **Lab 6, Task 1 (weeks 13–14)** you **will** rebalance the inode to
**11 direct + 1 singly-indirect + 1 doubly-indirect** (the on-disk slot count
stays 13, so
`sizeof(struct dinode)` and inodes-per-block are unchanged — explain why in one
sentence). Compute the new maximum file size in blocks, in bytes, and in MiB.

**(c)** For the Lab 6 inode, how many block reads (counting indirect blocks) are
needed to fetch the data block holding **byte offset 40 000 000**? Show which
arm of the inode that offset falls in and the index at each level.

**(d)** In `y2021p2q3(b)(i)` the classic V7 inode has **12 direct + single +
double + triple** indirect with **256** addresses per indirect block and **4 KiB**
blocks. Compute that maximum file size. Then explain the *read/write asymmetry*
you will meet in **Lab 6 Task 1**: reading and writing block *N*
both walk the same indirect blocks, so why does the **first write** of a block
cost so much more than a read?

---

## D. Directory-lookup cost

A Unix-style filesystem stores each directory as a file of (name, inode-number)
entries. Assume the **root inode is cached** in memory; every other inode read
and every directory-data-block read is a separate disk access; each directory's
entries fit in **one** block.

**(a)** A process opens and reads `/usr/ann/notes.txt`, whose data occupies
**3 blocks**. Count the disk accesses, listing each (inode reads, directory
reads, data reads).

**(b)** *Immediately afterwards* the process opens and reads
`/usr/ann/todo.txt` (**2 blocks**), which lives in the same directory. Assuming
a buffer cache, how many disk accesses now? Explain which reads were saved and
why (cf. `y2021p2q3(b)(iii)`).

**(c)** Directory lookup as described scans entries **linearly** — O(n) in the
number of entries, and the whole path costs O(depth × n). Name two real designs
that reduce per-directory lookup below O(n), and say what data structure each
uses. (Hint: FFS/Ext hashed directories; ext3/4 *htree*; think also of how the
name→inode step could be indexed.)

**(d)** Why does deepening the directory tree (many path components) cost disk
accesses even with a warm cache the *first* time a path is walked, and how does
the *dentry* (directory-entry) cache change the cost of walking the same path
again?

**(e)** A path walk can cross from one file system into another partway along.
What is a *mount point*, and how does mounting graft one file system's tree
onto another's name space without changing anything on either disk? What must
the lookup code special-case when a resolved component turns out to be a mount
point — and, symmetrically, when a walk follows `..` upward out of a mounted
file system's root?

---

## E. The fd / open-file / inode tables

Unix keeps three distinct structures on the read/write path: the per-process
**file-descriptor table**, the system-wide **open-file table** (each entry
holds the current offset and the open-mode flags), and the in-core **inode
table** (one entry per active file, cached from disk).

**(a)** Draw the three tables and the pointers between them for a single process
that has done `fd = open("f", O_RDWR)`. Which structure holds the *offset*?

**(b)** A process calls `fork()`. For **each** of the three tables, say what is
shared and what is duplicated between parent and child. In particular: if the
parent then reads 100 bytes, does the child's subsequent read start at offset 0
or 100? Justify from your diagram.

**(c)** Contrast that with two *independent* `open()` calls on the same file by
the same process, and with `dup(fd)`. Which of these share a **file offset** and
which get an independent one? (This is exactly why shell redirection uses `dup2`
and why appending shells share output correctly.)

**(d)** In Lab 6 / xv6 these are `struct file` (the open-file entry, with
`off`), the per-process `ofile[]` (the fd table), and `struct inode` (the in-core
inode). Using them, explain what `close(fd)` decrements and when the on-disk
inode is finally freed — i.e. why a file with link count 0 but still `open`
survives until the last `close`.

---

## F. The VFS / vnode dispatch layer

One running kernel serves `open`/`read`/`write` against many filesystem types at
once — ext4 on a disk, tmpfs in RAM, procfs synthesising files on the fly, NFS
over the network — all through the same system-call interface. This is the
machinery behind "everything is a file", and it sits directly on top of the fd /
open-file / inode chain of §E. *(Reading: FreeBSD ch. 7, the VFS/vnode layer.)*

**(a)** A process calls `read(fd, buf, n)` on an fd that might name a file on any
of those four filesystems. The syscall code is *generic* — compiled once, and it
knows nothing about ext4 or NFS. By what mechanism does one generic `read` end up
executing ext4's block-fetching code for one fd and NFS's RPC code for another?
Name the indirection, and say what classic object-oriented construct it is a
hand-written C version of.

**(b)** The FS-independent in-memory object at this layer is the **vnode** (Linux
splits the role across the VFS `inode`/`dentry`), sitting above each filesystem's
own on-disk-derived object (the ext4 inode, the NFS file handle, …). Extend the
§E fd → open-file → inode chain: where does the vnode slot in, and what **two**
things must a vnode carry for part (a)'s dispatch to work? Redraw the *tail* of
the §E(a) diagram (from the open-file entry rightwards) to show it, and confirm
where the offset still lives.

**(c)** procfs and tmpfs have no disk inodes at all — a procfs "file" is
generated on demand from kernel state. Explain how they nonetheless slot into the
identical interface, why the generic `read` cannot tell the difference, and why
this uniformity is the concrete meaning of "everything is a file". What must the
author of a new filesystem type supply, and what do they get to leave untouched?

**(d)** A **stackable** filesystem (overlayfs, or a classic null/union layer)
interposes one vnode layer on top of another. Given the operations-vector
interface, explain how a stacked vnode's ops behave when called, and why the
vnode abstraction is exactly what makes this composition possible — i.e. why one
filesystem can wrap another without knowing the lower layer's type.

---

## Past paper questions

Per this directory's `README.md`, attempt these after this sheet (~35 min each). As
cumulative Tripos papers they mix files with earlier material — take the paging
part of `y2021p2q3` as revision of Sheet 6.

* **`y2018p2q3`** (`../../cambridge-course/exam_questions/y2018p2q3.pdf`) — what metadata lives
  in the FCB, ACLs vs capabilities for file access, and inode arithmetic. A
  direct fit for the metadata and inode material above.
* **`y2021p2q3`** (`../../cambridge-course/exam_questions/y2021p2q3.pdf`) — part (b) is the
  target: inode maximum file size and counting the disk accesses in a path
  lookup (cf. D above). Part (a) is paging/TLB arithmetic — treat it as Sheet-6
  revision.

For extra, untimed drill on this sheet's material, two pre-2016 questions fit
(files in `../../cambridge-course/exam_questions/`):

* **`y2012p2q4`** — a file structured from disk blocks whose first block mixes
  control information, immediate data, and direct/indirect/double-indirect
  pointers: fetch-cost and maximum-size arithmetic in the style of Section C,
  plus where file metadata should live (directory vs inode) and the trade-offs
  of contiguous allocation.
* **`y2010p2q3(c)`** — compare and contrast the Unix buffer cache and the
  Windows NT cache manager: the file-system-caches material behind the
  directory-lookup and buffer-cache reasoning of Section D. Parts (a)–(b) of
  the paper are Sheet 3's scheduling drill, where it is listed "[not (c)]", so
  this part finds its home here.

For mechanical drill on inode/bitmap state transitions, the OSTEP homework
simulator `vsfs.py` (<https://github.com/remzi-arpacidusseau/ostep-homework>)
shows how each FS operation mutates the on-disk structures — ideal preparation
for the Lab 6 tasks this sheet feeds into.
