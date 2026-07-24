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

// ---------------------------------------------------------------------------
// Lab 4, Part 4: one reference count per physical page.
//
// Copy-on-write fork() maps one physical page into several page tables, so
// kfree() must return a page to the free list only when the LAST reference to
// it goes. Keep a count per page here.
//
// This declaration and its size are given -- deriving the bound from PHYSTOP
// and KERNBASE is arithmetic, not the point of the exercise. There are
// (PHYSTOP - KERNBASE) / PGSIZE physical pages the allocator manages; index the
// array by page number relative to KERNBASE.
//
// TODO (Part 4): use kref.count in kfree() (decrement; only really free at
//   zero), in kalloc() (a fresh page has exactly one reference), and in
//   krefinc() below (add a reference). Guard every access with kref.lock, as
//   with kmem.lock.
// ---------------------------------------------------------------------------
#define NRAMPAGES (((uint64)PHYSTOP - KERNBASE) / PGSIZE)
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[NRAMPAGES];
} kref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // TODO (Part 4): drop one reference to this page. Only when the last one
  //   goes should the page actually be filled with junk and pushed onto the
  //   free list below; while other page tables still map it, kfree() must
  //   return without freeing it.

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  // TODO (Part 4): a freshly allocated page has exactly one reference.
  return (void*)r;
}

// Add a reference to an already-allocated page. Called by uvmcopy() when a
// copy-on-write fork() maps a parent's page into a child.
void
krefinc(void *pa)
{
  // TODO (Part 4): increment this page's reference count under kref.lock.
  (void)pa;
}
