# CRASH-TABLE.md — model answer (Part 2)

Produced by `./sweep part2 --xfsck ../tests/oracle/xfsck --out CRASH-TABLE.md`
over the fixed sequence W1 dirent(13), W2 inode(10), W3 bitmap(12), W4
data(14), W5 data(15), then annotated. The annotation is what the rubric
marks; the table alone is a tool dump.

| crash after k writes | xfsck says | why |
|---|---|---|
| 0 | clean | nothing was issued; the image is mkfs's, untouched |
| 1 | dangling-entry | the root entry `f -> inode 2` is durable but inode 2 is still free: the name points at nothing |
| 2 | block-free-but-used | inode 2 now exists and claims blocks 14 and 15, but the bitmap still marks both free: the allocator would hand them out again |
| 3 | clean **(but wrong)** | entry, inode and bitmap all agree — yet blocks 14/15 still hold mkfs's zeroes, not the file's bytes. A structural checker has nothing to object to |
| 4 | clean **(but wrong)** | as k=3, except block 14 now holds real contents and block 15 still zeroes — half a file, invisibly |
| 5 | clean | the write completed; contents byte-exact |

## The rows that matter

k=1 and k=2 are fsck's home ground: a structural invariant is violated and a
scan finds it (at the price, on a real disk, of walking the whole tree).

k=3 and k=4 are fsck's *limit*, stated by ch. 42 §42.2: the metadata is
perfectly consistent and the data is garbage. No structure-only tool can
distinguish k=3 from k=5 — the information ("what should these blocks hold?")
simply is not on the disk. That is why Part 4's campaign adds content and
durability oracles on top of xfsck, and why journaling (which makes the whole
five-write update atomic) beats fsck (which can only repair structure after
the fact) as a crash-consistency mechanism.

Note the order-dependence: this sequence never produces ch. 42's "space leak"
(bitmap set, nothing pointing at the block). Issue W3 before W2 and k=2
becomes exactly that (`bitmap-leak` — plus the still-dangling entry). Which
inconsistencies are *reachable* is a function of the order alone; that no
order makes all six states unreachable is the chapter's point.
