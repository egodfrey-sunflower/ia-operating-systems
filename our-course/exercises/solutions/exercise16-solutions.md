> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 16 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. Where a
> part depends on a simulator run, the reasoning and the shape of the result
> are what earn marks; exact figures vary with simulator defaults.

---

## A. Warm-ups

**A1. FALSE.** 6 ms is the time of one *full* rotation (60,000 ms ÷
10,000). On average the desired sector is half a revolution away, so the
average rotational delay is **3 ms**. (Confusing these two is the most
common error in the I/O arithmetic questions — hence this warm-up.)

**A2. FALSE.** The drive exports a flat array of block numbers; the mapping
to tracks — and the head's current position — are hidden. The OS can
approximate with **nearest-block-first**, but true SSTF (and anything
rotation-aware) needs geometry only the drive's controller has. Hence A3.

**A3. TRUE.** SPTF must compare seek time against rotational delay for each
candidate, which requires knowing the head's rotational position and the
track layout at the instant of decision. The controller has that
information; the OS does not. Modern practice: the OS merges and batches a
few requests, the drive schedules them internally (SPTF-style).

**A4. FALSE.** The gap is *hundreds*, not tens: ch. 37's worked drives give
~0.3–0.7 MB/s random against 105–125 MB/s sequential — factors of ~190–340.
The reason is structural: a random 4 KB read pays ~7 ms of positioning for
~40 µs of transfer (B1). This number is why file systems are obsessed with
locality.

