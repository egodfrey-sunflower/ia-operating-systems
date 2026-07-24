# Week 25 — Virtual machine monitors, and distributed system security

> **Part V: Security (ch. 57) + Appendices A–B.** Week 25 of 27. Ch. 57 is
> taught here rather than with ch. 52–56 (weeks 19–20) because it depends on
> ch. 48, taught in week 23.

## What you'll learn

Two threads this week, both about trusting a layer you do not control.

Appendix B is OSTEP's treatment of **virtual machine monitors** — filed by
the book as a 13-page appendix, given a full week here because weeks 25–26
together form this course's virtualization block. The VMM is "an operating
system for operating systems": it extends **limited direct execution** (ch. 6)
one level down. The guest OS is deprivileged — run in a less-trusted mode —
so every privileged thing it tries (installing trap handlers, updating the
TLB, returning from a trap) itself **traps into the VMM**, which emulates the
operation and sustains the illusion. Memory gains a third name-space: the
guest maps virtual→"physical", the VMM maps physical→**machine**, and on
hardware-walked page tables the VMM maintains **shadow page tables** mapping
guest-virtual straight to machine frames so the MMU never notices. The
appendix closes with the **information gap** — the VMM cannot see intent
(idle loops scheduled as real work, pages zeroed twice), and the two escape
hatches: inference, or **para-virtualization**, where the guest is modified
to cooperate.

Chapter 57 asks what security means when the OS controls only one machine of
many: you cannot mediate a remote machine's behaviour, so everything rests on
**cryptography** over an untrusted network. Its spine: passwords authenticate
many parties to one; **public keys** authenticate one party to many;
**certificates** solve public-key distribution by having a trusted authority
sign the binding (bootstrapped, ultimately, by keys shipped in your
browser); **Diffie–Hellman** builds a shared secret over an open channel but
authenticates nobody, which is why TLS signs the exchange with the
certificate's key; and expensive public-key operations bootstrap a cheap
per-connection **symmetric** key. **SSL/TLS** is the worked example, with
Kerberos as the online-authentication-server alternative. The chapter's
refrain — use standard, existing implementations; never roll your own
crypto — is itself examinable judgement.

Fair warning: **Appendix B leans on ch. 6 and ch. 18–20, which you read in
weeks 3 and 8–9** — sixteen weeks ago. It is not readable standalone. The
refresher opening the reading section below exists for exactly this reason;
do not skip it.

**Key ideas:** deprivileging and trap-and-emulate · machine switch · guest
virtual / guest physical / machine memory · shadow page tables · the
information gap · para-virtualization · authentication across a network ·
certificates and CAs · Diffie–Hellman and its MITM hole · TLS handshake ·
symmetric bootstrap · Kerberos · zero-copy I/O.

## Read

### First, the refresher (30 minutes, weeks 3 and 8–9 material)

Appendix B assumes the following is at your fingertips. Re-derive each item —
on paper — before starting it; if any feels shaky, re-skim the named chapter.

- **Trap mechanics (ch. 6).** A system call is a *trap instruction*: the
  hardware switches user→kernel mode and jumps through the **trap table**
  the OS installed, privileged-ly, at boot; return-from-trap
  (a privileged instruction) drops privilege and resumes the user PC.
  A context switch saves and restores register state on a mode transition
  the OS did not request (timer interrupt). Everything a VMM does is this
  machinery, applied one layer down: *who installed the trap table, and
  what mode is the OS itself in?*
