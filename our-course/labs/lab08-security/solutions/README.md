# Lab 8 — Reference tools and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference pwstore, crack and accessmatrix, the dropdemo.c    ║
║  privilege-drop program, and the model CRACK-RESULTS / THREAT-    ║
║  MODEL / SETUID-AUDIT notes -- which are a third of the lab and   ║
║  self-marked. Reading the model notes before you have your own    ║
║  numbers turns the open-ended parts into a fill-in form.          ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
../tests/run.sh .          # on this directory: 12 passed, 0 failed
```

The reference is three small Python programs — `pwstore` (~150 lines), `crack`
(~110), `accessmatrix` (~180), comments included — plus `genstore` (given to the
student, unchanged here) and `dropdemo.c` for Part 4. **The starter skeleton
scores `2 passed, 10 failed` of 12.** The two it passes are passed *for the wrong
reason*: with `kdf` stubbed to a constant, `register`-then-`auth` of the *right*
password trivially matches (constant = constant), and two identical unsalted
passwords trivially agree — both pass a do-nothing stub and are earned properly
only by a real hash. Every other case is falsifiable by a stub and fails.

## Why the harness grades any language the same

`tests/run.sh` locates `pwstore`/`crack`/`accessmatrix` in the work directory
(an executable script, a `<name>.py`, or any executable) and drives each through
argv and stdin, capturing stdout — it never imports the submission. So the
language is invisible: anything producing the same output lines grades the same.
The reference is Python for one concrete reason — Part 1's slow hash needs a real
KDF, and `hashlib.pbkdf2_hmac` is in the standard library with nothing to link,
whereas a C build would need OpenSSL and the fixed compile line could not link
it. The grading logic lives in `tests/grade.py` (Part 2 is a property test that
generates random matrices and compares each projection against an oracle — far
cleaner in Python than in bash); `run.sh` is a thin wrapper.

---

## What the autograder checks, case by case, and the mutation that proves it

Every case below was checked by *building the wrong implementation* and
confirming that this named case — and the suite's exit code — flips. A
requirement no mutation can break is not tested; each row here has one that does.
All were run against the harness; all fired.

### Part 1 — pwstore and crack

| Case | Catches (mutation confirmed to fail it) |
|---|---|
| register then authenticate succeeds | (baseline; a broken re-derivation that never matches fails it) |
| wrong passwords are rejected (even a case near-miss) | `auth` accepts anything (`ok = True`) → wrong password returns OK; **or** a normalising `kdf` (lowercases before hashing) → the case-only-different `CorrectHorse` wrongly returns OK |
| **same password, salted, gives different hashes** | store does not salt (salted `kdf` ignores the salt) → two equal hashes |
| same password, unsalted, gives the same hash (control) | proves case above is the *salt*, not nondeterminism; an unsalted path that injected entropy would fail here |
| the dictionary attack recovers the weak passwords, full-scan | `crack` under-counts (skips the salted users → recovers 0) **or** over-counts (records a hit on every word) → set ≠ the three planted; **or** early-exits at the first hit → `hash_ops = 18`, not the full-scan `W·N = 30` |
| **salting turns W hashes into W·N (the cost asymmetry)** | unsalted path re-hashes per user (no shared table) → `hash_ops = W·N` not `W`; **or** salted path dedups via a shared table → `hash_ops = W` not `W·N` |

The **salt** case is the headline of Part 1's correctness: two users register the
same password under `salted`, and their stored hashes must differ — that
difference is the per-user salt and nothing else. The **cost-asymmetry** case is
the mechanism: the harness registers `N = 4` users whose passwords are *absent
from the wordlist*, so every cracker must scan all `W = 6` words for every user
(nothing short-circuits early), and then a correct attacker reports `hash_ops = W`
against the unsalted store (one shared table) and `hash_ops = W·N` against the
salted store (a pass per salt). Both numbers follow from `W` and `N` stated in the
handout, not from the grader. The **recovery** case complements it from the other
side: there some passwords *are* present (three of five salted users are in the
`W = 6` wordlist), so the handout's "scan the whole wordlist — do not stop at the
first hit" is what `hash_ops = W·5 = 30` asserts. An early-exit cracker recovers
the identical set but reports `18` (u0/u1/u2 short-circuit at list positions
0/1/2) and fails — without this assertion the full-scan contract was stated but
ungradeable, since the cost-asymmetry store has nothing to short-circuit on.
Timing (`seconds`/`rate`) is deliberately **not** asserted — it is
hardware-dependent and rubric-marked.

### Part 2 — the access matrix (the property test, the harness core)

| Case | Catches |
|---|---|
| **check, ACL and capability projections all agree** (8 matrices × 108 triples) | ACL drops a right (skips `x`); ACL transposes subject/object (returns the row); capability drops a right (skips `r`); **all-three-say-yes** (the vacuous-agreement guard); a single-cell projection bug (drop `x` at one `(s,o)` only) |
| storage figures follow the granted-triple formula | the starter's `granted = 0` stub → `granted=0 acl_entries=0 cap_entries=0`; a counter that double-counts a duplicate grant |

The property case is the plan's designated core, and it is worth reading how it
avoids being vacuous. For each of 8 random matrices (fixed base seed 20250721, so
it *never flakes*), the harness grants a random ~50% of the 108 `(s,o,r)` cells,
keeps its **own oracle** of what it granted, then for every triple asserts that
`check`, membership in `acl(o)`, and membership in `caplist(s)` *all equal the
oracle*. Comparing to the oracle — not merely to each other — is what kills the
**all-true mutant**: a tool where `check`/`acl`/`caplist` all say "yes" agrees
with itself perfectly but disagrees with the oracle on every ungranted cell, and
fails. The run reports its coverage (439 granted / 425 ungranted of 864 triples
at this seed), and refuses to pass a generator that produced an all-empty or
all-full matrix (which would make the property vacuous). Why 8 matrices and not
fewer: 8 is the first count at which **every one of the 108 cells is granted in
at least one matrix and ungranted in at least one**, so even a projection bug
confined to a single cell must disagree with the oracle somewhere — at 5
matrices, cell (4,2,x) happened to be granted in none, and a drop-`x`-there bug
slipped through. The seed stays fixed, so this is more coverage, not more flake.
Mutation results, all confirmed to fail the property case:

- **drop a right** in `acl` → granted `x` triples read "no" in the ACL, "yes" in
  check → disagreement.
- **transpose** subject/object in `acl` (return `caplist(o)`) → wrong column →
  disagreement on the first asymmetric matrix.
- **drop a right** in `caplist` → the mirror of the first.
- **all-true** → agrees with itself, contradicts the oracle → caught only because
  the oracle is in the loop.
- **drop `x` at cell (4,2) only** in `acl` → invisible over 5 matrices, caught at
  8 (the per-cell-coverage argument above).

The **storage** case drives `init 3 4 rw`, four distinct grants plus one
duplicate, then `storage`, and asserts the whole line: `granted=4` (a cell is a
set — the duplicate must not double-count), `acl_entries=cap_entries=4` (one list
entry per triple), `matrix_cells=3·4·2=24`. Every figure is the handout's formula
applied to the visible script, so it is derivable, not copyable; the starter's
`granted = 0` stub fails it.

### Part 3 — revocation both ways

| Case | Catches |
|---|---|
| revoking a subject's right removes it everywhere, and only it | `revoke_subject` is a no-op (does not touch grants) → checks still return "yes"; **or** it over-revokes — wipes the whole cell (`grants[(s,o)] = set()`, killing the subject's other rights) or discards the right from *every* subject — → a survivor check reads "no" |
| revoking an object's right removes it for everyone, and only them | `revoke_object` is a no-op → checks still return "yes"; **or** it over-revokes (whole cell, or every object) → a survivor check reads "no" |
| **revocation costs are asymmetric** (ACL O(objects), cap O(subjects)) | symmetric costs — `revoke_subject` reports `acl_ops = 1` (not `O`), or `revoke_object` reports `cap_ops = 1` (not `S`) |
| indirection makes capability revocation O(1) | indirection ignored (`revoke_object` still costs `cap_ops = S`), or faked (cheap count but `check` still "yes") |

The two **outcome** cases check casualties *and* survivors. Each script grants
the revoked subject (or object) a *second* right and gives a *bystander* subject
(or object) the revoked right, then asserts after the revoke that every affected
cell reads "no" **and** both survivors still read "yes". One-sided checks would
let "revoke too much" pass: assigning an empty set to the cell instead of
discarding one right, or discarding the right from every subject instead of one,
both revoke everything the test names — and both now fail on the survivors.

The **asymmetry** case is the Part 3 headline. On `init 4 5` (4 subjects, 5
objects): `revoke_subject` must report `acl_ops = 5, cap_ops = 1` (an ACL scans
all 5 objects for the scattered subject; a capability edits one row), and
`revoke_object` must report `acl_ops = 1, cap_ops = 4` (an ACL edits one list; a
capability scans all 4 subjects). Both numbers are the `init` dimensions, so they
are derivable, not copyable. A model that revokes *correctly* but reports
*symmetric* cost passes the two outcome cases and fails this one — which is why
outcome and cost are separate checks. The **indirection** case then shows the
standard fix: with `indirect on`, `revoke_object`'s `cap_ops` drops from 4 to 1
*and* `check` still returns "no"; a tool that only fakes the cheap number is caught
because the checks would then read "yes", and one that ignores indirection is
caught because `cap_ops` stays 4.

---

## The reference designs in a paragraph each

**pwstore.** One `kdf(scheme, cost, salt, password)` is the whole crypto surface:
`sha256(pw)`, `sha256(salt‖pw)`, or `pbkdf2_hmac(pw, salt, cost)`. `register`
draws a fresh `os.urandom(16)` salt for the salted/slow schemes (none for
unsalted) and appends `user scheme cost salt hash`. `auth` re-derives from the
*stored* fields and compares with `hmac.compare_digest`. The salt is stored in
clear — it is not a secret — which students sometimes discard and then cannot
authenticate.

**crack.** Splits records into unsalted and salted. Unsalted users share **one**
digest→word table (W evaluations); salted/slow users each get their own full
W-word pass (no table can be shared across distinct salts). `hash_ops` counts KDF
evaluations, so it is `W` for an unsalted store and `W·N` for a salted one,
deterministically — the salting lesson as a number, independent of the clock. It
does not early-exit, so the count is stable regardless of which passwords are
present — and the grader asserts exactly that on its recovery store, where an
early exit would report fewer ops.

**accessmatrix.** One dict `grants[(s,o)] = {rights}` is the only state. `check`
reads it; `acl(o)` and `caplist(s)` are **derived** by scanning it (columns and
rows), so they cannot drift — the property test exists to punish any design that
keeps them as a second structure. Revocation edits `grants` and reports the work
each representation *would* do (`O` vs `1`, or `1` vs `S`); the indirection layer
adds a per-object invalidation set that `has` consults, making capability
object-revocation `O(1)` at the cost of a per-access check and coarse (all-subject)
granularity.

**dropdemo.c.** `setgroups(0,NULL)` → `setgid` → `setuid`, in that order, then a
`seteuid(0)` that must fail. Compiles `-Wall -Wextra -Werror` clean; runs the real
drop under setuid-root and a narrated model path otherwise. It is not
auto-graded — Part 4 is rubric-marked — but it is the reference for the drop
sequence the rubric asks about.

---

## Rubrics for the hand-marked deliverables

Nothing in the autograder marks these; a full `12 passed, 0 failed` is reachable
with all three `.md` files absent, and that is not a pass. Model versions are in
this directory.

### `CRACK-RESULTS.md` — Part 1 timing (part of the 44%)

| | Marks | What it takes |
|---|---:|---|
| Completeness | 4 | All three configs (unsalted, salted, slow) tabled with N, W, cost, recovered, `hash_ops`, and wall-clock. Parameters stated so the run reproduces. |
| Mechanism | 4 | The `hash_ops` gap explained by *why* the table can/cannot be shared, and the slow-hash gap explained as cost-per-hash, not restated. |
| The trade | 2 | Names that salting changes cost not recovery, and that slow hashing taxes attacker and login alike. |
| Honesty | 2 | Notices that neither defence helps the weak passwords, and that the timing is machine-specific while `hash_ops` is not. |

### `THREAT-MODEL.md` — Part 1 at-rest note (part of the 44%)

| | Marks | What it takes |
|---|---:|---|
| Salt: does / does not | 3 | Salt defends against precomputation and amortisation across users; does *not* protect an individual weak password and is not a secret. |
| Slow hash | 2 | A per-guess constant paid by both sides; a linear tax hardware can pay down. |
| At rest + where the key lives | 5 | Encryption helps a *file-only* theft and not a running-system compromise, and its value rides entirely on the key **not** being co-located with the data. Full marks needs the key-location argument, not just "encrypt it". |

### `SETUID-AUDIT.md` — Part 4 (21%, entirely rubric)

| | Marks | What it takes |
|---|---:|---|
| Inventory | 4 | A real read-only scan of the student's own machine, listed. |
| Two analysed | 8 | For each of two binaries: the specific privilege it needs, and *why the design grants it* — the file/kernel state the ordinary user cannot reach. |
| Least-privilege judgement | 4 | The confused-deputy observation — the binary is trusted with more than the caller's action needs (all of `shadow` to change one row; arbitrary mounts to permit blessed ones), and its safety is in its own checks. |
| Privilege drop | 5 | Correct account of the drop **sequence** (groups, gid, uid) and *why* `setuid()` alone is not a drop (the saved-set-uid), demonstrated or correctly modelled. |

Do not auto-grade the security *judgement*. The reasoning is the assessable
thing; a checklist would reward naming the calls without understanding the trust.
