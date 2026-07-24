> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 26 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes. Numeric results are stated with their working;
> for open questions the notes flag what a supervisor wants to see.

---

## A. Warm-ups

**A1. FALSE.** Containers **share the host kernel** — that is both their
efficiency (no guest OS to boot or hold in memory) and the security worry
this whole week orbits: one kernel bug is every tenant's problem.

**A2. FALSE.** That is **cgroups**. Namespaces govern *visibility* — which
PIDs, mounts, network devices, users a process can even name; cgroups govern
*consumption* — CPU, memory, PIDs, I/O. A container needs both precisely
because they answer different questions (B4c).

**A3. FALSE.** gVisor's mechanism is a **user-space kernel** (the Sentry)
that intercepts the container's system calls and services them itself,
exposing only a narrow residue to the host kernel. (One of its interception
platforms borrows KVM machinery, but the isolation story is syscall
interposition, not a hardware VM per container.)

**A4. FALSE.** Firecracker is a **virtual machine monitor** — KVM-based,
with a deliberately minimal device model — that runs microVMs, each with its
own guest kernel. Container and FaaS platforms may sit *on top of* it, which
is exactly its intended role.

**A5. FALSE.** The kernel has no "container" object. A container is a
*convention*: a bundle of namespaces (`clone()`/`unshare()` flags), cgroup
memberships, a changed root, dropped capabilities and a seccomp filter,
assembled by user-space runtimes — Kerrisk's series builds one primitive at
a time, as does Lab 11.

**A6. FALSE.** A kernel module is arbitrary code at full kernel privilege.
eBPF programs pass a **verifier** first — provably bounded execution,
checked memory access, a restricted helper API — and run in a constrained
in-kernel machine. The restriction is the point: safety by construction,
not by trust.

**A7. TRUE.** The microVM guest reaches its host only through a handful of
virtio devices and low-level VM exits; a container reaches its kernel
through the entire system-call API — hundreds of entry points into
millions of lines of shared kernel. Counting those interfaces is the
Firecracker argument in one sentence.

**A8. FALSE.** The trade-off is real but not proportional — that is
Firecracker's thesis. By deleting everything a serverless guest doesn't
need (BIOS, PCI, legacy devices), it keeps hardware-VM isolation while
landing within a few percent of container density and within ~3× of
container start-up (B2). Isolation prices are design-dependent, not fixed.

---

## B. The design space, worked

**B1.** Model table (wording may vary; the structure should not):

| | (a) boundary + layer | (b) still shared | (c) attack interface | (d) start-up | (e) overhead |
|---|---|---|---|---|---|
| 1. process | user/kernel split + address space; host kernel | everything: kernel, FS, namespaces | full syscall API | ~ms | ~0 (page tables, kernel state) |
| 2. container | namespaces + cgroups + seccomp/caps; host kernel | the **host kernel** itself | syscall API, possibly seccomp-filtered | ~10s of ms | ~MBs |
| 3. sandboxed container | user-space kernel interposing on syscalls; user space (+ small host residue) | host kernel, but only via the Sentry's narrow residue | 211 of 319 Linux calls *to the Sentry*; a thin ~55-call set to the host | ~100 ms | ~15 MB (Sentry, by design) |
| 4. microVM | hardware virtualization via minimal VMM (KVM); hardware + VMM | host hardware, KVM, the tiny device model | virtio devices + VM exits | ~100 ms | ~MBs–10s of MB (guest kernel, minimal) |
| 5. full VM | hardware virtualization, full device model; hardware + VMM | host hardware, VMM, emulated platform | full emulated platform (BIOS, PCI, many devices) | ~10 s | ~GB (full guest OS) |
| 6. unikernel | hardware virtualization; hypervisor | host hardware + hypervisor | hypercalls + whatever devices the image drives | ~10 ms | ~MBs (library OS only) |

Justifications for (d): a process/container is a `clone()` plus setup — no
kernel to boot; sandbox and microVM must start a Sentry / boot a minimal
guest kernel — order 100 ms; a full VM boots firmware + full OS — order
10 s; a unikernel boots a single linked image — order 10 ms.

**The largest jump per unit of overhead is between rows 2 and 4**: crossing
it swaps "shared host kernel, full syscall surface" for "hardware boundary,
virtio-sized surface" at a cost of only ~100 ms and a few MB. That gap —
container convenience with VM isolation — is precisely the target both
week-26 systems aim at: Firecracker attacks it from the VM side (shrink the
VM until it is container-cheap), gVisor from the container side (shrink the
kernel exposure until it is VM-narrow).

