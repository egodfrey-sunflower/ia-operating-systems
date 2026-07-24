#!/usr/bin/env python3
# xv6fs.py -- the on-disk xv6 file-system format, as a small userspace library.
#
# GIVEN CODE, complete.  You do not change this file.  It is the same format
# Lab 7's xfsck reads (BSIZE 1024, 64-byte dinodes, 16-byte dirents), which is
# the whole reason that checker can be this lab's consistency oracle: every
# image your crashes produce is an image xfsck already knows how to judge.
#
# Everything here is expressed against a DEVICE OBJECT with this interface:
#
#     dev.read(bno) -> bytes(1024)      read a block (sees pending writes)
#     dev.write(bno, data)              write a structure (metadata) block
#     dev.write_data(bno, data)         write a file-content block
#     dev.barrier()                     wait until everything issued is durable
#
# A plain BlockDev (blockdev.py, Part 1) implements all four directly, with
# write_data the same as write.  A Journal (wal.py, Part 3) implements the same
# four on top of a BlockDev -- which is how the identical structure writers run
# unjournaled in Part 2 and journaled in Part 3 without changing a line here.
# The write/write_data split exists for Part 5: ordered journaling treats file
# content differently from metadata, and only the device wrapper needs to know.
#
# The helpers are split into read-only "find" steps and single-block "write"
# steps on purpose: Part 2 is about the ORDER writes are issued in, so the
# order is yours to choose -- the helpers never write more than they say.

import struct

# ---------------------------------------------------------------------------
# format constants (identical to Lab 7's xv6fs.h)
# ---------------------------------------------------------------------------

BSIZE    = 1024
FSMAGIC  = 0x10203040
NDIRECT  = 11
NADDR    = NDIRECT + 2          # addrs[] entries in a dinode
DIRSIZ   = 14
IPB      = 16                   # inodes per block (1024 / 64)
BPB      = BSIZE * 8            # bitmap bits per block
T_DIR    = 1
T_FILE   = 2
ROOTINO  = 1

# the fixed geometry of this lab's images (small, so a sweep copies them fast)
FSSIZE   = 64                   # total blocks
NINODES  = 32                   # 2 inode blocks
NLOG     = 8                    # 1 log header + 7 log slots
LOGSTART   = 2                  # blocks 2..9   : the log
INODESTART = 2 + NLOG           # blocks 10..11 : inodes
BMAPSTART  = INODESTART + 2     # block  12     : bitmap
FIRSTDATA  = BMAPSTART + 1      # blocks 13..63 : data (block 13 = root dir)

# the log header block: magic, seq, n, then n destination block numbers.
# (Its exact layout is a fixed contract -- see the handout, Part 3.)
LOGMAGIC = 0x6A726E6C
LOGCAP   = NLOG - 1             # 7 slots: blocks LOGSTART+1 .. LOGSTART+7

SB_FMT     = "<8I"              # magic size nblocks ninodes nlog logstart inodestart bmapstart
DINODE_FMT = "<4h14I"           # type major minor nlink, size, addrs[13]  (64 bytes)
DIRENT_FMT = "<H14s"            # inum, name


def zero_block():
    return bytes(BSIZE)


def fill_pattern(t, j):
    """The graded workloads' file contents: block j of file t is 1024 copies
    of byte 64 + 8*t + j.  Derivable, so every content check can be redone by
    hand."""
    return bytes([(64 + 8 * t + j) & 0xFF]) * BSIZE


# ---------------------------------------------------------------------------
# structs
# ---------------------------------------------------------------------------

class Dinode:
    def __init__(self, type=0, major=0, minor=0, nlink=0, size=0, addrs=None):
        self.type, self.major, self.minor = type, major, minor
        self.nlink, self.size = nlink, size
        self.addrs = list(addrs) if addrs else [0] * NADDR

    def pack(self):
        return struct.pack(DINODE_FMT, self.type, self.major, self.minor,
                           self.nlink, self.size, *self.addrs)

    @staticmethod
    def unpack(raw):
        f = struct.unpack(DINODE_FMT, raw)
        return Dinode(f[0], f[1], f[2], f[3], f[4], f[5:5 + NADDR])


def pack_sb():
    return struct.pack(SB_FMT, FSMAGIC, FSSIZE, FSSIZE - FIRSTDATA, NINODES,
                       NLOG, LOGSTART, INODESTART, BMAPSTART).ljust(BSIZE, b"\0")


def pack_log_header(seq, dests):
    """The commit record / log header: n == 0 means the log is empty."""
    n = len(dests)
    assert n <= LOGCAP
    body = struct.pack("<3I", LOGMAGIC, seq, n)
    body += struct.pack("<%dI" % LOGCAP, *(list(dests) + [0] * (LOGCAP - n)))
    return body.ljust(BSIZE, b"\0")


def parse_log_header(raw):
    """-> (magic_ok, seq, n, dests[:n])"""
    magic, seq, n = struct.unpack_from("<3I", raw, 0)
    dests = list(struct.unpack_from("<%dI" % LOGCAP, raw, 12))
    ok = (magic == LOGMAGIC and 0 <= n <= LOGCAP)
    return ok, seq, (n if ok else 0), (dests[:n] if ok else [])


# ---------------------------------------------------------------------------
# mkfs: an empty, valid image -- root directory with "." and ".."
# ---------------------------------------------------------------------------

