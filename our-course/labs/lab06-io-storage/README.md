# Lab 6 — I/O & storage

**Weeks 15–17 · 11.5 hours · OSTEP ch. 36 (I/O devices), ch. 37 (hard disks), ch. 38 (RAID) · Silberschatz §12.4.2, §12.4.4 (buffering)**

Userspace, from scratch. **C or Python — your choice** (see below). A simulator,
not a driver: no kernel work, no xv6, no QEMU, no root. You will need `bash`,
`gcc` and `make` for the C path, or `python3` for the Python path.

You are building two simulators and the analysis they support. The first is a
disk: a cost model (seek, rotation, transfer) driven by a request scheduler you
can swap between FCFS, SSTF, SCAN and C-SCAN. The second is a RAID array:
levels 0, 1, 4 and 5, with real XOR parity, degraded-mode reads that
reconstruct missing data, and a rebuild onto a replacement disk. A third, small
tool models a buffered character device. All three are yours end to end — you
write the block mapping and the parity arithmetic, not a script over someone
else's library.

The point is not the arithmetic; sheet 16 does the arithmetic. The point is
what the arithmetic lets you skip and a simulator does not: what SCAN does when
the queue empties mid-sweep, and exactly which blocks a RAID-5 rebuild has to
read. Owning the model is the lesson.

## Layout

```
lab06-io-storage/
  README.md          this handout
  starter/           iobuf.c, disksim.c, raidsim.c, Makefile   <- work here
  fixtures/          the scheduler test streams
  tests/run.sh       the autograder for Parts 1-5
  solutions/         SPOILERS. Reference tools, RESULTS.md, answer key. Later.
```

Copy the working directories somewhere of your own and work there. Copy all of
`starter/`, `tests/` and `fixtures/`, not just `starter/` — the autograder
refers to `../fixtures/…` and those paths only resolve if the layout comes with
you:

```sh
cp -r starter tests fixtures ~/lab6
cd ~/lab6/starter
make            # builds iobuf, disksim, raidsim
make test       # ../tests/run.sh on this directory
```

`solutions/` is deliberately left behind.

## What you hand in

| File(s) | Part | Weight |
|---|---|---|
| `iobuf` — buffering strategies, polling/interrupt/DMA | 1 | 22% |
| `disksim` — the disk cost model and workload generator | 2 | 18% |
| `disksim` — FCFS, SSTF, SCAN, C-SCAN | 3 | 22% |
| `raidsim` — RAID 0/1/4/5, parity, additive/subtractive updates | 4 | 26% |
| `raidsim` — degraded reads and rebuild | 5 | 12% |
| `RESULTS.md` — the three results tables and their interpretation | 1–5 | rubric |

`RESULTS.md` is marked by hand against the rubric in `solutions/README.md`,
which you read *after* you have your own numbers, not before. A green autograder
run is not a finished lab — see "What is checked", below.

---

# ⚠ Language and the text interface

**Write in C or in Python; the autograder does not care.** It drives your tools
entirely through their command line — arguments in, text out — and never links
against them. If you write C, `make` builds three executables `iobuf`,
`disksim`, `raidsim` and the autograder compiles your `.c` itself (its own
`-Wall -Wextra -Werror` line, so your Makefile cannot change the grade). If you
write Python, deliver three files named `iobuf`, `disksim`, `raidsim`, each with
a `#!/usr/bin/env python3` shebang and made executable (`chmod +x`), and the
autograder runs them directly.

**Whichever you choose, the command line and every output line are a fixed
contract.** The starter's `printf`/`print` statements are the specification;
the autograder greps for those exact strings. Change what a line *says* and the
autograder cannot read it. The sections below give the contract in full; the
starter carries it in comments.

---

# Part 1 — A buffered device interface (~2.5 h, weeks 15–16, 22%)

`iobuf` models a slow output device fed by a CPU producer, in two experiments.

## Buffering strategies

```
iobuf buf <unbuffered|double|circular> n=<N> tdev=<D> tcpu=<C> [depth=<K>] [burst=<B>]
```

