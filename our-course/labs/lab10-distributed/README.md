# Lab 10 — Distributed: RPC and a caching file client

**Weeks 23–24 · 8.5 hours · OSTEP ch. 48 (distributed systems), ch. 49 (NFS), ch. 50 (AFS) · TLPI ch. 56 as socket reference**

Userspace, **C**, from scratch. Two processes on one machine, talking UDP
over loopback — no second host, no container, no network configuration,
no root, no xv6, no QEMU. You will need `bash`, `gcc` and `make`.

Chapter 48's argument is that you build reliability where you need it
rather than inheriting TCP's, and it hands you the exercise directly:
build a UDP client and server, then your own reliability layer. This lab
is that exercise grown until it carries a file system. You get a datagram
across (Part 1), make delivery reliable over a link that eats 30% of your
packets (Part 2), wrap it into an RPC library whose calls execute exactly
once even when every reply is at risk (Part 3), build an NFS-style
stateless file service you can kill and restart mid-workload without the
client noticing (Part 4), and then buy performance with a client cache
and pay for it in consistency, precisely measured (Part 5).

The network's unreliability here is simulated — a seeded drop simulator
in the given `net.c` — and that is what makes the lab honest: every
"lost" packet is reproducible from a seed, so the hard cases run on
demand, identically, every time. Parts 1–3 stand on ch. 48 alone;
read ch. 49 (statelessness, file handles, attribute caching) before
Parts 4–5, which arrive in week 24. Sheets 23–24 own the pen-and-paper
side (sequence traces, retry arithmetic, NFS message counts, the AFS
timeline); this lab measures the real thing under real simulated loss.

## Layout

```
lab10-distributed/
  README.md          this handout
  starter/           udpecho.c, reliable.c, rpc.c, rpcdemo.c,
                     fileserver.c, fileclient.c            <- work here
                     net.[ch], msg.[ch], rpc.h, fsproto.h  <- given, complete
                     Makefile                              <- given
  tests/run.sh       the autograder
  tests/given/       the harness's own copies of the given files
  solutions/         SPOILERS. Reference code, model reports, rubrics. Later.
```

Copy the working directories somewhere of your own and work there — copy
`starter/` *and* `tests/` (the autograder builds against `tests/given/`):

```sh
cp -r starter tests ~/lab10
cd ~/lab10/starter
make            # builds the five tools
make test       # ../tests/run.sh on this directory
```

`solutions/` is deliberately left behind.

## What you hand in

| File(s) | Part | Weight | Marked |
|---|---|---|---|
| `udpecho.c` — datagrams, a checksum, injected corruption | 1 | 12% | auto |
| `reliable.c` — ack/timeout/retry/seq; exactly-once at 30% loss | 2 | 22% | auto |
| `RELIABILITY.md` — behaviour at 0/10/30% loss, measured | 2 | 7% | rubric |
| `rpc.c`, `rpcdemo.c` — the RPC library and the counter service | 3 | 23% | auto |
| `fileserver.c`, `fileclient.c` — the stateless file service | 4 | 19% | auto |
| `RESTART.md` — the kill-and-restart demonstration | 4 | 5% | rubric |
| `fileclient.c` — the attribute cache | 5 | 7% | auto |
| `CACHE.md` — request reduction, the staleness window, the guarantee | 5 | 5% | rubric |

The `.md` deliverables are marked by hand against the rubric in
`solutions/README.md`, which you read *after* you have your own numbers.
A green autograder run with empty reports is not a finished lab.

---

# ⚠ The text interface, and what the harness trusts

Every command line and output line quoted below is a **fixed contract**:
the autograder starts your tools as real processes, drives them through
argv and stdin, and greps their stdout for these exact strings. Change
what a line says and the grader cannot read it. The starter files carry
every contract line in comments, already in place.