def mkfs(dev):
    """Write a fresh file system through `dev` (5 block writes + a barrier).
    The result passes xfsck: root inode 1 is a directory of size BSIZE whose
    one data block (13) holds "." and "..", and the bitmap marks blocks 0..13
    (all metadata plus the root block) in use."""
    dev.write(1, pack_sb())
    dev.write(LOGSTART, pack_log_header(0, []))

    root = Dinode(type=T_DIR, nlink=1, size=BSIZE)
    root.addrs[0] = FIRSTDATA
    iblock = bytearray(BSIZE)
    iblock[ROOTINO * 64:(ROOTINO + 1) * 64] = root.pack()
    dev.write(INODESTART, bytes(iblock))

    bmap = bytearray(BSIZE)
    for b in range(FIRSTDATA + 1):          # blocks 0..13 in use
        bmap[b // 8] |= 1 << (b % 8)
    dev.write(BMAPSTART, bytes(bmap))

    dblock = bytearray(BSIZE)
    dblock[0:16] = struct.pack(DIRENT_FMT, ROOTINO, b".")
    dblock[16:32] = struct.pack(DIRENT_FMT, ROOTINO, b"..")
    dev.write_data(FIRSTDATA, bytes(dblock))
    dev.barrier()


# ---------------------------------------------------------------------------
# read-only "find" steps (no writes; safe at any point in a sequence)
# ---------------------------------------------------------------------------

def read_inode(dev, inum):
    raw = dev.read(INODESTART + inum // IPB)
    off = (inum % IPB) * 64
    return Dinode.unpack(raw[off:off + 64])


def find_free_inode(dev):
    """The lowest free inode number (type == 0), starting at 1."""
    for inum in range(1, NINODES):
        if read_inode(dev, inum).type == 0:
            return inum
    raise RuntimeError("no free inode")


def find_free_blocks(dev, k):
    """The k lowest data blocks the bitmap marks free (it does NOT mark them:
    that is a separate write, and where it falls in your sequence is Part 2's
    entire subject)."""
    bmap = dev.read(BMAPSTART)
    out = []
    for b in range(FIRSTDATA, FSSIZE):
        if not (bmap[b // 8] >> (b % 8)) & 1:
            out.append(b)
            if len(out) == k:
                return out
    raise RuntimeError("no free blocks")


# ---------------------------------------------------------------------------
# single-block "write" steps (each issues exactly one dev.write)
# ---------------------------------------------------------------------------

def write_inode(dev, inum, dino):
    """Install inode `inum` (one write of its inode block)."""
    bno = INODESTART + inum // IPB
    raw = bytearray(dev.read(bno))
    off = (inum % IPB) * 64
    raw[off:off + 64] = dino.pack()
    dev.write(bno, bytes(raw))


def bitmap_mark(dev, bnos):
    """Mark every block in `bnos` in use (one write of the bitmap block)."""
    raw = bytearray(dev.read(BMAPSTART))
    for b in bnos:
        raw[b // 8] |= 1 << (b % 8)
    dev.write(BMAPSTART, bytes(raw))


def dirent_add(dev, name, inum):
    """Add `name -> inum` to the root directory (one write of block 13).
    The root's size is already a whole block, so no inode update is needed."""
    root = read_inode(dev, ROOTINO)
    bno = root.addrs[0]
    raw = bytearray(dev.read(bno))
    nb = name.encode()
    assert len(nb) <= DIRSIZ
    for off in range(0, BSIZE, 16):
        ent_inum = struct.unpack_from("<H", raw, off)[0]
        if ent_inum == 0:
            raw[off:off + 16] = struct.pack(DIRENT_FMT, inum, nb)
            dev.write(bno, bytes(raw))
            return
    raise RuntimeError("root directory full")


# ---------------------------------------------------------------------------
# an offline reader, for the sweep driver and your own debugging.
# (The autograder carries its own independent copy of this logic.)
# ---------------------------------------------------------------------------

class ImageReader:
    """Read a raw image FILE (not a device): list the root's entries, pull a
    file's contents, peek at the log header."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()

    def block(self, bno):
        return self.data[bno * BSIZE:(bno + 1) * BSIZE]

    def inode(self, inum):
        raw = self.block(INODESTART + inum // IPB)
        off = (inum % IPB) * 64
        return Dinode.unpack(raw[off:off + 64])

    def root_entries(self):
        """[(name, inum), ...] for every live entry in the root directory."""
        root = self.inode(ROOTINO)
        out = []
        for fbn in range(min(root.size, NDIRECT * BSIZE) // BSIZE):
            bno = root.addrs[fbn]
            if bno == 0:
                continue
            raw = self.block(bno)
            for off in range(0, BSIZE, 16):
                inum, name = struct.unpack_from(DIRENT_FMT, raw, off)
                if inum != 0:
                    out.append((name.rstrip(b"\0").decode(), inum))
        return out

    def file_bytes(self, inum):
        """A file's contents via its direct blocks (this lab's files are small)."""
        ino = self.inode(inum)
        out = b""
        left = ino.size
        for fbn in range((ino.size + BSIZE - 1) // BSIZE):
            bno = ino.addrs[fbn] if fbn < NDIRECT else 0
            chunk = self.block(bno) if bno else zero_block()
            out += chunk[:min(left, BSIZE)]
            left -= min(left, BSIZE)
        return out

    def log_header(self):
        """-> (magic_ok, seq, n, dests) of the on-image log header."""
        return parse_log_header(self.block(LOGSTART))