**A5. FALSE on safety.** Faster, yes — write-back caching ("immediate
reporting") acknowledges before the media is updated. But if the OS or an
application depends on writes reaching the platter *in a particular order*
(or at all before a power cut), the drive lying about completion breaks the
guarantee: the data can be lost or misordered by a crash. Ch. 37 flags this
exactly; the machinery that copes with it (journaling) arrives in week 21.

**A6. FALSE.** One failure is guaranteed survivable; with luck, up to
**N/2** — one disk per mirror pair. In ch. 38's 4-disk layout, losing disks
0 and 2 (different pairs) loses nothing; losing 0 and 1 (one pair) loses
data. Sensible operators plan on the guarantee, not the luck.

**A7. FALSE.** RAID-5 removes the *bottleneck* — parity I/Os are spread
over all disks instead of hammering one — so small writes regain
parallelism (N/4 · R aggregate). But each small write still costs **four
physical I/Os** (read old data, read old parity, write both); that 4× tax
is the price of parity itself, and no rotation of layout removes it.

**A8. TRUE.** With the entire stripe's new data in hand, the new parity is
just the XOR of what is about to be written — no old contents are needed.
That is why RAID-4/5 write at (N−1)·S sequentially, and why systems that
can arrange *only* full-stripe writes (B4c) escape the small-write problem
entirely.

---

## B. Geometry and arithmetic

**B1.**
**(a)**
Time/rotation = (1 min ÷ 10,000 rot) × (60 s/min) × (1000 ms/s) =
60,000/10,000 = **6 ms**; average rotational delay = half a rotation =
**3 ms**.

**(b)** Transfer: 4 KB ÷ 100 MB/s ≈ **0.04 ms** (40 µs).
`T_I/O` = 4 + 3 + 0.04 ≈ **7.04 ms**;
`R_I/O` = 4 KB ÷ 7.04 ms ≈ **0.57 MB/s**.
Positioning (seek + rotation, 7 ms) outweighs transfer (0.04 ms) by a
factor of ~175: the disk spends 99.4% of the operation getting into place.

**(c)** Transfer: 10 MB ÷ 100 MB/s = 100 ms.
`T_I/O` = 4 + 3 + 100 = **107 ms**; `R_I/O` = 10 MB ÷ 107 ms ≈
**93.5 MB/s** — near peak, the positioning cost amortised away.

**(d)** ≈ 93.5 ÷ 0.57 ≈ **165×** (accept 150–200 with consistent rounding).
The tip it justifies: *use disks sequentially; when you can't, transfer in
the largest chunks you can.*

**(e)** Rotation halves: 3 ms rotation → average delay 1.5 ms. Holding the
quoted 100 MB/s media rate fixed, transfer stays 0.04 ms, so
`T_I/O` = 4 + 1.5 + 0.04 ≈ **5.54 ms** → speedup ≈ 7.04/5.54 ≈ **1.27×**.
(A real drive's media rate rises with RPM, halving the transfer term too —
but at ~0.02 ms it changes nothing.)
Doubling RPM only touches one of the three terms; the unchanged 4 ms seek
now dominates (Amdahl's law wearing a mechanical costume). Doubling the
money on spindle speed buys ~27% — which is why drive generations attack
seek, density and interface together.

**B2.** *(These request streams carry no seed, so on the default disk they
are fully deterministic — the totals below are what `-c` prints. Only if you
change the geometry do your numbers move; the marks are in the reasoning.)*
**(a)** FIFO services 7 → 30 → 8. On the chapter's three-track geometry, 7
and 8 share the outer track while 30 is inner: FIFO does outer → inner →
outer, paying the long seek **twice**, plus whatever rotation each
unfortunate arrival angle adds. The obvious better order (7, 8, 30) pays it
once — which is what SSTF finds. The simulator prints total **795** (seek
160 = 2 × 80, rotate 545, transfer 90) for FIFO here.

**(b)** SSTF greedily minimises *seek*: it stays on the outer track for 8
after 7, then goes in for 30. SATF minimises *positioning* (seek **and**
rotation). On this stream they usually agree — 8 is both nearest-track and
promptly reachable. The stream that separates them needs a nearer-track
candidate whose sector has *just passed* the head, against a farther-track
candidate arriving under the head after the seek: SATF takes the farther
request and wins by most of a rotation (ch. 37's Figure 37.8 situation).
SATF's extra input is **rotational position**; SSTF is blind to it. On
*this* stream both serve 7 → 8 → 30 and both print total **375**, so the run
alone won't separate them — the point is the geometry that would.

**(c)** Feed the scheduler a continuous stream of requests on (or rotationally
adjacent to) the current track — each is individually the shortest
positioning time, so a lone request on a distant track is deferred forever
while throughput looks excellent. **BSATF** admits a window of requests and
refuses to start the next window until every request in the current one is
served: the distant request's wait is bounded by one window. The cost is
throughput — sometimes the globally best next request sits in the *next*
window and the disk does extra positioning work instead. The trade-off —
performance vs starvation-freedom — is the disk-scheduling version of
week 12's TAS-vs-ticket-lock fairness question.

**(d)** 10, 11, 12, 13 is sequential *across a track boundary* (…11 ends
the outer track, 12 starts the next). With no skew, while the head performs
the track-to-track switch, sector 12 rotates past; the drive then waits
almost a full rotation to catch it. **Track skew** rotates the numbering of
each successive track by `k` sectors so that after a track switch the next
logical sector is just arriving. The right `k` = track-switch time ÷
per-sector rotation time, rounded up — so it depends on both the seek rate
and the rotational speed (slower seeks or faster spin ⇒ larger skew). The
simulator bears this out: `-a 10,11,12,13` with no skew prints **585**;
sweeping `-o 0..4` gives 585 / 615 / **285** / 315 / 345, so skew **2** is
optimal — exactly ⌈track-switch ÷ per-sector rotation⌉ on this disk.

**B3.**
**(a)** RAID-0: disk = 53 mod 8 = **5**; offset = ⌊53/8⌋ = **6**.
RAID-4 (convention: 7 data disks 0–6, parity on disk 7): data disk =
53 mod 7 = **4**, stripe row = ⌊53/7⌋ = 7, and parity for every stripe —
including this one — lives on **disk 7**.

**(b)** With N = 8, S = 80 MB/s, R = 1 MB/s:

| | Seq read | Seq write | Rand read | Rand write |
|---|---|---|---|---|
| RAID-0 | N·S = **640** | **640** | N·R = **8** | **8** |
| RAID-1 | (N/2)·S = **320** | (N/2)·S = **320** | N·R = **8** | (N/2)·R = **4** |
| RAID-4 | (N−1)·S = **560** | (N−1)·S = **560** | (N−1)·R = **7** | R/2 = **0.5** |
| RAID-5 | (N−1)·S = **560** | (N−1)·S = **560** | N·R = **8** | (N/4)·R = **2** |

One-line reasons: RAID-0 uses everything, always. RAID-1 seq: each mirror
pair stores every block twice, and even reads waste half of each disk's
pass (the skipped-block effect); random reads spread over all N; random
writes pay 2 physical per logical. RAID-4 seq: parity disk contributes no
user bandwidth; full-stripe writes hit (N−1)·S; random writes serialise on
the parity disk's read+write ⇒ R/2 *total*. RAID-5 as RAID-4 except random
I/O uses all N disks: reads N·R, writes N·(R/4) since each logical write is
4 physical I/Os spread evenly.

**(c)** Ranking for small random writes: **RAID-0 (8) > RAID-1 (4) >
RAID-5 (2) > RAID-4 (0.5)** MB/s. RAID-4 is **16×** slower than RAID-0.
Adding disks does nothing for RAID-4 because the parity disk must do two
I/Os for *every* logical write in the system — it is a serial bottleneck
whose capacity is fixed at R/2 regardless of N. (RAID-5's N/4 · R, by
contrast, scales with N — the point of rotating the parity.)

**(d)** Subtractive: read old data + old parity, write new data + new
parity = **4 I/Os, independent of N**. Additive: read the other **N−2**
data blocks, write data + parity = **N I/Os**. Equal at **N = 4**; below it
(a 3-disk array: 3 I/Os) additive wins, above it subtractive does — which
is why real arrays use the subtractive method.

**(e)** Parity = 0110 ⊕ 1011 ⊕ 0001 ⊕ 1101:
0110⊕1011 = 1101; 1101⊕0001 = 1100; 1100⊕1101 = **0001**.
Reconstruction of the lost `1011` = XOR of survivors and parity:
0110 ⊕ 0001 ⊕ 1101 ⊕ 0001 = (0110⊕0001) = 0111; 0111⊕1101 = 1010;
1010⊕0001 = **1011** ✓. Same operation both directions — parity *is*
reconstruction.

**B4.** *(Again: your run's exact times; this is the expected shape.)*
**(a)** Levels 0, 1 and 5 finish in essentially the same time: 100 random
reads spread across **all 4** disks (~25 per disk) — RAID-1 reads from
either copy, RAID-5's parity is spread so every disk holds data. RAID-4 is
noticeably slower: only **3** data disks share the load (~33–34 per disk),
the parity disk idling. Ranking: {0, 1, 5} then 4.

**(b)** Prediction for RAID-4: each of the 100 writes costs the parity disk
a read *and* a write — **200 I/Os serialised on one spindle** — while the
three data disks share 200 I/Os between them (~67 each). The parity disk is
the clock. But count its work carefully: the 100 writes go straight back to
the very offset just read (the read-modify-write of parity), so on this
simulator — whose timing model charges seek and transfer but has **no
rotation** — each write-back already sits under the head and costs ≈ 0. The
chargeable work is ~100 random reads, i.e. about **4× worse** than RAID-0's
~25 per disk, not 8×. RAID-5 spreads its 400 physical I/Os over 4 disks
(~100 each) — bad, but parallel, so it beats RAID-4. Running it (`raid.py
-t -n 100 -w 100 -D 4 -c` at each level): RAID-0 ≈ **276 ms**, RAID-4 ≈
**982 ms** (**≈ 3.5×**), RAID-5 ≈ **497 ms** — the ordering and the ~4×
parity-disk penalty both confirmed. Note the direction of the model's
simplification: on a *real* disk the read-modify-write costs *more*, not
less — after reading a sector you wait nearly a full rotation for it to come
back under the head before writing — so the physical penalty is real, but
raid.py omits rotation and the write-back is nearly free here. Full credit
for predicting the parity-disk bottleneck quantitatively before running, and
for reconciling the observed ~3.5× with the naive "200 I/Os = 8×" count.

**(c)** Writes become efficient when each request covers a **whole stripe**
— with 4 disks and 4 KB chunks: 3 data chunks = 12 KB (RAID-4), aligned to
the stripe. Then parity is computed from data in hand and no old data or
parity need be read (A8): every disk does one write, and the array streams
at (N−1)·S. The magic size *is* the stripe size (data disks × chunk), which
is why storage systems try so hard to batch and align writes.

---

## C. Discussion and design critique

**C1.**
**(a)** Measure, at the array's interface (below any write-back cache —
what matters is what *reaches the array*, so instrument the block layer,
not the application):

- **Read/write mix**, by operations *and* by bytes — the levels differ
  most on writes; a byte-dominated read workload hides a
  latency-dominated write problem.
- **Request-size distribution**, especially the fraction of writes smaller
  than a stripe — these are exactly the requests that pay RAID-5's 4×.
- **Sequentiality** — from the trace, the fraction of requests whose start
  LBA continues the previous request (per stream/process, not globally —
  interleaved streams look random at the device even when each is
  sequential); equivalently a run-length distribution. This directly tests
  X's "basically sequential" claim.
- **Full-stripe-write fraction** — writes that are stripe-sized *and*
  aligned; these escape the penalty entirely (B4c), so X's case survives
  only to the extent this fraction is high.
- **Queue depth / concurrency** — RAID-5's N/4·R random-write figure
  assumes enough outstanding requests to keep all spindles busy; a
  synchronous single-stream writer gets the latency (2T), not the
  bandwidth.
- **Peak vs average** — burstiness: provisioning happens at the
  95th–99th percentile, and Y's "falls off a cliff" is a claim about
  peaks.

**(b)** Decision rule, stated before measuring: plug the measured mix into
the ch. 38 table to predict each level's sustainable throughput. Let `W_sr`
be the measured peak demand of sub-stripe random writes. **Choose RAID-5**
if `W_sr < (N/4)·R` with at least 2× headroom *and* the capacity
requirement cannot be met by mirroring within budget. **Choose RAID-1** if
`W_sr ≥ (N/4)·R ÷ 2` (headroom gone), or if measured queue depths are too
shallow for RAID-5's parallelism to materialise. Either outcome is
reachable; the data decides. (Any rule with named thresholds and both
outcomes reachable earns full marks; the specific constants are
judgement.)

**(c)** Continuously monitor the same discriminators in production —
small-random-write rate against the array's computed ceiling, and write
latency percentiles (the cliff announces itself in p99 first) — with an
alert threshold well before saturation. Migration story: capacity headroom
sufficient to re-shape (rebuild as mirrored pairs, or add spindles), and a
tested data-migration procedure — because the honest answer to "the
workload changed" is re-provisioning, and discovering that during the
incident is the failure mode Y is remembering.

*Marking note: full credit requires (i) measuring below the cache, (ii)
per-stream sequentiality, (iii) an advance decision rule that could come
out either way. "Run it and see which is faster" is not a campaign.*

**C2.** Three faults the fail-stop model excludes, and what each does to a
RAID-5 array:

1. **Silent corruption.** A disk returns wrong data *with a success code*.
   Normal reads don't touch parity, so nothing checks the data — the array
   serves garbage as happily as a single disk would. Worse, a later small
   write to that stripe folds the corrupt block's value into the *new
   parity* (subtractive update reads it), quietly poisoning the redundancy
   itself.
2. **Latent sector errors.** A block is unreadable, but nobody knows until
   it is read. The array runs "healthy" with a hole in its redundancy. Now
   one honest fail-stop failure occurs and a rebuild starts: reconstruction
   must read **every** surviving block, the latent error surfaces
   mid-rebuild, and that stripe now has two missing members — unrecoverable
   data, in an array that believed itself one-failure-tolerant. The
   reliability claim was really "one failure, *provided every other sector
   in the array is readable*", which is a much weaker sentence.
3. **The consistent-update problem.** A crash between writing data and
   writing parity is not a disk failure at all, yet it leaves a stripe
   whose parity disagrees with its data. The array notices nothing — until
   a disk dies and reconstruction *uses* that stale parity to fabricate a
   block that never existed. (Hardware arrays buy their way out with
   battery-backed NVRAM logging the pending update.)

Implication: an array should spend idle time **scrubbing** — reading every
disk end-to-end, verifying parity, and repairing latent errors and
inconsistencies *while the redundancy to repair them still exists* — and,
beyond RAID's remit, the storage stack needs checksums to make corruption
detectable at all. That machinery is week 22's chapter; the honest
conclusion here is that ch. 38's guarantees are conditional on a fault
model the real world only approximates.

*Marking note: the rebuild-meets-latent-error scenario is the core of the
question; an answer without it is incomplete. Scrubbing (or an equivalent
idle-time verification proposal) is the expected "what should it be
doing".*