The autograder **never trusts your copies of the given files**. It
compiles your six `.c` files itself — its own `gcc -Wall -Wextra -Werror`
line, in its own directory, against **its own copies** of `net.c`,
`msg.c`, `rpc.h` and `fsproto.h` from `tests/given/` — so the loss
simulator, the wire-contract headers and the compiler flags are the
harness's ground, not yours. Your `Makefile` is run once as a smoke gate;
it is never the graded build. Keep `rpc.c` compatible with the given
`rpc.h`, and both file tools with the given `fsproto.h`.

## The given network, and the loss simulator

`net.h`/`net.c` give you one UDP endpoint: `net_open(0)` binds
127.0.0.1 on an **ephemeral port** (never hard-code a port — this
machine is shared; every server prints `port=N` and the port travels by
being read back, not by being agreed in advance). `net_send` sends one
datagram; `net_recv(..., timeout_ms)` receives one, waiting at most
`timeout_ms` ms (or forever if negative).

The loss simulator lives inside `net_send`: with `LOSS_RATE=30` in the
environment, each send is silently discarded with probability 30%,
decided by a PRNG seeded from `LOSS_SEED`. One draw per send, every
send. Two processes given different seeds drop **independently in the
two directions** — requests and replies each take their own risk — and a
run is exactly reproducible from (seed, rate, workload). `NET_STATS=1`
makes `net_close` report `net: sent=S dropped=D recvd=R` on stderr; the
autograder uses its own build of this counter to verify that a run it
calls lossy really did lose packets. Use it yourself:

```sh
LOSS_RATE=30 LOSS_SEED=31 NET_STATS=1 ./reliable client 40404 n=50
```

`msg.h` gives bounds-checked big-endian marshalling (`mb_put_u32`,
`mb_get_blob`, ...). Byte packing is not the objective; use it.

---

# Part 1 — UDP client and server (~1.0 h, week 23, 12%)

Get a datagram across, and learn what the network does *not* do for you.

```
udpecho server
udpecho client <port> [n=<N>] [corrupt=<i>] [timeout=<ms>]
```

The server prints `port=N` and echoes forever. Every packet is
`u32 checksum | payload`; the payload of message *i* is `ping-%04d`.
The server verifies the checksum of everything it receives and echoes
good packets back verbatim; a packet that fails verification gets the
reply payload `BAD` (itself correctly checksummed) instead of an echo.
The checksum algorithm is yours, but it must catch any single flipped
byte.

`corrupt=i` makes the client flip one payload byte of message *i*
**after** computing the checksum (`pkt[5] ^= 0x40`) — corruption you
inject yourself, which the server must catch. The client classifies each
reply against the **original** payload it sent — exact echo → `ok`,
`BAD` reply → `bad`, anything else → `mismatch`, no reply within the
timeout → `lost` — and prints one summary line (fixed):

```
echo done n=8 ok=8 bad=0 mismatch=0 lost=0
```

The client must terminate and print its summary at **any** loss rate:
run it under `LOSS_RATE=25` and watch a bare, unprotected exchange lose
datagrams — the autograder does exactly that, and expects the client to
come back anyway, with `ok+lost=n`. That losing run is the reason
Part 2 exists.

# Part 2 — The reliability layer (~2.5 h, week 23, 29% with the report)

Build reliable delivery on top of what Part 1 showed you cannot trust.

```
reliable server
reliable client <port> [n=<N>] [timeout=<ms>] [retries=<K>]
```

The client sends messages `msg-0000` … `msg-<N-1>` (defaults: n=50,
timeout=100, retries=32). The contract, which must hold with up to
**30% loss in each direction**:

- the server hands each message to the application — one line, as it
  happens, flushed —

  ```
  deliver payload=msg-0007
  ```

  **exactly once per message, in the order sent**. Retransmissions it
  recognises and does not deliver are counted, not printed.
