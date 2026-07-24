# CRACK-RESULTS.md — model report (Part 1)

> This is a *model* report on the author's machine, to show what the rubric
> expects. Your `hash_ops` will match; your `seconds` will not — they are
> hardware-dependent, which is the whole reason `hash_ops` is the graded
> quantity and the timing is not.

## Setup

- Store: `genstore store <wordlist> <scheme> n=100 seed=1` (and `n=10` for the
  slow scheme, see below), against `fixtures/wordlist.txt` (`W = 40` words).
- Population (from `fixtures/README.md`, seed 1): weak=63, moderate=24,
  strong=13. A plain dictionary attack should recover the 63 weak users — and in
  fact recovers **64**, because one moderate password (`"password"+"1"`) collides
  with the listed word `password1`. Recovery rate ≈ 64%, close to the 60% design
  point.
- Machine: PBKDF2-HMAC-SHA256 at cost 200000 runs at ≈ **5 hashes/second** here.

## Results

| Config | N | recovered | rate | `hash_ops` | wall-clock |
|---|--:|--:|--:|--:|--:|
| unsalted | 100 | 64 | 64% | **40** (= W) | ~0.0001 s |
| salted | 100 | 64 | 64% | **4000** (= W·N) | ~0.006 s |
| slow (cost 2×10⁵) | 10 | 7 | 70% | **400** (= W·N) | **78 s** |

The slow row uses `n=10`, not 100: at 5 hashes/second, the `n=100` salted-slow
attack is `4000 / 5 = 800 s ≈ 13 minutes`, past a comfortable budget — which is
the point, so the small run plus the arithmetic makes it without waiting.

## What the numbers say

**Salting does not change the recovery rate.** Unsalted and salted recover the
*same 64 users* — the salt is not a secret and hides nothing from a
dictionary. What it changes is `hash_ops`: `40` becomes `4000`, exactly the
factor `N = 100`. Against the unsalted store the attacker hashes each of the 40
words once and looks up all 100 users in the resulting table; against the salted
store every user's distinct salt forces its own 40-word pass. **Salting does not
make one password harder to crack; it makes N passwords cost N times as much** —
it kills the precomputed table (and the shared rainbow table) that make unsalted
stores fall in one pass.

**The slow hash attacks the constant, not the count.** `hash_ops` for slow is the
same `W·N` as salted — salt structure is identical. What changes is the *seconds
per hash*: cost 200000 turns a ~2-million-per-second SHA-256 into ~5 per second, a
~400000× tax paid equally by the attacker and by every honest login. That is the
trade: the login is now measurably (but tolerably) slower; the offline attack
becomes infeasible for anything but the weakest passwords within a real budget.

**The honest limit.** None of this helps the *weak* passwords themselves —
`123456` falls to the first word whether salted, slow, or both, because 40 guesses
is nothing. Salting and slow hashing buy time against *offline bulk* cracking;
they do not substitute for password strength, and the strong 15% are uncracked in
every configuration because they are not in the dictionary at all.