The CPU produces `N` units; the device drains each in `D` ticks. Producing unit
*i* costs some CPU time; the device cannot drain a unit until the CPU has
produced it, and the CPU cannot reuse a buffer until the device has drained it.
The three strategies differ only in how many buffers sit between them:
**unbuffered** is one buffer (K=1), **double** is two (K=2), **circular** is a
ring of `depth` buffers (default 8).

Model it as a bounded-buffer producer/consumer with K slots. Let `prod[i]` be
the CPU time for unit *i*. A slot must be held to produce into, and frees when
its unit is drained:

```
acquire_i = max(ready_{i-1}, drained_{i-K})     (producer free, and a slot free)
ready_i   = acquire_i + prod[i]
drained_i = max(ready_i, drained_{i-1}) + D
```

with `ready_j` and `drained_j` taken as 0 for *j* < 0. The completion time of
the burst is `drained_{N-1}`; print it. With K=1 this is fully serial —
`N × (C + D)`. With K=2 the CPU and device overlap. With K large the ring can
run ahead of a *bursty* producer.

The producer is steady (`prod[i] = tcpu`) unless you pass `burst=B`, which makes
it compute `B` cheap units (1 tick each) and then one expensive unit (`B × tcpu`
ticks), repeating. **A steady producer makes double and circular identical** —
depth buys nothing when the rates never vary. A bursty producer is what
separates them: the extra slots let the ring fill up during the cheap run so the
device never goes idle, while a double buffer stalls. Your `RESULTS.md` must
show both cases and explain why depth helps only one of them.

Output line (fixed):

```
buf strategy=circular n=40 tdev=10 tcpu=3 depth=8 burst=6 time=401
```

## Polling versus interrupts versus DMA

```
iobuf io <polling|interrupt|dma> n=<N> tdev=<D> [hoverhead=<H>] [setup=<S>]
```

Count the CPU cycles each mechanism *wastes* — spends without doing useful work
— moving `N` units:

- **polling**: the CPU busy-waits the whole device time for every unit → `N × D`.
- **interrupt**: the CPU sleeps and is woken; each unit costs `H` handler
  cycles → `N × H`.
- **dma**: the CPU programs the transfer once (`S` cycles) and takes one
  completion interrupt (`H`) for the whole burst → `S + H`.

Output line (fixed):

```
io mode=interrupt n=16 tdev=100 hoverhead=50 setup=100 wasted=800
```

The chapter-36 point falls out of these three numbers: polling wins for a fast
device (small `D`), interrupts win for a slow one (`D > H`), and DMA wins for
anything bulk. Find the crossover and put it in `RESULTS.md`.

---

# Part 2 — A disk model and a workload generator (~2.0 h, week 16, 18%)

```
disksim run <policy> <stream-file> [C=.. spt=.. seek=.. rot=.. xfer=.. start=.. dir=up|down]
disksim gen <seq|rand|mixed> n=.. seed=.. maxlba=.. [loc=..]
```

### The request stream

A text file, one request per line: `<lba> [nsec]`, where `nsec` (default 1) is
the number of sectors transferred. `#` starts a comment; blank lines are
ignored. `gen` writes streams in exactly this format — it is scaffolding, given
to you, and produces sequential, random or locality-controlled (`loc`) mixed
streams. **The grader does not test `gen`'s output** (your PRNG is your own); it
grades the model and schedulers against the fixed fixture streams in
`fixtures/`.

### The geometry and the cost model

The disk has `C` cylinders and `spt` sectors per track. Logical block `lba` maps
to **cylinder `lba / spt`, sector `lba % spt`**. Defaults:
`C=500 spt=100 seek=1 rot=1 xfer=1 start=0`.

`T_io = T_seek + T_rot + T_transfer`, in ticks:

