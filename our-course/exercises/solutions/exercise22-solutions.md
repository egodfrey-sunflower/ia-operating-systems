> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 22 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their
> working; for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. FALSE.** A programmed page can only be re-programmed after its entire
containing **block** is erased — programming can only turn erased 1s into
0s. That erase-before-rewrite asymmetry is the fact the whole FTL exists to
hide.

**A2. FALSE** — and the reversal is the interesting part. Ch. 44's
measurements show random *writes* matching or beating random reads on
log-structured SSDs: the FTL appends writes wherever the log currently is
(sequential programs), while a read must go to wherever the requested data
physically landed.

**A3. FALSE.** Wear leveling **spreads** erases evenly so all blocks age
together; migrating long-lived cold data to make popular blocks available
actually *adds* erases and write amplification. The gain is that no subset
of blocks dies early, not that fewer erases happen.

**A4. FALSE.** TRIM tells a device with a *dynamic* logical→physical mapping
that a range is dead, so the FTL can drop mappings and skip copying those
pages during garbage collection. A hard disk's mapping is static — block
address → fixed platter location — so there is nothing useful to discard.

**A5. FALSE.** A checksum **detects** corruption; repair needs another
intact copy — a mirror, parity reconstruction, or a backup. Detection
without redundancy just converts silent corruption into an honest error.

**A6. FALSE.** LSEs are precisely the *non-silent* partial fault: the
drive's in-disk ECC detects (sometimes corrects) the damage and returns an
**error** — which is why plain redundancy suffices for them. The silent
menace is corruption, where the disk cheerfully returns wrong bytes.

**A7. FALSE.** In a lost write the old block sits at the *correct address*
with a *valid checksum* — both checks pass, because the block is a perfectly
good stale version of itself. Catching it needs write-verify (read back
after write) or a checksum held in the parent structure, ZFS-style.

**A8. TRUE.** Entries per block instead of per page cut the table by the
block/page ratio (64× here), but a write smaller than a block forces the
FTL to read the block's other live pages and rewrite them all at the new
location — heavy write amplification for small writes, which real workloads
are full of. Hence hybrid and cached designs.

---

## B. Working the mechanisms

**B1.**
**(a)** Marking note: the skill being checked is reading the flash state
transitions (INVALID → ERASED → VALID) and the mapping table, then costing
the run as `#erases × 1000 + #programs × 40 + #reads × 10` µs and matching
`-S`. A typical 10-op run costs a handful of erases (one per block opened
by the log) plus one program per logical write and one read per logical
read.
**(b)** Direct-mapped: each logical overwrite of a page in a previously
written block costs read-the-block + erase + reprogram. On the **default
device (10 pages/block)** that is up to 9 reads (90 µs) + 1 erase (1000 µs) +
10 programs (400 µs) ≈ **1490 µs** worst-case, against the log FTL's 1 program
plus its amortised share of one erase (40 + 1000/10 ≈ **140 µs**) —
roughly **10×**. Over the actual `-s 1 -n 10` workload the whole-run totals
(via `-S`) are direct **4350 µs** vs log **1200 µs** ≈ **3.6×** — fewer than
ten overwrites land in already-written blocks, so the per-write worst case is
not hit every time. The direct FTL's time goes to erases and to re-programming
unmodified pages, i.e. pure write amplification.
**(c)** Cleaning triggers when used blocks exceed the high watermark
(`-G`), and runs until the low watermark (`-g`). Each cleaning step reads
the live pages of a victim block, programs them at the log head, and erases
the victim — visible with `-J`. The statistic to watch: the *excess* of
physical erases/programs over the ideal SSD's counts (compare `-T ideal`) —
that ratio is the measured write amplification. Concretely, `-T log -n 1000
-s 1 -G 6 -g 4` does **1818** physical programs against the ideal SSD's **492**
(`-T ideal -n 1000 -s 1`) → WA ≈ **3.7**. Pin the seed: on the default seed 0
the same pair gives 1737 vs 520 → WA ≈ 3.3, so quote whichever seed you ran.
(Note the default `-G 10` never fires on the seven-block device, so the
watermarks must be set explicitly.)

