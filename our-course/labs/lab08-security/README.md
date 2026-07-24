# Lab 8 — Security: authentication, access control, privilege

**Weeks 19–21 · 7 hours · OSTEP ch. 53 (OS security), ch. 54 (authentication), ch. 55 (access control), ch. 56 (cryptography)**

Userspace, from scratch, **Python** (see below). No kernel work, no xv6, no
QEMU, and no root — except optionally for the Part 4 demonstration, which has a
no-root path. You will need `python3` and `bash`.

You are building three small tools and the analysis they support. The first is a
**password store** with salted hashing and a configurable slow hash, and a
**dictionary attacker** you actually run against it. The second is an **access
matrix** that mechanically emits both of its projections — access-control lists
and capability lists — from one shared structure, so they cannot disagree. The
third piece is **revocation**, done both ways, with the cost of each counted. A
fourth part is an audit of the real `setuid` binaries on your own machine.

The point is not the arithmetic; sheet 20 already does the salting arithmetic and
projects a matrix on paper. The point is what a program forces that a hand
calculation lets you skip: a store that really does hash and salt, an attacker
that really pays *N* times as much against *N* salts, and two projections that
are the *same state* viewed two ways and therefore *cannot* drift. Owning the
mechanism is the lesson.

> **Attribution.** The **access matrix** — subjects as rows, objects as columns,
> ACLs as its columns and capability lists as its rows — is Lampson's unifying
> presentation, supplied here because the Tripos examines it. OSTEP ch. 55
> presents ACLs and capabilities directly, without the matrix.

## Layout

```
lab08-security/
  README.md          this handout
  starter/           pwstore, crack, accessmatrix, genstore   <- work here
  fixtures/          wordlist.txt and its documented store distribution
  tests/run.sh       the autograder for Parts 1-3 (delegates to grade.py)
  solutions/         SPOILERS. Reference tools, model notes, answer key. Later.
```

Copy the working directories somewhere of your own and work there. Copy all of
`starter/`, `tests/` and `fixtures/`, not just `starter/`:

```sh
cp -r starter tests fixtures ~/lab8
cd ~/lab8/starter
../tests/run.sh .          # runs the autograder against this directory
```

`solutions/` is deliberately left behind.

## What you hand in

| File(s) | Part | Weight | Marked |
|---|---|---|---|
| `pwstore` — register/authenticate, per-user salt, configurable cost | 1 | } | auto |
| `crack` — the dictionary attack and its work counter | 1 | 44% | auto |
| `CRACK-RESULTS.md` — recovery rate and wall-clock for all three configs | 1 | (in 44%) | rubric |
| `THREAT-MODEL.md` — what the store's defences do and do not cover | 1 | (in 44%) | rubric |
| `accessmatrix` — the matrix, both projections, storage figures | 2 | 21% | auto |
| `accessmatrix` — revocation both ways, with counted cost | 3 | 14% | auto |
| `SETUID-AUDIT.md` — inventory, two analysed binaries, privilege-drop demo | 4 | 21% | rubric |

The `.md` deliverables are marked by hand against the rubric in
`solutions/README.md`, which you read *after* you have your own numbers, not
before. A green autograder run is not a finished lab — the timing, the threat
model, and the setuid judgement are more than a third of the marks and none of
them is machine-checked. See "How it's checked", below.

---

# ⚠ Language and the text interface

**The reference is Python, and so should yours be.** Part 1 needs a real
key-derivation function for its slow-hash configuration, and Python's
standard-library `hashlib` supplies one (PBKDF2) with nothing to install or
link. Deliver three files named `pwstore`, `crack`, `accessmatrix`, each with a
`#!/usr/bin/env python3` shebang and made executable (`chmod +x`); the autograder
runs them directly. (It never imports them — it drives everything through argv
and stdin — so another language that produced the same output lines would grade
too, but the KDF is why this lab is written in Python.)

**Every command line and every output line is a fixed contract.** The starter's
`print` statements are the specification; the autograder greps for those exact
strings. Change what a line *says* and the autograder cannot read it. The starter
carries the contract in comments; the sections below give it in full.

---

# Part 1 — Password storage and a dictionary attack (~3.0 h, weeks 19–21, 44%)

## `pwstore` — the store

```
pwstore register <storefile> <user> <password> [scheme] [cost]
pwstore auth     <storefile> <user> <password>
pwstore dump     <storefile>
```

A store keeps, per user, only what is needed to re-derive a verifier at login: a
scheme, a per-user salt, a cost, and the digest. Three schemes, so the attacker
can be run against each:

