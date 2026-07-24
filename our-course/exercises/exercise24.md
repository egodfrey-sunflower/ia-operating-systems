# Exercise Sheet 24 — Distributed file systems: NFS and AFS

**Attempt after Week 24.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise24-solutions.md`](solutions/exercise24-solutions.md).

**This sheet leans on:** OSTEP ch. 49–50; Howard et al. (1988), *Scale and
Performance in a Distributed File System*. It assumes ch. 48's
timeout/retry machinery (week 23) and ch. 40's inodes (week 18).

**You will need:** nothing — the sheet is pen-and-paper. §B4 traces a cache
timeline by hand; the `afs.py` simulator (`ostep-homework/dist-afs/`)
generates more such traces if you want extra practice. (Ch. 49's own
measurement homework needs externally-downloaded traces and is not used here
— see the week doc.)

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing.*

**A1.** An NFSv2 server keeps no record of which clients currently have a
file open.

**A2.** An NFS file handle contains the file's pathname.

**A3.** Without the generation number, a client holding an old file handle
could read a file that was created after its file was deleted.

**A4.** Because WRITE is idempotent, an NFS server may safely acknowledge a
write as soon as the data is in its memory.

**A5.** Flush-on-close guarantees that two NFS clients never observe
different contents for the same file at the same time.

**A6.** AFS callbacks make the server stateful — the very thing NFS's design
set out to avoid.

**A7.** Under AFS, a write by a process on one machine becomes visible to
processes on other machines as soon as the `write()` returns.

**A8.** NFS transfers data in blocks proportional to what the application
reads or writes; AFS transfers whole files.

---

## B. Protocols and workloads

**B1. Count the messages.**
A client mounts an NFS export (so it holds the root directory's file handle)
and an application runs: `open("/home/alice/paper.txt")`, four `read()`s of
one block each, `close()`. The client's caches start cold.

  (a) List the protocol messages sent to the server, in order, and the total.
      Why does `close()` contribute none?
  (b) Thirty seconds later the same application repeats the whole sequence.
      The client's block cache still holds the data, and the attribute-cache
      timeout is 3 seconds. Which messages are sent now?
  (c) Now 100 clients each keep this file cached and each re-derives its
      freshness whenever its attribute cache expires (every 3 s). How many
      GETATTRs per second does this one file cost the server? With 1,000
      such hot files, what is the aggregate rate — and what does AFS's
      callback design reduce that steady-state number to?
  (d) State, in one sentence each, what the attribute cache bought and what
      it cost.

**B2. Idempotency does the crash handling.**
  (a) Classify LOOKUP, READ, WRITE, MKDIR and REMOVE as idempotent or not,
      with one clause of justification each. (Recall READ and WRITE carry an
      explicit offset.)
  (b) Give the message-level timeline in which a client is told `mkdir`
      failed with "already exists" even though its own request created the
      directory.
  (c) A "tuned" NFS server acknowledges WRITEs from RAM and flushes lazily.
      Reproduce the chapter's three-block argument: the exact sequence of
      writes, acknowledgments, one crash, and the resulting file state that
      no crash-free execution could produce. State the invariant the tuning
      broke.
  (d) Ch. 49 handles a lost request, a lost reply, and a server crash with
      one client mechanism. Name it, and explain what property of the
      protocol makes the client's ignorance of *which* failure occurred
      harmless.

**B3. The workload decides: NFS vs AFS, costed.**
Let a network block transfer cost **L_net = 1 ms**, a local-disk block access
**L_disk = 0.1 ms**, and local memory **L_mem = 0.001 ms**. A large file has
**N_L = 10,000** blocks: it fits on the client's local disk but not in its
memory. Ignore lookup traffic and server think-time; assume caches start
cold, and warm caches hold whatever fit.

For each workload give the approximate time under NFS and under AFS, and the
ratio:
  (a) first sequential read of the whole large file;
  (b) an immediate second sequential read of the whole file;
  (c) reading a single block from the middle of the (uncached) large file;
  (d) opening the existing large file, overwriting every block sequentially,
      and closing;
  (e) appending one block to the (uncached) large file and closing.

Then state the two workload assumptions the AFS designers made that these
numbers reward, and which of (a)–(e) violates them.

**B4. The consistency timeline, by hand.**
Machines C1 (processes P1, P2) and C2 (process P3) share file F via AFS.
Trace this sequence, giving after each step: C1's cached copy, C2's cached
copy, the server's copy, and any callbacks established or broken.

```
 1. P1 (C1): open(F); write "A"; close(F)
 2. P3 (C2): open(F); read -> ?; close(F)
 3. P1 (C1): open(F); write "B"          (no close yet)
 4. P2 (C1): open(F); read -> ?; close(F)
 5. P3 (C2): open(F); read -> ?; close(F)
 6. P1 (C1): close(F)
 7. P3 (C2): open(F); read -> ?; close(F)
 8. P1 (C1): open(F); write "D"          (no close)
 9. P3 (C2): open(F); write "C"          (no close)
