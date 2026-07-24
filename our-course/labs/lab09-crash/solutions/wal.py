# wal.py -- Lab 9 Parts 3 & 5: write-ahead logging with recovery.  REFERENCE.
#
# A Journal wraps a BlockDev and presents the same four-call device interface
# (read / write / write_data / barrier), so the given xv6fs structure writers
# run through it unchanged.  Inside a transaction nothing touches the disk:
# writes buffer in memory (reads see them -- read-your-writes), and end_op
# plays the whole update out under the write-ahead protocol:
#
#   data journaling (mode=data; write_data buffers like write):
#     1. write each buffered block to a log slot        (n installs)
#     2. barrier                -- the log must be durable before the commit
#     3. write the commit record (header, n > 0)        (1 install)
#     4. barrier                -- the commit must be durable before checkpoint
#     5. write each block to its real location          (n installs)
#     6. barrier                -- checkpoint durable before the log is freed
#     7. write the header again with n = 0              (1 install)
#     8. barrier
#   => 2n + 2 installs, 4 barriers per transaction.
#
#   ordered journaling (mode=ordered): write_data goes STRAIGHT to the device
#   inside the transaction, and end_op prepends:
#     0. barrier                -- data durable before anything is journaled
#   then runs steps 1-8 over the metadata blocks only.
#   => (2*3 + 2) + d installs, 5 barriers per create (3 metadata blocks).
#
# misorder="commit-first" is Part 4's deliberate violation: the commit record
# is issued BEFORE the log blocks, with no barrier between them.
#
# recover(dev) runs at mount: if the header holds a committed transaction
# (n > 0), replay every slot to its destination, then clear the header.  The
# replays land before the clear (a barrier between), so recovery may itself
# crash at any point and simply run again.

import xv6fs


class Journal:
    def __init__(self, dev, mode="data", misorder="none"):
        self.dev = dev
        self.mode = mode
        self.misorder = misorder
        ok, seq, _n, _d = xv6fs.parse_log_header(dev.read(xv6fs.LOGSTART))
        self.seq = seq if ok else 0
        self.in_op = False
        self.buf = {}                   # bno -> data
        self.order = []                 # bnos in first-write order
        self.wrote_data_direct = False

    # -- the device interface, transactional --------------------------------

    def read(self, bno):
        if bno in self.buf:
            return self.buf[bno]
        return self.dev.read(bno)

    def write(self, bno, data):
        assert self.in_op, "write outside begin_op/end_op"
        if bno not in self.buf:
            assert len(self.order) < xv6fs.LOGCAP, "transaction too big"
            self.order.append(bno)
        self.buf[bno] = data

    def write_data(self, bno, data):
        if self.mode == "ordered":
            assert self.in_op
            self.dev.write(bno, data)   # in place, before the commit
            self.wrote_data_direct = True
        else:
            self.write(bno, data)

    def barrier(self):
        pass                            # ordering is end_op's job

    # -- transactions --------------------------------------------------------

    def begin_op(self):
        assert not self.in_op
        self.in_op = True
        self.buf = {}
        self.order = []
        self.wrote_data_direct = False

    def end_op(self):
        assert self.in_op
        dev, ls = self.dev, xv6fs.LOGSTART
        n = len(self.order)
        self.seq += 1

        if self.mode == "ordered" and self.wrote_data_direct:
            dev.barrier()               # step 0: data before the journal

        commit = xv6fs.pack_log_header(self.seq, self.order)
        if self.misorder == "commit-first":
            dev.write(ls, commit)       # THE VIOLATION: commit before the log
            for k, bno in enumerate(self.order):
                dev.write(ls + 1 + k, self.buf[bno])
            dev.barrier()
        else:
            for k, bno in enumerate(self.order):        # step 1: the log
                dev.write(ls + 1 + k, self.buf[bno])
            dev.barrier()                               # step 2
            dev.write(ls, commit)                       # step 3: the commit
            dev.barrier()                               # step 4

        for bno in self.order:                          # step 5: checkpoint
            dev.write(bno, self.buf[bno])
        dev.barrier()                                   # step 6
        dev.write(ls, xv6fs.pack_log_header(self.seq, []))   # step 7: free
        dev.barrier()                                   # step 8

        self.in_op = False
        self.buf = {}
        self.order = []


def recover(dev):
    """Mount-time recovery.  Returns (replayed, seq): replayed == 0 means the
    log held no committed transaction."""
    ls = xv6fs.LOGSTART
    ok, seq, n, dests = xv6fs.parse_log_header(dev.read(ls))
    if not ok or n == 0:
        return 0, seq
    for k, bno in enumerate(dests):
        dev.write(bno, dev.read(ls + 1 + k))
    dev.barrier()
    dev.write(ls, xv6fs.pack_log_header(seq, []))
    dev.barrier()
    return n, seq
