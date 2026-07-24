# Lab 6 — Reference tools and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference iobuf.c, disksim.c and raidsim.c, the model        ║
║  RESULTS.md, and the rubric for the report — which is a fifth of  ║
║  the lab and self-marked. Reading RESULTS.md before you have your ║
║  own numbers turns the one open-ended part of the lab into a      ║
║  fill-in form.                                                    ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g, zero warnings
make test       # ../tests/run.sh on this directory: 33 passed, 0 failed
```

The reference is three small C programs: `iobuf` (~160 lines), `disksim`
(~380), `raidsim` (~510), comments included. **The starter skeleton scores `3 passed, 30 failed`
of 33** — the three it passes for free are `FIFO serves in arrival order`
(the one scheduler written for you), `RAID0 capacity is all N disks` (the naive
`data_disks() = ndisks` stub happens to be right for level 0 only), and
`a degraded read reconstructs the real data` (the stub maps every block to disk
0, so failing a *different* disk leaves the read intact — a pass for the wrong
reason, and one a correct mapping earns properly). Every other case is
falsifiable by a do-nothing stub, and fails.

## Why the harness grades C and Python the same

The autograder never links against the submission and never `#include`s
anything from it. It drives `iobuf`, `disksim` and `raidsim` entirely through
argv and stdout, so the *language* is invisible to it: a C build and a Python
script that produce the same lines for the same arguments grade identically.
This was verified, not assumed — a throwaway Python `disksim` implementing FCFS
and the cost model was dropped into a work directory with no `disksim.c`, and
`run.sh` found the executable, ran it, and passed the Part 2 oracle and the FCFS
order case against it. The claim "either language is accepted" is therefore an
implementation fact about the harness, not a promise simulated by grading C
twice.

For a C submission `run.sh` still does the graded compile itself
(`gcc -Wall -Wextra -Werror -std=gnu11 -g` on each `<tool>.c`, into its own
`mktemp -d`, failing on any `warning:`), so a Makefile that drops a flag cannot
change the grade; the student Makefile is run once only as a separate smoke gate.

---

## What the autograder checks, case by case, and the mutation that proves it

Every case below was checked by *building the wrong implementation* and
confirming that this named case — and the suite's exit code — flips. A
requirement no mutation can break is not tested; each row here has one that does.

### Part 1 — iobuf

| Case | Catches (mutation confirmed to fail it) |
|---|---|
| unbuffered transfer is fully serial (260) | unbuffered secretly overlaps (K=2) → time < 260 |
| double buffering beats unbuffered | double does not overlap (K=1) → not < unbuffered |
| double and circular hit the exact overlap floor for a steady producer (603) | any recurrence bug that shifts the steady fixed point → not 603/603 (pins the number, not just the ordering) |
| circular absorbs a burst that stalls double | circular capped at depth 2 → equals double, not less |
| polling waste scales with device time (160) | polling ignores D (`wasted = N`) → not 160 |
| interrupt cost is per-unit, not per-device-tick | interrupt uses D instead of H → not 800/800 |
| polling wins fast, interrupts win slow (crossover) | interrupt cost inflated → slow-device ranking does not flip |
| DMA overhead is paid once per burst, not per unit (150) | DMA cost made per-unit (`n*S`, `S+n*H`, garbage constant) → balloons with n instead of staying 150 at n=16 and n=64 |

The **circular** case is the "silently a weaker strategy" guard, the analogue of
Lab 3's fit-policy separation. A steady producer makes double and circular
identical, so the case uses a *bursty* producer (`burst=6`), the one workload on
which depth is not decoration; a circular buffer that quietly caps K at 2 comes
out equal to double and is caught. `RESULTS.md` shows both the steady case (603 =
603, depth buys nothing) and the bursty case (665 vs 601, depth buys a lot) so
the mechanism, not just the ordering, is on the record. The steady `603/603` is
also **asserted exactly** by the harness (a separate case), pinning the
recurrence's fixed point directly rather than only through the `< unbuffered`
comparisons; the value follows from the recurrence in the handout, so it is a
property of the spec, not of any one implementation. The DMA case is the
mechanism analogue for the `io` command: `S+H = 150` at both `n=16` and `n=64`
pins that DMA pays its overhead once for the whole burst, not per unit.