**B2.**
**(a)** Per instance: full VM 128 + 896 = 1024 MB; microVM 128 + 8 =
136 MB; container 128 + 2 = 130 MB. From 192 GB (196,608 MB):

```
full VM:   196,608 / 1024 = 192 instances      (12.7% of container)
microVM:   196,608 / 136  ≈ 1,445 instances    (95.6%)
container: 196,608 / 130  ≈ 1,512 instances    (100%)
```

**(b)** Within the 250 ms budget: container (50 ms) and microVM (150 ms)
can boot **on demand**; the full VM (10 s) cannot and needs a warm pool.
500 warm VMs × 1,024 MB = **512,000 MB = 500 GB ≈ 260% of the host's RAM**
— the warm pool for one host's spike capacity costs more than the host.
That absurdity, not the 10 s itself, is what kills full VMs for FaaS.
**(c)** 600 instances ÷ 150/s = **4 s** until the last creation begins; its
start latency is ~4 s of queueing + 150 ms of boot, and the 250 ms budget
is destroyed for almost the whole burst: with 150 ms of boot, an instance
can afford only ~100 ms in the queue, so only the first 150/s × 0.1 s =
**15 instances** make the budget. Once boot time is
solved, the bottleneck moves to **creation throughput and the control
plane** — scheduling, image distribution, network setup — which is where
real serverless engineering lives.
**(d)** Against the full VM: ~7.5× the density and on-demand cold starts
versus an unaffordable warm pool. Against the container: ~4–5% density
sacrificed and ~3× the boot time, in exchange for replacing the shared
syscall interface with a hardware boundary plus a virtio-sized surface —
which is the paper's whole case.

**B3.**
**(a)**
- *W1:* 500 × (5 − 0.5) µs = 2.25 ms of extra CPU per second ≈ **0.2%** —
  **negligible**.
- *W2:* 80,000 × 4.5 µs = 360 ms/s ≈ **36% of a core**, plus added tail
  latency on every request — **material**.
- *W3:* the job needs 500 MB/s; the sandboxed ceiling is 750 MB/s. It
  *runs*, but at 67% of the sandbox's I/O ceiling (versus 25% natively),
  with no headroom for growth and heavy CPU spent on interposed I/O —
  **material, and disqualifying at any higher target**.
**(b)** The Sentry stands on the **syscall path**: every kernel crossing
becomes an interception, a user-space emulation, and often a further
(filtered) host call — so cost accrues per *crossing*. Instructions that
never cross — pure computation — are executed natively and pay nothing.
Cost therefore tracks syscall rate, not work done.
**(c)** The paper itself stops at the cost verdict; the condition follows from
its measurements rather than being stated in it. Interposition is worth it when
the code is **untrusted and its profile is compute-dominated** — the isolation gain is
bought with overhead the workload barely exercises. Avoid it for
syscall- and I/O-intensive services (W2, W3), where the toll lands
precisely on the hot path — and where a microVM, whose tax is at boot
rather than per syscall, fits better.

**B4.**
**(a)** `--memory=256m` → **memory cgroup controller**; `--pids-limit=100`
→ **pids cgroup controller**; isolated process tree → **PID namespace**;
private `/` → **mount namespace** + `pivot_root`; no network → **network
namespace** (fresh, loopback only); apparent root ≠ host root → **user
namespace** with a UID/GID mapping.
**(b)** Parent `clone()`s with the new-namespace flags (user, PID, mount,
net, plus UTS/IPC) → parent writes the child's **UID/GID maps** and places
the child in the **cgroups** → child (or parent) mounts the new root
filesystem, `pivot_root`s into it and unmounts the old root → child drops
capabilities and installs its **seccomp** filter → `exec()` the workload.
Cgroup attachment and filesystem surgery must precede the privilege drop
and the `exec`: mounting and `pivot_root` require capabilities the final
workload must not hold, and limits must bind **before** the first tenant
instruction runs, or the tenant races them.
**(c)** *Resources:* the memory and pids limits (cgroups). *Visibility:*
PID, mount, network and user namespaces. The split mirrors ch. 53's
insistence that distinct security goals get distinct mechanisms: namespaces
serve confidentiality/containment (what can even be *named* — fail-safe
defaults: an empty namespace denies by construction), cgroups serve
availability (no tenant starves the rest). One mechanism per goal is
economy of mechanism in practice.
**(d)** **seccomp filters** — shrink the reachable syscall set to an
allowlist; cost: writing and maintaining the policy, and breakage when
legitimate code needs an unusual syscall. **gVisor-style interposition** —
reduce host-kernel exposure to the Sentry's small residue; cost: B3's
per-syscall toll. (Both appear in this week's reading; Firecracker's jailer
uses the first, sandboxed container runtimes are the second.)

