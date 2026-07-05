# ⚠️ SPOILER — Examples Sheet 7 model answers ⚠️

> **STOP.** Full worked solutions. Do the sheet closed-book first. Numeric
> answers were verified with Python; checks are noted inline.

---

## A. Warm-ups

**A1. False.** For slow, infrequent devices interrupts win (the CPU does other
work while waiting). But for very fast, high-rate devices (modern NICs, NVMe),
interrupt *overhead* — context save, handler dispatch, cache pollution — can
dominate, and **polling** (busy-poll / NAPI / `io_uring` SQPOLL) achieves higher
throughput and lower latency. The right choice depends on device rate.

**A2. False.** DMA removes the CPU from the *byte-copy loop* — the DMA
controller moves data device↔memory directly — but the CPU still programs the
transfer (descriptors, addresses) and handles the **completion interrupt**.
"Zero CPU cycles" is wrong; "the CPU is free during the transfer itself" is
right.

**A3. False.** An SSD has no seek and no rotational latency, so ordering
requests by block address to minimise head travel is pointless. SCAN helps HDDs
precisely because it minimises mechanical seek; on flash the gain is
essentially nil (and reordering can even hurt by breaking up sequential runs).

**A4. False.** *Non-blocking* = the call returns immediately with "would block"
(`EAGAIN`) if it cannot proceed; the app must retry/poll (a **readiness**
model). *Asynchronous* = the call starts the operation and the app is notified
on **completion**, the data transfer having been done by the kernel meanwhile.
Different models: retry-when-ready vs notify-when-done.

---

## B. Bookwork

Marking notes (full answers in OSTEP 36–37; buffering/spooling and the block-vs-char interface in Silberschatz §12.3–12.4, optional reference):

* **B1(a) True** — non-blocking is a property of the *descriptor/API*, not the
  device: a block device opened `O_NONBLOCK` returns `EAGAIN` rather than
  sleeping. **B1(b) False as stated** — DMA does not make the *device* faster;
  it removes CPU copy overhead and frees the CPU during transfer, improving
  *system* throughput, not device bandwidth.
* **B2** — polling: driver issues command, then loops reading a status
  register until ready, then copies the data (CPU-bound wait). Interrupt-driven:
  driver issues command and sleeps; device raises an interrupt when ready; the
  handler copies the data and wakes the waiter. DMA: driver programs a DMA
  descriptor (buffer address, length) and sleeps; the DMA engine transfers
  device↔memory; one completion interrupt signals the whole transfer. Data is
  copied by CPU (polling/interrupt) vs by the DMA engine (DMA).
* **B3** — blocking: thread sleeps until done, simplest, one op at a time.
  Non-blocking: returns immediately, app retries. Asynchronous: op proceeds in
  background, completion delivered later. Four performance techniques:
  **buffering** (decouple producer/consumer rates, allow large transfers),
  **caching** (avoid re-reading — the buffer cache), **spooling** (serialise
  access to a non-shareable device, e.g. a printer), and **I/O scheduling /
  read-ahead / DMA** (reduce seeks, prefetch, offload copy).

---

## C. Disk vs SSD service time

*All figures verified in Python.*

**(a) HDD random 4 KiB read.**

* Seek = **8 ms** (given, average).
* Rotational latency = half a revolution. One revolution at 7200 RPM =
  60 000 ms / 7200 = **8.333 ms**, so average latency = **4.167 ms**. It is
  *half* a revolution because on average the target sector is half a rotation
  away — uniform over [0, one revolution].
* Transfer = 4096 B / 200 MB/s = 4096 / 200×10⁶ s = **0.0205 ms**.

Total ≈ 8 + 4.167 + 0.0205 = **12.19 ms**.

**(b) SSD random 4 KiB read** = **0.075 ms** (75 µs), no seek, no rotation.
Ratio HDD/SSD = 12.19 / 0.075 ≈ **163×**. IOPS ≈ 1/service:
HDD ≈ 1000/12.19 ≈ **82 IOPS**; SSD ≈ 1000/0.075 ≈ **13 300 IOPS**. The SSD's
random-read advantage is ~160×.

**(c) 1 MiB sequential read.**

* HDD = seek 8 + latency 4.167 + 2²⁰/200×10⁶ s (= 5.24 ms) ≈ **17.4 ms**.
* SSD = 0.075 + 2²⁰/2×10⁹ s (= 0.524 ms) ≈ **0.60 ms**.

Ratio ≈ 29× — much smaller than the 163× of the random case. For a big
sequential transfer the HDD amortises its one seek+rotation over a megabyte, so
the mechanical fixed cost is diluted. The HDD's weakness is **random** access,
where every request pays the full ~12 ms mechanical tax; sequentially it is
"only" ~3–4× slower per byte.

**(d)** A buffer cache pays off in proportion to **re-use / locality** and to
the underlying device's per-access cost. In front of the **HDD** it is
enormously valuable: every cache hit saves ~12 ms and turns random into
no-I/O. In front of the **SSD** it still helps (a hit is ~free vs 75 µs, and
saves flash wear), but the *marginal* benefit is smaller because a miss is
already cheap — so a cold, low-locality workload benefits far less on SSD. The
deciding property is the workload's **hit rate / temporal locality**.

---

## D. I/O scheduling

