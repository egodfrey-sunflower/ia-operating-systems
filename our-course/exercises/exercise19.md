# Exercise Sheet 19 — Security fundamentals and authentication

**Attempt after Week 19.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise19-solutions.md`](solutions/exercise19-solutions.md).

**This sheet leans on:** OSTEP ch. 52–54; Saltzer & Schroeder (1975). It also
draws on ch. 39 (week 17) for the file API and on ch. 6 (week 3) for the
user/supervisor mode transition.

> **Note.** OSTEP ch. 52–54 ship **no homework, simulators or code** — every
> question below is original. No tooling is needed; the whole sheet is pen and
> paper. The salting-arithmetic questions you might expect here live on
> **sheet 20**, which revisits ch. 54 deliberately.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** A system that guarantees confidentiality and integrity thereby
guarantees availability.

**A2.** The principle of open design requires publishing your system's design.

**A3.** Under Unix, a newly created process runs under the identity of the
user who wrote the program it executes.

**A4.** If a server stores only cryptographic hashes of passwords, an
attacker who steals the password file learns nothing useful.

**A5.** Because the OS supplies security mechanisms, it should also fix the
security policies, so that applications cannot get them wrong.

**A6.** A biometric authentication system can be tuned so that both its false
positive rate and its false negative rate are minimised simultaneously.

**A7.** Complete mediation means checking an access request the first time
the resource is used.

**A8.** Security is a special case of fault tolerance, since both are about
the system misbehaving.

---

## B. Working the mechanisms

**B1. Where identity comes from.**
Ch. 54 sketches the Unix login procedure step by step.
  (a) Reconstruct the sequence: from the privileged login process's prompt to
      the user's shell running under the right UID, list the steps, and mark
      the exact point at which "a human at a keyboard" becomes "a process
      with an identity".
  (b) Login echoes the typed username but not the password. The chapter asks:
      is echoing the username a security hole, a necessity, or a trade-off?
      Answer it, for both fields.
  (c) When authentication fails, Unix does not say *whether the username or
      the password was wrong*. What attack does this frustrate, and what does
      the choice cost?
  (d) Every process the user ever runs inherits its UID from this one shell.
      State the inheritance rule, and explain why getting login right once is
      both sufficient and critical — what property of OS authentication makes
      mistakes here so hard to undo?

**B2. Guessing, online and offline.**
A system's users choose 8-character passwords over the 26 lowercase letters;
a hardened minority use 12 characters over a 62-symbol alphabet.
  (a) Compute the size of each password space, and the *expected* number of
      guesses to find a password by brute force in each case.
  (b) An **online** attacker can try 5 guesses per second against the login
      prompt. An **offline** attacker who has stolen the hashed password file
      computes 10¹⁰ hashes per second. For the 8-character passwords, how
      long does each expected attack take? What single event converts the
      first threat model into the second?
  (c) Ch. 54 reports that a good dictionary attack recovers ~90% of the
      passwords for a typical site. Reconcile this with your answer to (a):
      what does it say about the *effective* size of the space users actually
      choose from, and why does adding length requirements alone not fix it?
  (d) The chapter offers two defences against online guessing: locking the
      account after a few failures, or drastically slowing password checking
      after a few failures. Give one serious drawback of lockout that
      slowing does not share, and state why *neither* defence helps once the
      password file is stolen.

**B3. Tuning a biometric.**
A fingerprint reader can be tuned. Setting **A**: false negative rate 2%
(1 in 50), false positive rate 0.002% (1 in 50,000). Setting **B**: false
negative rate 10%, false positive rate 0.0001% (1 in 10⁶).
  (a) Define false positive and false negative in this context, and sketch
      why one rate rises as the other falls. What is the *crossover error
      rate*, and why is it not automatically the right operating point?
  (b) A phone owner unlocks 3,000 times a year; if the phone is stolen,
      assume the thief gets 10 attempts before lockout. For each setting,
      compute the expected number of false rejections per year and the
      probability a thief gets in. Which setting is right for the phone, and
      why?
  (c) A bank vault is opened 500 times a year by the manager; a robber who
      reaches the scanner gets one attempt, and a false positive is
      catastrophic. Which setting — and which *direction of further tuning* —
      is right here? Justify with the same style of estimate.
  (d) The bank proposes letting branch staff authenticate to head office by
      sending fingerprint scans over the network. Using the chapter's "bits
      are bits" argument, explain the structural problem, and why the same
      objection does not apply to the reader built into the phone.

**B4. A principles audit.**
*CampusPrint* is a university print service. Its design:

1. One 2,000-line daemon handles queueing, quota and printing; the authors
   kept it deliberately small and simple.
2. The daemon runs as **root**, so that it can read any user's files when
   they submit a print job by filename.
3. Per-department quota policies live in a config file; if the file is
   missing or unparsable, the daemon logs a warning and **allows all jobs**,
   so that printing is never blocked by a config error.
4. Access is checked **when a job is submitted**; the file itself is read
   when the job reaches the head of the queue, perhaps hours later.
5. All jobs spool through a single **world-writable** directory shared by
   every user.
6. The protocol between client and daemon is undocumented on purpose; the
   authors describe this as "our main line of defence".
7. Administrators found the smart-card login for the admin console so
   clumsy that they taped its password to the printer.

For **five** of behaviours 2–7, name the ch. 53 design principle violated,
explain the concrete attack or failure it invites, and propose a minimal fix.
One behaviour in the list of seven is actually *sound* — identify it and name
the principle it honours.

---

## C. Discussion and design critique

*This week's discussion questions ask you to **defend an unfashionable design** —
the text dismisses it; make the best honest case for it, and state the conditions
under which your defence fails.*

**C1.** Ch. 54 reports "a widely held belief in the computer security
community that passwords are a technology of the past" — at best one factor
among several. Defend the unfashionable position: **passwords, used alone,
are still the right authentication mechanism for many systems.** Draw on the
chapter's own evidence about what-you-have and what-you-are (loss, theft,
special hardware, error rates, remote scans), and on properties the other
factors lack (cost, revocability, deniability, no database of your body).
Then state precisely the class of systems for which your defence collapses,
and what minimum should be demanded there.

**C2.** Ch. 53's footnote admits that complete mediation "is often ignored
in many systems, in favor of lower overhead or usability" — and Unix is an
example: permissions are checked at `open()`, and the file descriptor is
thereafter honoured without re-checking. Defend Unix's choice. Give the
concrete costs of the pure principle, the concrete anomaly the compromise
creates (what happens when access is revoked while a file is open?), and
your judgement — with conditions — on whether the trade was correct.

**C3.** Saltzer & Schroeder is fifty years old. Choose **one** of the eight
principles that, in your judgement, real systems honour *least*, and one they
honour *most*. For the least-honoured: explain the economic or human forces
that defeat it, using at least one concrete system you know from this course.
For the most-honoured: explain what made compliance cheap. Finish with a
verdict: is the least-honoured principle wrong, or just expensive?
