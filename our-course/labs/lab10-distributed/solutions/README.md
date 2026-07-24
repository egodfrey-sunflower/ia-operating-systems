# Lab 10 — Reference tools and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference udpecho.c, reliable.c, rpc.c, rpcdemo.c,           ║
║  fileserver.c and fileclient.c, the model RELIABILITY.md,         ║
║  RESTART.md and CACHE.md, and the rubrics for all three — which   ║
║  are 17% of the lab and self-marked.  Reading the model reports   ║
║  before you have your own numbers turns the open-ended sixth of   ║
║  the lab into a fill-in form.                                     ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g, zero warnings
make test       # ../tests/run.sh on this directory: 26 passed, 0 failed
```

The reference is six C files: `udpecho` (~150 lines), `reliable` (~240),
`rpc.c` (~250), `rpcdemo` (~270), `fileserver` (~200), `fileclient`
(~470, of which the Part 5 cache is ~120), comments included. **The
starter skeleton scores `0 passed, 26 failed`** — every case is
falsifiable by a do-nothing stub, and every one fails on it. A full
reference run takes ~40 s (most of it deliberate loss-retry waiting and
the Part 5 timing windows); the starter takes longer (~2.5 min) because
its hangs run into per-case timeouts, which is itself the timeout
machinery working.

## Design notes (what the reference chose, and what else is acceptable)

- **Part 2 is stop-and-wait**: one DATA in flight, ACK per seq, receiver
  delivers `seq == expected`, re-acks anything older, FIN closes. A
  sliding window is more than the part asks for but grades identically
  as long as delivery to the application stays in-order exactly-once:
  the lossy cases assert the harness's own drop counters, `retrans>0`
  and the exact 50-line delivery — never your server's `dups` counter,
  which a windowed/cumulative-ack layer may honestly report as 0.
  The subtle half is the **close**: the server must keep re-acking FIN
  retries for a drain period after the first FIN (the reference waits
  for 400 ms of silence), because the FIN's ack crosses the same lossy
  wire; exit on first FIN and the client whose ack was dropped retries
  into a void and dies non-zero. The grader's 30%-loss seeds hit this.
- **Part 3's server keeps one cached reply per client** — (client, seq)
  of the most recent call, which under stop-and-wait is all a retry can
  refer to. Requests older than the cached seq are ignored (they can
  only be ghosts of a completed call — answering them is optional;
  re-executing them is the bug). The client discards any reply whose
  (client, seq) is not the call in flight; the `slowping` case exists
  precisely to queue stale replies on the client socket and watch what
  it does with them.
- **Part 4's handle is the inode number**, resolved on *every* request
  by scanning the export directory. O(files) per op and perfectly
  stateless: validity depends on the directory contents, never on the
  process's history. Any scheme with that property passes (e.g. the
  name itself embedded in the handle); any per-process table fails the
  restart cases, by design.
- **Part 5 caches the whole file plus its attributes**, trusts them for
  `ac` ms, revalidates with one getattr, and compares (size, mtime_sec,
  mtime_nsec) — size alone cannot see the same-length overwrite the
  staleness case performs, which is why FS_GETATTR carries mtime. The
  write path is write-through plus self-invalidation, the simplest
  policy that satisfies the write-through case.

## Why the harness builds against its own given files

`run.sh` copies the six student `.c` files into its own temp directory
next to `tests/given/`'s `net.c`, `msg.c`, `rpc.h` and `fsproto.h`, and
compiles there with its own `-Wall -Wextra -Werror` line. So: the loss
simulator that decides what "30% loss" means is the harness's; the wire
contract in `rpc.h`/`fsproto.h` is the harness's; the flags are the
harness's. Where the harness asserts that a run was lossy (`dropped>0`),
the number comes from **its own** `net.c` on stderr (`NET_STATS=1`),
never from a counter the submission prints about itself. Every server
binds an ephemeral port and the harness reads it back from `port=N` —
nothing squats on a fixed port, so parallel use of a shared machine
cannot fake a failure.

---

## What the autograder checks, case by case, and the mutation that proves it

Every case below was verified by building the wrong implementation and
confirming that the named case — and the suite exit code — flips. The
mutation driver is `solutions/mutants.py` (a build-time tool; it patches the
reference and runs the full suite per mutant -- it lives here, not in
tests/, because its patches name the bugs). All 15 mutants are
caught; none survives.

| Case | Catches (mutation confirmed to fail it) |
|---|---|
| P1: clean exchange echoes every payload | a client/server that does not complete the round trip (starter) |
| P1: injected corruption is detected | **M11** server echoes without verifying the checksum → the flipped byte comes back as a `mismatch`, not a `BAD` |
| P1: lossy link, bare client survives | **M13** client blocks forever on the first lost reply → case timeout kills it → FAIL (plus: `ok+lost=20` forbids a client that quietly sends fewer) |
| P2: 0% loss — exactly-once, retrans=0, dups=0 | a layer that spams unconditional duplicates or resends without cause |
| P2: 10%/30% loss × 5 seeds — deliver diff | **M1** no retransmission (drop-on-loss) → undelivered messages, client gives up → 5 cases FAIL. **M2** receiver delivers every arrival (no dedup) → extra deliver lines break the exact diff → 5 cases FAIL. **M14**-style seq bugs likewise break the diff |
| P2: server terminates on its own | **M3** server exits on first FIN with no drain → at seeds 31/33 the FIN ack is dropped, the client dies non-zero → FAIL (a deterministic, seed-pinned firing of the last-ack problem) |
| P3: ping / inc+get lossless | marshalling that never round-trips (starter) |
| P3: **CENTREPIECE** — inc with replies dropped → exactly 25 | **M4** no reply cache (re-executes retries) → `value>25` (timing-dependent — how many retransmissions race the reply out of the server; measured 37–39 across runs, `executed_inc` matching) → FAIL, along with the two-client and loss-both-ways cases. **M14** retry carries a new seq (a retry *is* a new call) → over-executes → FAIL. The case also requires `retrans>0` and harness-verified reply drops, so it cannot pass vacuously on a quiet wire |
| P3: two clients → 20, not 10 | **M5** reply cache keyed on seq alone → the second client's calls collide with the first's cache → FAIL. Measured, M5 is caught overwhelmingly — 9 cases fail, *including* the centrepiece: every P3 case ends with fresh `get`/`shutdown` clients whose `seq=1` collides with the previous client's cached seq and is starved as a ghost. This case is kept because it fails for the right, teachable reason (B's live calls against A's cache), not because it is the only discriminator |
| P3: two clients interleaved under loss → exactly once each | **M5** again, directly: with both clients live at once under reply loss, one client's call hits the other's single cache slot and is starved (or answered with the other's reply, which the caller must discard) → `ok=10` never prints → FAIL. Only a (client, seq) key passes. This is the one case where cross-client confusion is exercised *while retries are in flight*, rather than by the sequential tails |
| P3: lost requests → exactly-once | request-direction loss exercises retry without dedup pressure; fails with M1-style RPC clients |
| P3: loss both ways → exactly-once | the combined case; fails under M4/M14 |
| P3: late duplicate reply ≠ next call's answer | **M6** client accepts any reply → a stale `slow` reply is taken as a ping's answer → `pings_ok<3` → FAIL. Fully deterministic (no loss simulation; a 120 ms server sleep against a 50 ms timeout) |
| P4: basic ops over the wire | wrappers/procs that do not move real bytes (starter); also fails under M12 |
| P4: clean workload, rpcs=402 exactly | a chatty or short-cutting client; the exact 2K+2 count plus byte-identical file plus `verified=200` forbids the do-nothing pass |
| P4: workload at 30% both ways | an RPC layer that cannot carry file traffic under loss |
| P4: RESTART mid-writes | **M7** handles are a per-process table → post-restart ESTALE → FAIL. **M8** writes land at a server-side cursor → the restarted server's cursor restarts at 0, the file bytes are wrong → FAIL. Anti-vacuous guard: the harness asserts the client had NOT finished when the kill landed, and requires `retrans>0` (the outage was really ridden out) |
| P4: RESTART mid-reads | **M7** again — old handles must survive into the new process even when nothing is being written |
| P5: 40 reads → ≤6 RPCs, all bytes right | **M9** cache disabled → rpcs=42 → FAIL (and the 40 data checks forbid a cache that serves garbage) |
| P5: stale read inside the window | **M9** — a cache that always refetches shows NEW too early → FAIL. This case is what makes the hit-rate case non-fakeable: the cache must actually *be* a cache |
| P5: staleness ends past the window | **M10** attributes never expire → the third read still shows OLD → FAIL. **M15** revalidation compares size alone → the same-length overwrite is invisible, the third read still shows OLD → FAIL (this is why the staleness case overwrites without changing the size, and why FS_GETATTR carries mtime) |
| P5: write-through while A still runs | **M12** write-back flushed at quit → B, reading while A is alive and un-quit, sees nothing → FAIL (the case drives A over a FIFO precisely so A is still running when B reads) |

**Vacuous-pass guards, explicitly**: Part 2's verdict is an exact diff
against the full 50-line expected delivery (a silent client cannot pass
"0 of 0 lost"); Part 3's counter cases assert `executed_inc` equals the
calls made *and* that the harness's simulator really dropped datagrams
*and* that the client really retransmitted; Part 4's restart cases fail
themselves if the workload finished before the kill; Part 5's hit-rate
case requires exactly 40 read lines with correct bytes, not just a low
RPC count.

**Determinism**: all loss behaviour derives from `LOSS_SEED` through the
harness's own `net.c` (one PRNG draw per send, unconditionally). On
loopback the only timing dependence is "a reply beats a 50 ms timeout",
which holds by four orders of magnitude; the suite was run 20× back to
back with identical results (see the build report). Part 5's timing
windows carry seconds of margin around millisecond operations.

---

## Rubrics for the hand-marked reports

### RELIABILITY.md (7%)

| Points | For |
|---|---|
| 2 | A table over 0%, 10%, 30% (≥2 seeds each): retrans, dups, elapsed_ms, all 50 delivered at every cell |
| 2 | The scaling argument: cost grows with 1/(1−p_eff) where p_eff is the chance a *round trip* fails (≈ 1−(1−p)² both ways — at 30% that is ~0.5, i.e. about one retry per message), checked against the measured retrans |
| 2 | Dups explained: a dup at the receiver is a lost *ack*, not a lost message — the two directions separated in the argument |
| 1 | Elapsed time related to timeout × retrans (and why elapsed is noisy) |

Reference numbers (seed 1/2, n=50, timeout=50): 0% → retrans 0/0, ~5 ms;
10% → retrans 9/18, dups 2/11, ~0.5–0.9 s; 30% → retrans 38/50,
dups 13/23, ~1.9–2.5 s.

### RESTART.md (5%)

| Points | For |
|---|---|
| 1 | The experiment reproduced (kill point, outage length, retrans cost) with the client finishing verified=n |
| 2 | The *why*, tied to specific properties: the offset travels in the request; the handle names the file, not the process; a re-executed write is idempotent. Each lost-message case (request, reply, dead server) mapped onto plain retry |
| 2 | The one-paragraph counterfactual: what breaks with a server-side cursor, or table handles, or a non-idempotent op (append) — any one, argued concretely |

### CACHE.md (5%)

| Points | For |
|---|---|
| 2 | The measured reduction: stats at ac=0 vs ac>0 for the same read workload (reference: 42 RPCs → 4 for open+write+40 reads), hits/revals ledger explained |
| 2 | The staleness run reproduced with rough timestamps, and the guarantee stated precisely: *a read returns data no staler than ac ms* (equivalently: a write is visible to other clients within ac) — not "reads may be stale", which is not a bound |
| 1 | The AFS paragraph: callbacks move the cost from reads (periodic revalidation) to writes (notification), tighten the guarantee to close-to-open/di­rect notification, and put state back on the server — with the restart consequence noted |

A green autograder run plus these three reports at rubric standard is a
finished lab.
