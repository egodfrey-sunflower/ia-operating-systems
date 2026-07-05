> # ⚠️ SPOILER — MODEL ANSWERS AND MARK SCHEME ⚠️
> ## Do NOT read this until you have sat Midterm 2 under timed conditions.
> Complete solutions follow. Reading them first throws away your one honest
> measurement of where you stand. You have been warned.

---

# Midterm 2 — Solutions and Mark Scheme

Marking guidance as for Midterm 1: 1 mark ≈ one distinct correct point; full
method with an arithmetic slip keeps most of the calculation marks; a bare
number with no working earns at most half.

---

## Question 1 — Address translation

### (a) Splitting the address; the TLB [4 marks]

- Page size 4 KiB ⇒ **offset = 12 bits**. Remaining 30 − 12 = 18 bits split
  evenly across two levels ⇒ **9 bits per index**. **[2]**
- Each page table has 2⁹ = 512 entries × 8 bytes = 4096 bytes = exactly one
  4 KiB page. (This 9-bit/8-byte-PTE indexing is exactly one level of the
  RISC-V Sv39 scheme xv6 uses in Lab 4.) **[1]**
- A **TLB** is a small, fast associative cache of recent virtual-page →
  physical-frame translations. Without it, *every* user memory access would
  require walking both levels of the table — two extra memory reads per access,
  i.e. roughly a 3× slowdown on every load and store — which is why the TLB is
  essential. **[1]**

### (b) Translation and table space [10 marks]

**(i) Translate 0x004051C0 [4 marks]**
Split the 30-bit address as (9 bits)(9 bits)(12 bits):

- offset = low 12 bits = **0x1C0** (= 448)
- level-2 index = next 9 bits = **5**
- level-1 index = top 9 bits = **2**

(Check: 2×2²¹ + 5×2¹² + 0x1C0 = 4194304 + 20480 + 448 = 4215232 =
0x004051C0. ✓)
With the level-2 PTE holding PPN 0x2F3, the physical address is
(0x2F3 << 12) | 0x1C0 = 0x2F3000 + 0x1C0 = **0x002F31C0**. **[4]** — 1 per
index/offset, 1 for the physical address.

**(ii) Sparse page-table space [4 marks]**
Each level-1 slot maps a level-2 table covering 512 pages × 4 KiB = **2 MiB**
of contiguous virtual address space. Count the level-2 tables each region
forces to exist:

- 3 MiB at VA 0: spans [0, 3 MiB) → level-1 indices 0 (0–2 MiB) and 1
  (2–3 MiB) → **2** level-2 tables.
- 1 MiB shared library at 0x10000000: 0x10000000 >> 21 = index 128; 1 MiB fits
  inside that one 2 MiB slot → **1** level-2 table.
- 2-page stack at the very top: top address 0x3FFFFFFF → level-1 index 511 →
  **1** level-2 table.

Distinct level-2 tables = {0, 1, 128, 511} = 4, plus the single level-1 table
= **5 page-table pages** = 5 × 4 KiB = **20 KiB**. **[3]**
A single-level table would need 2¹⁸ entries × 8 bytes = **2 MiB**, whether or
not the address space is sparse. The two-level design wins because it only
materialises level-2 tables for regions that are actually used — a sparse
address space costs a few pages instead of 2 MiB. **[1]**

*Common error:* forgetting that 3 MiB crosses a 2 MiB boundary and so needs two
level-2 tables, not one.

**(iii) Effective access time [2 marks]**
Hit: 80 ns. Miss: 2 (walk) + 1 (the access) = 3 memory accesses = 240 ns.
EAT = 0.95 × 80 + 0.05 × 240 = 76 + 12 = **88 ns**. **[2]**

### (c) Inverted page table [6 marks]

**(i) Size [3 marks]** 512 MiB / 4 KiB = 2²⁹ / 2¹² = 2¹⁷ = **131 072 frames**,
hence 131 072 entries × 8 bytes = 2²⁰ bytes = **1 MiB**. This is fixed by
physical memory and does **not** grow with the number of processes (one table
for the whole system). **[3]**

