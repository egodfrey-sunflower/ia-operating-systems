# Lab 9 — Crash consistency: journaling and recovery

**Weeks 21–22 · 7.5 hours · OSTEP ch. 42 (crash consistency: FSCK and journaling) · xv6 book §10.4–10.6 (logging)**

Userspace, **Python**. A simulator, not a kernel: no xv6 changes, no QEMU, no
root. You will need `python3`, `bash`, `gcc` and `make` (the C compiler builds
only the given checker).

Chapter 42 opens with the six partial states an allocating write can be caught
in. This lab is that section made executable. You build a block device that
can lose power after any write, crash an unprotected file system at every
instant of an update and tabulate the wreckage, then build a write-ahead log
with recovery and prove — by crashing it at *every single point*, several
hundred times, automatically — that every crash now recovers to a consistent
image. Sheet 21 walks the same protocol on paper; here the crash actually
happens, and a checker judges the result each time.

The images you crash are **xv6-format images**, and that is a deliberate
choice: it means Lab 7's structural checker `xfsck` reads them unchanged, so
the checker you studied there becomes this lab's consistency oracle. (The
shipped oracle in `tests/oracle/` is the *reference* checker — build and run
it, but don't read it before you've finished Lab 7 Part 4.) The given
`xv6fs.py` supplies all the on-disk structure writers; **what you build is the
device, the ordering, and the log** — never the byte-level format.

## Layout

```
lab09-crash/
  README.md          this handout
  starter/           blockdev.py, crashfs, wal.py    <- work here
                     xv6fs.py, journal, sweep         <- given, complete
  tests/run.sh       the autograder (delegates to grade.py)
  tests/oracle/      Lab 7's reference xfsck + Makefile (the oracle)
  solutions/         SPOILERS. Reference code, answer key, rubrics. Later.
```

Copy the working directories somewhere of your own and work there — copy
`starter/` *and* `tests/` (the autograder compiles the oracle out of
`tests/oracle/`):

```sh
cp -r starter tests ~/lab9
cd ~/lab9/starter
make -C ../tests/oracle          # builds the xfsck oracle
../tests/run.sh .                # runs the autograder against this directory
```

`solutions/` is deliberately left behind.

## What you hand in

| File(s) | Part | Weight | Marked |
|---|---|---|---|
| `blockdev.py` — the crashable, reordering device | 1 | 20% | auto |
| `crashfs` — the unprotected create, in the fixed order | 2 | } | auto |
| `CRASH-TABLE.md` — the crash-point table, *explained* | 2 | 20% | rubric |
| `wal.py` — begin_op/end_op, the protocol, recovery | 3 | 34% | auto |
| `CAMPAIGN.md` — the sweep results + the broken-mode analysis | 4 | 13% | rubric+auto |
| `wal.py` — ordered mode; `JOURNAL-COST.md` — the comparison | 5 | 13% | auto+rubric |

The `.md` deliverables are marked by hand against the rubric in
`solutions/README.md`, which you read *after* you have your own numbers. The
`sweep` tool writes first-draft tables for both (`--out`); your marks are for
the explanation you add, not the table dump.

---

# ⚠ The text interface

Every command line and output line shown below is a **fixed contract**: the
autograder drives your tools through argv/stdin and greps for these exact
strings, and it never imports your code. The starter files carry the contract
in comments. Change what a line says and the grader cannot read it.

The autograder also never trusts your copies of the given files: its
consistency checks are its own, and its `xfsck` is compiled by the harness
itself, with `-Wall -Wextra -Werror`, from `tests/oracle/`.

---

# Part 1 — A crashable block device (~1.5 h, week 21, 20%)

`blockdev.py` — both a library (the rest of the lab drives everything through
it) and a script-driven tool. The model, which is the whole specification:

- The device is backed by a file of 1024-byte blocks: the **platter**. A
  write is **durable** only once it has been **installed** into that file.