- **seek** `= |cyl_to − cyl_from| × seek`. A linear model.
- **rotation**: the platter never stops turning — one sector passes under the
  head every `rot` ticks, so a full rotation is `spt × rot` ticks. After the
  seek, the head is at whatever rotational position the elapsed time `t` left it.
  The target sector's leading edge is at `sec × rot` within a rotation; the head
  is at `t mod (spt × rot)`. So

  ```
  T_rot = (sec × rot − (t mod (spt × rot)) + spt × rot) mod (spt × rot)
  ```

  This is the part arithmetic lets you skip: the rotational latency of a request
  depends on when the head arrives, which depends on every request before it.
- **transfer** `= nsec × xfer`.

Print, per request, and then a summary (both lines fixed):

```
served idx=0 lba=250 cyl=2 sec=50 seek=2 rot=48 xfer=1 done=51
summary policy=fifo served=1 seek_total=2 service_total=51 order_cyl=2 order_lba=250
```

`seek_total` is the total head movement **in cylinders**; `service_total` is the
completion time of the last request **in ticks**. `done` is a request's own
completion time. A worked example, from `start=0` under the default geometry,
serving `250` then `3070` then `30`: the three completion times are `51`, `171`,
`231`. Reproduce those before you trust the model.

---

# Part 3 — Disk scheduling policies (~2.5 h, week 16, 22%)

Add `sstf`, `scan` and `cscan` to `disksim run` (`fifo` is done for you). All
four schedule the **whole stream, offline**: every request is present at time 0,
and no new requests arrive mid-sweep. That removes the ambiguity that otherwise
makes two students' numbers incomparable, and it is the model sheet 16 uses too.

The policies are defined by the **service order** they produce, and the fixtures
assert that order exactly. The definitions, pinned down so everyone agrees:

| Policy | Chooses the next request by |
|---|---|
| `fifo` | arrival order |
| `sstf` | **nearest cylinder** to the head. Ties go to the **lower cylinder**, then to the earlier arrival |
| `scan` | sweep in the current direction (default **up**), serving requests in cylinder order; when none remain ahead, **reverse and serve the rest**. Reverse at the last *requested* cylinder, not at the disk edge (this is the LOOK form) |
| `cscan` | sweep up serving requests in cylinder order; then instead of reversing, **jump back to the lowest pending request** and sweep up again. The jump is real head movement and is counted in `seek_total` (the C-LOOK form) |

Run all four over each workload and report total and per-request service time in
`RESULTS.md`. **The numbers must genuinely differ** — a workload on which FCFS,
SSTF and SCAN post the same total is a workload that says nothing. The
separation fixture `fixtures/disk-sep.stream` is built to force the difference;
`fixtures/README.md` gives its hand-computed numbers.

Then the starvation demonstration. `fixtures/disk-starve.stream` is a cluster of
requests near the head plus one lone request far away. Under SSTF the head keeps
getting pulled back to the cluster and the outlier is served **last**; under
SCAN the sweep reaches it in one pass. Report the outlier's service position
under each, and note that this is *one* of two complementary fixes for the same
pathology: SCAN fixes it by **sweeping**, and sheet 16's BSATF fixes it by
**bounding the window**. They are different answers, not the same answer twice.

---

# Part 4 — A RAID engine (~3.0 h, weeks 16–17, 26%)

`raidsim` reads a command script from a file argument (or `-` for stdin) and
runs it against an in-memory array. The command loop, the argument parsing, the
physical-block accessors *with their I/O counters*, and the `checksum` are given
to you. You build the geometry, the parity and the read/write paths.

### Blocks, chunks and stripes

```
init <level> <ndisks> <chunk> <blocks_per_disk> <blocksize>
```

A **block** is the unit of I/O — one logical address, `blocksize` bytes. A
**chunk** (stripe unit) is `chunk` *consecutive blocks* placed on one disk
before striping moves to the next disk. **These are not the same thing**, and
conflating them breaks every mapping below — define both and keep them straight.
The starter uses `chunk=1` in most examples so you can check the mapping by
hand, and one test uses `chunk=2` to catch the confusion.

### The four levels