- both processes then terminate cleanly and report (fixed lines):

  ```
  server done delivered=50 dups=14
  client done sent=50 acked=50 retrans=33 giveups=0 elapsed_ms=1672
  ```

  The client exits 0 iff everything — including the close of the stream
  itself, which crosses the same lossy wire — was acknowledged. At 0%
  loss a correct layer has `retrans=0` and `dups=0`; the autograder
  checks both, and at 10% and 30% it checks the deliver sequence is
  `msg-0000..msg-0049`, each exactly once, in order, across several
  seeds. Ch. 48's ladder — acknowledgement, timeout, retry, sequence
  numbers — is the toolbox; the wire format between your client and your
  server is yours.

**`RELIABILITY.md`**: run the layer at 0%, 10% and 30% loss (both
directions, a few seeds each) and report retransmissions issued, dups
absorbed, and wall-clock cost (`elapsed_ms`), against the loss rate.
Two sentences on how the cost scales, and why: sheet 23's retry
arithmetic predicts the shape — check your numbers against it.

# Part 3 — An RPC library (~2.0 h, weeks 23–24, 23%)

Wrap the machinery in a call interface. `rpc.h` is given and fixed: the
client API (`rpc_client_open` / `rpc_call`), the server API
(`rpc_server_run` + a handler), and the wire header —
`magic | client | seq | proc-or-status | blob`. Every client instance
has its own `client` id and numbers its own calls 1, 2, 3…; a
retransmitted request is byte-identical to the original. You write
`rpc.c`. Two requirements carry everything:

1. **`rpc_call` returns exactly one result per call** over a wire that
   loses datagrams in both directions: retransmit the identical request
   on timeout, and never take anything except the awaited reply as the
   answer.
2. **The server dispatches each call to the handler exactly once**,
   however many times that call's request arrives. A retry of a call it
   has already executed is answered with the reply it already computed —
   never by executing the handler again.

`rpcdemo` is the proving ground — a counter service. `ping` echoes;
`inc` advances a shared counter and returns the new value — deliberately
**not idempotent**, so executing one call twice is a wrong answer that
never heals; `get` reads the counter; `slow ms=<M>` sleeps before
replying (long enough for a caller to retry meanwhile); `shutdown` stops
the server. Fixed lines:

```
rpcdemo server                 ->  port=N   ... then on shutdown:
server done handled=27 executed_inc=25 dup_replies=12
rpcdemo client <port> id=<I> [timeout=..] [retries=..] <op>
   ping n=5                    ->  ping done calls=5 ok=5 retrans=0
   inc n=25                    ->  inc done calls=25 ok=25 value=25 retrans=12
   get                         ->  get value=25
   slowping ms=120 n=3         ->  slowping done slow_ok=1 pings_ok=3 retrans=2
   shutdown                    ->  shutdown ok
```

(`slowping` issues one `slow` call, then n `ping`s on the same socket —
with a timeout shorter than the sleep, so the slow call is retried and
its late replies are still in flight when the pings begin.)

The autograder's central experiment: the **server's replies are dropped
35% of the time while every request arrives**, and 25 calls to `inc`
must leave the counter at exactly 25 — then again with two clients
interleaving, with requests dropped instead, and with loss both ways.
`executed_inc` must equal the calls made, every time. This is the
at-least-once versus exactly-once distinction from ch. 48 made
executable, and it is where this lab's marks concentrate.

# Part 4 — A stateless file service (~2.0 h, week 24, 24% with the report)

A file server and client over your RPC library. `fsproto.h` is given and
fixed: `FS_LOOKUP` (name → handle, optionally creating), `FS_GETATTR`
(handle → size, mtime), `FS_READ` and `FS_WRITE` (handle, **explicit
offset**, data). Read its header comment before writing a line — it
states what a file handle must *mean*: the file itself, not a
conversation with the server process that issued it. Any handle the
server has ever returned must still work after that process is killed
and a fresh one starts on the same directory. The server holds **no
per-client state** — no open files, no cursors, no tables that matter —
and every operation is idempotent: executed twice, it leaves what
executing it once leaves. Ch. 49's payoff: one mechanism, retry, then
covers a lost request, a lost reply, and a dead server alike.