- **fifo** mode: every write installs immediately, in issue order.
- **reorder** mode: writes queue in memory and install **only at a
  `barrier()`**. The drain installs the queue in **reverse issue order** —
  a disk that guarantees no ordering may legally pick any order, and this is
  the most adversarial one, chosen deterministically so that every run
  crashes the same way. A second write to a queued block replaces the queued
  data in place (the entry keeps its position).
- `read()` returns queued data if the block is pending, else the platter's —
  a disk cache serves reads from writes it has not yet installed.
- `crash_after=N`: the device performs at most N installs; the attempt to
  install number N+1 raises `Crash` instead, and anything still queued is
  lost. `crash_now()` discards the queue and raises immediately.
- Counters: `installs` and `barriers`. Part 5's cost comparison is these two
  numbers.

The CLI (given, at the bottom of the file) turns a command script into calls
on your class — `init`, `mode`, `crashafter`, `write <bno> <hexbyte>`,
`read`, `barrier`, `stats`, `crashnow`, `dump` — so you can watch a crash by
hand:

```sh
printf 'init 16\nmode reorder\ncrashafter 1\nwrite 1 aa\nwrite 2 bb\nbarrier\n' \
  | ./blockdev.py /tmp/d.img -
printf 'dump\n' | ./blockdev.py /tmp/d.img -     # what survived?
```

Work out on paper what that script must leave durable before you run it.

# Part 2 — An unjournaled file system, crashed (~1.5 h, week 22, 20%)

`crashfs workload <image> [crash=<k>]` performs ch. 42's allocating write
with no protection whatever: create file `f` with 2 data blocks, as exactly
**five block writes in this fixed order**:

| # | write | block |
|---|---|---|
| W1 | root directory block (the new entry) | 13 |
| W2 | the inode block (type, nlink 1, size 2048, both addrs) | 10 |
| W3 | the bitmap block (both data blocks marked) | 12 |
| W4 | data block 0 | 14 |
| W5 | data block 1 | 15 |

The block numbers follow from the given geometry (`mkfs ok size=64
logstart=2 inodestart=10 bmapstart=12 firstdata=13`; a fresh image's first
free inode is 2 and its first free blocks are 14 and 15). Block *j* of the
file holds 1024 copies of byte `64 + 8·1 + j` — `xv6fs.fill_pattern(1, j)`.

The given helpers make each row exactly one device write; your `workload()`
issues them in order. Then the experiment: **before running anything**,
predict for each `k` in 0..5 what `xfsck` will say about an image that
crashed after exactly `k` of those writes — which of Lab 7's violation
classes fires, or `clean`. Then run the given sweep and compare:

```sh
./sweep part2 --xfsck ../tests/oracle/xfsck --out CRASH-TABLE.md
```

`CRASH-TABLE.md` is that table **plus, for every row, the reason** — which
invariant broke, or why the checker has nothing to object to. Two rows
deserve special care: the crash points at which the image is *consistent but
wrong* — xfsck reports clean while the file's contents are not what was
written. That gap between "structurally consistent" and "correct" is fsck's
fundamental limit (ch. 42 §42.2), it is why the campaign in Part 4 checks
more than structure, and it is worth three sentences of your own.

# Part 3 — Write-ahead logging (~2.5 h, week 22, 34%)

`wal.py` — a `Journal` that wraps your device and presents the *same*
read/write/write_data/barrier interface, so the given structure writers run
through it unchanged. `journal` (given) drives it:

```sh
./journal mkfs img && ./journal workload img mode=data
./journal recover img
```

**The transaction.** Between `begin_op` and `end_op` nothing touches the
device: writes buffer in memory, and `read` must return buffered data (the
writers read-modify-write shared blocks). `end_op` then plays the update out
under the write-ahead protocol. Ch. 42 gives its three ordering rules:

1. the log copies of the blocks are durable **before** the commit record;
2. the commit record is durable **before** any block is checkpointed to its
   real location;
3. the checkpoint is durable **before** the log entry is freed.

"Durable" is a claim about the device, and only `barrier()` makes one.
Where the barriers go is yours to decide from the rules; the campaign runs
your journal on the **reordering** device as well as the fifo one, and the
reordering device is entitled to install anything unbarriered in the worst
possible order. That is not the harness being cruel — it is rule 1's actual
content, and xv6's `log.c` (read §10.4–10.6) pays the same cost in real
`write_disk` waits.

