# Exercise Sheet 16 — Disks and RAID

**Attempt after Week 16.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise16-solutions.md`](solutions/exercise16-solutions.md).

**This sheet leans on:** OSTEP ch. 37–38; Patterson, Gibson & Katz (1988),
the RAID paper.

**You will need:** the OSTEP simulators `disk.py` (from
`ostep-homework/file-disks/`) and `raid.py` (from `ostep-homework/file-raid/`)
for §B2 and §B4. Note: `disk.py` imports Tkinter at startup even for `-c`
compute runs, so install it first (`sudo apt install python3-tk` on Debian/
Ubuntu) or every §B2 command aborts with `ModuleNotFoundError: No module named
'tkinter'`. §B1 and §B3 are pen-and-paper — do them without a calculator if
you can; the exam room won't have one either.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns
nothing.*

**A1.** The average rotational delay of a 10,000 RPM disk is 6 ms.

**A2.** The OS can implement true shortest-seek-time-first scheduling.

**A3.** SPTF (shortest positioning time first) is best implemented inside
the drive rather than in the OS.

**A4.** A modern disk's random-I/O throughput is typically within a factor
of ten of its sequential throughput.

**A5.** A drive that acknowledges a write once the data reaches its cache
is faster, and just as safe, as one that waits for the media.

**A6.** A mirrored array of N disks (RAID-1) can only ever survive one disk
failure.

**A7.** RAID-5 eliminates RAID-4's small-write problem.

**A8.** In RAID-4, a full-stripe write requires no reads at all.

---

## B. Geometry and arithmetic

**B1. The disk model, by hand.**
A drive spins at 10,000 RPM, has an average seek of 4 ms, and transfers at
100 MB/s. Workloads: random 4 KB reads, and sequential 10 MB reads.
  (a) Using dimensional analysis (ch. 37 style — write the units out),
      compute the time of one rotation and the average rotational delay.
  (b) Compute `T_I/O` and the effective rate `R_I/O` for the random
      workload. Which term dominates, and by how much?
  (c) Compute both for the sequential workload.
  (d) Give the ratio of sequential to random throughput, and state the
      design tip this number justifies.
  (e) Marketing proposes doubling the spindle speed to 20,000 RPM. Recompute
      the random-read `T_I/O`. Roughly what speedup does the customer see,
      and why is it so much less than 2×?

**B2. Scheduling policies, simulated.**
Using `disk.py` (defaults unless stated):
  (a) For the request stream `-a 7,30,8`, work out the service order and
      the total (seek + rotation + transfer) time under FIFO, then check
      with the simulator. Why is FIFO poor here?
  (b) Run `-p SSTF` and `-p SATF` on the same stream. Explain any
      difference in the order chosen and in the total time — and if SATF
      wins, say what information it exploited that SSTF ignores.
  (c) Construct a request stream under which SATF, run greedily with a
      large scheduling window, can starve a request indefinitely while the
      total work done stays high. How does bounding the window (`-w`,
      BSATF) fix this, and what does the fix cost?
  (d) The stream `-a 10,11,12,13` performs badly on the default disk. Say
      why, and explain what track skew (`-o skew`) does about it. What
      determines the right skew value?

**B3. RAID arithmetic.**
An array has `N = 8` disks, each with sequential bandwidth `S = 80` MB/s and
random (small-request) bandwidth `R = 1` MB/s.
  (a) For a chunk size of one 4 KB block, which disk and offset hold
      logical block 53 under RAID-0? Under the RAID-4 layout of ch. 38
      (parity on disk N−1... state your convention), which disk holds the
      parity for block 53's stripe?
  (b) Build the full throughput table for RAID-0, RAID-1, RAID-4 and
      RAID-5 on this array: sequential read/write, random read/write.
      (Derive, don't recall — each entry should have a one-line reason.)
  (c) A transaction-processing workload is dominated by small random
      writes. Rank the four levels for it, and quantify: how many times
      slower is RAID-4 than RAID-0 here? Why does adding disks not help
      RAID-4's number at all?
  (d) A small write to a RAID-4 stripe can update parity **additively**
      (read the other data blocks, XOR with the new data) or
      **subtractively** (read old data + old parity; new parity =
      (C_old ⊕ C_new) ⊕ P_old). Count the physical I/Os for each on an
      N-disk array, and find the N at which the two methods cost the same.
  (e) A stripe on a 5-disk RAID-4 holds (in 4-bit miniature) data blocks
      `0110`, `1011`, `0001`, `1101`. Compute the parity block. Disk 2
      (holding `1011`) fails: show the reconstruction of its block from
      the survivors.

**B4. RAID, simulated.**
Using `raid.py`:
  (a) Run the timing mode over 100 random reads on 4 disks
      (`raid.py -t -n 100 -D 4 -c`) at levels 0, 1, 4, 5 (`-L`). Report and
      explain the ranking.
  (b) Repeat with 100 random writes (add `-w 100`). Predict the RAID-4 time
      before running, reasoning from your §B3 analysis.
      Then check, and explain any gap.
  (c) With the sequential workload (`-W sequential`), what request size
      makes RAID-4/5 writes efficient, and what property of the layout does
      that size correspond to?

---

## C. Discussion and design critique

**C1. What would you measure?**
Your team is provisioning storage for a new service and has stalled in a
meeting. Engineer X: "The workload is basically sequential — logs and bulk
reads — so RAID-5 gives us capacity for free; the small-write penalty is
irrelevant." Engineer Y: "Every service we've ever shipped turned out to be
random-write-dominated once the database went in; RAID-5 will fall off a
cliff and we should pay for mirroring."

Design the measurement campaign that settles this with data rather than
adjectives, assuming you can instrument a staging deployment running
realistic traffic.
  (a) List the quantities you would measure, and for each say *why it
      discriminates* between the two claims. (Think about what actually
      parameterises the ch. 38 analysis: read/write mix, request sizes,
      sequentiality — and how you'd detect sequentiality in a trace —
      queue depths, burstiness, and the full-stripe-write fraction.)
  (b) Give the decision rule in advance: state, in terms of your measured
      quantities and the ch. 38 throughput table, the conditions under
      which you choose RAID-5, and the conditions for RAID-1. A rule that
      cannot come out either way is not a rule.
  (c) Both engineers implicitly assume the workload at launch is the
      workload forever. What would you measure *in production, continuously*
      to catch the assumption failing, and what is your migration story if
      it does?

**C2. The fault model under the floorboards.**
Ch. 38 evaluates every RAID level under the fail-stop model: disks fail
loudly, entirely, and detectably, and working disks return correct data.
Make the strongest case **against** trusting a reliability claim built on
that model — the chapter itself hands you the two loopholes (silent block
corruption; latent sector errors), and the consistent-update problem
supplies a third failure that isn't a disk failure at all. For each, say
what a RAID-5 array does when it strikes — in particular, what happens
during a rebuild after one honest fail-stop failure if a second, *quiet*
fault has been sitting undetected on a surviving disk. What does your
argument imply an array should be doing during idle time?