```
fileserver <exportdir> [port=<P>]      # port=0 default; prints port=N
fileclient <port> workload name=<f> n=<K> [id=..] [timeout=..] [retries=..] [delay=<ms>]
fileclient <port> cmd [id=..] [ac=<ms>] [timeout=..] [retries=..]
```

The **workload** (the graded one, cache off): for i = 0…K−1 write the
32-byte record `rec%05d` + 24 dots at offset 32·i, then read every
record back and verify it byte-for-byte, then check `getattr` says
32·K. One `progress phase=write i=%d` / `progress phase=read i=%d`
line per operation, flushed as it happens — the harness watches your
output live — then (fixed):

```
workload done n=200 wrote=200 verified=200 size=6400 rpcs=402 retrans=0
```

Exactly 2K+2 RPCs: one lookup, K writes, K reads, one getattr — the
autograder asserts the count on a clean wire. `delay=<ms>` paces the
workload so that a mid-run server kill lands mid-run.

The **cmd** mode is a command loop on stdin — `open <name>`,
`read <off> <len>`, `write <off> <text>`, `getattr`, `sleep <ms>`,
`stats`, `quit` — with fixed output lines (see the starter's header
comment). Part 4 uses it for basic-operation checks; Part 5 lives in it.

**How it is graded**: the workload must complete, byte-identically, on a
clean wire (with the exact RPC count), at 30% loss both ways, and — the
point of the part — across a `kill -9` of the server at an arbitrary
moment mid-workload, with a fresh server started on the same port and
directory. The client rides out the outage on retries and **must not
notice**: exit 0, all records verified, file bytes identical. The
autograder does this twice, once killing during the writes and once
during the reads.

**`RESTART.md`**: run the restart experiment yourself (`delay=5`, kill
the server, restart it — the run.sh restart cases show the recipe) and
write up what the client observed, how many retransmissions the outage
cost, and *why* the design makes the restart invisible — which specific
properties of the protocol (where the offset lives, what the handle
names, what re-executing a write does) each lost-and-retried message
leans on. One paragraph on what would break if any one of them were
dropped.

# Part 5 — Client caching and the consistency problem (~1.0 h, week 24, 12%)

Give `fileclient` a cache, and then measure exactly what it costs you.
`ac=<ms>` sets the attribute-cache timeout (0 = off, the Part 4
behaviour). The model, from ch. 49:

- the client caches the file's data and its attributes; attributes are
  trusted for `ac` ms after they were fetched;
- a read inside that window is served **from the cache: zero
  messages** — a hit (`stats` counts it);
- a read after the window costs **one `getattr`** (a revalidation): if
  size and mtime are unchanged the cached data is still good, else it
  is discarded and refetched;
- a write goes through to the server **before the `write` command
  completes** — no write-back delay — and the writer's cache revalidates
  before it is trusted again;
- `stats rpcs=%ld reads=%ld hits=%ld revals=%ld` reports the ledger.

The autograder measures both sides of the bargain. The win: 40 reads of
an unchanged file must cost at most a handful of RPCs (one lookup, one
getattr, one fetch — everything else a hit), every read still returning
the right bytes. The price: two clients on one file — A reads through
its cache; B overwrites the same bytes; A reads again **inside** the
window and must see the old data (that is what a cache *is*); A reads
**past** the window and must see the new. Staleness, bounded by `ac`. The harness drives A one
command at a time and gives the timing generous margins.

**`CACHE.md`**: the measured request reduction (your `stats` numbers, at
two different `ac` values); the two-client staleness run, reproduced by
hand with timestamps; and — the graded core — a precise statement of the
consistency guarantee you now offer, in one or two sentences of the form
"a read returns data no staler than …". Then one paragraph against
ch. 50: what AFS's callbacks would change about both your numbers and
your sentence.

---

# Running the tests

