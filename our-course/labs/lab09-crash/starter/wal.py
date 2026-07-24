# wal.py -- Lab 9 Parts 3 & 5: write-ahead logging with recovery.  STARTER.
#
# A Journal wraps a BlockDev and presents the SAME four-call device interface
# (read / write / write_data / barrier), so the given xv6fs structure writers
# run through it unchanged.  Your job:
#
#   * begin_op/end_op: inside a transaction nothing touches the disk -- writes
#     buffer in memory, and reads must see the buffered data (the structure
#     writers read-modify-write shared blocks).  end_op plays the update out
#     under the write-ahead protocol, whose three ordering rules ch. 42 gives:
#
#       1. the log copies of the blocks are durable BEFORE the commit record;
#       2. the commit record is durable BEFORE any block is checkpointed to
#          its real location;
#       3. the checkpoint is durable BEFORE the log entry is freed.
#
#     "Durable" is a claim about the DEVICE, and only barrier() makes one.
#     The log's on-disk layout (header block + slots, and the header's exact
#     byte format) is fixed by the handout and by xv6fs.pack_log_header /
#     parse_log_header; the commit record IS the header with n > 0, and the
#     model assumes that single-block write is atomic.
#
#   * recover(dev): mount-time recovery.  If the header holds a committed
#     transaction, replay it and free the log.  Recovery must be safe to
#     crash at ANY of its own installs and simply run again -- the campaign's
#     recovery sweep crashes it at every one.
#
#   * mode="ordered" (Part 5): journal metadata only.  write_data sends file
#     content STRAIGHT to the device, and end_op must make it durable before
#     anything of the transaction is journaled.
#
#   * misorder="commit-first" (Part 4): the deliberate violation -- issue the
#     commit record BEFORE the log blocks, nothing between them.  Keep it a
#     two-line change; you will run the campaign against it and explain the
#     crash point it corrupts at.
#
# The STUB below is a null journal: it passes every write straight through
# and recovery does nothing.  `journal workload` therefore behaves like
# Part 2 -- run `sweep data` on it and watch the campaign fail before you
# make it pass.

import xv6fs


class Journal:
    def __init__(self, dev, mode="data", misorder="none"):
        self.dev = dev
        self.mode = mode
        self.misorder = misorder
        ok, seq, _n, _d = xv6fs.parse_log_header(dev.read(xv6fs.LOGSTART))
        self.seq = seq if ok else 0

    # -- the device interface, transactional --------------------------------

    def read(self, bno):
        # TODO (Part 3): a transaction must see its own buffered writes.
        return self.dev.read(bno)

    def write(self, bno, data):
        # TODO (Part 3): buffer, do not touch the device.  (At most
        # xv6fs.LOGCAP distinct blocks fit in one transaction.)
        self.dev.write(bno, data)

    def write_data(self, bno, data):
        # TODO (Part 5): in ordered mode this is NOT journaled.
        self.write(bno, data)

    def barrier(self):
        pass                            # ordering is end_op's job

    # -- transactions --------------------------------------------------------

    def begin_op(self):
        pass                            # TODO (Part 3)

    def end_op(self):
        pass                            # TODO (Part 3): the write-ahead
                                        # protocol, rules 1-3 above


def recover(dev):
    """Mount-time recovery.  Returns (replayed, seq): replayed == 0 means the
    log held no committed transaction.  TODO (Part 3)."""
    ok, seq, _n, _d = xv6fs.parse_log_header(dev.read(xv6fs.LOGSTART))
    return 0, (seq if ok else 0)
