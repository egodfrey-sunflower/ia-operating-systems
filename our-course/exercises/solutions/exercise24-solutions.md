> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 24 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their working;
> for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. TRUE.** That is statelessness, NFSv2's central design decision: no open
files, no file positions, no record of client caches. Every request carries
everything needed to serve it, which is what lets a rebooted server resume
answering requests with no recovery protocol at all.

**A2. FALSE.** A handle is (volume identifier, inode number, generation
number). Pathnames are resolved *by the client*, one LOOKUP per component;
what comes back — and what READ/WRITE then carry — names the file
structurally, not by path.

**A3. TRUE.** Inode numbers are reused. Without the generation number, a
handle minted for the deleted file would be bit-identical to a handle for the
new file occupying the recycled inode, and the old client would silently read
someone else's data. Incrementing the generation on reuse makes stale handles
detectably invalid.

**A4. FALSE — and this is a trap.** Idempotency makes *retrying* safe; it
does nothing for a write that was **acknowledged and then lost**, because the
client will never retry an acked write. The chapter's rule is absolute: a
server may not return success on WRITE until the data is on stable storage.
See B2(c) for the corruption this prevents.

**A5. FALSE.** Flush-on-close addresses **update visibility** — a close
pushes dirty data to the server, so a *subsequent open elsewhere* can see it.
The **stale cache** problem remains: another client that already holds cached
data serves it until its attribute cache times out (~3 s), so two clients can
observe different contents simultaneously, by design.

**A6. TRUE.** A callback is a server-side promise, and promises are state:
the server must remember which clients cache which files. That is exactly the
trade AFS made — polling traffic eliminated in exchange for state that makes
a server crash "a big event" (all callbacks, held in memory, are lost, and
every client must revalidate).

**A7. FALSE.** Between machines, updates become visible **at close**, when
the whole file is flushed and callbacks are broken. The exception is *within*
one machine: local processes see each other's writes immediately, following
ordinary UNIX semantics.

**A8. TRUE**, and it is the root of the workload asymmetry in B3: NFS does
I/O proportional to the request (block-based); AFS fetches the entire file at
open and stores the entire file at close if modified — wonderful for
whole-file sequential use, punishing for small accesses into large files.

---

## B. Protocols and workloads

**B1.**
**(a)** LOOKUP(root FH, "home") → LOOKUP(home FH, "alice") →
LOOKUP(alice FH, "paper.txt") → READ ×4 = **7 messages**. `close()`
contributes nothing because the server tracks no opens — there is nothing to
tell it; the client just frees local state.
**(b)** The data is cached but 30 s > 3 s, so the attribute cache has
expired: the client sends **one GETATTR** for the file; the returned
modification time shows no change, so all four reads are served from the
local cache. (Accept: additional GETATTRs for cached directory entries,
implementation-dependent.) Total ≈ 1 message instead of 7.
**(c)** Per file: 100 clients ÷ 3 s ≈ **33 GETATTR/s**. Across 1,000 hot
files: ≈ **33,000 GETATTR/s** — the flood the chapter describes, of clients
asking "has anyone changed this?" when almost nobody has. AFS steady state:
**zero** — the server speaks only when a file actually changes (callback
break). Polling versus interrupts, exactly.
**(d)** Bought: the common case (single-client repeated access) goes to the
server rarely instead of on every open. Cost: up to 3 seconds of staleness,
and — the deeper price — consistency semantics defined by an implementation
timer rather than by a rule anyone can reason about.

**B2.**
**(a)** LOOKUP: **idempotent** — pure read. READ: **idempotent** — carries an
explicit offset, no server-side cursor to advance. WRITE: **idempotent** —
data + explicit offset; writing the same bytes to the same place twice equals
once. MKDIR: **not** — a retry of a succeeded create fails with EEXIST.
REMOVE: **not** — a retry of a succeeded remove fails with ENOENT.
**(b)** Client sends MKDIR → server creates the directory → **reply lost** →
client times out and resends → server finds the directory exists → returns
"already exists" → application is told its own successful operation failed.
The chapter's verdict (Voltaire's Law): live with the corner case rather than
complicate the design.
**(c)** File blocks initially `x… / y… / z…`. Client writes block 1 = `a…`
(server commits to disk, acks), block 2 = `b…` (server acks **from RAM**),
block 3 — but the server **crashes** before flushing block 2, reboots, then
receives and commits block 3 = `c…`. All three writes were acknowledged, yet
the file reads `a… / y… / c…` — old data sandwiched between new, a state no
crash-free execution of those three writes could ever produce. Invariant
broken: **an acknowledged write must already be durable**, because the
client's failure handling (retry until acked, never after) is sound only on
that assumption.
**(d)** The mechanism is **timeout-and-retry**, uniformly. It is safe because
the common operations are **idempotent**: whether the request was lost (never
executed), the reply was lost (executed once), or the server crashed and
rebooted (maybe executed), re-execution yields the same result — so the
client need not know which of the three happened.