| Level | Layout | Usable capacity | Survives |
|---|---|---|---|
| 0 | stripe across all N disks, no redundancy | `N × blocks_per_disk` | nothing |
| 1 | mirror: every disk holds the same data | `blocks_per_disk` | up to N−1 failures |
| 4 | N−1 data disks + one **dedicated** parity disk | `(N−1) × blocks_per_disk` | one failure |
| 5 | N−1 data disks; parity **rotates** with the stripe | `(N−1) × blocks_per_disk` | one failure |

`capacity` prints the usable block count. `place <lba>` prints where a block
lives — for 4/5, its data disk, row, stripe and the parity disk for that stripe.

**RAID-5's parity disk must depend on the stripe number.** Stripe 0's parity is
on disk `N−1`, stripe 1's on disk `N−2`, and so on, wrapping. An off-by-one here
passes casual testing and fails rebuild, so `place` is checked against exact
expected values. The data columns of a stripe **skip** that stripe's parity
disk.

### Parity and the read/write paths

Parity is plain XOR: a stripe's parity block is the XOR of its data blocks. Any
one missing block in a stripe is the XOR of all the others — which is the whole
trick, and Part 5 depends on it. `readraw <disk> <row>` reads a raw physical
block (parity included) so you can check your parity directly; after writing
`0xAA, 0xCC, 0x0F` into a stripe's three data disks, its parity block must read
`0x69`.

A small write — one data block — must update the parity too, and there are two
ways, which you must **both** implement and select between by cost when
`parity <auto>` is set (`parity <additive|subtractive>` forces one):

- **subtractive**: `new_parity = old_parity ⊕ old_data ⊕ new_data`. Reads the
  **old** data and the **old** parity — 2 reads, 2 writes, whatever the array
  width.
- **additive**: read **every other** data block in the stripe and XOR with the
  new data — `(data_disks − 1)` reads, 2 writes.

The `iostat` counter makes the difference visible, and it is the whole content
of the "small-write problem": one logical block written costs four physical
I/Os at best. A write labelled subtractive that actually reads every other
block is the classic bug; `iostat` catches it. (For a *narrow* array additive is
cheaper — find where the crossover is, and confirm `auto` picks the cheaper
side.)

---

# Part 5 — Degraded mode and rebuild (~1.5 h, week 17, 12%)

```
fail <disk>        mark a disk failed: its contents become inaccessible
rebuild <disk>     reconstruct a failed disk onto a fresh replacement
```

After a `fail`, reads whose data lived on the failed disk must be served by
**reconstruction** — XOR the surviving blocks of the stripe — not by returning
the zeroed replacement. A `read` that cannot be recovered (RAID-0, or a second
failure) prints `LOST`. `rebuild` reconstructs every block of the failed disk
onto a fresh replacement and prints how many physical blocks it had to read;
for a 4-disk RAID-5 with `r` rows that is `3r` (the three survivors per row).

The verification is byte-identity. `checksum` prints a 64-bit hash over every
logical block, read through redundancy — so an intact array, a correctly
reconstructed degraded array, and a correctly rebuilt array all print the **same**
checksum. The autograder fills an array with a known pattern, checksums it,
fails **each disk in turn**, checks the degraded checksum is unchanged, rebuilds,
and checks the rebuilt checksum is unchanged. For RAID-0 it checks the opposite:
a failure changes the checksum and a read reports `LOST`, because the negative
case is as instructive as the positive.

---

# Running the tests

```sh
make            # C path: builds the three tools, -Wall -Wextra -Werror clean
make test       # ../tests/run.sh on this directory
```

`run.sh` compiles each `<tool>.c` **itself**, with its own `-Wall -Wextra
-Werror` line, into its own temporary directory — so your Makefile is never the
graded build and cannot quietly drop a warning flag. (It runs your Makefile once
too, as a smoke test, because `make` is the documented workflow.) For a Python
submission it finds the executable `iobuf`/`disksim`/`raidsim` in your directory
and runs them directly. Either way it drives every tool through its command line,
prints a PASS/FAIL table and an `N passed, M failed` summary, and exits non-zero
if anything failed.

