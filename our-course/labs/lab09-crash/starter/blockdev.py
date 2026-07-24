#!/usr/bin/env python3
# blockdev.py -- Lab 9 Part 1: a crashable simulated block device.  STARTER.
#
# The model you must implement (the handout states it in full; every line of
# it is a requirement the autograder drives scripts against):
#
#   * The device is backed by a file of BSIZE-byte blocks: the "platter".
#     A write is DURABLE only once it has been INSTALLED into that file.
#   * fifo mode: every write installs immediately, in issue order.
#   * reorder mode: writes queue in memory and install ONLY at a barrier().
#     The drain installs the queue in REVERSE issue order -- the most
#     adversarial schedule a no-ordering-guarantee disk may legally choose,
#     and deterministic, so every run crashes the same way.  A second write to
#     a block already queued replaces the queued data in place (the entry
#     keeps its queue position).
#   * read() returns the queued data if the block is pending, else the
#     platter's -- a disk cache serves reads from writes it has not installed.
#   * crash_after=N: the device performs at most N installs; the attempt to
#     install number N+1 raises Crash instead.  Anything still queued is lost.
#   * crash_now(): the queue is discarded and Crash raised immediately.
#
# Counters: `installs` (durable block writes) and `barriers`.  Part 5 reads
# them, and so does the autograder.
#
# The STUB below installs every write immediately and never crashes -- the
# file plumbing is done for you, the semantics above are not.  The CLI at the
# bottom (a fixed output contract) is given and complete.

import sys

BSIZE = 1024


class Crash(Exception):
    """The simulated power failure. Tools catch it, print CRASH, and exit 3."""


class BlockDev:
    def __init__(self, path, nblocks=None, mode="fifo", crash_after=None):
        self.path = path
        self.mode = mode
        self.crash_after = crash_after
        self.installs = 0
        self.barriers = 0
        self.queue = []                 # [(bno, data)] in issue order
        if nblocks is not None:
            with open(path, "wb") as f:
                f.write(bytes(nblocks * BSIZE))
        self.f = open(path, "r+b")

    def _install(self, bno, data):
        """Make one write durable.  TODO (Part 1): enforce crash_after --
        this stub never crashes."""
        self.f.seek(bno * BSIZE)
        self.f.write(data)
        self.f.flush()
        self.installs += 1

    def read(self, bno):
        """TODO (Part 1): pending writes must be visible to reads."""
        self.f.seek(bno * BSIZE)
        return self.f.read(BSIZE)

    def write(self, bno, data):
        """TODO (Part 1): only fifo mode installs immediately."""
        assert len(data) == BSIZE
        self._install(bno, data)

    write_data = write                  # a data write IS a write, to a device

    def barrier(self):
        """TODO (Part 1): in reorder mode, this is where installs happen."""
        self.barriers += 1

    def crash_now(self):
        """TODO (Part 1)."""
        raise Crash(self.installs)

    def close(self):
        self.f.close()


# ---------------------------------------------------------------------------
# the command-line interface.  GIVEN, complete: every output line is a fixed
# contract the autograder parses.  Do not change what the lines say.
# ---------------------------------------------------------------------------

def main(argv):
    if len(argv) < 2 or len(argv) > 3:
        sys.stderr.write("usage: blockdev.py <image> [script|-]\n")
        return 2
    path = argv[1]
    script = sys.stdin if len(argv) < 3 or argv[2] == "-" else open(argv[2])

    dev = None
    try:
        for raw in script:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            cmd, *args = line.split()
            if cmd == "init":
                dev = BlockDev(path, nblocks=int(args[0]))
                print("init nblocks=%s" % args[0])
            elif dev is None and cmd != "dump":
                dev = BlockDev(path)
            if cmd == "mode":
                dev.mode = args[0]
                print("mode %s" % args[0])
            elif cmd == "crashafter":
                dev.crash_after = int(args[0])
                print("crashafter %s" % args[0])
            elif cmd == "write":
                bno, byte = int(args[0]), int(args[1], 16)
                dev.write(bno, bytes([byte]) * BSIZE)
                print("write bno=%d byte=%02x" % (bno, byte))
            elif cmd == "read":
                data = dev.read(int(args[0]))
                print("read bno=%s byte=%02x" % (args[0], data[0]))
            elif cmd == "barrier":
                dev.barrier()
                print("barrier installs=%d" % dev.installs)
            elif cmd == "stats":
                print("stats installs=%d barriers=%d queued=%d"
                      % (dev.installs, dev.barriers, len(dev.queue)))
            elif cmd == "crashnow":
                dev.crash_now()
            elif cmd == "dump":
                with open(path, "rb") as f:
                    img = f.read()
                for bno in range(len(img) // BSIZE):
                    b = img[bno * BSIZE]
                    if b != 0:
                        print("dump bno=%d byte=%02x" % (bno, b))
                print("dump end")
    except Crash as c:
        print("CRASH installs=%s" % c.args[0])
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
