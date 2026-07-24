# A corruption xfsck cannot detect (reference)

Run:

```sh
./mkimg clean.img
cp clean.img d.img
./corrupt d.img data      # flips two bytes inside a file's data block
./xfsck d.img             # prints "xfsck: clean"
```

`xfsck` reports the image clean, and it is right to.

## Why no purely structural checker can catch it

A structural checker verifies that the file system's **bookkeeping is
self-consistent**. Every invariant it checks is a relationship *between*
metadata records:

- a block is owned by exactly one inode *and* the bitmap agrees;
- an inode's `nlink` *equals* the number of directory entries that name it;
- a directory's `..` *matches* who its parent actually is;
- every entry names an allocated inode, and every allocated inode is named.

These are all statements the format lets you cross-check, because the format
stores each fact more than once (a block's allocation is recorded both in the
bitmap and implicitly by whichever inode points at it; a link is recorded both
in `nlink` and by the directory entries). Corruption that makes two copies of
the same fact disagree is exactly what a checker finds.

The **contents** of a data block are stored exactly once. Nothing else in the
image says what byte 0 of block 49 *should* be. The `data` corruption changes
those bytes and touches nothing else: the block is still allocated to the same
inode, the bitmap still marks it in use, the inode's size and link count are
unchanged, every directory is still well formed. Every invariant still holds —
so a checker whose entire job is to confirm the invariants hold must, correctly,
say nothing.

To notice altered file contents you need **redundancy the xv6 format does not
carry** — a checksum or hash stored alongside each block, so the block can be
checked against something other than itself. That is a different kind of
mechanism: it is not verifying that existing bookkeeping agrees, it is adding
new bookkeeping whose only purpose is to detect content change. Filesystems
that want this (ZFS, btrfs) build per-block checksums into the on-disk format
from the start; a checker bolted on afterwards cannot recover information the
format never wrote down.

This is the line between a **checker** and a **consistency mechanism**. A
checker inspects a static image after the fact and can only catch disagreements
among facts that were recorded redundantly. It cannot catch a single-copy fact
that was silently rewritten, and — the subject of the journaling lab — it
cannot tell you anything about *how* the image came to be inconsistent, or
replay operations to repair it. Structural checking is necessary and cheap; it
is not sufficient, and it is not durability.
