# THREAT-MODEL.md — model note (Part 1)

> Model answer, to show the rubric. The store in question is the `pwstore` file
> built in Part 1.

## The asset and the assumed breach

The asset is the users' passwords — valuable because people reuse them elsewhere,
not because the login to *this* system matters most. The threat this note is
about is the realistic one: **the store file is stolen** (backup leak, SQL dump,
lost disk). Everything below assumes the attacker already has the file and is
working *offline*, with no rate limit and their own hardware.

## What salting defends against, and what it does not

- **Defends:** precomputation and amortisation. A per-user random salt means no
  precomputed table (rainbow or plain digest→word) applies, and cracking one
  user's salt gives no help against another's. It converts a single `W`-guess
  sweep over the whole store into a `W`-guess sweep *per user* — the `N×` cost
  Part 1 measures.
- **Does not defend:** the strength of any individual password. Salt is stored in
  clear beside the hash (it must be — authentication re-derives with it), so it is
  not a secret and hides nothing. A weak password (`123456`) falls to the first
  dictionary word whether salted or not; salting only stops the attacker getting
  *all* the weak ones for the price of one.

## What a slow hash adds

A slow KDF (here PBKDF2, cost 200000) multiplies the *per-guess* cost by a large
constant, paid identically by attacker and by honest login. It does not change
which passwords are crackable in principle, only how many guesses per second are
possible — turning "millions/second" into "a few/second" and pushing a bulk
offline attack past a practical budget. Its limits: it is a linear tax the
attacker can pay with more hardware (GPUs, ASICs — which is why memory-hard KDFs
like scrypt/Argon2 exist), and it does nothing for a password already in the
first hundred dictionary words.

## Encrypting the store at rest — and where the key lives

Encrypting the whole store with a symmetric cipher defends against exactly one
thing the hashes do not: a **theft of the file alone**. If the attacker gets the
ciphertext but not the key, they get nothing — not even the chance to crack weak
passwords, because they cannot read the hashes to attack.

But this only moves the problem to the key, and the honest question is *where the
key lives*:

- **In the same config/filesystem** the attacker just stole — then encryption is
  theatre; whoever takes the store takes the key with it.
- **In the application's memory / environment** — better against a cold disk
  theft, useless against an attacker who reads the running process (the same
  breach that dumps the DB often dumps memory too).
- **In a separate HSM or KMS the app calls out to** — now the key genuinely is
  not in the stolen file, and encryption-at-rest earns its keep; the cost is an
  external trust dependency and that the *running* app can still decrypt, so a
  live-server compromise still wins.

The load-bearing conclusion: encryption-at-rest raises the bar for a *file-only*
theft and does nothing for a *running-system* compromise, and its entire value
rides on the key not being co-located with the data it protects. Hashing+salting
protects the passwords even when the attacker has *everything on disk*; that is
the property encryption cannot give and why the store is hashed regardless of
whether it is also encrypted.
