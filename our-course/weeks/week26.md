# Week 26 — Modern virtualization: microVMs, containers, and kernel extensibility

> **Papers-only week — no textbook reading.** Week 26 of 27, the last teaching
> week.

## What you'll learn

This week has **no OSTEP chapters at all** — a deliberate departure from the
course's rhythm, and the only week built entirely from papers. The zero pages
of textbook are what pay for the heaviest remaining lab (containers, 4.0 h,
starting and ending this week) and a synthesis sheet that makes you place
every isolation technology you now know on one map. OSTEP's account of
virtualization ends where week 25 ended — trap-and-emulate, shadow paging,
Disco — and everything that industry actually deploys today sits past that
horizon. Four short modern readings carry the week.

**Firecracker** (Agache et al., NSDI 2020) is what AWS built to run Lambda:
a **microVM** monitor on KVM with a device model cut to almost nothing —
virtio net and block, a serial port, little else. The bet is that week 25's
hardware-supported VM isolation can be kept while boot time and per-instance
overhead shrink to near-container levels, so thousands of mutually-untrusting
tenants can share one host. Read it as a direct answer to Appendix B: same
mechanics, ruthlessly minimised attack surface and footprint.

**gVisor** (Young et al., HotCloud 2019) attacks the same fear — the shared
host kernel under every container — from above: a user-space kernel (the
Sentry) intercepts a container's system calls and serves them itself,
re-exposing only a narrow surface to the host. The paper measures the toll:
"the true cost of containing" is paid per syscall, so I/O- and
syscall-intensive workloads hurt while compute-bound ones barely notice.

**Kerrisk's LWN "namespaces in operation"** series is the clearest account
in print of what a container actually *is*: not a kernel object at all, but
a bundle of **namespaces** (what a process can *see*: PIDs, mounts, network,
users…) — which is the series' whole subject. The other half of the picture,
**cgroups** (what it may *consume*) plus assorted hardening, comes from the
Firecracker paper's jailer and from lab 11, not from Kerrisk.
Read parts 1–3 for the week; the rest of the series is the lab's reference.

**Fleming's LWN eBPF introduction** rounds out the map with kernel
*extensibility*: verified, resource-bounded programs loaded into a running
kernel — a third path between "make a syscall" and "load a module", and the
mechanism beneath much modern container networking and observability.

**Key ideas:** the VM/container/unikernel design space · isolation boundary
versus shared surface · microVMs and minimal device models · user-space
kernels and per-syscall cost · namespaces (visibility) vs cgroups
(consumption) · attack surface as a countable thing · density, cold-start
and boot time as first-class metrics · verified in-kernel extension.

## Read

| # | Source | What | Len | Time |
|---|--------|------|-----|------|
| 1 | **Agache et al., NSDI 2020** | *Firecracker: Lightweight Virtualization for Serverless Applications* | [M] | 1.5 h |
| 2 | **Young et al., HotCloud 2019** | *The True Cost of Containing: A gVisor Case Study* | [S] | 0.75 h |
| 3 | **Kerrisk, LWN** | *Namespaces in operation*, parts 1–3 (rest = lab reference) | [S] | 0.75 h |
| 4 | **Fleming, LWN 2017** | *A thorough introduction to eBPF* | [S] | 0.75 h |

Read Firecracker first and gVisor second — they are two answers to one
question and the sheet plays them against each other.

**No cross-reading, no textbook.** Week 25's Appendix B is this week's
theoretical substrate; have it within reach.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise26.md`](../exercises/exercise26.md) — budget 3 h. A **synthesis sheet**: VMs, microVMs, containers, sandboxed containers and unikernels compared on isolation strength, density, boot time and attack surface. Self-mark against [`../exercises/solutions/exercise26-solutions.md`](../exercises/solutions/exercise26-solutions.md) |
| **Lab** | [`../labs/lab11-containers/`](../labs/lab11-containers/) — **starts and ends this week**, 4.0 h. Namespaces, `pivot_root`, cgroups v2 limits. Runs unprivileged where user namespaces and cgroup delegation are available; `probe.sh` reports your tier first thing |
| **Project** | [`../labs/project/`](../labs/project/) — **continues**, 2.0 h. Implementation should be functional by week's end; week 27 is evaluation and write-up only |
| **Timed past paper** | `y2020p2q3` — five-level page-table walk, effective access time, and a partial-match TLB design part. 35 min closed book, then self-mark. The last timed paper of the course; see week 23's note on why the tail weeks drill memory |
| **Untimed drill** | `y2024p2q3` is fully attemptable as of this week: its closing part is a serverless/FaaS density-and-predictability critique, which is precisely this week's material. If you sat parts (a)–(b) in week 8, do part (c) now cold |

## Week load

```
Firecracker [M]               =  1.5 h
gVisor [S]                    =  0.75 h
Kerrisk parts 1-3 [S]         =  0.75 h
eBPF intro [S]                =  0.75 h
Exercise sheet 26             =  3.0 h
Timed paper (y2020p2q3)       =  1.0 h
Lab 11 (start ▸ end)          =  4.0 h
Project                       =  2.0 h
                                ------
                                13.75 h  ✅ within 12-14 h target
```

The papers are deliberately short-form: the week's depth lives in the lab
(where you build a container from raw primitives) and the sheet (where you
argue about what you built). If time runs short, trim project hours; the lab
cannot move — it is this week or never.

## Notes for the curious

- **Why a papers-only week exists.** OSTEP v1.10's virtualization coverage
  is Appendix B, whose systems predate the hardware support its last
  paragraph gestures at. Everything examinable about *modern* isolation —
  microVMs, container primitives, sandboxes, serverless density — postdates
  the book, so the course sources it from papers. This is also why the week
  can absorb the containers lab whole: no textbook pages compete with it.
- The four readings triangulate one question — *where should the isolation
  boundary live?* — with four answers: below the kernel (Firecracker),
  above it (gVisor), inside it (namespaces + cgroups + seccomp), and
  "dissolve the layers entirely" (unikernels, which the sheet introduces
  via a stem so the papers stay four). Manco et al.'s *My VM is Lighter
  (and Safer) than your Container* (SOSP 2017) and Madhavapeddy's original
  unikernels paper are the optional deep end.
- Firecracker repays a second read for its *engineering* choices: written
  in Rust (week 6's memory-safety thread, grown up), one process per
  microVM jailed by seccomp/cgroups — defense in depth, not a bet on any
  single boundary. Sheet 26 §C1 is about exactly that stacking.
- eBPF completes an arc that started in week 3: ch. 6 asked how the kernel
  can safely run *your* code on *its* privilege — the answer then was "it
  can't; trap in". eBPF is the modern middle answer: it can, if a verifier
  can prove the code bounded and memory-safe first. Compare Wahbe's SFI
  from week 6 — same idea, enforcement moved from binary rewriting to
  load-time verification.
