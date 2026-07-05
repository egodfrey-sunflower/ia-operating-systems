# Midterm 2 — Operating Systems

**Coverage:** weeks 1–11, with emphasis on 7–11 (memory, virtual memory, I/O,
file systems). One part reaches back to earlier material.
**Time:** 90 minutes. **Closed book.**
**Answer THREE of the four questions.** Each question is worth 20 marks.
Marks for each part are shown in brackets. Show your working on all
calculations.

---

## Question 1 — Address translation

An embedded RISC machine gives each process a byte-addressable **30-bit**
virtual address space (1 GiB), with a page size of **4 KiB** and a **two-level**
page table. Each page-table entry (PTE) is **8 bytes**. The virtual address is
split so that a single page table exactly fills one page.

*(a)* State the width in bits of the page offset, and of each of the two page
table indices, and confirm that one page table exactly fills a 4 KiB page.
Briefly explain what a TLB is and why the design just described would be
unusably slow without one. **[4 marks]**

*(b)* (i) Translate the virtual address **0x004051C0**. Give the level-1 index,
the level-2 index, and the offset. If the relevant level-2 PTE holds physical
page number **0x2F3**, give the full physical address. **[4 marks]**

  (ii) A process has exactly three mapped regions:

  - 3 MiB of contiguous text + data + heap starting at virtual address 0;
  - a 1 MiB shared library mapped starting at virtual address 0x10000000;
  - a 2-page (8 KiB) stack occupying the top of the address space, ending at
    the highest virtual address.

  Counting the top-level table and every level-2 table that must exist, how many
  4 KiB pages of physical memory does this process's page table occupy in total,
  and how many bytes is that? Compare with the space a **single-level** page
  table would need for the same 30-bit address space, and comment on why the
  two-level design wins here. **[4 marks]**

  (iii) A TLB hit costs one memory access (80 ns). On a TLB miss the hardware
  walks both levels of the table (two extra memory accesses) and then makes the
  access itself. With a TLB hit rate of 95%, compute the effective memory access
  time. **[2 marks]**

*(c)* The same machine has **512 MiB** of physical RAM. An **inverted** page
table would replace the per-process forward tables with a single system-wide
table.

  (i) How many entries would the inverted table have, and (at 8 bytes per entry)
  how large would it be? Note that, unlike the forward tables, this size does not
  grow with the number of processes. **[3 marks]**

  (ii) State **one** advantage and **one** significant disadvantage of the
  inverted table compared with the two-level forward table, and name the
  mechanism used to make the disadvantage tolerable. **[3 marks]**

---

## Question 2 — Page replacement

A process is given a fixed number of physical frames and issues the page
reference string:

```
2  3  4  1  4  2  3  5  2  3  4  1  5
```

Assume all frames start empty (every first touch of a page is a fault).

*(a)* Describe demand paging and list, in order, the steps the operating system
takes to service a page fault when a free frame **is** available. **[4 marks]**

*(b)* (i) Run the reference string through the **FIFO** replacement policy with
**3** frames and again with **4** frames. Show the frame contents after each
reference and count the page faults in each case. **[4 marks]**

  (ii) Your two counts should show that giving FIFO *more* memory made
  performance *worse*. Name this effect, and run **LRU** on the same string at
  3 and 4 frames to show that LRU does not suffer from it. Explain the property
  of LRU (which FIFO lacks) that guarantees this. **[4 marks]**

*(c)* Define a process's **working set**, and explain what **thrashing** is, how
it arises when the degree of multiprogramming is too high, and one action the
operating system can take in response. **[4 marks]**

*(d)* In Lab 4 you made `fork()` use **copy-on-write** (COW): parent and child
share physical frames read-only until one writes, at which point the writer
faults and gets a private copy. Explain how COW-sharing changes the *reference
behaviour* the replacement policy sees, and why the frame reference counts that
COW requires must be consulted before the replacement policy is allowed to evict
a shared frame. **[4 marks]**

---

## Question 3 — Input / output