**The log's on-disk shape** (fixed contract, constants in `xv6fs.py`): the
log region is blocks 2..9. Block 2 is the **header**: `u32 magic
(0x6A726E6C), u32 seq, u32 n`, then 7 `u32` destination block numbers.
Blocks 3..9 are the seven **slots**; slot *k* holds the journaled copy of the
block destined for `dest[k]`. The header with `n > 0` **is** the commit
record — one block, and the model assumes a single-block write is atomic
(state that assumption in CAMPAIGN.md; without it the scheme is circular).
The header with `n = 0` is the empty log. `seq` numbers transactions from 1,
and the write that frees the log entry (in `end_op` or in recovery) keeps the
just-committed transaction's `seq` in the cleared header — only `n` returns
to 0.

**Recovery** (`recover(dev)`): if the header holds a committed transaction,
replay every slot to its destination and free the log; otherwise do nothing
and report `(0, seq)`. Recovery runs on a device like any other code — it
can crash at any of its own installs, and the campaign will crash it at
every one and then run it again. Design for that from the start.

**Counting installs.** A create's transaction touches exactly 3 metadata
blocks (inode block, bitmap block, root directory block) plus its *d* data
blocks. In data-journaling mode all `n = 3 + d` go through the log, so one
transaction costs `n` slot writes + 1 commit + `n` checkpoint + 1 clear =
**2n + 2 installs and 4 barriers**. The graded workload (built into
`journal`) is four creates, `d = 1, 2, 1, 3` — block *j* of file `f<t>`
holds byte `64 + 8t + j` — so a correct data-mode run reports
`done ops=4 mode=data installs=46 barriers=16`, with the four `committed`
lines at installs 10, 22, 32, 46. Derive those numbers yourself before you
trust them; the autograder asserts every one.

# Part 4 — The crash campaign (~1.0 h, week 22, 13%)

Part 2's sweep, re-run against the journal — and this is the lab's
centrepiece. For **every** install count *i* in the workload: crash at *i*,
run recovery, and judge the image by three oracles:

- **structure** — the reference `xfsck` must report it clean;
- **atomicity** — the root must hold exactly `f1..fm` for some *m*, each
  byte-exact: no partial file, no gap, no leftover;
- **durability** — *m* must cover every transaction that was committed
  before the crash (the crashed image's own log header, plus the reference
  run's `committed` lines, say which those are).

```sh
./sweep data --xfsck ../tests/oracle/xfsck --out CAMPAIGN.md
./sweep data --xfsck ../tests/oracle/xfsck --dev reorder
./sweep recovery --xfsck ../tests/oracle/xfsck
```

A correct journal shows `bad=0` on all three — 46 points, 46 points, and a
recovery-crash sweep. Zero is the only acceptable number, and that is the
difference between "usually fine" and *consistent by construction*.

Then break it, deliberately. `misorder=commit-first` must make your `end_op`
issue the commit record **before** the log blocks, nothing between them —
keep it a two-line change. Run `./sweep broken --xfsck ...` and find the
first crash point that corrupts. `CAMPAIGN.md` gets the three clean sweeps,
the broken sweep, and a short analysis: *which* crash point corrupts first
under `commit-first`, what recovery does there, and why rule 1 is therefore
the whole mechanism. (Work out from the protocol what install 1 is in that
mode, and what the log slots contain at that instant.)

# Part 5 — Ordered versus data journaling (~1.0 h, week 22, 13%)

Data journaling writes every data block twice. `mode=ordered` is the classic
optimisation: journal **metadata only**, and write data blocks straight to
their final location — made durable, by a barrier, *before* anything of the
transaction is journaled. In `wal.py` that is `write_data`'s decision plus
one extra step in `end_op`; per create it costs `(2·3 + 2) + d` installs and
5 barriers, so the graded workload reports
`done ops=4 mode=ordered installs=39 barriers=20` (committed at 9, 19, 28,
39). The ordered campaign (`./sweep ordered ...`) must also come back
`bad=0`.

`JOURNAL-COST.md` is the analysis: both `done` lines; the reconciliation of
46 vs 39 against sheet 21 §B2(c)'s analytical counts (same formula, or if
not, say exactly where they diverge and why); the barrier counts and what a
barrier costs a real disk; and — the part that is graded hardest — **what
crash guarantee ordered mode gives up**. This workload only ever writes
fresh blocks, so the campaign cannot see the difference; describe the
workload that *would* (ch. 42's block-reuse/overwrite discussion is where to
look), and state what a crash can then expose.

