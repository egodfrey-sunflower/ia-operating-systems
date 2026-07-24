# Exercise Sheet 20 — Access control and cryptography

**Attempt after Week 20.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise20-solutions.md`](solutions/exercise20-solutions.md).

**This sheet leans on:** OSTEP ch. 54–56 (§B4 revisits week 19 authentication); Thompson (1984), *Reflections on
Trusting Trust*. It also draws on ch. 39 (week 17) for permission bits and file
descriptors.

> **Note.** OSTEP chapters 53–57 ship **no homework, simulators or code** — every
> question below is original. §B is deliberately arithmetic-heavy, because the
> Cambridge protection questions are, and nothing upstream drills it.

---

## A. Warm-ups

*True or false? Justify in one or two sentences.*

**A1.** An access control list and a capability list are alternative
implementations of the same abstraction.

**A2.** Unix's `rwx` permission bits can express any access matrix column.

**A3.** A file descriptor is a capability.

**A4.** Under a mandatory access control (MAC) policy, the owner of a file
decides who may read it.

**A5.** Revoking one subject's access to one object is equally cheap under ACLs
and under capabilities.

**A6.** Encrypting a file protects it from a process running as root on the same
machine while the file is in use.

**A7.** A cryptographic hash of a password is safe to store publicly, because
hashes cannot be inverted.

**A8.** Full-disk encryption protects against an attacker who steals the powered-off
laptop, and also against one who steals it while it is suspended with the screen
locked.

---

## B. Working the mechanisms

**B1. Build the matrix, then project it.**
A small system has subjects **alice**, **bob**, **carol**, and a backup daemon
**backupd**. Objects are `/home/alice/notes`, `/home/bob/report`, `/etc/passwd`,
and `/dev/printer`. The policy is:
- alice may read and write her notes; bob may read them.
- bob may read and write his report; nobody else may touch it.
- everyone may read `/etc/passwd`; only root may write it (treat root as a fifth
  subject).
- alice and bob may write to the printer; carol may not.
- backupd may read every file, and write none.

  (a) Draw the access matrix.
  (b) Write out the **ACL** representation — i.e. the per-object lists.
  (c) Write out the **capability-list** representation — i.e. the per-subject lists.
  (d) Suppose the system grows to `S` subjects and `O` objects, and each subject
      has access to on average `k` objects, with `k ≪ O`. Give the storage cost
      of the full matrix, of the ACL representation, and of the C-list
      representation. Which representation does the sparsity favour, and why do
      real systems still mostly use ACLs for files?

**B2. The revocation asymmetry.**
Using your representations from B1:
  (a) Revoke **bob's** read access to `/home/alice/notes`. Describe exactly what
      must be modified under each representation.
  (b) Now revoke **all** access to `/home/bob/report` from everyone, immediately.
      Describe the work required under each representation.
  (c) Generalise: state which representation makes *per-object* revocation cheap
      and which makes *per-subject* review cheap, and explain in one sentence why
      the asymmetry is unavoidable.
  (d) A capability that has already been handed out is, by construction, hard to
      recall. Name two mechanisms real systems use to make capabilities
      revocable anyway, and state the cost each introduces.

**B3. Unix bits as a lossy ACL.**
Unix compresses an object's ACL into 9 permission bits — `rwx` for owner, group,
and other — plus setuid, setgid and sticky.
  (a) A full ACL for a system with `N` users needs how many entries in the worst
      case? How many does Unix store?
  (b) Express this policy using only Unix bits, or prove it impossible: *"alice
      may read and write; bob may read; carol may read; nobody else has any
      access."*
  (c) Now this one: *"alice may read and write; bob may read; carol may write;
      nobody else has any access."* Same question.
  (d) POSIX ACLs (`setfacl`) exist precisely to fix this. Given they have existed
      for decades, suggest why the 9-bit scheme nonetheless remains the default
      almost everywhere.

**B4. What salting actually buys.**
A site stores password hashes for `N = 10⁴` users. An attacker has a dictionary
of `D = 10⁷` likely passwords and hardware computing `10¹⁰` hashes per second.
  (a) With **unsalted** hashes, how many hash computations must the attacker
      perform to test the whole dictionary against *every* user? How long does it
      take?
  (b) With a distinct random **32-bit salt** per user, how many computations, and
      how long?
  (c) What is the speedup factor the salt has cost the attacker, in terms of `N`?
  (d) Your answer to (b) should still be alarmingly fast. What does this tell you
      about the *real* defence, and what property must the password-hashing
      function have? Name a function with that property.
  (e) Separately, explain what salting does to a precomputed rainbow table, in
      terms of the table's size.

**B5. Threat models for encryption at rest.**
For each scenario, say whether full-disk encryption helps, and why:
  (a) The laptop is stolen while powered off.
  (b) The laptop is stolen while suspended, screen locked.
  (c) A malicious process runs as a normal user on the running machine.
  (d) A malicious process runs as root on the running machine.
  (e) The disk is decommissioned and sold without being wiped.
  (f) An attacker has physical access and can attach a debugger to the running,
      unlocked machine.

Then state, in one sentence, the general rule your six answers imply about what
encryption at rest can and cannot do.

---

## C. Discussion and design critique

**C1. The confused deputy.**
A compiler service runs with permission to write to a system-wide billing log at
`/var/billing`. It accepts a filename from the user and writes its output there.
A user invokes it with the output filename `/var/billing`, and the log is
destroyed.

  (a) Explain why this happens under an ACL-based system, in terms of *whose*
      authority is used when the compiler opens the file.
  (b) Explain how a capability-based design prevents it structurally, rather than
      by adding a check.
  (c) This is the standard argument that capabilities are safer than ACLs. Give
      the strongest counter-argument you can, drawing on your answers to B2.

**C2. Trusting trust.**
Thompson shows a compiler can be made to insert a backdoor into a program it
compiles, *and* to insert the backdoor-insertion into any compiler it compiles —
so the malicious code appears in no source anywhere.
  (a) What exactly is the trusted computing base of a system, on Thompson's
      argument?
  (b) Does it help to read the compiler's source and compile it yourself with
      that same compiler? Explain.
  (c) Modern practice offers partial answers Thompson did not have — reproducible
      builds, and diverse double-compiling. Explain how *one* of them attacks the
      problem, and be precise about what it does and does not guarantee.

**C3.** *An intrepid engineer proposes the following.* "Operating-system access
control is a mess of ACLs, groups, setuid bits and special cases, and it all
rests on trusting the kernel. I propose we delete it. Every file is encrypted
with its own key. Users hold the keys for the files they are allowed to read.
There is no permission checking at all — the OS hands any file to anyone who asks,
because without the key it is meaningless. This is strictly stronger: it works
even if the kernel is compromised, even on a stolen disk, and even on untrusted
cloud storage."

Evaluate this proposal. Address specifically: what happens when a file must be
*written* by several users; how a key is revoked when someone leaves; what the
system does at the moment a file is actually used for computation; whether the
"works even if the kernel is compromised" claim survives scrutiny; and what has
happened to the problem rather than been solved by it. Identify the one setting
in which a version of this proposal is genuinely the right answer. Conclude with
a recommendation.
