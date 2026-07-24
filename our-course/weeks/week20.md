# Week 20 — Access control and cryptography · **MIDTERM 2**

> **Part V: Security** — taught here rather than at OSTEP's chapter position
> (see the note at the end). Week 20 of 27.

## What you'll learn

This is the week the course has been building toward for a month, and the reason
the security chapters sit here rather than at the end of the book.

The **access matrix** is the unifying abstraction: subjects down the side, objects
across the top, permitted operations in the cells. (It is Lampson's framing, not
the chapter's — ch. 55 goes straight to the two mechanisms below; the course
teaches the matrix because the Tripos examines it directly.) It is a beautiful
abstraction and completely impractical to store — a real system has thousands of
subjects and millions of objects, and the matrix is overwhelmingly empty. So real
systems store one of its two projections:

- Store it **by column** — for each object, who may do what — and you have an
  **access control list**. Unix's `rwx` owner/group/other bits are a
  ruthlessly compressed ACL.
- Store it **by row** — for each subject, what it may touch — and you have a
  **capability list**. An open file descriptor is a capability: an unforgeable
  token that both names an object and conveys authority over it.

You have been using both for three weeks without either being named. Ch. 39 taught
you permission bits and file descriptors as file-system plumbing; this week
reveals they are the same structure viewed from two directions. That is why the
chapter sits here rather than at week 24 — see the note at the end.

Chapter 56 then asks what cryptography can do for an operating system, and the
honest answer is narrower than students expect. Crypto protects data the OS *no
longer controls* — at rest on a stolen disk, in transit over a network someone
else owns. It cannot substitute for access control on a running system, because
a running system must decrypt to compute, and whoever controls the decryption
controls the data. The chapter covers symmetric and public-key ciphers and
hashes — and public-key authentication, the digital-signature idea without the
name or the construction — but the payoff sections are the OS-specific ones: full-disk
encryption and its key-management problem, and **cryptographic capabilities** —
unforgeable signed tokens that carry authority across trust boundaries.

**Key ideas:** access matrix · ACLs as columns, capabilities as rows · DAC vs MAC
· revocation asymmetry · symmetric vs public-key ·
cryptographic hashes · what crypto cannot do for a running OS · key management as
the real problem.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 55** | Access Control | 18 | 2.6 h |
| 2 | **OSTEP ch. 56** | Protecting Information With Cryptography | 20 | 2.9 h |

**No cross-reading this week.** OSPP has no security chapter, and at 38pp with a
midterm there is no room. (Silberschatz ch. 16–17 would serve — ch. 16 §16.4
for cryptography, ch. 17 for protection and the access matrix — but the course
prefers OSPP where both would do, and here neither is needed.)

**Paper (required):** ★ Thompson (1984), *Reflections on Trusting Trust*, CACM —
his Turing Award lecture. Three pages, and it will change how you think about
every security mechanism in this course. OSTEP cites it (ch. 55) and wonders in
its notes whether Thompson ever really did it; the question it raises is still open.

**Paper (optional):** Dennis & Van Horn (1966), *Programming Semantics for
Multiprogrammed Computations*, CACM — where capabilities were first described.
OSTEP calls it "the earliest discussion of the use of capabilities to perform
access control in a computer". Skip it in midterm week unless you're ahead.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise20.md`](../exercises/exercise20.md) — budget 3 h. **Entirely original material**: ch. 55 and 56 ship no homework, simulators or code. Self-mark against [`../exercises/solutions/exercise20-solutions.md`](../exercises/solutions/exercise20-solutions.md) |
| **Assessment** | 🎓 **MIDTERM 2** — sat this week. 90 minutes, closed book. Covers weeks 1–20, emphasis on concurrency, persistence and security. See [`../exams/README.md`](../exams/README.md) |
| **Lab** | [`../labs/lab08-security/`](../labs/lab08-security/) — **continues**, finishing week 21. Budget only ~1.5 h this week; the midterm takes the slack |
| **Past papers** | **Midterm weeks.** Weeks 10 and 20 carry no timed past paper; the midterm is that week's timed practice. The six protection papers this week unlocks are spread across weeks 21–24 |

### The protection hump

Week 20 unlocks the largest single-topic block of Cambridge questions in the
course — six on protection: `y2006p1q2`, `y2007p1q2`, `y2011p2q3`, `y2015p2q3`,
`y2019p2q4`, `y2025p2q3` all need the access matrix. (Only the week-10 virtual-
memory block, at eight, unlocks more in raw count.) Access control is the most-examined topic in the entire
Tripos bank — 10 questions — and under OSTEP's own ordering it would have arrived
at week 24, leaving three weeks to rehearse it.

Those six are deliberately **not** set this week. They are distributed across
weeks 21–24 so that they are spaced rather than crammed, and so this week can
carry the midterm.

## Week load

```
OSTEP ch. 55-56     38pp ÷ 7  =  5.4 h
Thompson paper [S]            =  0.75 h
Exercise sheet 20             =  3.0 h
Midterm 2 (sit + prep)        =  3.0 h
Lab 8 (trimmed)               =  1.5 h
Past papers          none     =  0.0 h
                                ------
                                13.65 h  ✅ top of the 12-14 h band
```

**This week has no slack at all.** If you are behind, the optional Dennis & Van
Horn paper goes first, then lab hours. Do not cut the sheet — it is the only
place ch. 55 and 56 get exercised, since neither chapter supplies any homework.

## Notes for the curious

### Why security is here and not at week 24

This is the course's one deliberate departure from OSTEP's chapter order, and it
is worth understanding.

OSTEP files security at ch. 52–57, so strict ordering puts access control at week
24 — three weeks before the final. That is bad for two reasons. First, it is the
most-examined Cambridge topic, and three weeks is not enough rehearsal. Second,
and more importantly, **ch. 55's two worked examples are things you learned in
week 17**: ACLs are illustrated with Unix `rwx` bits, and capabilities with file
descriptors, both taught in ch. 39. Under strict order you meet both mechanisms
as file-system details, then wait seven weeks to discover they were instances of
one structure.

So this course teaches ch. 52–56 at weeks 19–20, immediately after the
file-system core.
**Ch. 57 stays at week 25** because it genuinely depends on ch. 48 (distributed
systems), which you haven't read yet.

The cost: persistence is split — you did ch. 39–41 in weeks 17–18, and you'll
return for crash consistency (ch. 42–43) in week 21. This is survivable because
ch. 42 depends on ch. 40, not on ch. 41, so nothing in the dependency chain is
broken by the interruption.

### On Thompson

*Reflections on Trusting Trust* argues you cannot trust code you did not write
yourself — and then shows the argument is worse than that, because you cannot
trust the compiler either, and a compiler can be made to reproduce its own
backdoor with no trace in any source. The modern partial answer is **reproducible
builds** plus **diverse double-compiling**, neither of which existed when he
wrote. Read it before the midterm; it is short, and it is the kind of idea that
makes a good discursive answer.
