# Part 1 fixtures — the wordlist and the store distribution

## `wordlist.txt`

Forty of the most common leaked passwords, one per line. It is the dictionary
your `crack` reads and the pool `genstore` draws weak passwords from. It is a
plain fixed file so recovery rates are comparable between students.

The autograder does **not** use this file — it builds its own tiny store and
wordlist so its numbers are pinned and derivable (see `tests/grade.py`). This
list is for *your* runs: `genstore` + `crack` + `CRACK-RESULTS.md`.

## The `genstore` population

`genstore <storefile> <wordlist> <scheme> [n=N] [seed=S] [cost=C]` registers `N`
users (default 100) whose passwords follow a **documented distribution**, so the
recovery rate `crack` reports means something:

| fraction | class | password | a plain dictionary attack |
|---:|---|---|---|
| 60% | weak | a word drawn straight from the wordlist | **recovers it** |
| 25% | moderate | a wordlist word with one or two digits appended (`dragon7`) | misses it (a rule-mangling attack would catch some) |
| 15% | strong | 16 random alphanumerics | never recovers it within any sane budget |

So a plain dictionary attack should recover **about 60%** of the users — the weak
fraction — and no more. The exact count wobbles with the seed; the *fraction*
does not, and 60% is the yardstick `CRACK-RESULTS.md` compares the measured rate
against. The unsalted, salted and slow runs all recover the **same** ~60% (same
passwords, same wordlist); what changes between them is `hash_ops` and the
wall-clock, not the recovery rate. That separation — recovery unchanged, cost
transformed — is the Part 1 lesson.

Reproducibility: `genstore` is seeded (`seed=`, default 1), so the same seed
gives the same population. State the seed, `N`, `W` (= 40 here) and `cost` in
`CRACK-RESULTS.md`.