### Part 2 — the disk cost model

| Case | Catches |
|---|---|
| seek+rotation+transfer match the analytic oracle | rotation ignored (`rot = 0`) → `done` times wrong (3/31/61 vs 51/171/231) |
| rotational latency is nonzero when it should be | rotation zeroed via the modular formula → rot = 0 |
| the transfer term scales with the sector count | transfer hardcoded to +1 (ignores `nsec × xfer`) → a 5-sector read at lba 250 gives `done = 51` not `55`. Every other fixture uses a 1-sector transfer, so this is the only case that exercises the multiply |

The oracle values are hand-derived in the case comment and in the handout, not
read back from the tool. A model that drops rotation entirely — the single most
common way to get "the seeks look right but the totals are wrong" — changes
every `done` time and is caught on the first request.

### Part 3 — the schedulers

| Case | Catches |
|---|---|
| FIFO serves in arrival order | fifo re-sorts → order changes |
| SSTF serves nearest-first | sstf picks farthest (`d > bestd`) → order changes |
| SCAN sweeps up then reverses | scan serves the low half ascending, not descending → order changes |
| C-SCAN sweeps up then wraps to low | cscan serves the wrapped half descending → order changes |
| **the four policies produce four different seek totals** | any policy silently FCFS → its total collides with 285 |
| SCAN serves the starved request far earlier than SSTF | sstf and scan produce the same order → outlier positions equal |

The **separation** case is the headline: it asserts the four seek totals are
`285 / 130 / 120 / 155` and all distinct, on a fixture built to force the
difference. A scheduler that returns arrival order under a different name (the
"silently FIFO" mutant) collides with FCFS's 285 and fails here even if its own
exact-order case were somehow satisfied. The **starvation** case is the second
guard: it needs SSTF and SCAN to *disagree* about when the outlier is served, so
a scan-is-really-sstf mutant fails it.

### Part 4 — the RAID engine

| Case | Catches |
|---|---|
| RAID0 / RAID1 / RAID4 / RAID5 capacity | `data_disks()` counting all N disks → wrong for 1/4/5 |
| RAID5 parity rotates with the stripe number | parity pinned to one disk (RAID-4 behaviour) → stripe-1 placement wrong |
| RAID5 parity block is the XOR of the stripe | XOR replaced by OR → parity byte not 0x69 |
| the stripe unit (chunk) is not the block | chunk conflated with block (`/1`) → chunk=2 mapping wrong |
| subtractive parity update is 2 reads + 2 writes | subtractive does additive I/O → reads = data_disks−1, not 2 |
| additive parity update reads every other data block | additive does subtractive I/O → reads = 2, not 4 |
| auto picks the cheaper parity update on a wide array | auto inverts the comparison → reads = 4, not 2 |

The RAID-5 rotation case checks three placements — `place 0`, `place 3`,
`place 5` — chosen so that a fixed-parity (RAID-4-style) implementation is right
on stripe 0 and wrong on stripe 1, and so that `lba 5`'s data column has to
*skip* the parity disk. Off-by-one in the rotation, or forgetting to skip, each
break a specific line. The additive/subtractive pair is graded through `iostat`,
which counts *physical* I/Os, so a write that claims one method and performs the
other is caught by the count — the trap the handout warns about.

### Part 5 — degraded mode and rebuild

| Case | Catches |
|---|---|
| RAID5 / RAID4 / RAID1 survives each disk, rebuilds byte-identical | reconstruct skips a survivor → checksum changes; RAID1 rebuild copies zeros |
| a degraded read reconstructs the real data | the whole stripe (lba 3,4,5) is written to distinct nonzero bytes first, so parity alone ≠ the answer: degraded read returns the zeroed block, reads a wrong surviving disk, **or reconstructs from parity only (omits the data columns)** → not the pattern |
| RAID0 loses data on a disk failure (negative case) | RAID0 fakes survival → no LOST, checksum unchanged |
| rebuild reads exactly the surviving blocks (12) | rebuild reads an extra block per row → count ≠ 12 |

