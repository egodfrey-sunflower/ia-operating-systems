> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 25 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their working;
> for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. FALSE.** It is the other way round: the VMM takes kernel mode, and the
guest OS is **deprivileged** — on Disco's MIPS, into *supervisor* mode (more
memory visibility than user mode, but no privileged instructions); on
hardware without such a mode, into user mode with page-table protection
around OS structures. That demotion is what makes every privileged attempt by
the guest trap to the VMM. (Later hardware added "direct support for an extra
level of virtualization" — the appendix's closing remark — which in practice
became VT-x/AMD-V root modes, but that is beyond the software design the
appendix describes.)

**A2. FALSE.** The hardware vectors through the trap table that the *true*
kernel-mode occupant installed — the VMM. The VMM does not know how to handle
the syscall, but it knows where the guest's handler is (it recorded the
address when the guest's boot-time attempt to install trap handlers itself
trapped), and jumps there at reduced privilege.

**A3. TRUE.** That is its definition and its point: with a hardware-walked
TLB the VMM gets no hook on misses, so it keeps a table mapping
guest-virtual → machine directly, installs it when the guest OS installs
that process's page table, and lets the hardware chug along none the wiser.

**A4. TRUE.** If the VMM handed one guest a page still holding another
guest's data, information would leak across the isolation boundary — the
same argument the OS applies between processes, one layer down. The
resulting double zeroing is the appendix's canonical **information gap**
example; Disco's fix was to modify IRIX to skip its own zeroing.

**A5. FALSE.** Diffie–Hellman produces a shared secret with *somebody* — it
authenticates no one. A man in the middle can run one exchange with each
party and relay (B3c). It must be paired with an authentication mechanism,
which is exactly what TLS does by signing the exchange.

