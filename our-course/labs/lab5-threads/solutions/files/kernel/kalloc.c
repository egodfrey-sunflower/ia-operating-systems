// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
//
// REFERENCE SOLUTION (Lab 5, Part B, task 1): per-CPU free lists.
//
// Each CPU gets its own free list and its own lock, so the common case --
// a CPU allocating/freeing its own pages -- never touches another CPU's lock
// and so never contends. When a CPU's list is empty it STEALS a batch of
// pages from another CPU's list (taking that CPU's lock only briefly). This
// is the only cross-CPU locking, and it is rare once each CPU has warmed up
// its own pool, so kalloctest's "kmem" contended-acquire count stays tiny.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// How many pages to grab in one go when stealing from another CPU. Stealing a
// batch (rather than a single page) keeps the number of cross-CPU lock
// acquisitions -- and thus contention -- low.
#define STEAL_BATCH 64

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// One free list + lock per CPU.
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  // freerange runs on the booting CPU, so all pages start on that CPU's list.
  // Other CPUs will steal from it as they warm up.
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
//
// The page goes onto the CURRENT CPU's free list.
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  push_off(); // pin us to this CPU while we read cpuid()
  int c = cpuid();
  acquire(&kmem[c].lock);
  r->next = kmem[c].freelist;
  kmem[c].freelist = r;
  release(&kmem[c].lock);
  pop_off();
}

// Try to steal up to STEAL_BATCH pages from some other CPU's list onto our
// own (list c). Returns a page to hand back to the caller, or 0 if every
// other list was also empty. Must be called with interrupts off (so c is
// stable); acquires only one other CPU's lock at a time (never two at once),
// so there is no lock-ordering deadlock.
static struct run *
ksteal(int c)
{
  for (int i = 0; i < NCPU; i++) {
    if (i == c)
      continue;
    acquire(&kmem[i].lock);
    struct run *victim = kmem[i].freelist;
    if (victim == 0) {
      release(&kmem[i].lock);
      continue;
    }
    // Unlink up to STEAL_BATCH pages from CPU i's list.
    struct run *head = victim, *tail = victim;
    int got = 1;
    while (got < STEAL_BATCH && tail->next) {
      tail = tail->next;
      got++;
    }
    kmem[i].freelist = tail->next;
    tail->next = 0;
    release(&kmem[i].lock);

    // Keep the first page for the caller; splice the rest onto our own list.
    struct run *r = head;
    struct run *rest = head->next;
    if (rest) {
      acquire(&kmem[c].lock);
      tail->next = kmem[c].freelist;
      kmem[c].freelist = rest;
      release(&kmem[c].lock);
    }
    return r;
  }
  return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off(); // pin us to this CPU while we read cpuid()
  int c = cpuid();

  acquire(&kmem[c].lock);
  r = kmem[c].freelist;
  if (r)
    kmem[c].freelist = r->next;
  release(&kmem[c].lock);

  if (r == 0) // our list was empty: steal from another CPU
    r = ksteal(c);

  pop_off();

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// Report cumulative acquire/contended-acquire counts summed across every
// CPU's kmem lock, for the statistics() system call.
void
kmem_lockstat(uint64 *n, uint64 *nts)
{
  uint64 tn = 0, tnts = 0;
  for (int i = 0; i < NCPU; i++) {
    tn += kmem[i].lock.n;
    tnts += kmem[i].lock.nts;
  }
  *n = tn;
  *nts = tnts;
}