```sh
make            # builds the five tools, -Wall -Wextra -Werror clean
make test       # ../tests/run.sh on this directory
```

`run.sh` prints a PASS/FAIL table and `N passed, M failed`, and exits
non-zero if anything failed. Every case runs under a timeout; every
server it starts is killed by PID before it exits; all ports are
ephemeral. A full run takes under a minute against a working submission.
To drive the tools yourself, the whole interface is two terminals:

```sh
./reliable server                     # prints port=N
LOSS_RATE=30 LOSS_SEED=31 ./reliable client <N> n=50
```

## What is checked, and what is not

Parts 1–5's tools are machine-checked in full: the exactly-once delivery
diff across seeds and rates, the counter arithmetic under every loss
direction, the restart-mid-workload byte-identity check, and the
staleness window from both sides. Where the harness claims a run was
lossy, it proves it from its own simulator counters, and the restart
cases verify the kill really landed mid-run. `RELIABILITY.md`,
`RESTART.md` and `CACHE.md` are **not** machine-checked — they are where
Parts 2, 4 and 5's rubric marks sit, and they need your numbers, not the
harness's.

## What is on your honour

The counters your tools print about themselves — `retrans`, `rpcs`,
`hits`, `elapsed_ms` — are cross-checked only where the harness can see
the truth from outside (drops from its own simulator, data correctness
from the bytes). That your `stats` ledger honestly counts what it says
it counts is on your honour — and on your interest: `CACHE.md` is built
from it. `elapsed_ms` is wall-clock and noisy; do not build an argument
on a small gap.

---

# Stretch goals

Unweighted. Do them if the five parts came easily.

- **AFS-style callbacks**: replace the attribute timeout with a promise —
  the server records who has a file cached and notifies on change. (The
  server now carries state; note what that does to Part 4's restart
  story.) Compare message counts and the consistency sentence against
  Part 5's. This is ch. 49 versus ch. 50 in your own code.
- **Exponential backoff**: replace the fixed retransmission timeout and
  measure retries and elapsed time at 30% loss against the fixed
  version.
- **Fragmentation and a sliding window**: let a call carry a payload
  larger than one datagram, then keep several fragments in flight and
  measure the throughput gain over stop-and-wait.

---

# If you get stuck

- **A client hangs forever the first time a reply goes missing** — some
  call in your loop can wait without a bound. `net_recv`'s last argument
  exists for this.
- **Part 2 at 30% loss prints more than 50 deliver lines** — what, at
  the receiver, distinguishes a retransmission from a new message?
- **`reliable client` exits non-zero at high loss with all 50 messages
  acked** — the close of the stream crosses the same lossy wire as
  everything else. Which process is still waiting, and for what? Run
  both sides with `NET_STATS=1` and look at who dropped the last
  datagram.
- **Fine at 0% loss, but the 30% cases crawl or time out** — compare
  your timeout to a loopback round trip, and check what you send while
  waiting.
- **`get` returns more than the number of `inc` calls made** — the
  server saw more requests than the client made calls, and executed
  them. Part 3's requirement 2 says whose job that is.
- **The two-client case comes up short by exactly the second client's
  calls** — how does your server decide that two requests are "the same
  call"?
- **`slowping`: `slow_ok=1` but the pings fail** — the slow call's late
  replies are still arriving when the pings begin. What makes a reply the
  answer to *this* call rather than the previous one?
- **After the restart, every operation fails** — what does your file
  handle mean to a server process that has just started? `fsproto.h`
  says what it must mean.
- **Restart mid-writes: the client finishes happily but the file bytes
  are wrong** — which side of the protocol decides where a write lands?
- **Staleness: the third read still shows the old data** — what ends a
  cache entry's validity, and when did your client last ask the server
  anything at all?
- **The hit-rate case says `rpcs` is too high** — cost your reads
  against Part 5's model: a hit is zero messages, a revalidation is one.
  Which reads of yours are paying more?