**(ii) Trade-off [3 marks]**
- Advantage: total size depends on **physical** memory, not on the number or
  size of virtual address spaces — with many processes or huge sparse address
  spaces it uses far less memory than one forward table per process. **[1]**
- Disadvantage: translation is no longer a direct index — you must **search** the
  table for the entry matching (pid, virtual page), which is slow; and sharing a
  physical page between address spaces is awkward. **[1]**
- Mitigation: hash the (pid, VPN) to index the table (a **hashed** inverted page
  table with collision chains), and rely on the **TLB** to keep the expensive
  search off the common path. **[1]**

---

## Question 2 — Page replacement

Reference string: `2 3 4 1 4 2 3 5 2 3 4 1 5` (13 references).

### (a) Demand paging and the fault path [4 marks]

- **Demand paging:** pages are not loaded until first referenced; a PTE is marked
  invalid/not-present until then, and the first access traps as a page fault
  which the OS services by bringing the page in. **[1]**
- Steps to service a fault with a free frame available (order matters, ~half a
  mark each — need the sense of the sequence for [3]):
  1. Trap to the kernel; save state; check the faulting address is **legal** for
     this process (else signal/kill).
  2. Find the page on backing store (swap/file) from the OS's own metadata.
  3. Grab a **free frame** and schedule the disk read into it; block the process
     and run something else meanwhile.
  4. On I/O completion, **update the page table** (set the frame, mark valid,
     permissions) and the reverse maps.
  5. **Restart** the faulting instruction; the access now succeeds. **[3]**

### (b) FIFO, Belady, LRU [8 marks]

**(i) FIFO at 3 and 4 frames [4 marks]** (frames shown oldest→newest; ✗ = fault)

3 frames:

| ref  | 2 | 3 | 4 | 1 | 4 | 2 | 3 | 5 | 2 | 3 | 4 | 1 | 5 |
|------|---|---|---|---|---|---|---|---|---|---|---|---|---|
|frames| 2 |2,3|2,3,4|3,4,1|3,4,1|4,1,2|1,2,3|2,3,5|2,3,5|2,3,5|3,5,4|5,4,1|5,4,1|
|fault | ✗ | ✗ | ✗  | ✗  |hit | ✗  | ✗  | ✗  |hit |hit | ✗  | ✗  |hit |

**9 faults.**

4 frames:

| ref  | 2 | 3 | 4 | 1 | 4 | 2 | 3 | 5 | 2 | 3 | 4 | 1 | 5 |
|------|---|---|---|---|---|---|---|---|---|---|---|---|---|
|frames| 2 |2,3|2,3,4|2,3,4,1|2,3,4,1|2,3,4,1|2,3,4,1|3,4,1,5|4,1,5,2|1,5,2,3|5,2,3,4|2,3,4,1|3,4,1,5|
|fault | ✗ | ✗ | ✗  | ✗    |hit   |hit   |hit   | ✗    | ✗    | ✗    | ✗    | ✗    | ✗    |

After the fill, 4 frames hit on 4, 2, 3 — but then page 5 evicts the oldest
resident (2), and every subsequent reference evicts exactly the page about to
be needed: 6 consecutive faults. **10 faults.** **[4]** — 2 per correct trace
and count.

**(ii) Belady's anomaly and LRU [4 marks]**
Giving FIFO an extra frame *increased* faults from 9 to 10 — this is **Belady's
anomaly**. **[1]**

LRU on the same string:

- 3 frames: faults on 2,3,4,1 (fill), hit 4, then faults on 2,3,5, hits on 2,3,
  faults on 4,1,5 → **10 faults**.
- 4 frames: faults on 2,3,4,1 (fill), hits on 4,2,3, fault on 5 (evicts LRU
  page 1), hits on 2,3,4, faults on 1,5 → **7 faults**.

More memory helped (10 → 7): LRU shows no anomaly. **[2]**
The guarantee comes from the **stack property**: for a stack algorithm the set
of pages resident with *k* frames is always a **subset** of the set resident
with *k*+1 frames, so any page that is a hit with *k* frames is also a hit with
*k*+1. LRU (and OPT) have this property; FIFO does not, because eviction order
depends on load order rather than on a total ordering by recency, so the
resident sets are not nested. **[1]**

