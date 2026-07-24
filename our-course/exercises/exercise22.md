# Exercise Sheet 22 — Flash-based SSDs and data integrity

**Attempt after Week 22.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise22-solutions.md`](solutions/exercise22-solutions.md).

**This sheet leans on:** OSTEP ch. 44–45. It draws on ch. 43 (week 21) for
log-structuring and cleaning, ch. 42 (week 21) for journaling, and ch. 38
(week 16) for the RAID fault model.

**You will need:** the `ssd.py` (ch. 44) and `checksum.py` (ch. 45)
simulators from the `ostep-homework` repo, and a C compiler for B4(d).

> **Note.** Week 22 is the lightest in the course, so this sheet runs
> slightly fuller than usual: §B has five questions. §B2–B3 and §B5 are
> pen-and-paper.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** An SSD can overwrite a 4 KB page in place, provided the page has been
programmed before.

**A2.** On a modern SSD, random reads are faster than random writes, since a
read involves no erasing.

**A3.** Wear leveling extends an SSD's lifetime by reducing the total number
of erase operations performed.

**A4.** Adding TRIM support to a hard disk drive would improve its
performance in the same way it improves an SSD's.

**A5.** A per-block checksum allows a file system to repair a corrupted
block.

**A6.** Latent sector errors are dangerous chiefly because they are silent.

**A7.** Storing a physical identifier (disk and block number) alongside each
block's checksum catches lost writes.

**A8.** Block-level FTL mapping needs far less memory than page-level
mapping, but makes writes smaller than a flash block expensive.

---

## B. Working the mechanisms

**B1. The FTL under simulation.**
Using `ssd.py` defaults (erase 1000 µs, program 40 µs, read 10 µs):
  (a) Run `python3 ssd.py -T log -s 1 -n 10 -q`, work out which operations took
      place, and check with `-c`. Then, from the op counts alone, *estimate*
      the total time before verifying with `-S`.
  (b) Repeat with `-T direct`. Predict the ratio of write costs
      (direct-mapped vs log-structured) before checking. Where does the
      direct FTL's time go?
  (c) Run a larger workload (`-T log -n 1000 -s 1`) with garbage collection
      enabled via watermarks — set them low enough to fire on the small
      default device, e.g. `-G 6 -g 4` (the default `-G 10` never triggers, so
      the device fills and writes start failing) — plus `-J` to watch the
      collector. Pin `-s 1` throughout so your numbers are reproducible.
      What triggers cleaning, what does each cleaning step cost, and which
      statistic reported by `-S` best captures the write-amplification
      overhead?

**B2. Write amplification, by hand.**
An SSD has 64-page blocks (4 KB pages).
  (a) For a workload of isolated 4 KB overwrites to a **full** device,
      compute the write amplification of a direct-mapped FTL, and its
      per-write time cost using the latencies above. Compare with the
      log-structured FTL's cost for the same overwrite (amortise the erase
      over a block's worth of programs).
  (b) A log-structured FTL cleans blocks at live fraction `u`. Derive the
      cleaning traffic per freed block, and the resulting write
      amplification 1/(1−u). Evaluate at u = 0.5, 0.8, 0.9.
  (c) The manufacturer adds 20% hidden over-provisioning. Give two distinct
      mechanisms by which this reduces write amplification or its perceived
      cost.
  (d) Rank these workloads by expected write amplification on the same
      device, justifying each placement: (i) sequential large-file writes;
      (ii) uniform random 4 KB writes over the whole device; (iii) random
      4 KB writes where 80% of writes hit 20% of the blocks, after a bulk
      load of cold data.

**B3. The mapping-table problem.**
A 2 TB SSD uses 4 KB pages, 256 KB blocks, and 4-byte table entries.
  (a) Compute the memory needed for a pure page-level mapping table.
  (b) Compute it for a pure block-level mapping. State the performance
      penalty this buys, and the workload that triggers it worst.
  (c) A hybrid FTL keeps 1% of capacity as page-mapped log blocks and the
      rest block-mapped. Compute its table size. Then explain the three
      merge operations (switch, partial, full) that keep the log-block set
      small, identifying the workload pattern that produces each.
  (d) State the page-mapping-plus-caching alternative and the workload
      property it bets on. What happens — mechanically — when the bet fails?

**B4. Checksums, computed.**
  (a) By hand, compute the additive, XOR-based, and Fletcher checksums of
      the 4-byte data `1, 2, 3, 4`, then of `4, 3, 2, 1`. (Check with
      `python3 checksum.py -D ...`.) What do the results demonstrate about the
      three functions?
  (b) In general, when do two different data blocks collide under the
      additive checksum? Under XOR? Why does Fletcher's second sum fix the
      case from (a)?
  (c) An 8-byte checksum guards each 4 KB block. Compute the on-disk space
      overhead. Why can no checksum of this shape be collision-free, and
      why is that acceptable?
  (d) *(Code.)* Write `check-xor.c` and `check-fletcher.c` computing the
      XOR (one unsigned byte) and Fletcher checksums of a file. Time both
      over a large input. Which wins on speed, which on detection, and what
      does ch. 45 suggest systems do to hide checksum cost on the read
      path?

**B5. Matching defence to failure.**
  (a) Build the matrix: for each failure — (1) latent sector error,
      (2) bit-level corruption in place, (3) misdirected write,
      (4) lost write, (5) whole-disk failure — say whether each defence
      detects and/or recovers it: in-disk ECC; per-block checksum beside
      the data; checksum + physical ID; checksum stored in the parent
      structure (ZFS-style); RAID-1/RAID-5 redundancy; scrubbing. Justify
      the four cells you found least obvious.
  (b) Using ch. 45's measured rates (cheap drives: 9.4% LSE, 0.5%
      corruption over ~3 years; costly: 1.4%, 0.05%), compute the expected
      number of affected drives in a 1,000-drive fleet of each class. What
      do these numbers imply about whether "buy better drives" substitutes
      for checksums?
  (c) An LSE encountered *during* RAID-5 reconstruction of a failed disk is
      qualitatively worse than one met in normal operation. Explain why,
      and name the design response the chapter describes.
  (d) Why does scrubbing exist at all, given that every read already
      verifies checksums? State the failure dynamic it interrupts and a
      sensible scheduling policy for it.

---

## C. Discussion and design critique

*This week's discussion questions ask **what would you measure?** Given a
disagreement, design the experiment or measurement that settles it. Say what
you would measure, how, what result decides which way, and what confounds
you have controlled.*

**C1.** Colleague A: "On an SSD the file-system journal is redundant — the
FTL is already log-structured and never overwrites in place, so torn and
reordered writes can't happen; we should mount without a journal and enjoy
the speed." Colleague B: "The FTL's log protects the FTL's invariants, not
yours; on power loss the device may still reorder or drop acknowledged
writes, so the journal stays." Design the experiment that settles what this
*specific* SSD actually guarantees across power failure, and state the
decision rule: which observed behaviours would justify dropping the
journal, which would mandate keeping it, and why "we ran it 100 times and
saw nothing" is a weak result. (Assume you can cut power at will and
inspect the device afterwards.)

**C2.** Your fleet's MLC SSDs are rated at 10,000 P/E cycles, and ch. 44
notes measured lifetimes often far exceed ratings. Storage purchasing
claims the drives will wear out in 18 months and wants costly replacements;
you suspect they will last five years. What would you measure — on the
running fleet, not in a lab — to settle it? Identify the quantity that
converts host writes into device wear (and why ignoring it wrecks the
estimate), the skew effects that make a fleet *average* misleading, and the
decision rule you would hand purchasing.

**C3.** How often should the fleet scrub? One camp says weekly ("ch. 45
says corruption is real"), another says monthly ("scrubbing steals I/O from
customers"). Using the study's findings that corruptions and LSEs show
spatial and temporal locality, and that most LSEs were *found* by
scrubbing, design the measurement programme that would let you set — and
keep adjusting — the scrub rate for your own fleet. State what you would
record per scrub pass, how you would estimate the risk of the window
between passes, and the operational cost you would trade it against.
