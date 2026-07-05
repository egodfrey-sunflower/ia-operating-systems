#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGBLOCKS];
};

struct log {
  struct spinlock lock;
  int start;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  int ncommit;
  struct logheader lh;
};
struct log log;

// ---------------------------------------------------------------------------
// Lab 6, Task 3: crash-injection mechanism (CRASHPOINT).
//
// This lets the autograder induce a power-loss simulation at a precisely
// chosen instant inside commit(). It is compiled OUT entirely unless the
// kernel is built with a crashpoint selected on the make command line:
//
//     make CRASH=BEFORE_HEAD   -> defines CRASHPOINT_BEFORE_HEAD
//     make CRASH=AFTER_HEAD    -> defines CRASHPOINT_AFTER_HEAD
//
// When no crashpoint is compiled in (the normal build) none of this code
// exists, so it is completely harmless. Even when a crashpoint IS compiled
// in, nothing happens until a process ARMS it with the crashnow(1) syscall.
// Once armed, the next real commit (log.lh.n > 0) freezes the kernel at the
// chosen point, having issued to disk exactly the writes that precede that
// point -- so a subsequent reboot on the same disk image sees precisely the
// state a real crash at that instant would have left behind.
//
// crash_armed is a plain global; the whole experiment runs single-CPU and
// single-threaded, so no locking is needed for it.
static int crash_armed = 0;

// sys_crashnow(enable): arm (enable != 0) or disarm the crashpoint.
// Always returns 0. On a kernel built without a crashpoint this simply
// toggles a flag that nothing ever reads.
uint64
sys_crashnow(void)
{
  int enable;
  argint(0, &enable);
  crash_armed = (enable != 0);
  return 0;
}

#if defined(CRASHPOINT_BEFORE_HEAD) || defined(CRASHPOINT_AFTER_HEAD)
// Print a recognisable marker and freeze. panic() spins forever without
// issuing any further disk I/O, so the on-disk image is left frozen at
// exactly this instant -- which is what the autograder captures by killing
// QEMU once it sees the marker.
static void
crashpoint(char *where)
{
  printk("CRASHPOINT: %s reached; simulating power loss\n", where);
  panic("crashpoint");
}
#endif
// ---------------------------------------------------------------------------

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    if (recovering) {
      printk("recovering tail %d dst %d\n", tail, log.lh.block[tail]);
    }
    struct buf *lbuf = bread(log.dev, log.start + tail + 1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);   // read dst
    memmove(dbuf->data, lbuf->data, BSIZE); // copy block to dst
    bwrite(dbuf);                           // write dst to disk
    if (recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)(buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *)(buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while (1) {
    if (log.committing) {
      sleep(&log, &log.lock);
    } else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGBLOCKS) {
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if (log.committing)
    panic("log.committing");
  if (log.outstanding == 0) {
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if (do_commit) {
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    log.ncommit += 1;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start + tail + 1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to); // write the log
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();  // Write modified blocks from cache to log
#ifdef CRASHPOINT_BEFORE_HEAD
    // Power loss AFTER the log blocks are on disk but BEFORE the header
    // count is committed. The transaction is NOT yet durable.
    if (crash_armed)
      crashpoint("BEFORE_HEAD");
#endif
    write_head(); // Write header to disk -- the real commit
#ifdef CRASHPOINT_AFTER_HEAD
    // Power loss AFTER the header count is committed but BEFORE the blocks
    // are installed at their home locations. The transaction IS durable and
    // recovery must replay it.
    if (crash_armed)
      crashpoint("AFTER_HEAD");
#endif
    install_trans(0); // Now install writes to home locations
    log.lh.n = 0;
    write_head(); // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGBLOCKS)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno) // log absorption
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) { // Add new block to log?
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}

uint64
sys_sync(void)
{
  acquire(&log.lock);
  if (log.committing || log.outstanding > 0) {
    int n = log.ncommit + 1;
    while (log.ncommit < n) {
      sleep(&log, &log.lock);
    }
  }
  release(&log.lock);
  return 0;
}