*For reference (not required from the candidate):* on this string **OPT** takes
8 faults at 3 frames and 6 at 4 (the unbeatable lower bound, and monotone). The
**clock / second-chance** approximation takes 9 and 10 here — like FIFO it too
exhibits the anomaly on this string, because every page is re-referenced before
long, so the use-bits give little discrimination and clock degenerates towards
FIFO.

### (c) Working set and thrashing [4 marks]

- **Working set** W(t, Δ): the set of distinct pages a process has referenced in
  the last Δ references (the window). Its size estimates the process's current
  memory demand / locality. **[1.5]**
- **Thrashing:** when the total working sets of the running processes exceed
  physical memory, processes continually evict each other's needed pages, so
  almost every reference faults; the system spends nearly all its time paging and
  CPU utilisation collapses. It arises when the **degree of multiprogramming is
  raised too high** — adding another process pushes total demand over capacity.
  **[1.5]**
- Response: **reduce multiprogramming** — suspend/swap out whole processes until
  the remaining working sets fit (a medium-term scheduler / working-set or
  page-fault-frequency admission control). **[1]**

### (d) COW fork and replacement [4 marks]

- After a COW `fork`, parent and child **share** the same physical frames
  read-only, so where eager fork created two copies (two streams of references to
  two frame sets), COW leaves **one** frame referenced through **two** page
  tables — the replacement policy now sees a frame that is "hot" from either
  process, and physical memory pressure is much lower until writes force copies.
  **[2]**
- A shared frame must **not** be treated like a private one on eviction. The
  replacement code must consult the frame's **reference count**: evicting/freeing
  a frame that still has other references would corrupt the other address space.
  So the policy may reclaim the physical frame only when the last reference is
  gone; while shared, "evicting" one mapping means dropping that PTE and
  decrementing the count, not freeing the frame. **[2]**

*Common error:* saying COW "doubles" the references the policy sees — it is the
opposite; sharing *reduces* distinct frames until a write triggers a copy.

---

## Question 3 — Input / output

### (a) Polling vs interrupts vs DMA [4 marks]

- **Programmed I/O with polling:** the **CPU** moves the data word-by-word
  through device registers, and it **busy-waits** (repeatedly reads a status
  register) until the device is ready. CPU fully occupied. **[1.5]**
- **Interrupt-driven I/O:** the **CPU** still copies the data word-by-word, but
  it does *not* busy-wait — it blocks/does other work and the device **raises an
  interrupt** when ready, at which point the handler transfers the next unit.
  CPU freed between units but still copies each byte. **[1.5]**
- **DMA:** a **DMA engine** copies the data between device and memory *without*
  the CPU; the CPU programs the transfer, does other work, and takes a single
  interrupt at completion. CPU almost entirely free during the transfer. **[1]**

### (b) Control flow and when polling wins [6 marks]

**(i) Pseudo-code [4 marks]**

Polling:

```
read_poll(dev, buf, n):
    for each unit:
        start request on dev
        while (status(dev) != READY)   # busy-wait, burning CPU
            ;
        buf[i] = data_register(dev)
```

Interrupts:

```
read_intr(dev, buf, n):
    start request on dev
    sleep(current_process)             # block; scheduler runs someone else

# elsewhere, on the device interrupt:
isr(dev):
    buf[i] = data_register(dev)
    if more units: start next request
    else: wakeup(waiting_process)      # unblock the reader
```

**[4]** — 2 for the polling busy-wait loop, 2 for the interrupt path clearly
showing sleep-in-caller / wake-in-ISR.

**(ii) When polling is faster [2 marks]** — any two:
- **Very fast / low-latency devices:** if the device completes in less time than
  it takes to take an interrupt (save state, run the ISR, return), polling wins —
  you would spend more on interrupt overhead than on the short spin. **[1]**
- **High throughput / interrupt storms:** at very high request rates, one
  interrupt *per* completion swamps the CPU with overhead and cache/pipeline
  disruption; polling (or hybrid NAPI-style "interrupt then poll") amortises the
  cost by handling many completions per poll. **[1]** (Also acceptable: when the
  CPU has nothing else to do anyway, so busy-waiting costs nothing.)

### (c) Disk vs SSD calculation [6 marks]

