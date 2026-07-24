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
// Copy-on-write fork() maps a parent's page into the child instead of copying
// it, so a page can now have more than one page table pointing at it. The
// count says how many, and kfree() actually returns a page to the free list
// only when the last reference goes -- so a decrement that is skipped leaks
// the page, and one done twice frees it while a process is still using it.
//
// The array is indexed by physical page number relative to KERNBASE, so it
// only has to cover the RAM the allocator manages, from KERNBASE up to
// PHYSTOP. (PHYSTOP - KERNBASE) / PGSIZE such pages exist.
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
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    // Hand each page to kfree() as if it held exactly one reference, so the
    // kfree() below decrements it to zero and puts it on the free list.
    kref.count[PA2IDX(p)] = 1;
    kfree(p);
  }
}

// Drop one reference to the page of physical memory pointed at by pa. When
// the last reference goes, the page (which normally should have been returned
// by a call to kalloc()) is returned to the free list.
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kref.lock);
  if(kref.count[PA2IDX(pa)] < 1)
    panic("kfree: refcount underflow");
  if(--kref.count[PA2IDX(pa)] > 0){
    // Other page tables still map this page; it is not free yet.
    release(&kref.lock);
    return;
  }
  release(&kref.lock);

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

  if(r){
    // A freshly allocated page has exactly one reference: this one.
    acquire(&kref.lock);
    kref.count[PA2IDX(r)] = 1;
    release(&kref.lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}

// Add a reference to an already-allocated page. Called by uvmcopy() when a
// copy-on-write fork() maps a parent's page into a child.
void
krefinc(void *pa)
{
  acquire(&kref.lock);
  if(kref.count[PA2IDX(pa)] < 1)
    panic("krefinc: page not allocated");
  kref.count[PA2IDX(pa)]++;
  release(&kref.lock);
}
