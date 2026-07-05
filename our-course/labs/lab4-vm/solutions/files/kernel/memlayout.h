// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk
// 80000000 -- qemu's boot ROM loads the kernel here,
//             then jumps here.
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0     0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0     0x10001000
#define VIRTIO0_IRQ 1

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC                 0x0c000000L
#define PLIC_PRIORITY        (PLIC + 0x0)
#define PLIC_PENDING         (PLIC + 0x1000)
#define PLIC_SENABLE(hart)   (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart) * 0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP  (KERNBASE + 128 * 1024 * 1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// Lab 4 Task 2: a read-only page, mapped into every process below the
// trapframe, that the kernel keeps populated with per-process data. It lets
// user code read that data (here, the pid) with an ordinary load instead of
// a system call -- the same idea as a Linux vDSO. The kernel writes it; user
// space only ever reads it (mapped PTE_R | PTE_U, never PTE_W).
//
// Placement: NOT at MIT's canonical TRAPFRAME - PGSIZE. This fork's
// usertests `lazy_copy` asserts that read()/write() FAIL for every address
// from MAXVA - 4*PGSIZE (0x3fffffc000) upward; a user-readable page there
// makes copyin succeed and the test reports "write succeeded". So the page
// sits one page below that asserted range, at TRAPFRAME - 3*PGSIZE
// (0x3fffffb000). Nothing else lives near it: the heap is capped at
// TRAPFRAME by sys_sbrk/growproc but runs out of physical RAM (128 MB)
// long before reaching this address.
#define USYSCALL (TRAPFRAME - 3 * PGSIZE)

#ifndef __ASSEMBLER__
struct usyscall {
  int pid; // the owning process's pid
};
#endif