**(i) One random 4 KiB block [4 marks]**
- Disk: seek 6 ms + avg rotational latency (half a revolution at 10 000 RPM =
  (60 000 ms / 10 000) / 2 = **3 ms**) + transfer (4096 B ÷ 150 MB/s ≈
  0.027 ms) ≈ **9.03 ms**. Throughput ≈ 1000 / 9.03 ≈ **111 IOPS**. **[2]**
- SSD: 60 µs latency + transfer (4096 B ÷ 1 GB/s ≈ 4.1 µs) ≈ **64.1 µs ≈
  0.064 ms**. Throughput ≈ 1000 / 0.0641 ≈ **15 600 IOPS**. **[2]**
- (The SSD is ~140× faster on random 4 KiB reads — reward the observation.)

**(ii) 1 MiB, contiguous vs random [2 marks]**
- Disk contiguous: one seek + one rotational latency + transfer of 1 MiB
  (1 048 576 B ÷ 150 MB/s ≈ 6.99 ms) ≈ 6 + 3 + 6.99 ≈ **16.0 ms**. **[0.5]**
- Disk random (256 separate 4 KiB reads): 256 × 9.03 ms ≈ **2310 ms** — ~145×
  slower, because each block pays its own seek + rotation. **[1]**
- The SSD has no mechanical seek or rotation, so its per-request overhead is
  fixed and tiny; contiguous vs random differ only by whether the controller can
  pipeline/parallelise across flash chips, so the gap is far smaller (≈ 1.1 ms
  vs ≈ 16.4 ms here, not 145×). **[0.5]**

### (d) Buffering [4 marks]
A kernel buffer is a staging area between device and user. Two distinct reasons
(any two): **(1)** decouple transfer sizes/speeds — the device delivers in
fixed blocks at its own rate while the application asks for arbitrary amounts;
**(2)** allow the process to be **descheduled** during the transfer (the device
can't write into a user page that may be swapped out or whose process isn't
running); **(3)** enable caching and safe copy semantics (the data is validated
and stable before the user sees it). **[2]**

- **Read-ahead:** on a sequential read the kernel fetches *further* blocks than
  asked into the buffer cache before they are requested, so the next `read`
  hits in the cache — it hides latency for sequential access. **Write-behind
  (write-back):** a `write` returns as soon as the data is in the buffer cache,
  and the kernel flushes it to the device later/asynchronously. **[1]**
- Write-behind's **benefit:** the application isn't blocked on slow device
  writes, writes can be **batched/coalesced and reordered** for throughput, and
  data overwritten again soon may never be written at all. Its **risk:**
  acknowledged data that is not yet on stable storage is **lost on a crash/power
  failure** (hence `fsync` and journaling) — a durability/consistency hazard the
  write-through alternative avoids. **[1]**

---

## Question 4 — File systems

Block size 512 B, pointer 4 B ⇒ **128 pointers per indirect block.**

### (a) Inode contents and directories [4 marks]

- Besides block pointers, an inode holds: file **type** (regular/dir/device/
  symlink), **size** in bytes, **link count**, **owner/group**, **permission**
  bits, and **timestamps** (and often the block count). **[2]** (half a mark
  each, four needed).
- A **directory is just a file** whose data is a table of (**filename → inode
  number**) entries. Looking up a name means reading the directory's data blocks
  and finding the matching entry, which gives the inode number; the inode (not
  the directory) holds the file's metadata and data-block map. So the name lives
  in the directory, everything else lives in the inode. **[2]**

### (b) Inode arithmetic [8 marks]

**(i) Stock inode [3 marks]** 9 direct + 1 singly-indirect (128 blocks) =
9 + 128 = **137 blocks** = 137 × 512 = **70 144 bytes = 68.5 KiB**. **[3]**

**(ii) Rebalanced inode [3 marks]** 8 direct + 1 singly-indirect (128) + 1
doubly-indirect (128 × 128 = 16 384):
8 + 128 + 16 384 = **16 520 blocks** = 16 520 × 512 = **8 458 240 bytes ≈
8.07 MiB**. **[3]** (Reward noting the max grew ~120× at the cost of one
direct slot re-purposed as the doubly-indirect arm — the slot count is
unchanged at 10, so the on-disk inode size is too.)

