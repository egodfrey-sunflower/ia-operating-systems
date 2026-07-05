// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Lab 4 Task 3: a reference count per physical page, so that a frame shared
// by several page tables (copy-on-write fork) is only returned to the
// freelist once the last reference is dropped. Indexed by physical page
// number relative to KERNBASE; guarded by its own lock.
#define NPHYPAGES ((PHYSTOP - KERNBASE) / PGSIZE)
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[NPHYPAGES];
} kref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");
  freerange(end, (void *)PHYSTOP);
}

// Increment the reference count of the page containing pa.
// Called when a frame gains a new sharer (copy-on-write fork).
void
krefinc(void *pa)
{
  acquire(&kref.lock);
  if (kref.count[PA2IDX(pa)] < 1)
    panic("krefinc");
  kref.count[PA2IDX(pa)]++;
  release(&kref.lock);
}

// Read the current reference count of the page containing pa.
int
krefcount(void *pa)
{
  int n;
  acquire(&kref.lock);
  n = kref.count[PA2IDX(pa)];
  release(&kref.lock);
  return n;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    // Pretend there is one outstanding reference so the kfree below drops it
    // to zero and puts the page on the freelist.
    kref.count[PA2IDX(p)] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Drop one reference. Only actually free the frame once no page table
  // refers to it any more.
  acquire(&kref.lock);
  if (kref.count[PA2IDX(pa)] < 1)
    panic("kfree: ref underflow");
  kref.count[PA2IDX(pa)]--;
  if (kref.count[PA2IDX(pa)] > 0) {
    release(&kref.lock);
    return;
  }
  release(&kref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    memset((char *)r, 5, PGSIZE); // fill with junk
    // A freshly allocated frame has exactly one owner.
    acquire(&kref.lock);
    kref.count[PA2IDX(r)] = 1;
    release(&kref.lock);
  }
  return (void *)r;
}