| scheme | digest |
|---|---|
| `unsalted` | `sha256(password)` |
| `salted` (default) | `sha256(salt ‖ password)`, with a fresh random salt per user |
| `slow` | `PBKDF2-HMAC-SHA256(password, salt, cost)` |

**PBKDF2** (RFC 2898) is the deliberately slow key-derivation function this lab
uses for the slow configuration; `cost` is its iteration count. It is
`hashlib.pbkdf2_hmac` — do not hand-roll a slow hash. Draw salt bytes from
`os.urandom`.

`register` appends a record and prints `registered user=<u> scheme=<s>`. `auth`
re-derives from the *stored* salt/scheme/cost and prints
`auth user=<u> result=OK` or `result=FAIL`. `dump` prints one
`record user=<u> scheme=<s> salt=<hex-or-dash> hash=<hex>` per user.

The store file is one record per line: `<user> <scheme> <cost> <salt> <hash>`,
salt as hex or `-` for none.

Two properties the autograder pins, both of which follow from the definitions
above and neither of which you should have to read off the grader:

- registering then authenticating with the right password gives `OK`, and with
  any other password gives `FAIL`;
- **two users with the same password, registered `salted`, have different stored
  hashes** — that difference is the salt, and nothing else. (Registered
  `unsalted`, the same password gives the *same* hash: the control that proves
  the difference is the salt.)

Constant-time comparison of the candidate against the stored hash
(`hmac.compare_digest`) is the right habit and is mentioned in the starter, but
it is **out of scope for grading**.

## `crack` — the dictionary attack

```
crack <storefile> <wordlist>
```

Given a store you generated and a wordlist, recover every password that is in the
list, and **report the work as a count, not just a time**. Print one
`cracked user=<u> password=<w>` per recovered user, then a summary:

```
crack recovered=<k> users=<n> words=<W> hash_ops=<h> seconds=<t> rate=<r>
```

`hash_ops` is the number of KDF evaluations you perform, and it is the
deterministic heart of the salting lesson — it does not depend on your hardware:

- **Against an unsalted store**, hash each of the `W` words once into a
  digest→word table, then look every user up for free. `hash_ops = W`, whatever
  the number of users `N`.
- **Against a salted (or slow) store**, every user has a different salt, so the
  table cannot be shared: you re-hash all `W` words for each of the `N` users.
  `hash_ops = W × N`.

That is the whole content of "the salt does not make one password harder to
crack; it makes *N* passwords cost *N* times as much." Scan the whole wordlist
for each salted user — do not stop at the first hit — so `hash_ops` is a clean
`W` or `W × N` regardless of which passwords happen to be present. The autograder
asserts exactly these two counts, on a store it builds itself — and it asserts
the full-scan `W × N` again on its recovery store, where some passwords *are*
present and stopping at the first hit would show up as a smaller count.

`seconds` and `rate` are wall-clock and therefore **rubric-marked, not
auto-graded**. That is what `CRACK-RESULTS.md` is for.

## `genstore` — scaffolding (given)

`genstore` is complete and given; you do not modify it. It registers a population
of users whose passwords follow the distribution documented in
`fixtures/README.md` (weak / moderate / strong), so the recovery rate you report
is interpretable. Run it three ways — `unsalted`, `salted`, `slow` — and time
`crack` against each.

Size the timing run down first. At `genstore`'s defaults a `slow` crack is
`W·N` PBKDF2 evaluations of `cost` iterations each — on a small machine that is
of order ten minutes for one pass, long enough to look like a hung process.
Build the timing stores with a small `n=` and a reduced `cost=` (a store an
order of magnitude smaller still shows the same `hash_ops`-vs-wall-clock gap),
and state those parameters in `CRACK-RESULTS.md`. `hash_ops` is the same whatever
the machine; only the seconds move.

## `CRACK-RESULTS.md`

Report, for all three configurations, the recovery rate and the wall-clock time,
with the parameters (`N`, `W`, `cost`) stated so the run is reproducible. Then a
short interpretation: the unsalted and salted runs recover the *same passwords*
but at very different `hash_ops`, and the slow run becomes impractical within the
time budget. The distribution in `fixtures/README.md` tells you the recovery rate
to expect; your job is to measure it and explain the gap between `hash_ops` and
wall-clock.

## `THREAT-MODEL.md`

A short note on the store you built: what salting defends against and what it does
not; what a slow hash adds; and what would change if the store were **encrypted
at rest** — including the honest answer to "where does the key live?". This is
ch. 56's at-rest question asked of your own store.

