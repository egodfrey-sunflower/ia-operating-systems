#define SBRK_EAGER 1
#define SBRK_LAZY  2

// Lab 4, Part 4: copy-on-write marker.
//
// Bits 8 and 9 of a RISC-V Sv39 PTE are reserved for supervisor software
// (the "RSW" field) and are ignored by the MMU, so the kernel may use one of
// them to remember things the hardware does not care about. PTE_COW marks a
// page that fork() shared read-only between parent and child and that must be
// copied on the first write. It is NOT a hardware permission bit: a COW page
// has PTE_W clear (so a write faults) and PTE_COW set (so the fault handler
// knows to copy rather than to kill the process).
#define PTE_COW (1L << 8)