10. P3 (C2): close(F)
11. P1 (C1): close(F)
12. P3 (C2): open(F); read -> ?; close(F)
```

  (a) Fill in the five `read -> ?` results and justify each in a phrase.
  (b) After step 11, what is on the server, and what happened to C2's copy?
      Name the rule.
  (c) Contrast with NFS: for steps 8–11, what could the server's file look
      like under NFS's block-level writes, and why does AFS exclude that
      outcome?
  (d) At which steps does a client actually contact the server? Count the
      interactions and compare with what NFS polling would have cost across
      the same twelve steps.

**B5. Scale, measured then rebuilt.**
AFSv1 saturated a server at about **20 clients**. Suppose, in the spirit of
the paper's measurements, profiling shows the server CPU spending **45%** of
its time answering TestAuth validations and **25%** walking full pathnames
for Fetch/Store, the remaining **30%** doing genuine file work.

  (a) AFSv2 eliminates TestAuth polling (callbacks) and moves pathname
      traversal to clients (FIDs). If each client's genuine file work is
      unchanged, predict the new client capacity.
  (b) The measured improvement was to roughly 50 clients. Give two reasons
      the prediction from (a) overshoots.
  (c) The chapter names this methodology after Patterson: state it in one
      sentence, and point to the specific AFSv1→v2 design change that could
      *not* have been justified without the measurements.

---

## C. Discussion and design critique

**C1.** Ch. 50's aside insists cache consistency "is not a panacea": a code
repository with concurrent check-ins cannot rely on AFS's guarantees alone.
State precisely what AFS does guarantee, identify the two ways it falls short
of what the repository needs, and say what the application must add. What
general division of labour between file system and application does this
imply?

**C2.** For each deployment, choose **NFS, AFS, or neither**, and defend the
choice from this week's analysis — naming the workload feature that decides
it and the condition that would change your answer:
  (a) home directories for 500 developers who mostly edit small files at one
      machine at a time;
  (b) a render farm where hundreds of nodes read multi-gigabyte scene files
      start to finish, re-reading them across many jobs;
  (c) a transactional database whose files receive small random writes that
      must be durable at commit.

**C3.** You inherit a workgroup running NFSv2, and three tickets are open:

- *Ticket 1:* "I save a header on machine A, run `make` on machine B a
  second or two later, and the build sometimes uses the **old** header. Doing
  it again a few seconds later always works."
- *Ticket 2:* "During Friday's power failure the server rebooted mid-run.
  Every `write()` in our logger had returned success, but one output file
  has a stretch of **last month's data in the middle**. The previous admin
  had recently 'tuned the server for write speed'."
- *Ticket 3:* "Two batch jobs on different machines append status lines to
  one shared log file. The file now contains **interleaved fragments** of
  half-written lines from both."

For each ticket: identify the mechanism responsible (with the timeline that
produces the symptom), say whether it is a bug, a misconfiguration, or the
documented semantics of NFS, and give the fix or workaround with its cost.
Finish by stating which ticket would simply not occur under AFS, which would
occur in a different form, and which would remain — with one sentence of
justification each.