---

## C. Discussion and design critique

**C1.** The argument for stacking is ch. 53 applied to hostile tenancy.
**No single mediator is trustworthy enough:** each boundary — KVM, the
VMM's device code, seccomp, cgroups, a chroot — is software with its own
bug rate, and the layers are *independent codebases*: an attacker who
escapes the guest through a virtio bug lands not on the host but inside a
process that can make only a few dozen syscalls, owns no filesystem, and is
resource-capped. Compromise now requires **a chain of unrelated
vulnerabilities**, which multiplies attacker cost rather than adding to it.
This is least privilege (each layer grants the next only what it needs) and
separation of privilege (two independent mechanisms must both fail) — both
Saltzer–Schroeder principles from ch. 53 — applied as defense in depth (a
later idea, not one of their eight). The cost taken too
far: each layer adds code (economy of mechanism cuts the other way — more
mechanism is more bugs), latency, and operational opacity (five layers of
"permission denied" to debug). A workable stopping rule: add a layer only
if it is enforced by a mechanism *independent* of the layers it backs up,
and its failure would otherwise be a single point; stop when a proposed
layer merely re-checks what an existing independent layer already enforces,
or when its steady-state cost lands on the hot path rather than at setup.

**C2.** A strong answer works the claims one at a time and lets the
numbers and the papers judge.

**Isolation.** The unikernel's isolation *is* the hypervisor's — the same
hardware boundary the microVM uses. So the claim "VM-grade isolation" is
true, and precisely for that reason it is **not an improvement over the
microVM**: both stand on the hypervisor. Inside the boundary the unikernel
is actually *weaker*: a single address space with no user/kernel split
means the function's own bugs — a buffer overflow in a parser — own the
entire image, including its network stack and its keys. The microVM keeps
an internal OS boundary as a second line; the unikernel deletes it.

**Arbitrary customer code.** A FaaS platform's contract is "bring any
code". A library OS demands that every function — every language runtime,
native extension, glibc-assuming binary — be **compiled and linked against
it**. That silently narrows "arbitrary code" to "code our toolchain can
rebuild", pushes a build-and-compatibility matrix onto either the platform
or the customer, and breaks the ecosystem of prebuilt images. The microVM
sidesteps all of it by booting a Linux guest: existing binaries just run.

**Operability.** No shell, no `ps`, no debugger inside the image: fleet
operations (incident response, profiling, security forensics) lose their
tools, and every diagnostic capability must be reinvented inside the
library OS. This is B1's row-6 attack surface being small partly because
the *operator's* surface vanished too.

**Density and boot.** From §B2's magnitudes, the microVM already boots in
~150 ms with ~8 MB overhead — inside the platform's cold-start budget, at
~96% of container density. The unikernel's ~10 ms and ~MBs are *better*,
but they improve numbers that no longer bind: once boot fits the budget,
B2(c) showed the bottleneck moves to creation throughput and the control
plane, which unikernels do nothing for. The remaining advantage is real
but marginal, and it is bought with the compatibility and operability
costs above.

**The market evidence.** The serverless provider in this week's reading —
with maximal freedom and maximal incentive — evaluated this exact space
and built **Firecracker**: a minimal VMM booting *unmodified Linux
guests*. That is a revealed preference: when "run arbitrary code" is in
the contract, the winning design keeps the guest general-purpose and
minimises everything else.

**Where unikernels win.** A *closed-world* appliance: the operator owns
all the code, the workload is fixed and specialised (network functions,
an embedded gateway, a single-protocol server), rebuild-from-source is
routine, and per-instance footprint at extreme density or extreme
cold-start sensitivity dominates. There, the compatibility cost is zero
by construction and the numbers are pure win.

**Recommendation.** Reject for a general FaaS platform: the isolation
gain over microVMs is nil at the boundary and negative inside it, and the
contract of arbitrary code is broken by the toolchain requirement. Adopt
microVMs for the shared-kernel worry; revisit unikernels for first-party,
fixed-function services where the platform controls the whole build. The
verdict flips if the platform's workload stops being arbitrary — a
curated set of runtimes the provider compiles itself — or if control-plane
engineering someday makes sub-10 ms cold starts the binding constraint.

*Marking note: the pivotal move is spotting that "VM-grade isolation" is
the hypervisor's property, shared with the microVM — so the proposal's
headline gain is zero where it claims most. Second-tier answers recite
pros and cons; top answers also read Firecracker's existence as evidence,
and notice the internal-boundary regression (single address space) that
the proposal quietly celebrates as "no kernel inside".*
