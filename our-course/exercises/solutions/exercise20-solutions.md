> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 20 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their working;
> for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. TRUE.** Both store the access matrix; they differ only in orientation. An
ACL is a **column** (per object, which subjects), a capability list is a **row**
(per subject, which objects). Everything else — revocation cost, audit cost,
propagation behaviour — follows from that one choice.

**A2. FALSE.** The 9 bits partition all subjects into exactly **three** classes:
owner, one group, everyone else. Any policy needing four or more distinct
permission sets is inexpressible. See B3(c) for a proof by counting.

**A3. TRUE**, and it is the cleanest example in the course. A file descriptor is
unforgeable (you cannot manufacture one for a file you may not open), it
simultaneously *names* the object and *conveys authority* over it, it is checked
once at `open()` and not on every `read()`, and it can be passed to another
process (over a Unix socket) — transferring authority with it.

**A4. FALSE.** That is **discretionary** access control (DAC), which is what Unix
does. Under **mandatory** access control the policy is set system-wide by an
administrator and the owner *cannot* override it — the point being to stop a
careless or malicious owner from granting away access the organisation intended
to withhold.

**A5. FALSE.** Under ACLs it *is* cheap — one localised edit: strike that
subject's entry from that object's list. Under capabilities it is **not**
equally cheap, because a capability can be copied and passed on; to revoke it
you must find and destroy every copy the subject may hold or have propagated
(ch. 55: capability revocation "can be easy or hard"), or pay for the
indirection/expiry machinery of B2(d). So the single-cell case is *not*
symmetric. (The whole-object and whole-subject sweeps of B2 are where the
representations trade places — that asymmetry is a different claim from this
one.) See B2.

**A6. FALSE.** For a file in use, the data has been decrypted and root can read
it through the file system — or read the key out of memory. Encryption protects
data the OS is *not currently mediating*; it is not a substitute for access
control on a live system.

**A7. FALSE.** Hashes need not be inverted — they can be **guessed**. An attacker
hashes candidate passwords and compares. Since human passwords have low entropy,
this succeeds routinely. Publishing hashes converts an online attack (rate-limited
by the server) into an offline one (limited only by hardware). See B4.

**A8. FALSE for the second case.** Powered off, the key is not in memory and FDE
works as intended. Suspended-to-RAM, the key **is** in memory, and the screen lock
is an application-level check, not a cryptographic boundary — cold-boot and
DMA attacks recover the key. Suspend-to-disk with the key evicted is the fix.

---

## B. Working the mechanisms

**B1.**
**(a)** Access matrix (`r` read, `w` write, `–` none):

| | /home/alice/notes | /home/bob/report | /etc/passwd | /dev/printer |
|---|---|---|---|---|
| **alice** | rw | – | r | w |
| **bob** | r | rw | r | w |
| **carol** | – | – | r | – |
| **backupd** | r | r | r | – |
| **root** | – | – | rw | – |

**(b) ACL representation** (per object):
```
/home/alice/notes : {alice:rw, bob:r, backupd:r}
/home/bob/report  : {bob:rw, backupd:r}
/etc/passwd       : {alice:r, bob:r, carol:r, backupd:r, root:rw}
/dev/printer      : {alice:w, bob:w}
```
**(c) C-list representation** (per subject):
```
alice   : {notes:rw, passwd:r, printer:w}
bob     : {notes:r, report:rw, passwd:r, printer:w}
carol   : {passwd:r}
backupd : {notes:r, report:r, passwd:r}
root    : {passwd:rw}
```
**(d)** Full matrix costs **S·O**. Both projections store only the non-empty
cells, of which there are **S·k**, plus headers — `O(S·k + O)` for ACLs and
`O(S·k + S)` for C-lists. With `k ≪ O` both are dramatically better than the
matrix, and **neither is favoured over the other on storage grounds**.

Real systems use ACLs for files because of *queries and lifetimes*, not storage:
the administrative question is almost always per-object ("who can read this?"),
objects outlive subjects, per-object revocation must be immediate (B2b), and the
OS already has a per-file metadata structure — the inode — to hang the list on.
Credit for noticing storage is a red herring here.

**B2.**
**(a)** *ACL:* delete `bob:r` from `/home/alice/notes`'s list — one localised
edit, and it takes effect on the next check. *C-list:* delete the `notes` entry
from bob's list — also one edit, **provided you can find every copy**. If bob has
passed the capability to another subject or cached it, you have not revoked
anything.
**(b)** *ACL:* clear one list. **O(1)**, immediate, complete. *C-list:* you must
scan **every subject's list** for entries naming that object — **O(S·k)** — and
you still cannot recall capabilities that have propagated off-system.
**(c)** **ACLs make per-object revocation cheap; capabilities make per-subject
review cheap.** The asymmetry is unavoidable because you have stored a
two-dimensional matrix in one orientation: querying along the stored axis is a
lookup, querying along the other is a scan. It is exactly the row-major /
column-major trade-off.
**(d)** Any two of:
- **Indirection.** The capability names a slot in a table the owner controls;
  invalidate the slot and every copy dies at once. *Cost:* an extra dereference
  on every use, and the table is a centralised bottleneck and failure point —
  you have partly reintroduced the ACL.