*(a)* Contrast the three principal ways a CPU can transfer data to and from a
device: **programmed I/O with polling**, **interrupt-driven I/O**, and **DMA**.
For each, state who does the copying of the data and what the CPU is doing while
the transfer is in progress. **[4 marks]**

*(b)* (i) Write short pseudo-code for the device-driver read path under (1)
polling and (2) interrupts, making clear where the CPU busy-waits versus where
it blocks and is later woken. **[4 marks]**

  (ii) Interrupts are usually assumed to be more efficient than polling, yet
  real high-performance drivers sometimes poll. Give **two** distinct situations
  in which polling is actually the faster choice, and explain each. **[2 marks]**

*(c)* A storage device is accessed in 4 KiB blocks. Two devices are available:

  - a **hard disk**: average seek time 6 ms; rotational speed 10 000 RPM;
    sustained transfer rate 150 MB/s (1 MB = 10⁶ bytes);
  - an **SSD**: read latency 60 µs; sustained transfer rate 1 GB/s
    (10⁹ bytes/s).

  (i) Compute the time to read **one random 4 KiB block** from each device, and
  hence the random-read throughput of each in IOPS (I/O operations per second).
  For the disk, include the average rotational latency. **[4 marks]**

  (ii) A workload reads 1 MiB (256 blocks). Compare the time on the hard disk if
  the 256 blocks are (A) contiguous versus (B) scattered at random, and state in
  one sentence why the SSD's contiguous-versus-random gap is far smaller.
  **[2 marks]**

*(d)* Explain what a **buffer** in the kernel I/O path is for, and give **two**
distinct reasons the kernel copies device data through a buffer rather than
straight into the user's supplied address. Then explain the difference between
**read-ahead** and **write-behind** buffering, and state one benefit and one
risk of write-behind. **[4 marks]**

---

## Question 4 — File systems

A Unix-style file system stores each file's metadata in an **inode**. On the
system below, a disk block is **512 bytes** and each block pointer is **4
bytes**, so one indirect block holds 128 pointers. The stock inode has **9
direct** block pointers and **1 singly-indirect** pointer.

*(a)* State what information an inode holds (other than the block pointers), and
explain how a directory maps a filename to the file's inode — i.e. what a
directory *is*, on disk. **[4 marks]**

*(b)* (i) Compute the maximum file size, in blocks and in bytes, that the stock
inode (9 direct + 1 singly-indirect) can address. **[3 marks]**

  (ii) An inode design of the kind you rebalance in Lab 6 — scaled down here —
  instead uses **8 direct + 1 singly-indirect + 1 doubly-indirect**, keeping
  the same total number of pointer slots (so the on-disk inode size is
  unchanged). Compute the new maximum file size in blocks and in bytes.
  **[3 marks]**

  (iii) To read one data byte that lives in the **doubly-indirect** region,
  how many disk block reads are needed in the worst case (assume the inode is
  already in memory and nothing is cached)? Explain why the **first write** to a
  previously-unallocated block in that region can cost several more block I/Os
  than a read of an already-allocated one. **[2 marks]**

*(c)* Distinguish a **hard link** from a **symbolic (soft) link**. For each of
the following, state how the two differ and why: (i) where the name-to-inode
association is stored; (ii) whether the link can point at a file on a different
disk (file system); (iii) what happens to the link when the file it names is
deleted. **[4 marks]**

*(d)* A student proposes simplifying the file system: "The separate inode table
is pointless indirection. Store each file's metadata — size, permissions,
timestamps, and the block pointers — **inside its directory entry**, the way
CP/M and FAT do. One disk read then fetches the name *and* everything needed to
open the file."

  Critique the proposal. State **one** thing it genuinely simplifies, then
  identify **at least three** distinct things it breaks or complicates in a
  Unix-style system (consider: the hard links of part (c) and the link count;
  `fstat` or `read` on a file that is still open when its last name is
  `unlink`ed; renaming/moving a file to a different directory). Conclude by
  stating what the V7 split — a directory entry is *only* (name, inode
  number) — buys. **[4 marks]**

---

*End of Midterm 2.*