**B2.**
**(a)** Direct-mapped: one 4 KB logical write causes 64 pages of
programming (63 of them copies) → **WA = 64**, at ~630 + 1000 + 2560 ≈
**4.2 ms** per write (63 reads + 1 erase + 64 programs, on this 64-page
device — distinct from B1's default 10-page device). Log FTL: **WA ≈ 1**
before cleaning; cost 40 µs + 1000/64 ≈ **56 µs**.
**(b)** Cleaning a block that is fraction `u` live: read + program `u`
blocks' worth of pages to free `(1−u)` blocks' worth of space, so per
*freed* block the FTL moves u/(1−u) blocks of live data; adding the new
data itself gives **WA = 1/(1−u)**:
u = 0.5 → **2**; u = 0.8 → **5**; u = 0.9 → **10**.
**(c)** (i) Lower effective utilisation: with 20% invisible spare, the
cleaner can always find emptier victims, so the `u` it faces — and hence
1/(1−u) — drops. (ii) Deferral and background scheduling: spare capacity
lets cleaning wait for idle periods, so its I/O stops competing with client
traffic even when the amplification itself is unchanged.
**(d)** Lowest: **(i) sequential** — whole blocks die together, so victims
are fully dead and cleaning is nearly free (WA → 1). Highest: **(ii)
uniform random** — every block ends up uniformly, highly live, so victims
are ~as live as the average fullness forces (the B3-style worst case).
Middle: **(iii) skewed after a cold load** — the hot 20% churns and its
blocks die quickly (cheap to clean), while cold blocks stay untouched
*except* that wear leveling must occasionally rewrite them, adding back
some amplification. Credit hinges on the mechanism named for each.

**B3.**
**(a)** 2 TB / 4 KB = 2⁴¹⁻¹² = 2²⁹ ≈ 5.4 × 10⁸ pages × 4 B = **2 GiB** of
table — ch. 44's headline absurdity for a device-internal SRAM budget.
**(b)** 2 TB / 256 KB = 2²³ ≈ 8.4 × 10⁶ entries × 4 B = **32 MiB**. Penalty:
any write smaller than 256 KB forces read-modify-write of the whole chunk
(B2(a)'s pattern at chunk scale); worst triggered by small random writes —
the dominant pattern of metadata-heavy file systems.
**(c)** Data table 32 MiB + log table: 1% of capacity page-mapped =
2²⁹ × 1% ≈ 5.4 × 10⁶ entries × 4 B ≈ 20.5 MiB → **~52 MiB** total. Merges:
**switch** — the log block was written with exactly the pages of one chunk
in order; repoint one block pointer at it (cheapest; produced by sequential
overwrite of a whole chunk). **Partial** — only some pages were
overwritten; copy the remaining live pages from the old block into the log
block, then switch (produced by partial sequential overwrite). **Full** —
the log block holds pages from *several* chunks; for each, gather that
chunk's pages from wherever they live and write a fresh consolidated block
(produced by scattered random writes; the expensive case to avoid).
**(d)** Keep the full page-level map on flash but cache only the hot
entries in memory — betting on **workload locality** of the translation
working set. When the bet fails: each access first *reads* the missing
mapping from flash (extra read), and installing it may evict a dirty
mapping that must be *written* first — so a cache-busting random workload
pays up to twice extra I/O per access, on top of the data access itself.

**B4.**
**(a)** Data 1, 2, 3, 4: additive = **10**; XOR = 1⊕2⊕3⊕4: 1⊕2 = 3, 3⊕3 =
0, 0⊕4 = **4**. Fletcher: s1 runs 1, 3, 6, **10**; s2 runs 1, 4, 10, **20**
→ (10, 20).
Data 4, 3, 2, 1: additive = **10** (same); XOR = **4** (same); Fletcher: s1
runs 4, 7, 9, **10** — but s2 runs 4, 11, 20, **30** → (10, 30).
Demonstrated: additive and XOR are **order-blind** (any permutation
collides); Fletcher's position-weighted second sum detects the reordering.
**(b)** Additive collides whenever the byte multiset sums to the same total
(permutations, or compensating changes like +1 here, −1 there). XOR
collides whenever each bit position flips an even number of times — e.g.
the same two-bit change in two different words. Fletcher's s2 effectively
weights each byte by its position (s1 is added into s2 after every byte),
so pure reorderings and many compensating changes alter s2 even when s1
matches.
**(c)** 8 B / 4 KB = 1/512 ≈ **0.19%**. Pigeonhole: 2^(8·4096) possible
blocks map onto 2⁶⁴ checksums, so collisions must exist. Acceptable because
corruption is not choosing adversarial blocks: the chance a *fault* lands
on one of the astronomically rare colliding blocks is negligible, and the
checksum needs only to beat the fault model, not a cryptographic adversary.
**(d)** XOR is the faster loop (one op per byte, no modulo); Fletcher costs
slightly more and detects far more (all single- and double-bit errors, many
bursts — "almost as strong as CRC"); both are memory-bandwidth-bound on
large files, which is the practical point. Ch. 45's mitigation: **fuse
checksumming into a data copy** the system performs anyway (kernel cache →
user buffer), so the pass over the bytes is shared and the marginal cost
nearly vanishes.

**B5.**
**(a)** Matrix (D = detects, R = enables recovery, — = neither):

| Failure | In-disk ECC | Checksum beside data | + physical ID | Parent checksum | RAID redundancy | Scrubbing |
|---|---|---|---|---|---|---|
| (1) LSE | D (returns error) | — (block unreadable anyway) | — | — | R | D (surfaces it early) |
| (2) In-place corruption | — (silent) | D | D | D | R (once detected) | D (applies checks to cold data) |
| (3) Misdirected write | — | — ✻ | D | D | R | D (with ID checks) |
| (4) Lost write | — | — | — | D | R | D (with parent checks) |
| (5) Whole-disk failure | — | — | — | — | D + R | — |

✻ The four least-obvious cells, justified: **(3)/checksum-beside-data — no**:
the misdirected block arrives *with its own valid checksum*, which verifies
perfectly at the wrong address; only an embedded (disk, block) identity
exposes the mismatch. **(4)/physical ID — no**: the stale block is at the
right address with the right ID and a checksum valid for its (old)
contents; nothing local is wrong — hence parent-held checksums or
write-verify. **(1)/checksum — no**: the read fails before any comparison
can happen; ECC has already spoken, and what's needed is another copy, not
detection. **(2)/RAID — recovery only**: RAID alone doesn't *notice* silent
corruption (it reconstructs on error signals); paired with checksum
detection it supplies the good copy.
**(b)** Cheap fleet: 1,000 × 9.4% ≈ **94 drives** with an LSE and
1,000 × 0.5% ≈ **5** with silent corruption over ~3 years. Costly fleet:
**14** and **0.5** (about one drive every other fleet-lifetime). Better
drives cut both by ~10× but neither to zero — at fleet scale corruption
remains an *expected* event, so checksums are required either way;
paying for enterprise drives changes the constant, not the conclusion.
**(c)** In normal operation an LSE is recovered from redundancy. During
reconstruction, the failed disk's redundancy is *already spent*: rebuilding
each stripe requires reading **all** surviving members, and an LSE on any
of them means that stripe cannot be reconstructed — a partial fault
promoted to data loss by coinciding with a full fault. Response: an extra
degree of redundancy — double parity (RAID-DP/RAID-6-style), which
tolerates one whole-disk failure *plus* an LSE.
**(d)** Read-time checking only protects data that gets read; cold data
can rot unobserved until *all* copies are bad, at which point detection is
useless. Scrubbing interrupts that dynamic by periodically reading and
verifying everything, finding single-copy damage while a good copy still
exists (the study: most LSEs were found by scrubbing). Policy: nightly or
weekly passes scheduled in idle windows (ch. 45's "middle of the night"),
rate-limited so patrol I/O doesn't degrade foreground service — and see C3
for tuning it by measurement.

---

## C. Discussion and design critique

**C1.** Marking notes — a strong answer designs a *power-fault torture
rig*: a writer issues a known, logged pattern of writes (each block
self-describing: sequence number, timestamp, checksum), with and without
flush/FUA barriers; a controller cuts power at random offsets mid-burst,
thousands of trials; after each cut, mount read-only and classify every
block as current / old-but-intact / torn / vanished-after-acknowledge.
- **Decision rule:** if acknowledged-and-flushed writes are ever missing or
  torn, or if write *ordering across a flush* is violated, the device
  cannot support journal-free operation — B wins outright (the journal's
  correctness itself also needs the barrier honoured, so such a device is a
  problem either way). If acknowledged writes always persist atomically and
  in order across cuts, A's position becomes *plausible* — but only for
  this firmware revision.
- **Why 100 clean runs is weak:** the failure is a race between the power
  cut and the FTL's internal state (mapping-table persistence, cached
  acknowledgements); the window may be microseconds and workload-dependent,
  so absence of evidence over a small sample bounds nothing. The test must
  target the window (cut *during* heavy small-write bursts, during GC —
  provoked by prefilling the device) and report a rate with confidence, not
  an anecdote.
- **Confounds to control:** device write cache on/off, TRIM state and
  prefill (a fresh SSD's FTL behaves unlike a full one), firmware version,
  and the host's own reordering (issue via direct I/O).
- The conceptual point earning top marks: A confuses *the FTL's* crash
  consistency (its mapping survives) with *the file system's* (your
  transaction semantics survive) — the experiment is designed exactly to
  separate the two.

**C2.** The measurement: sample drives across the fleet and record, over
several weeks, (i) host bytes written per drive, (ii) device-reported
program/erase activity — the ratio being **write amplification**, the
quantity that converts host writes into wear; multiply out P/E consumption
per block against the 10,000 rating to project lifetime. Ignoring WA wrecks
the estimate because host-side accounting can be off by the whole
amplification factor (2–10× for random-write-heavy hosts, ~1× for
sequential ones) — purchasing's 18-month figure and your 5-year figure can
*both* be produced from the same host numbers by assuming different WA.
Skew effects: fleets are heterogeneous — a database shard doing constant
small syncs ages 20× faster than a log-archival node, so report the
*distribution* (p95 drive, not the mean), and check within-drive skew is
already handled by wear leveling (device wear counters, where exposed, are
per-drive maxima). Decision rule to hand over: project the p95 drive's
time-to-rated-cycles under measured WA; replace classes of drives whose
projection falls inside the hardware refresh horizon, keep the rest, and
re-measure quarterly — with the caveat (ch. 44 / Boboila & Desnoyers) that
rated cycles are conservative, so treat the rating as a floor and instrument
for actual failures rather than pre-emptively replacing on the rating
alone.

**C3.** Marking notes — the programme, not a number:
- **Record per pass:** per-drive counts of checksum mismatches and LSEs
  found, their *addresses* (for spatial-locality analysis), drive model and
  age, and whether redundancy still held a good copy at detection time —
  the last being the safety margin the whole exercise protects.
- **Exploit the findings:** locality means errors cluster — a drive with
  one detected fault is disproportionately likely to hold or grow more; so
  the policy should not be one global rate but *adaptive*: baseline passes
  fleet-wide, immediately escalated (full re-scrub, neighbourhood scans,
  pre-emptive rebuild/replacement) for any drive with a hit. Temporal
  locality similarly argues for re-scrubbing recent offenders sooner.
- **Risk of the window:** what matters is the probability that a *second*
  fault lands on the last good copy before the next pass finds the first —
  estimate it from the measured per-drive fault rate, the observed
  clustering, and rebuild time; that yields expected-data-loss as a
  function of scrub interval.
- **Cost side:** measure foreground latency degradation during scrubs and
  size passes into idle windows; the trade is expected-loss(interval)
  against interference(interval), and the right interval is where marginal
  risk reduction stops paying for marginal interference — recomputed as the
  fleet ages, since error rates rise in year two (the study's finding).
- Weak answers pick "weekly" and defend it; strong answers hand back a
  feedback loop with an escalation rule and show which measured quantity
  moves the dial.