The **survive-each-disk** case is the strongest single check in the lab, and the
one the plan singles out. It fills with a known pattern, checksums, then for
*every* disk in turn: fails it, checks the degraded checksum is unchanged
(reconstruction works), rebuilds, and checks the rebuilt checksum is unchanged
(byte-identity). Because the verdict is a checksum over data read *through*
redundancy, a reconstruction that reads the wrong blocks — the classic
"rebuild from the wrong parity" — changes the checksum and is caught, once per
level, without the case needing to know anything about the internal layout. The
RAID-0 **negative** case is the counterpart: it fails you if a level with no
redundancy claims to survive a failure.

---

## The reference designs in a paragraph each

**iobuf.** One `simulate()` implements all three buffering strategies as a
bounded-buffer producer/consumer with K slots (1, 2, or `depth`); the recurrence
in the handout is the whole model. Polling/interrupt/DMA are three closed-form
costs. Nothing is timed; everything is modelled ticks.

**disksim.** Requests are mapped to `(cylinder, sector)` once; each scheduler
fills a service-order array (SSTF greedy with a documented tie-break; SCAN and
C-SCAN by sorting on cylinder and splitting at the head, LOOK-style); then one
`serve()` threads absolute time through the seek/rotation/transfer model. The
rotation formula is modular on absolute time, so a request's latency depends on
every request before it — the thing a simulator forces and arithmetic skips.

**raidsim.** `map_data()` is the geometry — chunk index, stripe, column, and for
RAID-5 the parity-disk rotation and the skip. `reconstruct()` is one XOR loop
over the surviving disks of a stripe, and it is shared by degraded reads and
rebuild, which is why getting it right is most of Part 5. The physical
read/write helpers increment I/O counters, so additive and subtractive are
distinguished by counting, not by inspecting the code.

---

## The rubric for `RESULTS.md`

`RESULTS.md` is 20 marks of the lab (a fifth), marked by hand; the model report
is in this directory. Nothing in the autograder marks its numbers — a full
`33 passed, 0 failed` is reachable with an empty report, and that is not a pass.

| | Marks | What it takes |
|---|---:|---|
| **Completeness** | 6 | All three tables present: buffering (both steady and bursty) + the I/O-mechanism crossover; the four schedulers on at least one workload that separates them, with the starvation figure; and the RAID capacity + small-write + rebuild numbers. Parameters (n, geometry, ndisks) stated so the numbers are reproducible. |
| **Mechanism** | 6 | Each result explained by *what the design does*, not restated. "Circular is faster" is the observation; "the extra slots let the producer run ahead during the cheap run so the device never idles, which is why depth helps the bursty producer and not the steady one" is the mechanism. Full marks needs the mechanism for the buffering-depth result, the seek/service gap, and the small-write cost. |
| **The trades** | 4 | Name the trade in each half: buffer depth vs variance (not vs device speed); polling vs interrupt as a function of `D/H`; seek-optimal (SSTF) vs fair (SCAN); capacity vs redundancy; and additive vs subtractive vs array width. |
| **Honesty** | 4 | Does the conclusion follow from the tabled numbers? The separation must be shown, not asserted — a report that claims the policies differ without a workload on which they do loses here. One noticed surprise (e.g. SSTF and SCAN nearly tie on seek but not on fairness; or additive *beating* subtractive on a narrow array) scores well. |

**What a full-marks report notices.** Any of these, and there are others:

- Depth buys nothing for a steady producer (double = circular) and a lot for a
  bursty one — the steady case is the control that stops "circular is best" from
  becoming a rule.
- SSTF and SCAN are within a percent on total seek but differ sharply on the
  starved request's wait: throughput and fairness are separate axes.
- The small-write problem is four physical I/Os for one logical block *at best*,
  and additive vs subtractive is a width-dependent choice with a crossover, not a
  strict winner.
- RAID-4 and RAID-5 have identical capacity; the rotation is about spreading the
  parity *write* load, which this model shows in placement but not in throughput
  (a single-threaded simulator cannot show the parity-disk bottleneck — worth
  naming as a limit of the model).

---

## What is on your honour

Declared in the handout and repeated here: the autograder never grades `gen`'s
output (the workload generator's statistical quality is yours to get right for
`RESULTS.md`), and there is no wall-clock timing anywhere in the graded path, so
"my timing numbers are honest" only arises if a student adds timing as a stretch.
Everything the autograder marks is a deterministic modelled quantity.
