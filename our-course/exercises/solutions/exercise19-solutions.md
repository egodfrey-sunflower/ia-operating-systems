> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 19 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their
> working; for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. FALSE.** The three goals are independent — that is why all three are
named. A system that encrypts everything and refuses all writes has
confidentiality and integrity while an attacker who blocks all access
destroys availability; conversely nothing about being available implies data
is protected. (Ch. 52's peach: stolen, swapped for a turnip, or slapped out
of your hand are three different attacks.)

**A2. FALSE.** Open design says *assume the adversary knows every detail* —
build so that security survives full disclosure. Ch. 53 is explicit that this
"does not necessarily mean that you actually tell everyone all the details".
Publishing may be wise for review, but the principle is about what you rely
on, not what you release.

**A3. FALSE.** Identity is **inherited from the parent process** — normally
the invoking user's shell — regardless of who wrote or owns the program. (The
case where ownership of the executable does affect identity is the setuid
mechanism, treated with access control in week 20.)

**A4. FALSE.** The attacker gains an **offline guessing** opportunity: hash
candidate passwords and compare, at hardware speed, with no rate limiting.
Since human passwords have low entropy, dictionary attacks succeed routinely.
Hashing means the file doesn't hand over the passwords directly; it does not
make the theft harmless. (Sheet 20 §B4 quantifies this.)

**A5. FALSE.** Ch. 53's point is the opposite: the OS supplies **general
mechanisms** and the deployment supplies the **policy**, because different
installations need different policies (who may write file X is not the OS's
business to decide). The OS hard-codes only a few near-universal defaults,
such as private address spaces.

**A6. FALSE.** The two rates trade off through the sensitivity threshold:
demand closer matches and false positives fall while false negatives rise,
and vice versa. You choose an operating point (the crossover error rate is
merely the point where the two curves meet), you don't minimise both.

**A7. FALSE.** Complete mediation means checking **every single time** the
action is performed. Check-once-then-trust is precisely the compromise real
systems make (Unix checks at `open()` — see C2), and the chapter's footnote
concedes the principle is often traded for overhead and usability.

**A8. FALSE.** Fault tolerance addresses accidents and poor planning;
security addresses an **adversary** — deliberate, adaptive, and aimed at
your weakest point. A design safe against random faults (say, replicating
data three times) can be useless against an attacker who compromises the
component all three replicas share. Perfect virtualization would incidentally
solve much of security, but virtualization is imperfect and sharing is
intentional — those relaxations are the attack surface.

---

## B. Working the mechanisms

**B1.**
**(a)** (1) A login process running under a **privileged system identity**
prompts for a username; (2) prompts for the password, unechoed; (3) looks the
name up, retrieving UID, GID, home directory, shell, and the stored **salt +
salted hash**; (4) hashes salt‖typed-password and compares with the stored
value; (5) on a match, **forks**, sets the child's user and group (which its
privileged identity permits), changes to the home directory, and **execs the
user's shell**. The human→process hand-off is step 5: the moment the forked
process's PCB is stamped with the user's UID.
**(b)** Echoing the password would expose it to shoulder-surfers — and the
system loses nothing by suppressing it. The username, by contrast, is
near-public (visible in email addresses, file listings, `who`), and echoing
it lets the user catch typos that would otherwise produce baffling failures —
a **security-for-convenience trade-off** whose security cost is close to
zero. So: password suppression is necessary; username echo is a reasonable
trade, not a necessity.
**(c)** It frustrates **username harvesting**: an attacker probing names
cannot distinguish "no such user" from "wrong password", so guessing does not
reveal the set of valid accounts (which would focus a later password attack).
The cost is genuinely worse usability — a user who mistypes their *name*
gets no hint — plus the general lesson: give non-authenticated parties as
little information as possible.
**(d)** Children **copy the parent's identity at `fork()`**. Sufficient:
every process descends from the shell, so one correct stamp propagates to
all. Critical: the chapter notes systems rely on an authentication decision
*for the lifetime of the process (tree)* — it is checked once and then
trusted, so an error is not readily corrected; there is no continuous
re-authentication to catch it later.