Head at 58, queue `91,174,24,119,8,133,70,74`, disk 0–199, moving up first.
*All totals verified in Python.*

**(a)**

| Scheduler | Service order | Total head movement |
|-----------|---------------|--------------------:|
| **FCFS** | 91,174,24,119,8,133,70,74 | **664** |
| **SSTF** | 70,74,91,119,133,174,24,8 | **282** |
| **SCAN** (up, to 199) | 70,74,91,119,133,174,→199, then 24,8 | **332** |
| **C-SCAN** (up to 199, wrap to 0, up) | 70,74,91,119,133,174,→199,→0,8,24 | **364** |

(FCFS: 33+83+150+95+111+125+63+4 = 664. SSTF greedily nearest each step:
12+4+17+28+14+41+150+16 = 282. SCAN goes 58→199 then reverses to 8:
141+191 = 332. C-SCAN goes 58→199, jumps to 0, sweeps up to 24:
141+199+24 = 364, the middle term being the 199-cylinder return.)

**(b)** SSTF can **starve** far-away requests: a steady stream of nearby
requests keeps the head local and a request at cylinder 8 waits indefinitely.
SCAN/C-SCAN avoid this by sweeping the whole disk in one direction, giving every
request a bounded wait (one sweep). C-SCAN gives more **uniform** waiting times
than SCAN because SCAN services the middle cylinders twice as often as the edges
(it passes them on the way out *and* back), whereas C-SCAN always sweeps the
same direction and treats the disk as circular, so all cylinders get equal
treatment.

**(c)** On an SSD there is no head and no seek, so total "head movement" is
meaningless and LBA-order reordering buys nothing. The FTL instead cares about:
**erase-block granularity** (flash is written in pages but erased in much larger
blocks), **wear levelling** (spreading writes so no block wears out early),
**write amplification** and **garbage collection** (relocating live pages to
reclaim partially-valid erase blocks). Its "scheduling" is really placement and
GC, not ordering by address.

**(d)** On a hard disk the elevator's reordering saves tens of milliseconds of
seek per request, so the CPU spent reordering is trivially repaid. On NVMe
flash a request is already ~10–75 µs and there is no seek to save, so the
scheduler's queueing, sorting and lock contention become *pure overhead* that
can exceed the (near-zero) benefit — and reordering can break up sequential runs
the device would otherwise coalesce. Hence Linux ships `none` for NVMe: doing
nothing keeps latency low and lets the device's deep internal parallelism sort
itself out.

---

## E. Asynchronous I/O and `io_uring`

**(a)** A blocking `read()` traps into the kernel, which starts the device I/O
and puts the calling thread to **sleep** (off the run queue) until the data
arrives; on completion the thread is woken, the data copied into its buffer, and
the call returns. The thread's context sits blocked the whole time. This
serialises that thread's I/O: it can have only **one** request outstanding,
even though the device (or a RAID/NVMe queue) could service dozens in parallel —
you would need many threads to fill the device.

**(b)** A **readiness** model (`select`/`poll`/`epoll` on non-blocking fds) says
"the fd is now ready; *you* issue the I/O without blocking." A **completion**
model (POSIX AIO, `io_uring`) says "I have *done* the I/O; here is the result."
Readiness works for sockets/pipes because "ready to read" is a real, cheap-to-
test state. It works poorly for **regular disk files** because a disk file is
almost always "ready" — the question is not readiness but the latency of the
actual transfer — so `epoll` degenerates into a blocking read anyway. Disk I/O
wants completion, not readiness.

**(c)** `io_uring` shares two ring buffers in memory between app and kernel: the
app writes submission-queue entries (SQEs) and reads completion-queue entries
(CQEs) **without a syscall per operation**. This attacks:

* **Syscall overhead** — one `io_uring_enter` can submit *N* operations
  (batching), amortising the trap; and in **SQPOLL** mode a kernel thread polls
  the submission ring, so steady-state I/O needs essentially **no syscalls at
  all**.
* **Copying / scanning** — the rings are shared memory (no copying the request
  descriptors in and out), and completions are posted directly to the CQ rather
  than the app re-scanning a large fd set as `select` does each call (O(N) per
  wait). Buffers can be pre-registered so the kernel need not re-pin them per op.

Batching turns *N* syscalls into one and lets the kernel dispatch a whole
burst; SQPOLL removes even that.

**(d)** Sharing kernel-visible rings with user space means the kernel must treat
every field the app can write as **untrusted and possibly changing under it**
(TOCTOU): indices, buffer pointers and lengths in the SQ can be raced or
poisoned, so the kernel must validate/copy-once carefully — several early
`io_uring` CVEs were exactly this class, plus the general attack surface of a
powerful async interface reachable from unprivileged code. A blocking `read()`
copies its arguments once, at a well-defined trap boundary, with no shared
mutable state — which is why it is far easier to make correct and safe than a
shared-memory completion ring.

---

*Python verification summary:* HDD random 4 KiB ≈ 12.19 ms (seek 8 + rot 4.167
+ xfer 0.0205), SSD 0.075 ms, ratio ≈ 163×; HDD 1 MiB seq ≈ 17.4 ms, SSD
≈ 0.60 ms. Disk scheduling totals: FCFS 664, SSTF 282, SCAN 332, C-SCAN 364.
