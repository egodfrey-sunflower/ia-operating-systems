# Examples Sheet 7 — I/O subsystem

**Attempt after Week 10.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet07-answers.md` (spoilers — attempt first).

*Covers: polling vs interrupts vs DMA; block/char devices;
blocking, non-blocking, asynchronous and vectored I/O; buffering, caching,
spooling; disks vs SSDs; I/O scheduling; io_uring.*

Reading: OSTEP ch. 36–37 (I/O devices, hard disks) and ch. 44 (flash/SSDs —
mass storage and the polling/interrupts/DMA mechanism); the LWN io_uring
introduction or Axboe's 'Efficient IO with io_uring' (for §E); Silberschatz
§12.3–12.4 as optional reference for the application-I/O interface (buffering,
spooling, block-vs-char in §B). For Section B's device-driver
control flow, the honest pairing is the **optional guided read of xv6's
`kernel/virtio_disk.c`** in the week-10 reading — the course's only in-kernel
device code. (Lab 5, threads & locks, gives you the I/O path's concurrency
background but contains no device code of its own.)

---

## A. Warm-ups (true / false, with a one-line justification)

**A1.** Interrupt-driven I/O is always more efficient than polling.

**A2.** DMA lets a disk read complete with *zero* CPU cycles spent on the
transfer.

**A3.** The elevator (SCAN) disk scheduler improves throughput on an SSD as much
as on a hard disk.

**A4.** Non-blocking I/O and asynchronous I/O are two names for the same thing.

---

## B. Bookwork from the IA sheet (do by citation)

**B1.** Do **IA Examples Sheet 3, Q1(a) and Q1(b)**
(`../../cambridge-course/examples_sheets/examples_sheet3.pdf`) — true/false with justification:
"non-blocking I/O is possible even when using a block device" and "DMA makes
devices go faster". *Note:* for (b), distinguish device speed from CPU
*overhead* — DMA offloads the CPU, it does not clock the device faster.

**B2.** Do **IA Examples Sheet 3, Q3** — explain polling, interrupt-driven
programmed I/O, and DMA, and give the device-driver control flow (pseudo-code or
flowchart) for a *read* in each case. *Note:* make the three flows explicitly
comparable — who copies the data, and what the CPU does while the device works.

**B3.** Do **IA Examples Sheet 3, Q4** — compare blocking, non-blocking and
asynchronous I/O; then give four techniques that improve I/O performance.
*Note:* your four techniques (buffering, caching, spooling, and e.g.
scheduling/read-ahead/DMA) set up C, D and E below.

---

## C. Disk versus SSD service time

A hard disk drive (HDD) has: average seek time **8 ms**, rotation speed **7200
RPM**, and a sustained transfer rate of **200 MB/s** (1 MB = 10⁶ bytes). A
NAND-flash SSD services a random read in **75 µs** (controller + flash array)
and sustains **2 GB/s** sequential.

**(a)** Compute the average service time for a **single random 4 KiB read** on
the HDD, showing the three components (seek, rotational latency, transfer).
State clearly why the rotational-latency term is *half* a revolution.

**(b)** Compute the same for the SSD. Give the HDD/SSD ratio, and each device's
approximate random-read **IOPS** (1 / service time).

**(c)** Now compute the service time for a **1 MiB sequential read** (1 MiB =
2²⁰ bytes) on each device. Explain why the HDD/SSD *ratio* is so much smaller
for the sequential case than for the random case — i.e. why random access is
the HDD's weakness specifically.

**(d)** Using your numbers, argue whether it is worth adding a large in-RAM
**buffer cache** in front of (i) the HDD and (ii) the SSD, and what workload
property determines the answer.

---

## D. I/O scheduling

A disk has cylinders **0–199**. The head is at cylinder **58** and, at that
instant, the request queue (in arrival order) is

```
  91, 174, 24, 119, 8, 133, 70, 74
```

Assume seek cost is proportional to the number of cylinders traversed, and
(for SCAN/C-SCAN) that the head is currently moving *towards higher* cylinder
numbers.

**(a)** Compute the **total head movement** (cylinders traversed) for each of
**FCFS**, **SSTF**, **SCAN** (elevator), and **C-SCAN**.

**(b)** SSTF gives the least movement here but is not used unmodified in
practice. Give the failure mode it can suffer, and explain how SCAN/C-SCAN avoid
it. Why does C-SCAN give more uniform *waiting times* than SCAN?

**(c)** Repeat the *conceptual* exercise for an SSD. Explain why reordering the
queue by logical block address buys essentially nothing on flash, and name two
things an SSD's internal scheduler (in the FTL) *does* care about instead
(hint: erase blocks, wear, write amplification, garbage collection).

**(d)** Linux's default scheduler for NVMe SSDs is often `none` (no reordering).
Explain, in terms of your parts (a) and (c), why "do nothing" can *beat* a
clever elevator on fast flash.

---

## E. Asynchronous I/O and `io_uring`

Read OSTEP ch. 36 and the LWN/Axboe io_uring articles, then discuss.

**(a)** Sketch the control flow of a *blocking* `read()`: what the calling
thread does, what the kernel does, and where the thread's context goes while the
device works. Why does this serialise a single thread's I/O even when the device
could handle many requests in flight?

**(b)** Two classic ways to get *concurrency* out of one thread are (i)
non-blocking descriptors with a readiness interface (`select`/`poll`/`epoll`)
and (ii) POSIX AIO. Explain the essential difference between a **readiness**
model ("tell me when I *can* do the I/O") and a **completion** model ("do the
I/O and tell me when it's *done*"), and why readiness works poorly for regular
files on disk.

**(c)** `io_uring` uses two shared ring buffers — a **submission queue** and a
**completion queue** — mapped between the application and the kernel. Explain
how this design attacks the two main costs of the older interfaces: **system-
call overhead** (per operation) and **data copying / descriptor scanning**.
What does *batching* many submissions per `io_uring_enter` buy, and how does
the kernel-side polling mode remove syscalls almost entirely?

**(d)** Give one correctness or security hazard introduced by sharing kernel-
visible submission/completion rings with user space, and relate it to why
asynchronous, shared-memory I/O interfaces are harder to get right than a plain
blocking `read()`.

---

## Past paper questions

Per this directory's `README.md`, attempt this after the sheet (~35
minutes, closed-book) — it is *the* I/O paper in the post-2016 set:

* **`y2020p2q4`** (`../../cambridge-course/exam_questions/y2020p2q4.pdf`) — single, double and
  circular buffering strategies (a direct fit for the buffering material
  above), plus what state must be saved on a context switch.

For extra, untimed drill on this sheet's material, two pre-2016 questions fit
(files in `../../cambridge-course/exam_questions/`):

* **`y2014p2q4`** — blocking vs non-blocking I/O, reducing the elapsed time
  of a read-then-process loop over a slow device, polled I/O versus
  interrupts, and DMA — Sections B and E material.
* **`y2010p2q4 (c)–(d)`** — blocking vs non-blocking vs asynchronous I/O, and
  the design of a hard-disk device driver: when to issue requests, what the
  interrupt handler does, and how to improve performance given the disk's
  mechanics — Sections B and D material. (Parts (a)–(b) are address-space
  material — Sheet 6 revision.)

For mechanical drill on seek-time reasoning, the OSTEP homework simulator
`disk.py` (<https://github.com/remzi-arpacidusseau/ostep-homework>) lets you
replay Section D's disk-scheduling policies on arbitrary request queues.