**A6. FALSE.** The chapter is explicit: certificate security rests on the
cryptography *inside* the certificate — the CA's signature over the hashed
contents — not on how it travelled. Anyone can verify the signature with the
CA's public key, so a certificate fetched from an untrusted channel is
exactly as good as one delivered by hand. (What you must already hold,
securely, is the CA's key — the bootstrap.)

**A7. FALSE** for the common case. The handshake authenticates the
**server** (its certificate signs its Diffie–Hellman contribution); the
client typically has no certificate and is authenticated *later* — usually
by a password sent over the now-encrypted channel. Mutually-authenticated
TLS exists but is not the web's default.

**A8. FALSE** twice. Public-key crypto is not "more secure" — it is
different, and vastly more **expensive**. TLS uses it only to authenticate
and to establish a fresh **symmetric** key; the bulk of the session is
encrypted symmetrically (e.g. AES) under that key, a new one per connection.

---

## B. Translation, traps, and key exchange

**B1.**
**(a)** Compose guest (VPN→PFN) with VMM (PFN→MFN):

```
VPN 0 -> PFN 10 -> MFN 5      shadow: VPN 0 -> MFN 5
VPN 1    invalid              shadow: VPN 1 -> invalid
VPN 2 -> PFN 3  -> MFN 6      shadow: VPN 2 -> MFN 6
VPN 3 -> PFN 8  -> MFN 10     shadow: VPN 3 -> MFN 10
```

**(b)** The guest's page tables live in memory the VMM controls, and the
acts by which the guest *activates* mappings are privileged. The VMM
therefore interposes: it write-protects (monitors) the guest's page-table
pages and/or traps the guest's privileged attempt to install a page-table
base register, sees the new VPN 1 → PFN 12 entry, consults its own table
(PFN 12 → MFN 7), and installs VPN 1 → MFN 7 in the shadow table. The
mechanism is the same one running through the whole appendix:
**deprivileging plus trap-and-emulate** — the guest's privileged action
traps, and the VMM substitutes its own version.
**(c)** One **per-guest physical→machine table** per OS: 4. One **shadow
table per process** per guest: 4 × 50 = 200. (The guests themselves hold
their own 200 page tables, but those belong to the guests.) A single load
conceptually crosses **two mapping layers** — guest-virtual → guest-physical
→ machine — collapsed into one shadow lookup by the hardware.
**(d)** The shadow entry for VPN 2 is marked not-present, so the access
faults **to the VMM**. The VMM sees that the guest's mapping is valid but
that *it* paged out the backing frame; it reads the page back from disk into
some machine frame, updates its PFN→MFN table and the shadow entry, and
resumes the guest at the faulting instruction, which now hits. The guest
never learns because nothing it can observe changed: its page table still
says VPN 2 → PFN 3, and the fault was serviced below its horizon — the same
trick the OS plays on its processes, one level down.

**B2.**
**(a)** Native: **2** crossings — user→kernel trap, return-from-trap. With
VMM: **4** — (1) user traps to VMM; (2) VMM enters the guest handler at
reduced privilege; (3) the guest's return-from-trap is privileged, so it
traps to the VMM; (4) VMM performs the real return to user.
**(b)** Native: **2** — miss traps to OS handler, return. With VMM: **6** —
(1) miss traps to VMM; (2) VMM jumps into the guest's miss handler; (3) the
guest's privileged TLB-update instruction traps to the VMM; (4) VMM installs
the VPN→**MFN** translation instead and re-enters the guest; (5) the guest's
return-from-trap traps to the VMM; (6) VMM really returns to the process.
**(c)** Misses: 10⁹ / 1,000 = 10⁶. Per-miss cost: native 2c + m = 600 ns;
virtualized 6c + m = 1,000 ns.

```
native:      10⁹ × 1 ns + 10⁶ × 600 ns  = 1.0 s + 0.6 s = 1.6 s
virtualized: 10⁹ × 1 ns + 10⁶ × 1000 ns = 1.0 s + 1.0 s = 2.0 s
slowdown: 2.0 / 1.6 = 1.25  →  25%
```

**(d)** *Transparent:* Disco's **VMM-level software TLB** — the VMM records
every virtual→physical mapping it sees the guest install; on a later miss it
finds the translation there and installs virtual→machine directly, removing
the whole bounce through the guest (crossings 2–5) in the hit case.
*Guest-modifying:* **para-virtualization** — the guest is changed to
cooperate with the VMM (e.g. requesting mappings explicitly rather than
executing privileged instructions that must be caught), removing the
trap-on-privileged-instruction crossings (3)–(4) and, more generally,
replacing emulation-by-trapping with explicit calls. The appendix notes a
well-designed para-virtualized system approaches native efficiency.

**B3.**
**(a)** X = 5⁶ mod 23: 5² = 2, 5⁴ = 4, so 5⁶ = 4·2 = **8**.
Y = 5¹⁵ mod 23: 5¹ = 5, 5² = 2, 5⁴ = 4, 5⁸ = 16; 15 = 8+4+2+1 →
16·4·2·5 = 640 mod 23 = **19**.
Alice: k = Yˣ = 19⁶ mod 23; 19 ≡ −4, (−4)⁶ = 4096 mod 23 = **2**.
Bob: k = Xʸ = 8¹⁵ = 2⁴⁵ mod 23; 2¹¹ mod 23 = 2048 mod 23 = 1, so
2⁴⁵ = (2¹¹)⁴·2 = **2**. Both parties hold k = 2. ✓
**(b)** She observes n = 23, g = 5, X = 8, Y = 19 — but not x or y. To get k
she must recover x from X = gˣ mod n (or y from Y): the **discrete
logarithm** problem. Her options are trying all exponents (defeated by the
size of n) or computing a discrete log directly — solvable in principle,
computationally infeasible for large n (the chapter suggests ~600-digit
primes). XY mod n, which she *can* compute, is not k.
**(c)** Mallory picks her own exponent m. She intercepts Alice's X and
forwards X_M = gᵐ mod n to Bob; she intercepts Bob's Y and forwards X_M to
Alice in its place. Alice computes k₁ = (X_M)ˣ = gˣᵐ, which Mallory also
computes as Xᵐ; Bob computes k₂ = (X_M)ʸ = gʸᵐ, which Mallory computes as
Yᵐ. Two keys exist; Alice holds k₁, Bob holds k₂, **Mallory holds both**, and
decrypts, reads, re-encrypts and relays every message. Both endpoints see a
protocol that completed flawlessly.
**(d)** TLS has the server **sign its Diffie–Hellman contribution with its
private key**, and the client verifies the signature using the public key
from the server's **certificate** (ch. 57's artefact) — Mallory cannot
produce a valid signature over her substituted values, so the substitution
is detected. For this to mean anything the client must already hold, and
actually check against, a trusted CA public key (the browser's pre-installed
roots) — an unverified certificate reduces the whole construction back
to (c).