**(iii) Reads and the write asymmetry [2 marks]**
- To read a byte in the doubly-indirect region: read the **top** (doubly-indirect)
  block, then the **mid** (singly-indirect) block it points to, then the **data**
  block — **3 block reads** in the worst case (inode already in memory). **[1]**
- The **first write** to a not-yet-allocated block there costs more because the
  data block *and possibly the mid indirect block and the top indirect block must
  be allocated*: `balloc` must consult/update the free-block bitmap and zero and
  write each newly-created indirect block, whereas a read of an already-allocated
  block just follows existing pointers. So a first write can touch the bitmap
  plus one or two indirect blocks plus the data block, all of which must be
  written out. **[1]**

### (c) Hard vs soft links [4 marks]

For each aspect, the contrast (~1.3 marks each; [4] total):
- **(i) Where the name→inode mapping lives:** a **hard link** is a directory
  entry pointing directly at the *same inode number* — the inode's link count is
  incremented and there is no distinguished "original". A **soft link** is a
  separate inode of type symlink whose *data* is the target **pathname**;
  resolution re-walks that path.
- **(ii) Crossing file systems:** a hard link **cannot** cross a file-system
  boundary (inode numbers are only meaningful within one file system). A soft
  link stores a pathname, so it **can** point anywhere, including another device.
- **(iii) Deleting the target:** removing a name decrements the link count; the
  file's data survives while **any** hard link remains and is freed only at count
  zero — so a hard link keeps the file alive. A soft link is unaffected by (and
  does not keep alive) its target: delete the target and the symlink becomes
  **dangling** and resolves to an error.

### (d) Critique of metadata-in-the-directory-entry (CP/M/FAT style) [4 marks]

Mark scheme: 1 for the genuine simplification, up to 3 for distinct breakages
(1 each, cap [3] — the strongest answers connect back to part (c)), and the
final point on what the V7 split buys can substitute for one breakage if
well made.

- **What it simplifies [1]:** opening a file needs **one** disk read — the
  directory block yields the name, the permissions, the size, *and* the block
  map together; there is no separate inode region to allocate, no inode-table
  seek between the lookup and the data, and no "dangling inode without a name"
  state for fsck to worry about. For a floppy-era system with no links, this is
  a real saving.
- **Hard links become impossible (or incoherent) [1]:** a hard link means **two
  directory entries naming one file**. If each entry *contains* the metadata,
  two names mean **two copies** of the size and block pointers with no
  authoritative one — append via one name and the other name's size and block
  list are now stale/inconsistent. The **link count** also has nowhere
  canonical to live, so the system cannot know when the last name is gone and
  the blocks may be freed. (Part (c)'s semantics simply cannot be implemented.)
- **Open-but-unlinked files break [1]:** Unix guarantees that a file stays
  usable by processes that hold it open even after its last name is
  `unlink`ed — the kernel's file object points at the **inode**, which survives
  until both the link count and the open count reach zero. If the metadata *is*
  the directory entry, unlinking destroys the size and block pointers out from
  under the open file: `fstat` and `read` have nothing to consult. (The classic
  "write to an unlinked temp file" idiom dies.)
- **Rename/move across directories gets heavy and non-atomic [1]:** in V7,
  `mv` between directories is just "add (name, inum) entry here, delete one
  there" — two small directory edits; the file's identity (the inode) never
  moves. With embedded metadata the **whole record** (including the block
  pointers) must be copied from one directory block to another — a multi-write
  update that is harder to make crash-safe, and any cached/open reference to the
  old location is invalidated.
- **What the V7 split buys [1 — may replace one of the above]:** making the
  directory entry *only* (name → inode number) turns names into cheap,
  many-to-one references to a **single authoritative record** of the file. That
  one indirection is what gives: any number of hard links plus a meaningful
  link count; a stable identity for open files independent of any name; cheap
  atomic-ish rename; and a fixed-size, separately-allocated inode table that is
  easy to index, scan, and check (fsck).

*Common wrong answer:* "FAT can't do long filenames / is slow" — irrelevant;
the question is about *where metadata lives*, and marks are only for
consequences of that placement.

---

*End of Midterm 2 solutions.*