**B2.**
**(a)** 26⁸ ≈ **2.1 × 10¹¹**; 62¹² ≈ **3.2 × 10²¹**. Expected guesses ≈ half
the space: ~1.0 × 10¹¹ and ~1.6 × 10²¹.
**(b)** Online: 1.0 × 10¹¹ / 5 ≈ 2 × 10¹⁰ s ≈ **660 years**. Offline:
1.0 × 10¹¹ / 10¹⁰ ≈ **10 seconds**. The converting event is the **theft of
the password file**: the attack moves from the server's rate limit
(5 guesses/s) to the attacker's hardware (10¹⁰ hashes/s) — a factor of
10¹⁰/5 = 2 × 10⁹, about **nine** orders of magnitude.
**(c)** If 90% fall to a curated dictionary, users are effectively choosing
from a space the size of that dictionary — of order 10⁷ entries (the
convention sheet 20 §B4 uses, `D = 10⁷`), not 10¹¹ — because the arithmetic
in (a) measures the space *available*, not the space *used*. Length rules alone don't help
because users satisfy them predictably (`password123!`), which the dictionary
already contains; the defence must raise actual entropy (random generation,
vaults) or slow the attacker.
**(d)** Lockout hands attackers a **denial-of-service** weapon: five wrong
guesses at a victim's name locks the victim out; slowing checking degrades
the attacker without disabling the account. Neither matters offline: both act
at the login interface, and the offline attacker never touches it — the
defence there is the hash's own cost (sheet 20's territory).