---

# Running the tests

```sh
../tests/run.sh .        # from your working copy of starter/
```

The harness compiles the oracle itself, locates `blockdev.py`, `crashfs` and
`journal` in your directory, and drives them through their command lines —
a PASS/FAIL table, `N passed, M failed`, non-zero exit on any failure, a
timeout on every case. The campaign cases print `[info]` lines with their
point counts; a full run is ~170 crash-recover-check cycles and takes about
15 seconds.

## What is checked, and what is not

Parts 1–5's tools are machine-checked in full, including the complete
campaign at every crash point on both device modes, the recovery-crash
sweep, and the exact install counts. The prose halves of `CRASH-TABLE.md`,
`CAMPAIGN.md` and `JOURNAL-COST.md` are **not** machine-checked — a green
run with empty notes is not a finished lab; the explanation is where the
marks for Parts 2, 4 and 5 sit.

## What is on your honour

The `committed` lines the campaign's durability oracle consumes are printed
by the given `journal` CLI when your `end_op` returns. The oracle
cross-checks them against the crashed image's own log header, but a
`wal.py` that deliberately delayed its acknowledgement could shrink what
durability demands of it. Don't: `end_op` returning *is* the commit
acknowledgement, exactly as `end_op` returning in xv6 means the operation
is on disk.

---

# Stretch goals

Unweighted. Do them if the five parts came easily.

- **Revoke records** (ch. 42): delete a file, reuse its block for user data,
  and construct the crash in which replaying the old journal entry clobbers
  the new data. Then add revoke records to prevent it. The subtlest
  correctness issue in the chapter, very satisfying to reproduce.
- **Transaction batching**: hold several creates in one commit; measure
  installs saved against the window of loss widened. xv6's `begin_op` does
  exactly this — compare.
- **A circular log**: replace the fixed header+slots region with a wrapping
  log and a separate superblock pointer. The wrap point's off-by-one is a
  classic; make your sweep long enough to cross it.

---

# If you get stuck

- **`sweep` says `BAD no-crash: the device sailed past install N`** — your
  device is not enforcing `crash_after`. Part 1's crash semantics come
  first; nothing in Parts 2–5 is trustworthy until a crash actually stops
  the device.
- **Part 2's table disagrees with your prediction at one k** — the issue
  order is the contract. Compare your `issue` lines against the W1..W5
  table, then re-derive what is on disk after exactly k of them.
- **`installs=44` (or 19, or anything but 46)** — count your protocol
  against the step list: n slots, 1 commit, n checkpoint, 1 clear, per
  transaction. A pass-through write, or a skipped clear, shows up here
  first.
- **The fifo campaign is clean but `--dev reorder` fails at points inside
  end_op** — look at the order the drain installed your writes, and ask
  which of the three rules that order violates. `blockdev.py`'s model
  section says exactly what a barrier does and does not promise.
- **`sweep recovery` fails but the main sweeps pass** — trace what your
  recovery makes durable, in what order, and what a second recovery finds
  if the first stops between those installs.
- **A later op's campaign points corrupt an *earlier* file's data** — how
  many slots does your recovery replay, and which header field says so?
- **`read` inside a transaction returns stale data** — the transaction
  buffer is part of the device your writers see; `Journal.read` must look
  there before the disk.
- **The broken mode doesn't corrupt** — `commit-first` must remove every
  ordering constraint between the commit record and the log blocks,
  including the barrier between them. If a barrier still separates them,
  you have reordered the code but not the installs.
