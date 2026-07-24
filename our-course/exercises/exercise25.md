# Exercise Sheet 25 — Virtual machine monitors, and distributed system security

**Attempt after Week 25.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise25-solutions.md`](solutions/exercise25-solutions.md).

**This sheet leans on:** OSTEP App. B and ch. 57; OSPP §10.1 (zero-copy I/O);
Neuman & Ts'o (1994), *Kerberos* — its ticket/TGT mechanics are drilled in §B5,
and it underpins the online-authentication-server comparison in §C1. It draws
on ch. 6 (traps), ch. 18–20 (page tables, TLBs) and ch. 56 (week 20) for the
cryptographic primitives.

> **Note.** Ch. 57 and Appendices A–B ship **no homework, simulators or
> code** — every question below is original. §B is arithmetic-heavy on
> purpose: translation and trap accounting is where the Cambridge calculation
> style meets this material.

No tooling is needed — pen and paper throughout.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing.*

**A1.** Under the VMM design of Appendix B, the guest OS runs in kernel mode
and the VMM runs in a new, even more privileged mode.

**A2.** When an application in a guest OS makes a system call, the trap
transfers control directly to the guest OS's trap handler.

**A3.** A shadow page table maps guest-virtual page numbers directly to
machine frame numbers.

**A4.** A VMM must zero a page before giving it to a guest OS, even though
the guest will typically zero it again before giving it to a process.

**A5.** Diffie–Hellman key exchange lets two parties establish a shared
secret over an insecure channel, and in doing so assures each party of whom
it is sharing the secret with.

**A6.** A certificate is only trustworthy if it was delivered over a secure,
authenticated channel.

**A7.** In a typical TLS connection to a website, both server and client are
authenticated during the handshake.

**A8.** Because public-key cryptography is more secure than symmetric
cryptography, TLS uses it to encrypt the whole session.

---

## B. Translation, traps, and key exchange

**B1. Two page tables deep.**
A guest OS's page table for one process reads: VPN 0 → PFN 10, VPN 1 →
invalid, VPN 2 → PFN 3, VPN 3 → PFN 8. The VMM's table for this guest reads:
PFN 3 → MFN 6, PFN 8 → MFN 10, PFN 10 → MFN 5.

  (a) The hardware walks page tables itself (hardware-managed TLB). Write out
      the **shadow page table** the VMM must supply for this process.
  (b) The guest OS now maps VPN 1 → PFN 12 (a frame the VMM backs with
      MFN 7). The guest cannot be allowed to install this itself. Describe
      how the VMM learns of the change and what it updates, naming the
      mechanism that gives the VMM control at the right moment.
  (c) A machine runs V = 4 guest OSes, each with P = 50 processes. How many
      page tables of each kind must the VMM maintain, and how many
      translation layers does one user-level load conceptually cross?
  (d) The VMM has quietly swapped the machine page backing the guest's
      PFN 3 out to disk. Walk through what happens when the guest process
      touches VPN 2, and explain why the guest OS never finds out.

**B2. The price of interposition.**
Take a privilege crossing (any trap into, or return from, a more privileged
layer) to cost **c = 100 ns**; the OS's own miss-handling or syscall work
costs **m = 400 ns**.

  (a) Using the appendix's control flows: count privilege crossings for one
      system call **without** a VMM, and **with** a VMM interposing (the OS
      runs deprivileged). State each crossing.
  (b) Now a TLB miss on a **software-managed** TLB: count the crossings
      without and with the VMM, remembering that the guest's TLB-update
      instruction is itself privileged.
  (c) A workload executes 10⁹ instructions at 1 ns each and suffers one TLB
      miss per 1,000 instructions. Using your counts and the costs above,
      compute total runtime native and virtualized, and the percentage
      slowdown.
  (d) Name and explain the two mechanisms from the appendix that attack this
      overhead — one transparent to the guest, one requiring guest changes —
      and state for each which crossings it removes.

**B3. Diffie–Hellman, by hand, then broken, then fixed.**
Use n = 23 and g = 5. Alice picks x = 6; Bob picks y = 15.

  (a) Compute X = gˣ mod n, Y = gʸ mod n, and the shared key k as computed
      by *each* party. Show working (repeated squaring keeps it painless).
  (b) List exactly what an eavesdropper observes, and state the problem she
      must solve to obtain k. Why does a large n defeat her when brute force
      and discrete logarithms are her only options?
  (c) Mallory sits between Alice and Bob and can intercept, alter and inject
      messages. Give the message-level trace by which both Alice and Bob
      complete "successful" exchanges yet all traffic is readable by
      Mallory. How many keys exist at the end, and who holds which?
  (d) State precisely how TLS closes this hole, and which artefact from
      ch. 57 the fix depends on. What must additionally be true on the
      *client* side for the fix to mean anything?

**B4. Zero-copy, counted.**
A server responds to each request by sending a 4 KB cached file over the
network. The traditional path is `read()` into a user buffer, then `write()`
on the socket. The pipeline is: DMA disk→page cache (already done — the file
is cached); `read()` copies page cache→user buffer; `write()` copies user
buffer→socket buffer; DMA socket buffer→NIC.

  (a) For one request, count the CPU (memcpy-style) copies and the
      user/kernel boundary crossings on the traditional path, and then on a
      zero-copy path (`sendfile()`-style: one system call, kernel hands the
      page-cache pages to the NIC by reference).
  (b) At 100,000 requests/s, with memcpy bandwidth 10 GB/s and a boundary
      crossing costing 150 ns: how much CPU time per second does each path
      spend on copies and crossings? Express the saving as a fraction of one
      core.
  (c) The service now moves to HTTPS. Explain what per-connection TLS
      encryption does to the zero-copy argument, and why. What class of
      hardware restores it?

**B5. Kerberos: why the ticket-granting ticket.**
Ch. 57 names Kerberos as an online authentication server but leaves the
mechanism to the paper; this question draws on Neuman & Ts'o. A user logs in
once in the morning, then contacts a dozen services through the day via the
Kerberos KDC (its authentication server plus ticket-granting server, TGS).
  (a) At login the user proves knowledge of their long-term, password-derived
      key exactly once and receives a **ticket-granting ticket (TGT)**. What
      does the TGT buy — why need the long-term key never be used again that
      day, and what does the user present to reach the *n*-th service instead?
  (b) Every ticket the KDC issues carries a fresh **session key**, and the
      ticket body is sealed under the key of the service it is for. Say who can
      read (i) the session key and (ii) the ticket body, and why the user
      cannot forge a ticket for a service even though they carry it across the
      network.
  (c) A Kerberos **authenticator** bundles the current time and a checksum
      (among other fields), encrypted under the session key. Say what the
      checksum proves and what replay the timestamp defeats that a bare ticket
      would allow — and what infrastructure assumption a timestamp (rather than
      a server-issued nonce) imposes.
  (d) The KDC holds every principal's long-term key. State the two distinct
      consequences: what fails if the KDC is **unreachable**, and what an
      attacker gains if the KDC is **compromised**.

---

## C. Discussion and design critique

**C1.** Ch. 57 offers two machineries for learning whom you are talking to:
certificates signed by an offline CA, and an online authentication server in
the style of Kerberos. Compare them on: what must be trusted and for how
long; what happens when a key must be **revoked today**; and what happens
when the trusted party is **unreachable**. Give one deployment where each is
clearly the right choice, and say what property of the deployment decides it.

**C2.** Appendix B holds that **transparency** is a major goal of a VMM —
"the OS above has little clue that it is not actually controlling the
hardware" — and presents the VMM as achieving it. **Argue the strongest case
against this claim**: that full transparency is neither actually achieved by
the systems the appendix describes, nor even the right goal. Draw your
evidence from the appendix itself — the information gap and its costs, what
Disco's authors did to IRIX, what para-virtualization concedes, and where
the performance of §B2 shows through the illusion. Then — since a strong case
must survive its rebuttal — state the conditions under which transparency
nonetheless remains exactly the right design goal, and give the modern
deployment fact that shows the market agreeing with *both* positions at once.