**B3.**
**(a)** False positive: accepting someone who is *not* the claimed user.
False negative: rejecting the genuine user. Matching is approximate (two
scans of one finger never yield identical bits), so a tolerance threshold is
required; tightening it rejects more genuine-but-noisy readings (FN↑) while
admitting fewer imposters (FP↓). The **crossover error rate** is the setting
where the two rates are equal — a single-number accuracy summary, but not
automatically the right operating point because the two errors rarely cost
the same (the chapter's phone-vs-vault example).
**(b)** Setting A: FN 2% × 3,000 = **60 rejections/year** (about one every
six days); thief: 1 − (1 − 1/50,000)¹⁰ ≈ 10/50,000 = **0.02%**. Setting B:
FN 10% × 3,000 = **300 rejections/year** (nearly daily); thief ≈ 10/10⁶ =
**0.001%**. For a phone, A: the daily-annoyance cost of B is what makes
users disable biometrics entirely (acceptability), while both breach
probabilities are small against a casual thief — and the phone's residual
risk is bounded by the passcode fallback.
**(c)** The vault inverts the costs: a false positive is catastrophic and
false negatives are cheap (the manager re-scans; 2% of 500 is ten re-scans a
year, 10% is fifty — both tolerable). Choose B, and tune *further toward low
FP* even at FN rates that would be absurd on a phone; add a second factor
(separation of privilege) rather than relax the threshold.
**(d)** Over the network, the scanner's reading arrives as **a pattern of
bits, and anyone can create any bit pattern**: an attacker who has (or
synthesises) the fingerprint's representation replays it without ever owning
a finger or a scanner. The check no longer establishes "a finger was
present", only "these bits arrived". The phone's reader is physically
attached to the checking hardware, so there is far less opportunity to
inject a spurious pattern between measurement and check.

**B4.** Behaviour **1 is the sound one** — *economy of mechanism*: small and
simple enough to understand and audit. For the rest (any five):
- **2 — least privilege.** A root daemon parsing untrusted requests means any
  bug is a total compromise, and it can read files the submitter cannot.
  Fix: run unprivileged; have the *client* open the file and pass the open
  descriptor (or copy the data) so the daemon acts with the user's
  authority.
- **3 — fail-safe defaults.** A broken config silently becomes allow-all;
  an attacker need only corrupt or delete the policy file. Fix: missing or
  unparsable policy ⇒ deny (or minimal safe service) and alert loudly.
- **4 — complete mediation.** Authority is checked at submission but
  exercised at print time: revoked access, or a file swapped via the name
  between check and use, is honoured anyway (the TOCTTOU shape from Bishop &
  Dilger, week 17). Fix: capture the file's content or an open descriptor at
  submission, or re-check at dequeue.
- **5 — least common mechanism** (with an integrity bonus). One
  world-writable spool shared by all users lets any user read, replace or
  delete others' queued jobs. Fix: per-user spool directories with owner-only
  permissions, or a daemon-owned spool reached only through the daemon.
- **6 — open design.** Secrecy of the protocol is the "main line of
  defence"; assume the adversary reverse-engineers it (they will), after
  which there is no defence at all. Fix: design so that knowing the protocol
  helps an attacker not at all; keep only keys secret.
- **7 — psychological acceptability** (and it defeats separation of
  privilege). The clumsy mechanism drove users to create a far worse channel;
  a security measure users route around is negative security. Fix: make the
  approved path convenient enough to use.

*Marking note: full credit needs principle named + concrete attack + minimal
fix; naming a plausible neighbouring principle (e.g. separation of privilege
for 5) with a correct attack still earns most marks.*

---

## C. Discussion and design critique

**C1.** Marking notes — the defence should be built from real properties,
not nostalgia:
- **Against what-you-have:** nothing to buy, ship, or lose. The chapter's own
  accounting: lose the token and you can't get in *and* someone else may; a
  password can't be pickpocketed, and replacing one is instant and free,
  versus re-issuing hardware.
- **Against what-you-are:** biometrics need special hardware most machines
  lack, carry irreducible FP/FN error, are hazardous to check remotely (bits
  are bits — B3d), and are **unrevocable**: a leaked fingerprint template is
  leaked forever, where a leaked password is rotated in a minute. A password
  also creates no permanent database describing your body.
- **Positive case:** universal (any device with a keyboard, any network),
  well-understood server side (salted hashes — sheet 20 adds deliberately
  slow hashing), cheap, and — done right,
  with generated passwords in a vault — high-entropy. The chapter's SMS
  aside supports the meta-point: mechanisms that horrify purists can provide
  quite reasonable security in practice.
- **Where the defence collapses:** high-value targets exposed to phishing
  and credential reuse — the failure modes intrinsic to secrets humans type
  into things (a password can be *given away*; a hardware key's response
  cannot). For bank operations, remote administration, anything where one
  compromised secret is catastrophic: demand a second factor — which is the
  chapter's own landing spot (multi-factor), not password abolition.
Answers that merely list password weaknesses have answered the fashionable
question, not this one.

**C2.** The defence of check-at-`open()`:
- **Cost of the pure principle:** re-checking permissions on every `read()` puts
  a policy evaluation on the innermost I/O path — and the check needs the
  file's metadata, so "complete" mediation taxes exactly the operations that
  matter (millions of reads per open). Checking once and handing back a
  descriptor makes the common case a table lookup.
- **The anomaly:** revocation is not immediate. Remove a user's read
  permission and their *already-open* descriptor keeps working until close —
  the access decision was cached in the descriptor and is trusted for its
  lifetime (the same check-once property as B1d).
- **Judgement:** correct trade for a general-purpose OS: revocation races
  are rare, bounded (close the file / kill the process), and auditable,
  while per-access checks cost every process all the time. Conditions that
  flip it: environments where instant revocation is a hard requirement
  (long-lived handles to sensitive resources, multi-tenant services) — there
  you need re-validation or revocable handles, paid for knowingly.
Best answers notice the descriptor is behaving like a *capability* — a
credential per ch. 54 — and that week 20 gives this anomaly its proper name.

**C3.** Marking notes: any well-argued pair earns marks; commonly strongest:
- **Least honoured — complete mediation** (or least privilege). Forces:
  performance (checks on hot paths), and the economics of retrofit — Unix's
  check-at-open, or setuid programs and root daemons holding far more
  privilege than the action needs, persist because the coarse mechanism was
  cheap and the fine one wasn't. Concrete examples from the course: the open
  file descriptor (C2); a root print/mail daemon (B4).
- **Most honoured — least common mechanism** (or economy of mechanism at
  the hardware level): per-process page tables give every process separate
  translation state by construction — compliance was cheap because the
  hardware mechanism (Part I) delivered isolation as a side effect of
  virtualization; no per-application effort is needed.
- **Verdict:** the neglected principle is *expensive, not wrong* — every
  compromise of it (B4's spool, the revocation anomaly) is a live bug class,
  and where the cost has been paid the attacks vanish. Accept the opposite
  verdict if argued with conditions rather than asserted.
