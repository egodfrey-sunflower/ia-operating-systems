# Week 19 — Security fundamentals and authentication

> **Part V: Security** — taught here rather than at OSTEP's chapter position;
> [week 20](week20.md) explains the placement in full. Week 19 of 27.

## What you'll learn

Security begins with a two-page dialogue (ch. 52) whose one serious point is
worth holding onto all week: security is **not** reliability. Reliability
defends against accident; security defends against an *adversary* — clever,
adaptive, lazy in the productive sense, and aiming at your weakest link.
Everything else in Part V is a response to that difference.

Chapter 53 sets the frame. What is the OS protecting? Everything — the OS
has total control of the hardware, so a compromised OS can read any memory,
corrupt any file, and lie in answer to any system call; processes are at its
mercy. Goals are organised as **CIA** — confidentiality, integrity,
availability (plus non-repudiation) — and refined into explicit **security
policies** enforced by general **mechanisms**: the same policy/mechanism
split you know from scheduling. The chapter's core asset is the
Saltzer-and-Schroeder-tradition **design principles**: economy of mechanism,
fail-safe defaults, complete mediation, open design, separation of
privilege, least privilege, least common mechanism, and (psychological)
acceptability. These are not platitudes — sheet 19 makes you apply each one
to concrete design flaws, and the chapter is honest that real systems trade
some of them (complete mediation especially) for performance and usability.
The enforcement hooks are ones you already own: memory protection from
Part I, and the system-call trap, where the OS can check *every* request
against policy.

Chapter 54 asks how identity gets attached to a process in the first place.
The vocabulary: a **principal** (user, group, or service) is represented by
an **agent** process requesting access to **objects**, with past decisions
cached as **credentials**. Mechanically, identity is a UID in the process
control block, **inherited across `fork()`**, and set at the root of the
process tree by a privileged login process. That reduces the problem to
authenticating humans, classically by **what you know** (passwords —
hashing, salting, dictionary attacks, and why the system never needs to
store the password itself), **what you have** (tokens, smart cards, phones),
and **what you are** (biometrics — false positives versus false negatives,
and the crossover error rate). The chapter closes with the cases that don't
fit the login story: authenticating **non-humans** (`webserver`,
`lightbulb`), groups, and `sudo`.

One boundary to respect: this week's sheet exercises the identity machinery,
the design principles, and the biometric trade-off. The *arithmetic* of
salting — what it costs an attacker, and why slow hashes are the real
defence — is exercised on **sheet 20**, which revisits ch. 54 with a week's
hindsight. Don't be surprised that sheet 19 leaves it alone.

**Key ideas:** adversaries vs accidents · CIA + non-repudiation · policy vs
mechanism · the eight design principles · complete mediation via the trap ·
principal / agent / object / credential · UID inheritance across `fork` ·
what-you-know / what-you-have / what-you-are · hashed + salted passwords ·
false positive/negative trade-off · authenticating non-humans.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 52** | A Dialogue on Security | 2 | 0.3 h |
| 2 | **OSTEP ch. 53** | Introduction to Operating System Security | 12 | 1.7 h |
| 3 | **OSTEP ch. 54** | Authentication | 19 | 2.7 h |

**No cross-reading this week.** OSPP has no security chapter; OSTEP's
treatment plus the paper below stand alone. (FreeBSD ch. 5 exists as optional
depth, but nothing this week needs it.)

**Paper (required):** ★ Saltzer & Schroeder (1975), *The Protection of
Information in Computer Systems*, Proc. IEEE — the source of ch. 53's design
principles; OSTEP calls it "highly influential". It is long: read §I.A
(concepts and the principles) carefully and skim the mechanism catalogue —
week 20's access-matrix material will make the skimmed half land properly.
Sheet 19 §C3 asks you to pass judgement on it, so read actively.

**Held back:** ch. 55–56 (access control, cryptography) are **week 20**,
where ACLs and capabilities arrive while ch. 39's permission bits and file
descriptors are fresh. Morris & Thompson's original salting paper is cited
by ch. 54 but not assigned — sheet 20 covers the ground.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise19.md`](../exercises/exercise19.md) — budget 3 h. **Entirely original material**: ch. 52–54 ship no homework, simulators or code. Self-mark against [`../exercises/solutions/exercise19-solutions.md`](../exercises/solutions/exercise19-solutions.md) |
| **Lab** | [`../labs/lab07-filesystem/`](../labs/lab07-filesystem/) **ends** · [`../labs/lab08-security/`](../labs/lab08-security/) **starts** — 6.0 h combined this week (5.0 h finishing lab 7, 1.0 h opening lab 8). Lab 8 is where the password-hashing and access-matrix machinery gets built for real |
| **Timed past paper** | `y2006p1q7` — page replacement, CLOCK without hardware support, and the buffer cache. Unlocked in week 10; timed here as spaced retrieval (the security chapters unlock almost no timed material of their own). 35 min closed book, then self-mark |
| **Untimed drill** | `y2011p2q4` unlocks this week: mostly address binding and fragmentation (week 8 material, 16 marks) with a Unix-authentication tail (4 marks) — hashing, salting, `/etc/passwd` vs shadow files. A good bridge into sheet 20 |

## Week load

```
OSTEP ch. 52-54     33pp ÷ 7  =  4.7 h
Saltzer & Schroeder [M]       =  1.5 h
Exercise sheet 19             =  3.0 h
Timed paper y2006p1q7         =  1.0 h
Lab 7 ends · Lab 8 starts     =  6.0 h
                                ------
                                16.2 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

If you are behind, the Saltzer & Schroeder skim can shrink — but do not skip
§I.A; ch. 53 is a précis of it and week 20 builds on both.

## Notes for the curious

- **Why security is here:** ch. 55's worked examples are Unix `rwx` bits and
  file descriptors — both taught two weeks ago in ch. 39. Teaching access
  control now means the access matrix arrives while its own examples are
  fresh. [Week 20's notes](week20.md#notes-for-the-curious) give the full
  reasoning, including why ch. 57 waits until week 25.
- **A grounding subtlety about `setuid`:** ch. 54 describes mechanisms for
  changing a process's identity and names `sudo`, but defers the
  "temporary change of process identity, while still remembering the original
  identity" to "a future chapter" — and, when deferring `sudo`, names the
  access-control chapter explicitly. That chapter is ch. 55, week 20, which is
  where the `setuid` mechanism proper lands; lab 8's setuid analysis task sits
  in the lab for exactly that reason.
- The Part V chapters are by **Peter Reiher** (UCLA), not the
  Arpaci-Dusseaus — the register changes noticeably. They also ship no
  homework of any kind, which is why sheets 19 and 20 are wholly original.
  (Sheets 25 and 26 are original too, but for a different reason — week 26 is
  a papers-only week rather than a chapter week with no homework behind it.)
- Ch. 54's aside on SMS-based two-factor authentication — theoretically weak,
  practically reasonable, later deprecated by NIST anyway — is a compact
  lesson in the gap between academic and operational security, and feeds
  directly into sheet 19 §C1.
