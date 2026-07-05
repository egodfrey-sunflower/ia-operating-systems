// Buffer cache.
//
// REFERENCE SOLUTION (Lab 5, Part B, task 2): bucketed (hash-table) cache.
//
// Instead of one lock protecting one global LRU list, the cache is split into
// NBUCKET hash buckets keyed by block number, each with its OWN lock. A hit
// (the common case) touches only one bucket lock, so lookups on different
// blocks rarely contend -- bcachetest's "bcache" contended-acquire count drops
// by orders of magnitude versus the stock single lock.
//
// Eviction (a miss) is the interesting part. To recycle a buffer we pick the
// least-recently-used refcnt==0 buffer across ALL buckets, using each buffer's
// `lastuse` timestamp. Two rules keep this deadlock-free:
//   1. During the scan we hold at most our current "best" bucket's lock plus
//      the bucket we are examining, and we only ever move to HIGHER bucket
//      indices -- so bucket locks are always taken in ascending order.
//   2. Before re-inserting into the target bucket we UNLINK the victim and
//      release its lock, so we never hold two bucket locks in descending
//      order. A re-check of the target bucket after taking its lock prevents
//      two CPUs from caching the same block twice.
//
// Interface (unchanged):
// * bread/bwrite/brelse/bpin/bunpin as before.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(blockno) ((blockno) % NBUCKET)

struct bucket {
  struct spinlock lock;
  struct buf head; // sentinel; head.next is first buf in the bucket
};

struct {
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET];
} bcache;

// Link b at the front of bucket bk's list. Caller holds bk->lock.
static void
blink(struct bucket *bk, struct buf *b)
{
  b->next = bk->head.next;
  b->prev = &bk->head;
  bk->head.next->prev = b;
  bk->head.next = b;
}

// Unlink b from whatever bucket list it is on. Caller holds that bucket's lock.
static void
bunlink(struct buf *b)
{
  b->next->prev = b->prev;
  b->prev->next = b->next;
}

void
binit(void)
{
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket[i].lock, "bcache");
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }
  // Spread the buffers across the buckets so eviction has candidates
  // everywhere at the start.
  for (int i = 0; i < NBUF; i++) {
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->lastuse = 0;
    blink(&bcache.bucket[i % NBUCKET], b);
  }
}

// Look through the buffer cache for block on device dev.
// If not found, allocate a buffer (recycling the global LRU free buffer).
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int h = HASH(blockno);
  struct bucket *hb = &bcache.bucket[h];

  // Fast path: is the block already cached in its bucket? A hit touches only
  // this one bucket's lock, so lookups on blocks in different buckets never
  // contend -- that is what keeps contention low versus the stock single lock.
  acquire(&hb->lock);
  for (b = hb->head.next; b != &hb->head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&hb->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&hb->lock);

  // Miss. Recycle the least-recently-used refcnt==0 buffer, searching all
  // buckets. We hold at most ONE bucket lock at a time (short holds keep
  // contention low and make deadlock impossible), so between scanning and
  // claiming a victim another CPU may grab or move it -- hence the retry loop
  // and the re-validation when we claim.
  for (;;) {
    struct buf *best = 0;
    int bestbkt = -1;
    uint bestuse = 0;

    for (int j = 0; j < NBUCKET; j++) {
      struct bucket *jb = &bcache.bucket[j];
      acquire(&jb->lock);
      for (b = jb->head.next; b != &jb->head; b = b->next) {
        if (b->refcnt == 0 && (best == 0 || b->lastuse < bestuse)) {
          best = b;
          bestbkt = j;
          bestuse = b->lastuse;
        }
      }
      release(&jb->lock); // short hold: release before moving on
    }

    if (best == 0)
      panic("bget: no buffers");

    // Claim the victim: re-lock its bucket and confirm it is STILL there and
    // still free (a concurrent hit or another miss may have taken/moved it in
    // the gap since we scanned). If not, rescan.
    struct bucket *vb = &bcache.bucket[bestbkt];
    acquire(&vb->lock);
    int usable = 0;
    for (b = vb->head.next; b != &vb->head; b = b->next) {
      if (b == best && b->refcnt == 0) {
        usable = 1;
        break;
      }
    }
    if (!usable) {
      release(&vb->lock);
      continue; // victim was taken; scan again
    }
    bunlink(best);
    release(&vb->lock);

    // Insert into the target bucket, re-checking for a concurrent insert of
    // the same block so we never cache a block twice.
    acquire(&hb->lock);
    for (b = hb->head.next; b != &hb->head; b = b->next) {
      if (b->dev == dev && b->blockno == blockno) {
        // Someone else cached it while we were scanning. Use theirs and hand
        // our unlinked buffer back to this bucket as a free buffer.
        b->refcnt++;
        best->refcnt = 0;
        blink(hb, best);
        release(&hb->lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    // No duplicate: repurpose best for (dev, blockno).
    best->dev = dev;
    best->blockno = blockno;
    best->valid = 0;
    best->refcnt = 1;
    blink(hb, best);
    release(&hb->lock);
    acquiresleep(&best->lock);
    return best;
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer. Record when it became free (for LRU).
void
brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  struct bucket *bk = &bcache.bucket[HASH(b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  if (b->refcnt == 0)
    b->lastuse = ticks; // approximate LRU timestamp (racy read is fine)
  release(&bk->lock);
}

void
bpin(struct buf *b)
{
  struct bucket *bk = &bcache.bucket[HASH(b->blockno)];
  acquire(&bk->lock);
  b->refcnt++;
  release(&bk->lock);
}

void
bunpin(struct buf *b)
{
  struct bucket *bk = &bcache.bucket[HASH(b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  release(&bk->lock);
}

// Report cumulative acquire/contended-acquire counts summed across every
// bucket lock, for the statistics() system call.
void
bcache_lockstat(uint64 *n, uint64 *nts)
{
  uint64 tn = 0, tnts = 0;
  for (int i = 0; i < NBUCKET; i++) {
    tn += bcache.bucket[i].lock.n;
    tnts += bcache.bucket[i].lock.nts;
  }
  *n = tn;
  *nts = tnts;
}