---

# Part 2 — The access matrix and its projections (~1.5 h, weeks 20–21, 21%)

`accessmatrix` reads a command script from a file argument, or `-` for stdin:

```
init <S> <O> <rights>     e.g.  init 6 6 rwx
grant <s> <o> <r>
check <s> <o> <r>         -> check s=<s> o=<o> r=<r> = yes|no
acl <o>                   -> acl o=<o> : <s>:<rights> <s>:<rights> ...
caplist <s>               -> caplist s=<s> : <o>:<rights> <o>:<rights> ...
storage                   -> storage subjects=.. objects=.. rights=.. granted=.. acl_entries=.. cap_entries=.. matrix_cells=..
```

Subjects are rows, objects are columns, each cell a set of rights. Keep **one**
underlying structure. The two classical representations are **projections** of
it, computed on demand:

- `acl(o)` is the **column**: for a fixed object, which subjects hold which
  rights. This is an access-control list.
- `caplist(s)` is the **row**: for a fixed subject, which objects it may touch and
  how. This is a capability list.

and `check(s,o,r)` reads the cell directly. Because both projections are derived
from the same state — never stored alongside it — they **cannot disagree** with
each other or with `check`. That invariant is the whole part, and the autograder
tests it as a property: over randomly generated matrices, for every
`(subject, object, right)` triple, `check`, membership in `acl(o)`, and
membership in `caplist(s)` must all give the same answer. A projection kept as a
second structure and updated by hand will pass casual tests and fail this one.

For `storage`, report the cost of each representation. A list stores one entry per
granted **distinct** `(s,o,r)` triple — a cell is a *set* of rights, so granting
the same triple twice counts once — so `acl_entries` and `cap_entries` both equal
`granted`; the full matrix stores `S × O × R` cells whatever is granted. The gap —
lists at `O(granted)`, matrix at `O(S·O·R)` — is the storage argument for a
sparse matrix, and it closes as the matrix fills. The autograder runs `storage`
on a small matrix it builds and asserts every figure against these formulas, so
the starter's storage stub is graded work, not decoration.

---

# Part 3 — Revocation, both ways (~1.0 h, week 21, 14%)

Extend `accessmatrix`:

```
revoke_subject <s> <r>    remove right r from subject s for EVERY object
revoke_object  <o> <r>    remove right r from object o for EVERY subject
indirect on|off           enable an indirection layer for object revocation
```

Each revoke prints the outcome-bearing line

```
revoke_subject s=<s> r=<r> acl_ops=<a> cap_ops=<c>
revoke_object  o=<o> r=<r> acl_ops=<a> cap_ops=<c>
```

and afterwards `check` must return `no` for every affected cell — and still
`yes` for everything untouched. Revocation removes exactly the named right from
the named row or column, nothing more: a subject's *other* rights, and every
*other* subject's rights, survive it, and the autograder checks the survivors as
well as the casualties. The counts report the **work each representation would
do**, and the asymmetry is the point:

- To revoke a **subject's** right everywhere, an ACL implementation must visit
  every object's list (the subject is scattered across all of them):
  `acl_ops = O`. A capability implementation edits that subject's one row:
  `cap_ops = 1`.
- To revoke an **object's** right for everyone, an ACL implementation edits that
  object's one list: `acl_ops = 1`. A capability implementation must visit every
  subject's row (capabilities for the object are scattered across all of them):
  `cap_ops = S`.

Cheap in one direction for ACLs, cheap in the other for capabilities — which is
the single best argument for why real systems use both. The autograder asserts
both the outcome *and* the two counts; an implementation that revokes correctly
but reports symmetric costs fails.

Then the indirection layer. Capability object-revocation is the expensive,
historically intractable direction — you cannot easily find every capability for
an object. With `indirect on`, model the standard fix: route access through one
per-object entry that a single operation can invalidate, so `revoke_object` costs
`cap_ops = 1` instead of `S` (and `check` still returns `no`). Say what it costs —
`THREAT-MODEL.md` is Part 1's; here the answer belongs in a comment or in
`SETUID-AUDIT.md`'s margins, and the rubric names what a full answer says.

---

# Part 4 — `setuid` in the wild (~1.5 h, week 21, 21%, rubric-marked)

Produce `SETUID-AUDIT.md`:

1. **Inventory.** Find every setuid binary on your machine — a read-only scan,
   e.g. `find / -perm -4000 -type f 2>/dev/null` — and list them.
