# Week 24 — Distributed file systems: NFS and AFS

> **Part IV: Distribution.** Week 24 of 27.

## What you'll learn

Last week built the communication layer; this week two complete systems are
built on it, and they disagree about almost everything. The disagreement is
the syllabus.

Chapter 49 is NFS (v2, the classic protocol), and its design is one goal
pursued ruthlessly: **simple and fast server crash recovery**. The means is
**statelessness** — the server remembers nothing about its clients. No open
files, no file positions, no cache lists. Every request carries everything
needed to serve it, packaged in a **file handle** (volume id, inode number,
generation number), and nearly every operation is **idempotent**, so the
client handles lost requests, lost replies and server reboots with one
mechanism: retry. The price is paid elsewhere: caching clients must ask the
server whether their copies are stale, the engineering patch is an
**attribute cache** with a ~3-second timeout, and the resulting consistency
story is famously hard to state — what version you read can depend on a
client-side timer. The chapter's other jewel: a server may **not**
acknowledge a WRITE before it reaches stable storage, and the three-block
example showing why is the cleanest crash argument in the book.

Chapter 50 is AFS, designed at CMU with the opposite obsession: **scale** —
how many clients can one server carry? AFS caches **whole files on the local
disk**, and replaces client polling with **callbacks**: the server *promises*
to tell you when a cached file changes. That is deliberate server state, with
everything statefulness costs (server crash recovery is now an event —
callbacks were in memory), bought because it makes the common case free and
the consistency story simple: you get the latest close's version when you
open, updates become visible **on close**, and concurrent writers resolve by
**last closer wins** — never NFS's block-level intermingling. The chapter
closes with the analytical NFS-vs-AFS workload comparison (Figure 50.4),
which is the spine of this week's sheet: whole-file caching wins re-reads and
loses badly on small I/O within big files.

The required paper is Howard et al. (1988) — OSTEP calls it "a classic" — and
it is the week's methodological lesson as much as its technical one: the AFS
designers **measured their prototype** (server CPU eaten by pathname
traversal and TestAuth validation polling), and redesigned what the
measurements indicted. The paper names four such areas — cache validation,
server process structure, name translation and load balancing — of which OSTEP
foregrounds the two protocol problems above; read the paper for the other two.
Chapter 51's two-page dialogue then closes Part IV.

**Key ideas:** statelessness · file handles and generation numbers ·
idempotency as the crash-recovery unifier · client caching and the cache
consistency problem · flush-on-close · attribute-cache staleness ·
write-through-to-stable-storage on the server · whole-file caching ·
callbacks (interrupts, versus NFS's polling) · last-closer-wins · scale as a
first-class metric · measure, then build.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 49** | Sun's Network File System (NFS) | 18 | 2.6 h |
| 2 | **OSTEP ch. 50** | The Andrew File System (AFS) | 14 | 2.0 h |
| 3 | **OSTEP ch. 51** | Summary Dialogue on Distribution | 2 | 0.3 h |

**Paper (required):** ★ Howard et al. (1988), *Scale and Performance in a
Distributed File System*, ACM TOCS. OSTEP (ch. 50) calls it "a wonderful
combination of the science of measurement and principled engineering." Read AFSv1's two measured
bottlenecks first (§50.2 primes you), then the protocol changes they forced.

**Cross-reading (optional):** OSPP §14.1, the transaction abstraction — the
general frame behind "a file is updated atomically at close". Take it only if
the week runs light; it is the relief valve, not required.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise24.md`](../exercises/exercise24.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise24-solutions.md`](../exercises/solutions/exercise24-solutions.md). §B4 can optionally use the `afs.py` simulator |
| **Lab / project** | Combined 5.5 h this week (3.5 h finishing lab 10, 2.0 h opening the project). [`../labs/lab10-distributed/`](../labs/lab10-distributed/) — **ends this week**: NFS-style client caching with a consistency policy, plus failure injection. Then [`../labs/project/`](../labs/project/) — the **final project starts**: read the option list and commit to one before week 25 |
| **Timed past paper** | `y2015p2q3` — 35 min closed book, then self-mark (~1 h total). **ACLs versus capabilities**: the two models; how Unix file access control implements one of them and what it simplifies away (owner/group/other, 9 bits); the principle of minimum privilege, and an argued case for which model supports it better. Bookwork with a genuine comparison in (d) — the closest the bank comes to examining week 20 directly. Marks 4 + 4 + 6 + 6 = 20 |

## Week load

```
OSTEP ch. 49-51     34pp ÷ 7  =  4.9 h
Howard AFS [M]                =  1.5 h
Exercise sheet 24             =  3.0 h
Timed paper (y2015p2q3)       =  1.0 h
Lab 10 (end) + project start  =  5.5 h
                                ------
                                15.9 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

If pressed, the optional OSPP §14.1 goes first (it is already outside the
total), then project reading can slip a few days — but choose your project
option this week regardless; weeks 25–26 assume the decision is made.

## Notes for the curious

- **Ch. 49's homework is measurement, with a catch:** it analyses real NFS
  traces (Ellard/Seltzer) that must be downloaded separately from the OSTEP
  homework page — the full sets are not in the `ostep-homework` repo (though
  `dist-nfs/` does ship one small `sampleTrace.csv`). This course
  doesn't schedule it: Lab 10's failure injection covers the empirical slot,
  and sheet 24 drills the protocol instead. If traces appeal to you, it is a
  fine rainy-afternoon extension.
- **Ch. 50 ships `afs.py`** (`ostep-homework/dist-afs/`), a simulator that
  generates cache-consistency timelines like Figure 50.3. Sheet 24 §B4 works
  such a trace by hand; the simulator's README shows how to generate more.
- NFSv2 is the *teaching* protocol. v3 made small changes (notably
  asynchronous writes with a COMMIT operation, easing the stable-storage
  bottleneck); v4 broke with the past — it is stateful, with opens, leases
  and delegations that look distinctly AFS-ish. The ideas you learned this
  week are the axes on which those versions moved.
- The security hole ch. 49 ends on — early NFS trusted clients to state
  their own UIDs — is worth holding onto: week 25 (distributed system
  security) is about exactly why "the server checks a field the client
  filled in" is not authentication, and what Kerberos does instead.
- The chapter's aside "cache consistency is not a panacea" repays a slow
  read: even AFS's clean model only promises something *per file, at close
  granularity*. Applications wanting multi-file or intra-file atomicity need
  their own locking or transactions — OSPP §14.1's subject, and the reason
  databases run on raw volumes more often than on NFS.