To drive a tool yourself while debugging, just run it — that is the whole
interface:

```sh
./disksim run scan ../fixtures/disk-sep.stream start=50
printf 'init 5 4 1 4 16\nfill 7\nfail 2\nread 5\nrebuild 2\nchecksum\n' | ./raidsim -
```

## What is checked, and what is not

Parts 2–5 are machine-checked in full: the disk model against analytic oracles,
the four schedulers against hand-computed service orders, and the RAID array by
the write-pattern / fail-each-disk / rebuild / byte-identity test that is the
strongest check in the lab. A green run means those simulators do what the
handout says.

`RESULTS.md` is **not** machine-checked beyond its existence — its numbers and
its argument are marked by the rubric in `solutions/README.md`. A full `N passed,
0 failed` is reachable with an empty `RESULTS.md`, and that is not a finished
lab. Write the tables and the interpretation; they are a fifth of the marks.

## What is on your honour

Two things no test enforces, listed here so the gap is declared rather than
hidden:

- **The workload generator's *quality*.** The grader checks the model and
  schedulers against fixed fixtures, never against `gen`'s output, because your
  PRNG is yours. That `gen mixed loc=90` really is more local than `loc=10` is
  on your honour — but you will want it to be, for `RESULTS.md`.
- **That `ns_per_op`-style wall-clock timing, if you add any, is honest.** This
  lab measures *modelled* ticks and I/O counts, which are deterministic. If you
  time real execution as a stretch, remember it is noisy and do not build an
  argument on a small gap.

---

# Stretch goals

Unweighted. Do them if the five parts came easily.

- **SPTF (shortest positioning time first)**: schedule by seek *plus* rotational
  latency, not seek alone. It needs your model to expose rotational position to
  the scheduler — a good test of whether the Part 2 abstraction was drawn in the
  right place. Show a workload where it beats SSTF.
- **RAID 6**: a second, independent parity block, and survival of *two*
  simultaneous disk failures. The second parity is where XOR alone stops being
  enough.
- **A `dir=down` starvation fixture**, and the observation that SCAN's fairness
  depends on the sweep direction relative to where the load is.

---

# If you get stuck

- **`make test` cannot find the autograder or fixtures**: you copied only
  `starter/`. Copy `tests/` and `fixtures/` alongside it, or pass
  `make test TESTS=/path/to/lab06-io-storage/tests/run.sh`.
- **The disk model's `done` times are wrong but the seeks look right**: you are
  probably dropping rotational latency, or computing it from a fixed head
  position instead of from the elapsed time `t`. Reproduce the worked example
  (51/171/231) one request at a time.
- **SSTF passes but SCAN's order is wrong**: check where you reverse. SCAN
  reverses at the last *requested* cylinder in the sweep direction, and then
  serves the rest in the opposite order — not from the disk edge, and not by
  re-sorting.
- **The four policies produce the same total on your workload**: your workload
  does not separate them. Use `fixtures/disk-sep.stream` with `start=50` and
  check against the numbers in `fixtures/README.md` (285/130/120/155) before
  trusting a workload of your own.
- **RAID-5 `place` is right for stripe 0 but wrong for stripe 1**: your parity
  disk is not rotating with the stripe, or your data columns are not skipping
  the parity disk. Both must move together.
- **`checksum` differs after rebuild**: the reconstruction is reading the wrong
  blocks. Think about exactly which blocks of a stripe take part in recovering
  one missing block — no more and no fewer.
- **A degraded `read` returns `0000000000000000`**: you returned the zeroed
  replacement instead of reconstructing. The failed disk's data is the XOR of the
  survivors, not zero.
- **`iostat` shows the same count for additive and subtractive**: one of them is
  secretly the other. Subtractive reads exactly the old data block and the old
  parity block (2 reads); additive reads every other data block. On a wide array
  they cannot match.
