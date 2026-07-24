# Lab 9 — Reference implementation and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference blockdev.py, crashfs and wal.py, and the model     ║
║  CRASH-TABLE / CAMPAIGN / JOURNAL-COST notes.  The prose parts    ║
║  are a third of the marks and self-marked: reading the models     ║
║  before you have your own sweeps turns them into a fill-in form.  ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
# assemble a graded workdir: the given files plus the reference three
mkdir /tmp/ref && cp ../starter/* /tmp/ref/ && cp blockdev.py crashfs wal.py /tmp/ref/
../tests/run.sh /tmp/ref          # 24 passed, 0 failed  (~15 s)
```

The student-owned files are three: `blockdev.py` (~90 lines of class),
`wal.py` (~120) and `crashfs`'s `workload()` (~20). Everything else in
`starter/` is given and identical here. **The starter skeleton scores
`3 passed, 21 failed` of 24.** The three it passes are earned for the wrong
reason and documented as such: *fifo installs immediately* and *a barrier
makes the whole queue durable* are trivially true of the stub's
install-everything-now device (cases 1.3/1.4/1.6 are the ones a real queue
must earn), and *recovery of an intact image is empty* is true of a recovery
that is always empty (cases 3.3–3.5 and the campaign kill that stub).

## Design notes, where students will differ

- **The reorder drain is REVERSE issue order by specification**, not an
  implementation accident. A shuffled drain would need a seed and would make
  the campaign flake; LIFO is maximally adversarial for exactly the bug that
  matters (an unbarriered commit record issued last installs *first*) and
  keeps every run byte-identical. Case 1.4 pins the drain order; case 1.6
  pins that a coalesced rewrite keeps its slot in that order.
- **The commit record carries its destinations** (header = magic, seq, n,
  dest[7]). This makes recovery self-contained and — deliberately —
  makes the "replay by capacity instead of count" bug unbuildable with the
  given `parse_log_header`, which returns only `dests[:n]`. The stale-slot
  trap is closed by format rather than tested; the circular-log stretch goal
  is where that class of bug lives.
- **`end_op` returning is the durability acknowledgement.** The campaign's
  durability oracle is two-legged: the crashed image's own log header (a
  committed-but-uncheckpointed transaction is visible on disk) covers the
  commit-to-clear window; the reference run's `committed` lines cover
  everything after the clear. Both legs are load-bearing — see M5b vs M11
  below — and only their union spans every crash point.
- **Recovery clears the header after the replays, with a barrier between**;
  a second recovery after any prefix of the first is then a no-op or a
  complete redo. That is the entire idempotence argument, and sweep
  `recovery` tests it rather than trusting it.

## What the autograder checks, case by case, and the mutation that proves it

Every case was verified by building the named wrong implementation and
confirming that the case — and the suite's exit code — flips. A requirement
no mutation can break is not tested; each row here has one that fired.

| Case | Catches (mutation confirmed) |
|---|---|
| 1.1 fifo installs immediately | baseline (earned by the stub; the reorder cases carry Part 1) |
| 1.2 crash after N loses exactly the writes past N | **M1**: device ignores `crash_after` → wrote all 4 blocks, no CRASH line (15 cases fail suite-wide, incl. every campaign point via its sailed-past guard) |
| 1.3 reorder queues until a barrier | **M3**: reorder installs immediately → `installs=0` becomes 1, durable state non-empty |
| 1.4 the drain is reverse issue order | **M2**: FIFO drain → crash mid-drain keeps block 1, not block 3 |
| 1.5 a barrier drains the queue | baseline (stub passes; 1.3/1.4/1.6 are the discriminating cases) |
| 1.6 a coalesced write keeps its queue position | **M12**: coalesce removes the old entry and appends to the tail → the reverse drain installs block 1 first; durable `{1: cc}`, not `{2: bb}` (without this case M12 passed the whole suite — the contract line was untested) |
| 2.1 create is exactly 5 writes, clean, byte-exact | starter stub (0 writes) and any pass-through miscount |
| 2.2–2.7 the six-row crash table | **M9**: W2↔W3 swapped → k=2 yields `bitmap-leak + dangling-entry`, key wants `block-free-but-used` |
| 3.1 data mode = 46 installs, commits at 10/22/32/46 | **M11**: end_op skips the checkpoint → final image empty, installs wrong; also any missing barrier (16 asserted) |
| 3.2 recovery of an intact image is empty | baseline (see above) |
| 3.3–3.5 pinned points i=2/5/7 of op 1 | **M5** (below) fails i=5; **M6b** (half replay) fails i=5 with `clean=False` and torn contents |
| 4.1 campaign, fifo, 46 points | **M6**: checkpoint-before-commit → 15 points bad, first i=5 `dangling-entry` (uncommitted state applied = the atomicity inversion). The point count itself is asserted (`pts == 46`, from 2n+2): the sweep's length comes from the no-crash run's own install count, so without it a **null journal** (M13, the starter `wal.py` stub atop a finished Part 1) yields a 0-point sweep and `bad == 0` vacuously |
| 4.2 campaign, reordering device, 46 points | **M4**: missing log→commit barrier → **fifo campaign PASSES, reorder campaign fails** at i=1 `FAIL root` — M4 also trips 3.1/5.1's barrier counts (12/16 ≠ 16/20); what 4.2 *alone* catches is barrier placement given a correct count (a compensating no-op barrier elsewhere passes 3.1). `pts == 46` asserted: **M13** never barriers, so the reordering device installs 0 blocks (`done installs=0`) and this case previously PASSED on an empty sweep |
| 4.3 recovery idempotent under its own crashes, 6 points | **M7**: recovery clears the header before replaying → only this case fails (j=1 loses committed op 2); the pass condition also demands `rtotal >= 4` recovery installs, so a do-nothing recovery cannot pass a 0-point sweep here |
| 4.4 commit-before-log corrupts at point 1 | reference `misorder` handling absent/ineffective → run completes or image stays clean; also asserts the crashed image's header shows the committed txn (`pending_seq == 1` — under commit-first, install 1 IS the commit record), which **M13** fails: its unclean crash=1 image previously passed `rc==3 and not clean` |
| 5.1 ordered = 39 installs, commits at 9/19/28/39 | ordered mode not implemented (write_data journals) → 46 installs |
| 5.2 campaign, ordered, 39 points | same mutants as 4.1 apply on the ordered path; `pts == 39` (8+d) asserted against the empty-sweep loophole |

**The durability mutants (the review's centrepiece), M5 and M5b.**
**M5** — recovery replays nothing but still clears the log: the Part 3 i=5
case fails (`clean=True files=0`) and all three campaigns fail **first at
i=5** with `durability: 1 op(s) committed, 0 present` (19/46, 19/46, 12/39
points) — at each op's commit-point crash the image is **xfsck-clean** and
only the durability oracle sees the loss. A structure-only campaign does
*not* pass M5, though: at partial-checkpoint points a replay-nothing
recovery leaves the checkpoint half-applied and xfsck objects — with the
header leg deleted the fifo campaign still fails 15/46 points, first
`i=6: structure: FAIL dangling-entry`. The sharp witness that the header
leg itself is load-bearing is **M5b** — a *rollback* recovery that undoes
the committed transaction instead of replaying it (deletes the pending
`f<seq>`, fsck-repairs the image to clean, never replays). Under the full
grader all three campaigns fail M5b (23/46, 23/46, 16/39 points, every one
`durability`, first at i=5); with the header leg deleted, M5b **passes all
131 campaign points** while silently losing every
committed-but-uncheckpointed transaction — structure, atomicity and the
committed-lines leg are all blind to it at every point. (M5b also defeats
4.4, since a recovery that never replays cannot corrupt; the pinned Part 3
points i=5/i=7 and the recovery sweep still catch it.) That is the proof
the durability oracle — and specifically its header leg — is load-bearing.

**Oracle-necessity table** (each oracle catches a mutant the others miss):

| Mutant | structure (xfsck) | durability: header leg | durability: committed-lines leg |
|---|---|---|---|
| M5 replay-nothing (clean at each op's commit point, data lost) | fires at partial-checkpoint points only | **fires first** (i=5) | blind (op 1 acked at i=10) |
| M5b rollback recovery (every point clean, committed txns undone) | blind at every point | **fires** (sole detector) | blind |
| M11 no checkpoint (header already cleared at i≥6) | blind | blind | **fires** |
| M10 one extra bitmap bit per create (all files byte-exact) | **fires** (`bitmap-leak`) | blind | blind |

(The atomicity oracle is omitted as a column because it overlaps: with the
committed-lines leg deleted, M11 is still caught at 3 of 27 points by the
atomicity prefix check — a later op's replay lands f2 while f1 is missing —
but only the committed-lines leg fires at every post-clear window, first and
with a diagnostic message. M5b is the one mutant here that
structure+atomicity+committed-lines cannot see at any campaign point.)

M10 also shows the scope boundary working: content oracles cannot see a
space leak; only the Lab 7 structural checker can, which is why it is here.

## Every asserted number, and its derivation

No graded value exists only in the grader: each is derivable from the
handout (README.md states all of the inputs).

| Asserted | Derivation |
|---|---|
| Part 1 durable sets per script | the stated device model applied by hand (fifo prefix; LIFO drain prefix; coalesce in place; queue lost) |
| Part 2: 5 installs; classes at k=0..5; data bytes zero/pattern | the W1..W5 table + xfsck's invariants (Lab 7) + fill_pattern(1,j) |
| geometry 64/2/10/12/13; first inode 2; first blocks 14,15 | printed by mkfs; stated in handout |
| 46 installs, 16 barriers; commits 10/22/32/46 | 2n+2 per op, n = 3+d, d = 1,2,1,3; cumulative sums |
| 39 installs, 20 barriers; commits 9/19/28/39 | 8+d per op, 5 barriers, same d |
| op-1 commit at install 5 (cases i=2/5/7) | 4 slots then the record |
| recovery installs = 6 at i*=16 | n=5 replays + 1 clear for op 2's transaction |
| broken mode corrupts first at i=1 | install 1 is the commit record; slots are still mkfs zeroes; recovery replays zeroes over blocks 10/12/13 |

## Rubrics for the prose deliverables

**CRASH-TABLE.md** (with Part 2's 20%): the six rows right (auto-checked
anyway) — marks are for the *why* column: (a) k=1/k=2 named as violations of
specific invariants; (b) k=3/k=4 identified as consistent-but-wrong and
tied to fsck's information-theoretic limit, not hand-waved as "still fine";
(c) the observation that reachable inconsistencies are a function of issue
order (any concrete alternative order accepted). Model: `CRASH-TABLE.md`.

**CAMPAIGN.md** (Part 4's 13%): (a) the atomic-commit-record model
assumption stated; (b) all three sweeps reported at `bad=0` with point
counts; (c) the broken mode's first corrupting point identified as i=1 with
the causal chain — commit record durable, slots not, recovery replays
zeroes — and the conclusion that recovery *armed by a lying commit record*
does the damage; (d) rule 1 identified as what the removed barrier enforced.
Full marks requires (c) argued from the protocol, not just the sweep output
pasted. Model: `CAMPAIGN.md`.

**JOURNAL-COST.md** (with Part 5's 13%): (a) both measured `done` lines;
(b) the 8+2d vs 8+d reconciliation against sheet 21 (or an honest account
of any divergence); (c) the barrier trade (16 vs 20) explained as a real
cost, not a curiosity; (d) the surrendered guarantee stated precisely —
overwrite tearing and block reuse — with the explicit acknowledgement that
*this* workload cannot exhibit it and what workload would. (d) is the
discriminator; "ordered is faster but less safe" without the mechanism
scores half. Model: `JOURNAL-COST.md`.

## Known limits, declared

- The campaign's install ranges are capped by the workload's own length
  (46/39/6 points; `sweep` additionally hard-caps at 200), so a runaway
  implementation cannot wedge the grader beyond its per-case timeouts.
- The log never wraps: this is xv6's fixed-region log (checkpoint-and-reset),
  chosen because xv6 §10.4–10.6 is the reference design. The circular-log
  wrap bug class is deferred to the stretch goal, and the plan's "sweep long
  enough to wrap" is realised as "workload long enough to *reuse* the log
  region with differing transaction sizes" (ops of n = 4,5,4,6).
- A `wal.py` that deliberately delays its `committed` acknowledgement
  weakens the committed-lines leg of the durability oracle; the header leg
  still covers the commit-to-clear window. Declared in the handout's honour
  section, mirroring Lab 6's. (Measured, the residual gap is narrower than
  that declaration implies: for the graded workload, 3.1/5.1's exact
  install/commit assertions close the delayed-ack loophole — deferring,
  padding or batching the acknowledgement shifts the pinned commit-install
  numbers and fails there, and the header leg, proven load-bearing by M5b,
  covers the commit-to-clear window.)