- **Expiry.** Capabilities carry a lifetime and must be refreshed. *Cost:*
  revocation is not immediate — it is bounded by the lifetime — and refresh
  traffic scales with usage. (This is how bearer tokens work in practice.)
- **Generation / version numbers.** The object carries a version; a capability
  embeds the version it expects; bumping the object's version invalidates all
  outstanding capabilities. *Cost:* revocation is all-or-nothing — you cannot
  revoke one holder without revoking everyone and re-issuing.

**B3.**
**(a)** A full ACL needs up to **N** entries, one per user. Unix stores **3**
(owner, group, other).
**(b) Possible.** Owner = alice with `rw`; a group containing bob and carol with
`r`; other = none. Mode `0640`. **But** it requires that such a group exists —
and creating groups is an administrative operation. That is a real cost: the
policy is expressible only if you can also mutate the system's group table.
**(c) Impossible.** Proof by counting: the 9-bit scheme partitions all subjects
into exactly **three** classes (owner, the file's single group, everyone else),
and assigns one permission set per class. This policy requires **four** distinct
sets — alice `rw`, bob `r`, carol `w`, others none. A file has exactly one group,
so bob and carol cannot be given different permissions while both being non-owner
and non-other. 4 > 3, so no assignment exists.
**(d)** Because the 9-bit scheme is *predictable and universal*. Every tool
understands it; POSIX ACLs need filesystem support and are silently dropped by
`tar`, `scp`, `rsync` and most archive formats unless explicitly preserved — so
policies expressed in ACLs quietly evaporate on copy. They also cost a
variable-length extended attribute rather than 9 bits in the inode, and they are
harder to reason about (ordering, masks). Most real policies get approximated
into the 3-class model instead, which is worse security but survives contact with
the toolchain.

**B4.**
**(a)** Unsalted, each dictionary word hashes to the *same* value for every user,
so the attacker hashes the dictionary **once**: **10⁷ hashes**, then does cheap
lookups against all 10⁴ stored hashes. At 10¹⁰ hashes/s that is
**10⁷ / 10¹⁰ = 10⁻³ s ≈ 1 millisecond.**
**(b)** With a distinct salt per user the work cannot be shared: each word must
be re-hashed with each user's salt. **D × N = 10⁷ × 10⁴ = 10¹¹ hashes**, taking
**10¹¹ / 10¹⁰ = 10 seconds.**
**(c)** A factor of **N = 10⁴** — the salt exactly removes the attacker's ability
to amortise across users. Note the cost is `N`, *not* 2³²; the salt's *length*
governs precomputation (see (e)), not this factor.
**(d)** Ten seconds is no defence at all. The lesson: **salting defeats sharing
and precomputation, but does nothing about the cost of a single guess.** The real
defence is that the password-hashing function must be **deliberately slow**, and
ideally **memory-hard** so that GPUs and ASICs lose their advantage. Suitable
functions: **Argon2** (current best practice), **scrypt**, **bcrypt**, or
**PBKDF2** with a high iteration count. A general-purpose fast hash — SHA-256,
MD5 — is the wrong tool no matter how well salted.
**(e)** A rainbow table trades precomputation for lookup speed, but it is built
for one hash function *and one salt*. With a 32-bit salt the attacker needs a
separate table per salt value — **2³² ≈ 4.3 × 10⁹ times** the storage. That is
what makes precomputation infeasible rather than merely expensive.

**B5.**
**(a) Powered off — YES.** The canonical case: ciphertext on disk, key nowhere.
**(b) Suspended, locked — NO (mostly).** The key is in RAM; the lock screen is an
application check, not a cryptographic one. Cold-boot and DMA attacks apply.
**(c) Unprivileged process on a running machine — NO.** FDE is transparent above
the block layer; the filesystem serves plaintext. What stops this attacker is
**access control**, not encryption.
**(d) Root on a running machine — NO.** Root reads the plaintext through the
filesystem, and can extract the key from kernel memory besides.
**(e) Decommissioned disk sold unwiped — YES.** Exactly the threat FDE handles,
and the reason "crypto-erase" (destroy the key) is a valid sanitisation method.
**(f) Debugger on a running unlocked machine — NO.** Keys and plaintext are both
live.

**General rule:** encryption at rest defends data only when the system is *not
running* and the key is *not present* — it protects against loss of physical
media, not against a live or compromised system. Protecting a running system is
access control's job.

---

## C. Discussion and design critique

**C1.**
**(a)** Under an ACL system the check is made against the **compiler's own
authority** — its effective UID, i.e. its *ambient* authority — not the invoking
user's. The user supplies only a *name*, a string with no authority attached.
Because designation (naming the file) and authorisation (being allowed to touch
it) are separate, the compiler unwittingly lends its greater authority to the
user's designation. The access check passes, correctly, and the wrong thing
happens.
**(b)** Under capabilities the user must **hand the compiler a capability** to
the output file, not a name. You cannot pass a capability you do not hold, so a
user with no access to `/var/billing` simply cannot express the attack — there is
nothing to pass. Designation and authority are unified in one unforgeable object.
Note this is *structural*: no check was added, the attack became inexpressible.
**(c)** The strongest counter comes from B2. Capabilities make the live policy
**undiscoverable and unrevocable**: you cannot answer "who can currently write
`/var/billing`?" without scanning every subject, and once a capability propagates
you may be unable to recall it without the indirection or versioning machinery of
B2(d) — which reintroduces a central table, i.e. an ACL in disguise. Real
organisations need per-object audit and immediate revocation on personnel change,
and those are precisely the ACL's strengths. The honest conclusion is that
capabilities are better at *preventing* confused-deputy errors and worse at
*administering* a policy over time.

**C2.**
**(a)** The trusted computing base is not just the kernel and the source you have
read; it is **the entire toolchain and its whole history** — the compiler binary,
the compiler that built that binary, and so on backwards — plus the hardware and
microcode underneath. Trust is transitive through the build, and the chain has no
natural terminus.
**(b) No, and this is the trap.** If the compiler *binary* is already
compromised, compiling clean compiler source with it reproduces the backdoor in
the new binary. The source is clean, the binary is not, and the binary is what
perpetuates the attack. Self-hosting a compiler means the artefact certifies
itself. Inspection of source is worthless against it.
**(c)** *Either* answer, done precisely:

**Diverse double-compiling** (Wheeler). Compile the compiler's source with a
*second, independently developed* compiler, giving binary A. Use A to compile the
compiler source again, giving B. Separately, use the suspect compiler to compile
the same source, giving C. If B and C are bit-identical, the suspect compiler is
not inserting a Thompson backdoor. **Guarantees:** detects the attack provided at
least one of the two compilers is clean and both produce deterministic output.
**Does not guarantee:** anything about hardware or microcode backdoors, nor about
a conspiracy in which *both* compilers carry the same attack.

**Reproducible builds.** Make the build bit-for-bit deterministic so that many
independent parties can compile the published source and confirm they all get the
published binary. **Guarantees:** that the shipped binary corresponds to the
shipped source — closing the "we compiled something else" gap. **Does not
guarantee:** protection from Thompson's attack itself, since if everyone uses the
same compromised compiler everyone reproduces the same backdoored binary and
agrees.

**C3.** A strong answer works through the mechanism rather than the slogan.

**Multiple writers.** A shared key gives no accountability — every holder can
produce ciphertext indistinguishable from any other's — and no integrity, since
anyone with the key can forge. You need signatures for integrity, and separate
keys for read versus write, so the "one key per file" model immediately becomes
several. Key count grows with files × permission levels.

**Revocation on departure.** The leaver *keeps the key*, and may keep copies of
the ciphertext. Genuine revocation means re-encrypting every file they could read
under a fresh key and redistributing it to everyone remaining —
O(files × remaining users) work — and it still does nothing about data already
copied. This is exactly the capability-revocation problem of B2(b), in its worst
form, now with cryptographic rather than merely bookkeeping costs.

**At the moment of use.** To compute on data you must decrypt it. There is
therefore always a window in which plaintext exists in memory under the OS's
control, and whatever protects it during that window is **access control**. The
proposal has not removed the need for it; it has relocated it to a place where it
is no longer being managed.

**"Works even if the kernel is compromised."** **False for data in use.** A
compromised kernel sees both the plaintext and the key while the file is open —
it is the thing performing the decryption. The claim holds only for data at rest
that is *never opened on the compromised machine*, which is a much narrower and
much less useful statement than the one made.

**What has happened to the problem.** It has become **key management** —
distribution, storage, rotation, revocation, escrow, and recovery when a user
loses their key (at which point the data is simply gone, with no administrator
able to help). This is harder than ACL administration, not easier, and it is
ch. 56's central lesson: cryptography converts a data-protection problem into a
key-management problem, and does not make it disappear.

**Where a version of this is right: untrusted storage.** If the storage provider
is *itself* the adversary — third-party cloud storage, backups held offsite,
media that may be lost — then client-side end-to-end encryption is exactly
correct, and the key-management pain is the accepted price. The distinguishing
condition is that the data is **never computed on by the untrusted party**; they
only hold bytes.

**Recommendation.** Reject as a replacement for OS access control — it fails at
sharing, fails at revocation, and its strongest claim collapses for data in use.
Adopt it as a *complement*, for untrusted storage and media loss, where it is not
merely defensible but best practice. The verdict flips only if the threat model
is "the storage holder is the adversary and never executes on the data."

*Marking note: the best answers identify that the proposal is not wrong so much
as **mis-scoped** — it is a correct design for a different threat model — and
name key management as the thing that has absorbed the difficulty. Answers that
simply assert "you still need access control" without saying why the decryption
moment forces it earn little.*
