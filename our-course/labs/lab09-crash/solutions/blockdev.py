#!/usr/bin/env python3
# blockdev.py -- Lab 9 Part 1: a crashable simulated block device.  REFERENCE.
#
# The model (fixed by the handout):
#
#   * The device is backed by a file of BSIZE-byte blocks: the "platter".
#     A write is DURABLE only once it has been INSTALLED into that file.
#   * fifo mode: every write installs immediately, in issue order.
#   * reorder mode: writes queue in memory and install ONLY at a barrier().
#     The drain installs the queue in REVERSE issue order -- the most
#     adversarial schedule a no-ordering-guarantee disk may legally choose,
#     and deterministic, so a missing barrier is punished at the same crash
#     point every run.  A second write to a block already queued replaces the
#     queued data in place (its queue position does not change).
#   * read() returns the queued data if the block is pending, else the
#     platter's -- a disk cache serves reads from writes it has not installed.
#   * crash_after=N: the device performs at most N installs; the attempt to
#     install number N+1 raises Crash instead.  Anything still queued is lost.
#   * crash_now(): the queue is discarded and Crash raised immediately.
#
# Counters: installs (durable block writes) and barriers, for Part 5.

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

    # -- the durable side ---------------------------------------------------

    def _install(self, bno, data):
        if self.crash_after is not None and self.installs >= self.crash_after:
            raise Crash(self.installs)
        self.f.seek(bno * BSIZE)
        self.f.write(data)
        self.f.flush()
        self.installs += 1

    # -- the device interface ------------------------------------------------

    def read(self, bno):
        for qbno, qdata in reversed(self.queue):
            if qbno == bno:
                return qdata
        self.f.seek(bno * BSIZE)
        return self.f.read(BSIZE)

    def write(self, bno, data):
        assert len(data) == BSIZE
        if self.mode == "fifo":
            self._install(bno, data)
            return
        for i, (qbno, _) in enumerate(self.queue):
            if qbno == bno:
                self.queue[i] = (bno, data)     # coalesce, keep position
                return
        self.queue.append((bno, data))

    write_data = write

    def barrier(self):
        self.barriers += 1
        pending, self.queue = self.queue, []
        for bno, data in reversed(pending):     # adversarial LIFO drain
            self._install(bno, data)

    def crash_now(self):
        self.queue = []
        raise Crash(self.installs)

    def close(self):
        self.f.close()


# ---------------------------------------------------------------------------
# the command-line interface (a fixed contract; see the handout)
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