**B3.** With N_L = 10,000, L_net = 1 ms, L_disk = 0.1 ms:

| | NFS | AFS | ratio |
|---|---|---|---|
| (a) first sequential read | N_L·L_net = **10 s** | N_L·L_net = **10 s** | 1 |
| (b) sequential re-read | N_L·L_net = **10 s** (too big for memory → refetch) | N_L·L_disk = **1 s** (local disk cache) | AFS 10× faster (= L_net/L_disk) |
| (c) one block from middle | 1·L_net = **1 ms** | N_L·L_net = **10 s** (whole-file fetch) | NFS 10,000× faster |
| (d) full sequential overwrite | N_L·L_net = **10 s** | fetch + store = 2·N_L·L_net = **20 s** | NFS 2× faster (the useless initial fetch; the chapter's O_TRUNC footnote applies) |
| (e) append one block | ≈ 1·L_net = **1 ms** | fetch + store ≈ 2·N_L·L_net ≈ **20 s** | NFS ~20,000× faster |

The AFS designers assumed (i) **files are read sequentially in their
entirety**, and (ii) **files are rarely write-shared** (mostly one user's
files, used at one machine). Workloads (c) and (e) violate the whole-file
assumption — (e) is the chapter's own killer example, the little log append
into a big file.

**B4.**
**(a)**
- Step 2 → **"A"**: P1's close flushed A; C2's open fetches the latest
  closed version (and gets a callback).
- Step 4 → **"B"**: P2 is on the *same machine* as the writer; local
  processes see writes immediately (UNIX semantics).
- Step 5 → **"A"**: B has not been closed, so the server still holds A; C2's
  cached copy is still valid (callback unbroken) and is served locally. Not
  staleness — the model says updates appear at close.
- Step 7 → **"B"**: step 6's close stored B and **broke C2's callback**, so
  this open re-fetches.
- Step 12 → **"D"**: see (b).

**(b)** Step 10 stored C (server = C, C1's callback broken); step 11 stored D
(server = **D**, C2's callback broken). C2's copy was invalidated by that
break, so step 12 refetches D. The rule: **last writer** — more precisely
**last closer** — **wins**: the file on the server is one client's version
*in its entirety*.
**(c)** Under NFS both clients would flush **individual dirty blocks** (on
cache pressure and at close), so the server could hold an arbitrary
interleaving — some blocks of C, some of D — a file neither client ever
wrote. AFS excludes it because the unit of update is the whole file at close:
mixed output is unrepresentable in the protocol.
**(d)** Server interactions: step 1 (store at close, plus the initial
fetch/creation), step 2 (fetch), step 6 (store + callback break → C2),
step 7 (fetch), step 10 (store + break → C1), step 11 (store + break → C2),
step 12 (fetch) — roughly **8 client-initiated interactions**; the opens at
steps 3, 4, 8, 9 and the read at step 5 are fully local under valid
callbacks. NFS across the same steps would issue a GETATTR at essentially
every open *plus* the block reads and writes — and would keep
paying GETATTRs forever after (one per open, indefinitely), while AFS's
steady-state cost for an unchanging file is zero.

**B5.**
**(a)** Only the 30% of genuine file work remains per client, so one server
sustains 20 × (100/30) ≈ **67 clients**.
**(b)** Any two of: the eliminated work is not entirely eliminated (the
server still resolves FIDs to files, and now pays a *new* cost — creating,
tracking and breaking callbacks); the remaining costs do not scale linearly —
queueing, context switching and memory pressure grow with client count, so
the last clients cost more than the first; a profile taken at 20 clients
extrapolates poorly to 60+; load is uneven across servers/volumes, so a
nominal average capacity overstates what the hot server can take.
**(c)** *Measure, then build* — demonstrate the problem with data before
designing the fix (Patterson's Law). The callback design is the change that
needed the measurements: nobody adds server state, crash-recovery complexity
and client-invalidation machinery unless profiling has proved that validation
polling (TestAuth) is what is actually burning the CPU. The measurements are
also what made "did v2 work?" answerable — same instrument, before and after.

---

## C. Discussion and design critique

**C1.** AFS guarantees: an `open()` yields the file as of the **latest
close** anywhere in the system (enforced by callbacks); a client's updates
become visible **atomically at close**, as a whole file; concurrent
cross-machine updates resolve to the last closer's version. It falls short of
the repository's needs in two ways. First, **no concurrency control**: two
racing check-ins both succeed locally and one entire update is then silently
discarded (last closer wins) — consistency is not mutual exclusion. Second,
**granularity**: the guarantee is per single file at open/close boundaries; a
check-in touching many files (objects + index) gets no atomicity across
them, and no isolation while it is in progress. The application must supply
its own machinery — file-level locking, or its own write-ahead/transactional
discipline. The general division: the file system provides *baseline session
semantics for casual sharing*; any application that genuinely cares about
concurrent updates brings its own concurrency control. (Marking note: the
aside says exactly this; credit answers that name last-closer-wins as the
specific data-loss mechanism, not just "there might be races".)

**C2.**
**(a) AFS.** Small files used at one machine at a time is the measured
workload AFS was built for: whole-file caching makes edits local-speed,
callbacks make the unshared case free, and per-server client capacity is the
design metric — right for 500 users. Flips if the "home directory" grows
shared append-heavy files (team logs, mail spools) — B3(e)'s catastrophe —
at which point NFS's block granularity is kinder.
**(b) AFS.** Multi-GB scene files read start-to-finish and re-read across
jobs is B3(b): the local-disk cache turns every re-read into L_disk, and the
read-only sharing pattern never triggers the write-sharing weaknesses. Flips
if jobs read *slices* of scene files rather than whole files (B3(c)) — then
whole-file fetch is a 10,000× tax and NFS wins.
**(c) Neither.** Small random writes durable at commit hit both systems where
they are weakest: AFS turns every commit into fetch-whole + store-whole and
resolves concurrent transactions by discarding one (last closer); NFS gives
block writes but a consistency model defined by attribute-cache timers, no
useful locking in v2, and sync-write latency on every commit. The database
belongs on a local disk or raw volume with its own transaction log — the
C1 division of labour, applied. Flips only for a single-instance,
never-shared database where NFS with synchronous export is merely slow
rather than incorrect.

**C3.**
**Ticket 1 — attribute-cache staleness; documented semantics, not a bug.**
Timeline: A's editor closes the file → flush-on-close puts the new header on
the server. B's `make` opens it within 3 s; B's attribute cache entry for
the header has not expired, so B trusts its cached copy *without asking the
server*, and compiles last save's bytes. Seconds later the entry has timed
out, a GETATTR notices the new mtime, the cache is invalidated — "doing it
again always works". Fixes: wait out/force the window (crude but real);
mount with the attribute cache disabled (`noac`) and pay B1(c)'s GETATTR
flood; or restructure the workflow (build on the machine that edits). The
cost axis is the whole lesson: consistency vs server load.

**Ticket 2 — server acknowledging writes before stable storage;
misconfiguration, and a protocol violation.** The "tuning" made the server
ack WRITEs from RAM (async). The logger's acked-but-unflushed writes died in
the crash; writes after reboot succeeded; result is B2(c)'s signature — a
stretch of old file contents *between* newer data, with every `write()`
having returned success. Fix: restore synchronous export (the protocol's
requirement). Cost: write latency — the honest mitigations are the ones the
chapter names, battery-backed RAM and a write-optimised file system
(the NetApp trick), not lying to clients.

**Ticket 3 — uncoordinated block-granularity appends; documented
semantics.** Each client extends the file based on its own cached view of
the size, buffers dirty blocks, and flushes them independently; without
cross-client coordination the flushed regions overlap and interleave — NFS
promises nothing about concurrent writers. Fixes, cheapest first: give each
job its own file and merge afterwards; funnel appends through a single
process (syslog pattern); or use file locking, accepting that NFSv2 locking
is a separate, fragile protocol. 

**Under AFS:** Ticket 1 **does not occur** — the close breaks B's callback,
so B's next open fetches the new header immediately; there is no timer
window. Ticket 2 **occurs in a different form** — visibility is whole-file
at close, so mid-file interleaving of old and new vanishes, but a server
that lies about the durability of a Store can still lose the *entire*
update; the failure unit changes from blocks to files. Ticket 3 **remains,
and worsens**: the two jobs' closes race and the last closer's file replaces
the other's *entirely* — one job's appends vanish without trace, B3(e) and
C1's data-loss mechanism in action.

*Marking note: full credit requires, per ticket: the named mechanism, a
timeline, the bug/misconfig/semantics verdict, and a fix with its cost.
The AFS coda separates students who memorised the comparison table from
those who can apply it.*