2. **Two analysed.** For **two** of them, say what privilege each needs and why
   the design grants it. (`ping` and `sudo` and `passwd` are classic; pick any
   two you can reason about.)
3. **A privilege-drop demonstration.** Write a small setuid-style program that
   acquires a privilege, uses it, and **drops it permanently**, then show the
   dropped privilege cannot be regained. If you lack root, *model* it: state the
   exact call sequence and show, by the return values, that the regain attempt
   would fail. The ordering — supplementary groups, then gid, then uid — is the
   substance; `setuid()` alone does not drop a saved set-user-ID.

This part is **rubric-marked, not auto-graded**: an audit of your own machine has
no fixed answer, and that is the point. The rubric in `solutions/README.md` names
what a good answer contains — the privilege identified, the least-privilege
judgement, and a correct account of the drop sequence — rather than an answer key.

> **Safety and scope.** The dictionary attack runs only against a store *you*
> generated with the supplied wordlist. The setuid audit is a read-only inventory
> of your own machine. Nothing here targets a system you do not own.

---

# Running the tests

```sh
../tests/run.sh .          # from your work directory
```

`run.sh` locates your `pwstore`, `crack` and `accessmatrix` (executable scripts,
or `<name>.py` files) and drives each through its command line. It prints a
PASS/FAIL table and an `N passed, M failed` summary and exits non-zero if
anything failed. Part 2 is a **property test**: it generates several random
matrices from a fixed seed (so it never flakes), grants a random ~50% of the
cells, and checks that every projection agrees with an independent oracle for
every triple — reported as one line with the triple count and seed.

To drive a tool yourself while debugging, just run it:

```sh
./pwstore register /tmp/s alice hunter2 salted && ./pwstore auth /tmp/s alice hunter2
printf 'init 3 3 rwx\ngrant 0 1 r\nacl 1\ncaplist 0\ncheck 0 1 r\n' | ./accessmatrix -
```

## How it's checked, and what is not

Parts 1–3 are machine-checked: the store's salt behaviour and the cracker's
recovery count and `hash_ops` (including the full-scan count on the recovery
store), the matrix property and the `storage` figures, and the revocation
outcomes — casualties *and* survivors — and counts. A green run means those
tools do what the handout says.

Four things are **rubric-marked** and no test touches them, because a checklist
would reward the wrong behaviour:

- **`crack`'s timing** — `seconds`/`rate` are hardware-dependent; `hash_ops` is
  the deterministic quantity, and it *is* graded.
- **`CRACK-RESULTS.md`** — the three-configuration table and its interpretation.
- **`THREAT-MODEL.md`** — the at-rest / key-management argument.
- **`SETUID-AUDIT.md`** — the whole of Part 4.

A full `N passed, 0 failed` is reachable with those four absent, and that is not
a finished lab.

---

# Stretch goals

Unweighted.

- **A cryptographic capability**: a signed, unforgeable token carrying its rights,
  needing no server-side table — then say what you gave up (revocation).
- **A second authentication factor** on `pwstore`, and which Part 1 attacks it
  defeats and which it leaves untouched.
- **Encrypt the store at rest** with a library cipher and check the result
  against the threat-model note you already wrote — ch. 56's at-rest material.
- **A rule-mangling cracker** (append digits, capitalise) that reaches the
  *moderate* passwords `genstore` plants, and the recovery rate it buys.

---

# If you get stuck

Each entry is a symptom and where to look — not a fix.

- **`auth` says `OK` for the right password but also for wrong ones.** Look at
  what `auth` compares, and at what `kdf` returns when you have not filled it in.
- **The two salted hashes come out equal in the salt test.** Look at where the
  salt is generated in `register`, and at whether `kdf` actually reads it.
- **`crack` recovers nothing.** Look at whether `crack`'s `kdf` derives digests
  the *same way* `pwstore` did — a store hashed one way and probed another never
  matches. Look also at the salt each record carries.
- **`crack` recovers the right passwords but `hash_ops` is wrong.** Look at
  whether the unsalted path shares one table and the salted path does not, and at
  where you increment the counter.
- **Part 2's property test fails though `check` looks right.** Look at whether
  `acl` and `caplist` are *derived* from the same grants `check` reads, or kept
  separately. Look at whether one of them drops or mislabels a right.
- **Part 3's outcome passes but the asymmetry case fails.** Look at the two
  `_ops` numbers you print, and at which representation is scattered for each
  kind of revocation.
- **The indirection case fails.** Look at whether `revoke_object` under
  `indirect on` both records the cheap cost *and* makes `check` honour the
  invalidation, and at whether `has` consults it.