**B4.**
**(a)** Traditional: **2 CPU copies** (page cache → user buffer; user buffer
→ socket buffer) and **4 boundary crossings** (enter/exit `read()`,
enter/exit `write()`); the disk→cache and socket→NIC moves are DMA.
Zero-copy: **0 CPU copies** (the kernel hands page-cache pages to the NIC by
reference; DMA does the moves) and **2 crossings** (one `sendfile()`-style
call).
**(b)** Traditional: copies 100,000 × 2 × 4 KB = 800 MB/s; at 10 GB/s that
is **0.08 s/s = 8%** of a core. Crossings: 100,000 × 4 × 150 ns = 60 ms/s =
**6%**. Total ≈ **14%**. Zero-copy: copies 0%; crossings 100,000 × 2 ×
150 ns = 30 ms/s = **3%**. Saving ≈ **11% of a core** — at this request
rate, most of the I/O path's CPU cost was data movement and boundary
crossing, not "real work".
**(c)** TLS encrypts every byte under a **per-connection key**, so the bytes
that reach the wire are not the bytes in the page cache — the CPU must read
and transform the entire payload anyway, and handing cached pages to the NIC
by reference becomes meaningless. Zero-copy's premise (the CPU never needs
to touch the data) is destroyed by a security layer whose whole job is to
touch every byte. It is restored by pushing the cipher below the copy
boundary: NICs with **inline TLS/crypto offload** (kernel-TLS handoff),
which encrypt during the DMA — the same lesson as the VMM's software TLB,
that the fix for an expensive indirection is to do the work where the data
already is.

**B5.**
**(a)** The TGT is *itself a ticket* — a ticket for the ticket-granting server
(TGS), issued at login, sealed under the TGS's key, and carrying a session key
shared between the user and the TGS. The user proves knowledge of their
long-term (password-derived) key exactly once, to decrypt the login reply and
recover that session key; thereafter they cache the TGT and its session key.
To reach the *n*-th service the user presents the **TGT** to the TGS and asks
for a **service ticket** for that service — so the long-term key never travels
the wire at all (it is used locally to decrypt the login reply, and can then be
wiped from memory), and an eavesdropper never sees password-derived material. What the
user presents to a service is a service ticket plus a fresh authenticator, not
any long-term secret.
**(b)** (i) The **session key** reaches both parties encrypted under a key each
already holds: the *service's* copy is sealed inside the ticket (under the
service's key), and the *requester's* copy is sealed under the key/session key
the requester shares with the KDC. (ii) The **ticket body** is sealed under the
service's long-term key, so only that service (and the KDC, which knows all
keys) can open and validate it — the user carries it opaquely. The user cannot
forge a ticket because minting one requires the service's key, which they do
not have; they can only relay tickets the KDC already issued.
**(c)** A bare ticket is replayable: an eavesdropper who captures a ticket in
flight could resend it and impersonate the user. The **authenticator** — the
current time plus a **checksum**, encrypted under the session key — proves the
sender holds the session key (the service decrypts with its copy, and a matching
checksum is what establishes the authenticator was minted by a holder of that
key) *and* is fresh; the service rejects authenticators whose timestamp falls
outside a small acceptance window (and remembers recent ones), defeating
replay. The cost: timestamps require client and server clocks to be **loosely
synchronised** (within that window), so Kerberos assumes a synchronised-clock
infrastructure (e.g. NTP) — a nonce/challenge scheme avoids the clock
dependency but pays an extra round trip.
**(d)** *Unreachable* → an **availability** failure: no new tickets can be
minted, so users cannot obtain tickets for services they do not already hold —
but tickets already in hand keep working until they expire, because the KDC is
*not* on the per-request path once a service ticket exists (part of why the
design scales). *Compromised* → **catastrophic**: the attacker learns every
principal's long-term key (or can mint tickets for any service), so it can
impersonate any user to any service and any service to any user. The KDC is a
single point of total trust; recovery means re-keying every principal.

*Marking note:* ch. 57 deliberately does not teach the protocol — these answers
rest on the Neuman & Ts'o paper. Credit the shape (login once → TGT → per-service
tickets; session keys; timestamp-for-replay with its clock cost; KDC as single
point of trust), not exact message formats, which vary by Kerberos version.

