# RESULTS.md — the model report

**Spoiler.** This is the reference report. Produce your own before reading it —
the tables below are for the reference tools with the parameters named, and your
job is to run the same experiments and explain *your* numbers. The mechanisms
are the same; the point is to have measured them yourself.

All numbers are modelled ticks / physical I/O counts, which are deterministic:
the same command reproduces them exactly. There is no wall-clock timing here and
nothing to average.

---

## Part 1 — buffering and the I/O mechanisms

### Buffering (n=60, tdev=10, tcpu=3)

| Producer | unbuffered | double | circular (depth 8) |
|---|---:|---:|---:|
| steady (`burst=0`)   | 780 | 603 | **603** |
| bursty (`burst=6`)   | 796 | 665 | **601** |

Two findings, and the second is the interesting one:

- **Overlap is worth about 23%.** Unbuffered serialises every unit
  (`60 × (3 + 10) = 780`); double buffering lets the CPU produce unit *i+1* while
  the device drains unit *i*, so the device — the bottleneck at 10 ticks against
  the CPU's 3 — runs almost back-to-back and the total drops to 603, close to the
  floor of `60 × 10`.
- **Depth buys nothing for a steady producer and a lot for a bursty one.** With a
  steady producer, double and circular are *identical* (603 = 603): two buffers
  are enough to keep a 10-tick device fed by a 3-tick producer, and more slots
  sit empty. Make the producer bursty — cheap runs punctuated by an expensive
  unit — and the double buffer stalls whenever the expensive unit lands (the
  device drains its one spare buffer and then waits), so it *rises* to 665, while
  the depth-8 ring absorbs the burst by running ahead during the cheap run and
  stays at 601. Buffer depth is insurance against *variance*, not against a slow
  device.

### Polling vs interrupt vs DMA (n=16), cycles wasted

| Device | polling (`N×D`) | interrupt (`N×H`, H=50) | dma (`S+H`, S=100) |
|---|---:|---:|---:|
| slow (`D=100`) | 1600 | **800** | **150** |
| fast (`D=5`)   | **80** | 800 | 150 |

The crossover is exactly the chapter-36 argument. Polling's cost is the time
spent spinning, `N×D`, so it is cheap for a fast device (80 at D=5) and ruinous
for a slow one (1600 at D=100). Interrupt cost is fixed per unit, `N×H = 800`,
independent of the device — better than polling once `D > H` (here, once D > 50),
worse when the device is fast enough that the spin would have been shorter than a
context switch. DMA is in a different class because it pays its overhead **once
for the whole burst**, not per unit: 150 regardless. The lesson is that the
right mechanism is a property of the device speed relative to the interrupt cost,
not an absolute.

---

## Part 2 / Part 3 — the disk model and the schedulers

### Separation on `disk-sep.stream` (start=50)

| Policy | seek_total (cyl) | service_total (ticks) |
|---|---:|---:|
| fifo  | 285 | 601 |
| sstf  | 130 | 601 |
| scan  | **120** | 601 |
| cscan | 155 | 601 |

The four seek totals are all different, which is the whole point of the fixture:
FCFS pays 285 cylinders of head movement chasing arrival order, SSTF cuts that to
130 by always taking the nearest, and SCAN beats even SSTF here (120) by never
doubling back within a sweep. C-SCAN pays more than SCAN (155) because its wrap
from the top of the sweep back to the bottom is real head movement — the price of
the uniform latency it gives in exchange. (Service time is identical across
policies on this fixture only because every request sits at sector 0; see below
for a workload where it separates too.)

### The same four policies on a random workload (`gen rand n=200 seed=42 maxlba=50000`, start=250)

| Policy | seek_total (cyl) | service_total (ticks) |
|---|---:|---:|
| fifo  | 34175 | 44844 |
| sstf  |   749 | **10608** |
| scan  |   748 | 11063 |
| cscan |   992 | 11022 |

On 200 scattered requests the gap is enormous: FCFS moves the head 34,175
cylinders, the elevator policies about 750 — a **46×** reduction — and the total
service time falls from 44,844 ticks to about 10,600. SSTF and SCAN are within a
whisker of each other on seek here (749 vs 748); they diverge on *fairness*, not
on throughput, which is what the starvation fixture shows.

### Starvation on `disk-starve.stream` (start=15)

The lone request at cylinder 490 (lba 49000) is served at:

| Policy | service position of the outlier |
|---|---:|
| sstf | **10 (last)** — it waits for the entire cluster |
| scan | 5 — reached on the up-sweep |

Under SSTF the head is pinned to the cluster around cylinder 15 and the outlier
starves until nothing else is left; the elevator reaches it in a single pass. The
two complementary fixes:

- **SCAN sweeps**, bounding any request's wait by one sweep of the disk — this
  part.
- **BSATF bounds the window** (sheet 16 §B2(c)): keep SSTF's greedy choice but
  cap how far ahead it may look, so an old request eventually forces its own
  service. Same pathology, opposite lever: SCAN gives up greedy locality for a
  hard fairness bound; BSATF keeps most of the locality and buys a soft one.

---

## Part 4 / Part 5 — the RAID array

### Capacity across levels (ndisks=5, blocks_per_disk=8)

| Level | usable blocks | data disks | redundancy overhead |
|---|---:|---:|---|
| 0 | 40 | 5 | none |
| 1 | 8 | 1 | 4 of 5 disks (mirror) |
| 4 | 32 | 4 | 1 of 5 (dedicated parity) |
| 5 | 32 | 4 | 1 of 5 (rotated parity) |

RAID-4 and RAID-5 have identical capacity; the rotation changes *where* parity
lives, not how much there is. RAID-1's one-in-five usable is the price of
surviving up to four failures; RAID-0's five-in-five is the price of surviving
none.

### The small-write problem (write one logical block, RAID-5)

Physical I/Os for a single small write, by array width:

| ndisks (data disks) | subtractive | additive | `auto` picks |
|---|---|---|---|
| 3 (2) | 2R + 2W | **1R + 2W** | additive |
| 4 (3) | 2R + 2W | 2R + 2W | subtractive (tie) |
| 6 (5) | **2R + 2W** | 4R + 2W | subtractive |
| 8 (7) | **2R + 2W** | 6R + 2W | subtractive |

This is the small-write problem made concrete: writing *one* logical block costs
**four physical I/Os at best**, because the parity block has to be updated too.
The two methods trade off against width. Subtractive is constant — read the old
data and old parity, write both — so it is 2R+2W however wide the array.
Additive reads *every other* data block, so it grows with width: cheaper than
subtractive only on a narrow array (1R+2W at 3 disks), and steadily worse beyond
the break-even at 4 disks. `auto` computes both costs and takes the smaller, so
it picks additive at 3 disks and subtractive from 4 up. A large full-stripe write
avoids the problem entirely (compute parity once over the data you already have);
it is the single-block write that stings.

### Rebuild cost (RAID-5, ndisks=4, blocks_per_disk=8)

Rebuilding one failed disk read **24 physical blocks** — the three surviving
blocks of each of the 8 rows. That is the reliability claim made concrete: the
array reconstructs a whole disk from the redundancy it was carrying, and the cost
is one full read of every *surviving* disk. The `checksum` before the failure,
while degraded, and after the rebuild were byte-identical across a fail of every
one of the four disks in turn — the array survives exactly the single-disk
failure the level promises, and RAID-0 under the same test loses data on the
first failure.
