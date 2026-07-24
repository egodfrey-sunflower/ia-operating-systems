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
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   ...
//   USYSCALL  (Lab 4 Part 2: read-only, holds this process's pid)
//   (two unmapped pages)
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// Lab 4, Part 2: a single read-only page mapped into every process at this
// fixed virtual address. The kernel writes the process's pid into it; a user
// program reads the pid straight out of it with ugetpid(), with no system
// call. It sits well above any heap the shell will grow.
//
// It is deliberately NOT the page directly below the trapframe. The kernel and
// some test programs probe the topmost few pages of the address space as
// addresses that MUST be inaccessible to user code, and a readable page among
// them would defeat that check; USYSCALL is placed below that band, at the
// fifth page down from the top.
#define USYSCALL (MAXVA - 5*PGSIZE)

// The layout of the USYSCALL page. Anything the kernel is willing to publish
// to a process without a trap into supervisor mode could live here; for this
// lab it is just the pid. Guarded because this header is also included by
// assembly (trampoline.S), which cannot parse a C struct.
#ifndef __ASSEMBLER__
struct usyscall {
  int pid;
};
#endif