---

## C. Discussion and design critique

**C1.** **Trust:** a CA is trusted *offline and long-term* — its public key
sits in clients for years, and verification needs no interaction with it; a
Kerberos-style server is trusted *online and continuously* — it participates
in (or underwrites) every authentication. **Revocation today:** this is the
CA's structural weakness — a signed certificate is self-contained and valid
until expiry, so "un-saying" it needs bolted-on machinery (revocation lists,
online status checks) that clients may not consult; the online server
revokes **instantly**, by refusing further tickets — the chapter says
exactly this: an online server "can invalidate authentication for a
compromised party instantly". **Unreachable:** the CA design shrugs —
verification is local, so an offline CA costs nothing day-to-day; the online
server is a single point of failure — when it is down, *nobody*
authenticates. **Deployments:** the open web wants CAs — millions of
mutually-strange parties, no shared administrative domain, no authority that
could be online for all of them. A university or company wants Kerberos —
one administrative domain, accounts that must die the day an employee
leaves, and an authority the organisation can keep available on its own
network. The deciding property: **whether the parties share an
administrative domain with a managed, reachable authority — and how fast
revocation must take effect.**

**C2.** A strong answer prosecutes the claim on three counts, then concedes
precisely.

**Count one — transparency is not achieved.** The appendix documents its own
counter-evidence: the **information gap**. The VMM schedules an idle-looping
guest as if it were working; it zeroes pages the guest will zero again;
below the illusion, real resources are misallocated because the interface
hides intent in *both* directions. And the illusion leaks upward too: a
guest can observe timing that no real machine would produce — the syscall
and TLB-miss inflation §B2 quantifies (25% on a modest model), and, by
extension, wall-clock time that appears to jump when the guest is descheduled
across a machine switch (the inflation is the appendix's; the clock-jump is a
fair inference from it, not stated in the text). An illusion that costs measurably and can be
detected from inside is not transparency; it is a lie both parties can
catch.

**Count two — the practitioners abandoned it whenever it mattered.**
Disco — the appendix's own flagship — **modified IRIX** (the demand-zeroing
change) because the transparent alternative was too expensive; and the
appendix's remedies for the information gap are inference (structured
guessing across the sealed interface) or **para-virtualization**, which is
by definition the surrender of transparency, and which the appendix credits
with near-native performance. When the transparent design and the fast
design diverge, the field picked fast.

**Count three — transparency is the wrong goal.** Everywhere else in this
course, layers communicate through explicit interfaces — syscalls, VFS,
the block layer — because explicit contracts are auditable, stable and
optimizable. A transparent VMM instead *infers* what the guest means and
the guest *accidentally reveals* it: fragile, version-dependent, and
structurally identical to the implicit-information techniques the appendix
relies on. (The appendix's TIP actually *praises* implicit information — "a
powerful tool in layered systems", "quite useful in a virtual machine
monitor"; the judgement that it is research cleverness rather than sound
interface design is this critique's own, not the text's.)

**The concession.** Transparency is exactly right when the guest **cannot
be changed**: proprietary or legacy OSes, binary-only appliances, or — the
appendix's own scenario — running your VMM under "an unfriendly
competitor's operating system". Consolidating unmodified workloads was the
founding use case, and it is only served by transparency.

**The modern fact.** Hardware support (the appendix's closing note: Intel and
AMD added "direct support for an extra level of virtualization" — in practice
nested page tables and VT-x/AMD-V root modes) made *transparent*
CPU/MMU virtualization essentially free — vindicating transparency where
hardware absorbed its cost — while real deployments simultaneously run
**para-virtual I/O drivers** in every serious guest, conceding transparency
where it still costs. The market's verdict is "both, by subsystem", which
is the strongest available evidence that transparency was never a goal in
itself — only a constraint to be paid for exactly where modification is
impossible.

*Marking note: the question demands a case built from the appendix, not
free-floating opinions about VMware. Wanted: information gap with both
examples, Disco's IRIX change, para-virtualization as concession, §B2's
numbers as visible overhead — then a genuine flip condition. Answers that
argue only one side, or that rebut with facts not derivable from the
week's reading, sit mid-band.*