- **Page-table translation (ch. 18–20).** A virtual address splits into
  VPN + offset; the page table (per process, multi-level in practice) maps
  VPN→PFN; the **TLB** caches translations, and a miss is serviced either
  by hardware walking the table or by the OS's TLB-miss handler
  (software-managed, as in App. B's MIPS setting) — which installs the
  entry with a *privileged* instruction. Hold on to that word: the trap it
  causes under a VMM is the hinge of the whole appendix.
- The composed question App. B answers: what happens when the *translator
  itself* runs translated — when the OS's "physical" addresses are one more
  layer of virtual?

### Then the reading

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP App. A** | A Dialogue on Virtual Machine Monitors | 1 | 0.1 h |
| 2 | **OSTEP App. B** | Virtual Machine Monitors | 13 | 1.9 h |
| 3 | **OSTEP ch. 57** | Distributed System Security | 18 | 2.6 h |
| 4 | **OSPP §10.1** | Zero-copy I/O | ~5 | 0.7 h |

Read in that order: App. B while the refresher is warm; ch. 57 next (it
picks up ch. 48's sockets and ch. 56's crypto); OSPP §10.1 last — it is the
week's systems-performance palate cleanser, and sheet 25 §B4 drills it.

**Paper (required):** Neuman & Ts'o (1994), *Kerberos: An Authentication
Service for Computer Networks*, IEEE Communications. OSTEP calls it "hugely
influential". Read it against ch. 57's certificate story: an **online**
authentication server versus an **offline** CA, and what that buys
(instant revocation) and costs (availability, symmetric-key trust).

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise25.md`](../exercises/exercise25.md) — budget 3 h. **Entirely original material**: ch. 57 and Appendices A–B ship no homework, simulators or code. Self-mark against [`../exercises/solutions/exercise25-solutions.md`](../exercises/solutions/exercise25-solutions.md) |
| **Project** | [`../labs/project/`](../labs/project/) — **continues**, 4.0 h this week. By the end of the week you should have a design sketch and a running skeleton |
| **Timed past paper** | `y2018p2q4` — page replacement: OPT, a practical policy with its overheads, Bélády's anomaly. 35 min closed book, then self-mark. Spaced retrieval; see week 23's note — and it is no accident the VM block re-arms exactly the material App. B leans on |

## Week load

```
OSTEP App. A + B    14pp ÷ 7  =  2.0 h
OSTEP ch. 57        18pp ÷ 7  =  2.6 h
OSPP §10.1           5pp ÷ 7  =  0.7 h
Neuman & Ts'o [M]             =  1.5 h
Exercise sheet 25             =  3.0 h
Timed paper (y2018p2q4)       =  1.0 h
Project                       =  4.0 h
                                ------
                                14.8 h   — over the 12-14 h band (labs are not
                                         trimmed to fit)
```

The refresher's half hour is inside the App. A/B reading allowance — it is
what makes 13pp of appendix readable at all. If the week runs long, trim
project hours before anything else.

## Notes for the curious

- **Why an appendix gets a week.** OSTEP files VMMs as Appendix B — 13pp, no
  homework — which alone would make virtualization a footnote in a course
  that follows the book. This course keeps OSTEP's *position* (after
  distribution, before the end) but gives the topic two weeks: this week is
  the appendix's classical mechanics — trap-and-emulate, shadow paging, the
  semantic gap; week 26 is the modern practice built on them, with no
  textbook at all. The appendix's own dialogue argues the case: "if you can
  understand how VMMs work, then you really understand virtualization quite
  well."
- App. B describes the software-only world of Disco (MIPS, 1997) and notes
  at the end that Intel and AMD later added **direct hardware support** for
  "an extra level of virtualization" — which in practice means nested/extended
  page tables and VT-x/AMD-V root modes doing in hardware what shadow tables
  and trap-bouncing did in software (the appendix names only the extra level;
  the specific mechanisms are the real-world realization). Week 26's
  Firecracker paper assumes that hardware world; understanding this week's
  software version is what makes the hardware version legible.
- Ch. 57 is the last of Peter Reiher's six security chapters, and the one
  that completes week 20's story: cryptographic capabilities reappear here
  as **web cookies** — a server-minted, unforgeable token standing in for
  re-authentication. Same access-matrix ideas, now with no shared kernel to
  enforce them.
- The Diffie–Hellman aside is presented symbolically (g, n, x, y — its only
  number is "600-digit primes"); sheet 25 §B3 turns it into small-number
  arithmetic you can and should reproduce by hand. The
  man-in-the-middle hole it leaves, and the certificate signature that
  closes it, is the single most transferable idea in the chapter.
- Zero-copy I/O (OSPP §10.1) is placed here with intent: a VMM interposing
  on guest I/O and a kernel copying file data to user space are the same
  story — indirection layers cost data movement, and the fix in both cases
  is arranging for the data not to move. It also completes unfinished
  business: `sendfile()`-style paths are how the week-15 I/O stack meets
  the week-23 network stack.
