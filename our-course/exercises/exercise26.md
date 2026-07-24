# Exercise Sheet 26 — Synthesis: VMs, containers, unikernels

**Attempt after Week 26.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise26-solutions.md`](solutions/exercise26-solutions.md).

**This sheet leans on:** the four week-26 papers (Firecracker; the gVisor
case study; Kerrisk's namespaces series; Fleming's eBPF introduction) and
OSTEP App. B (week 25). It reaches back to ch. 6 (limited direct execution,
week 3) and ch. 53 (least privilege and the design principles, week 19).

> **Note.** This is a **synthesis sheet**: no chapter stands behind it, so
> every question is original, and several stems supply the numbers you need —
> use the given values, not remembered ones. Where a technology (the
> unikernel) was not in the four papers, the stem defines it.

No tooling is needed — pen and paper throughout. (Lab 11 is where you build
the real thing.)

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing.*

**A1.** Each container on a host runs its own kernel.

**A2.** Namespaces limit how much CPU time and memory a container may
consume.

**A3.** gVisor strengthens container isolation by running each container in
its own hardware virtual machine.

**A4.** Firecracker is best described as a container runtime.

**A5.** A "container" is a single kernel object that the `clone()` system
call creates.

**A6.** eBPF allows users to load arbitrary code into the kernel, so it is
equivalent to loading a kernel module.

**A7.** A microVM guest attacks its host through a narrower interface than a
container attacks its kernel.

**A8.** Stronger isolation necessarily means proportionally worse density
and boot time.

---

## B. The design space, worked

**B1. One map of everything.**
Construct the synthesis table. Rows: (1) plain process; (2) container
(namespaces + cgroups + seccomp); (3) sandboxed container (gVisor-style);
(4) microVM (Firecracker-style); (5) traditional full VM; (6) unikernel —
defined for this sheet as: the application linked directly against a library
OS into a single-address-space image that boots as a guest on a hypervisor,
with no user/kernel split inside.

Columns:
  (a) what enforces the isolation boundary (the mechanism, and which layer
      hosts it);
  (b) what the tenant still **shares** with everyone else on the host;
  (c) the interface the tenant can attack, roughly sized (full syscall API?
      a filtered subset? virtio devices + hypercalls?);
  (d) order-of-magnitude start-up time (ms / 100 ms / 10 s — justify each);
  (e) order-of-magnitude per-instance memory overhead beyond the
      application itself.

Then answer: which **two rows** are separated by the largest jump in
isolation per unit of overhead, and why is that exactly the gap Firecracker
and gVisor both aim at? (They need not be adjacent — indeed the technologies
that target the gap sit *inside* it.)

**B2. Density and cold starts, costed.**
A serverless host has **192 GB** of RAM available for tenant functions; each
function's application working set is **128 MB**. Assume these per-instance
overheads beyond the working set — full VM (guest kernel + userland):
**896 MB**; microVM: **8 MB**; container: **2 MB**. Assume start-up times:
full VM **10 s**, microVM **150 ms**, container **50 ms**, and a platform
target that a *cold-start* request must begin executing within **250 ms**.

  (a) Compute the maximum resident instances per host for each technology,
      and each one's density as a percentage of the container's.
  (b) Which technologies can create an instance *on demand* within the
      cold-start budget, and which force the platform to keep a warm pool?
      For the one(s) that need it, compute the RAM cost of keeping 500 warm
      instances, and what fraction of the host that consumes.
  (c) Assume a microVM monitor can create **150 instances per second** per
      host. A traffic spike demands 600 new instances on one host in one
      burst. How long until the last one starts, and does the cold-start
      budget survive for it? What does this say about where the bottleneck
      moves once boot time is solved?
  (d) Put (a)–(c) together in two sentences: state the quantitative case the
      microVM design makes against both of its neighbours in the table.

**B3. The syscall toll.**
A gVisor-style sandbox interposes a user-space kernel on every system call:
assume a native syscall costs **0.5 µs** end-to-end, and interposition
multiplies that to **5 µs**; assume file-read bandwidth through the sandbox
drops from **2 GB/s** to **0.75 GB/s**. Three tenant workloads:

- *W1:* compute-bound — 500 syscalls/s;
- *W2:* a web API server — 80,000 syscalls/s;
- *W3:* an ETL job streaming 500 MB/s from files.

  (a) For each workload, quantify the sandbox's cost (extra CPU-seconds per
      second for W1/W2; achievable throughput for W3), and give each a
      verdict: negligible / material / disqualifying.
  (b) Explain *structurally* — from where the Sentry sits — why the cost
      scales with syscall rate rather than with computation.
  (c) The gVisor paper's own conclusion is a blunt cost verdict — "the true
      costs of effectively containing are high: system calls are 2.2× slower".
      Reason past it: state the condition under which gVisor is nonetheless the
      right choice despite (a), and the workload profile that should avoid it.

**B4. A container from parts.**
A minimal runtime launches a tenant workload with:
`--memory=256m` · `--pids-limit=100` · an isolated process tree · its own
private `/` filesystem · no network · running as apparent root that is
**not** host root.

  (a) For each of the six requirements, name the primitive that implements
      it (which namespace, which cgroup controller, or which syscall), per
      Kerrisk's series and the lab.
  (b) Order the runtime's setup steps sensibly from `clone()` to `exec()`,
      and state which step must happen before dropping privileges, and why.
  (c) Two of the six requirements protect the *host's resources* and four
      protect *visibility*. Sort them, and explain why the
      visibility/consumption split (namespaces vs cgroups) mirrors ch. 53's
      separation of policy goals.
  (d) Even with all six in place, the tenant still talks to the shared host
      kernel through the full syscall interface. Name the two hardening
      layers from this week's reading that shrink that interface, and state
      what each one costs.

---

## C. Discussion and design critique

**C1.** Firecracker does not *choose* a single isolation boundary: each
microVM process is itself confined by seccomp filters, cgroups and a
chroot jail — a VMM wrapped in container clothing. gVisor, conversely, is
typically deployed *inside* a container. If each mechanism were sufficient,
stacking would be waste. Make the argument for stacking anyway — ground it
in ch. 53's principles and in what an attacker must now do — and then state
the cost of the argument taken too far, with a rule for where to stop.

**C2.** *An intrepid engineer proposes the following.* "Our
function-as-a-service platform runs tenant functions in containers, and the
security team loses sleep over the shared kernel. MicroVMs would help, but I
propose we leapfrog them: compile every function into a **unikernel**. Each
function becomes a single-address-space image linked against a library OS,
booting on the bare hypervisor in tens of milliseconds with a few megabytes
of overhead. We get VM-grade isolation from the hypervisor, better density
and boot times than microVMs — which still boot a whole Linux guest — and a
tiny attack surface, since there is no general-purpose kernel inside the
image at all. It is strictly better than both containers and microVMs, and
we should migrate the platform to it."

Evaluate this proposal. Address specifically: whether the isolation claim
actually exceeds the microVM's, and what enforces it in each case; what
happens to the platform's promise of running **arbitrary customer code**
(languages, runtimes, native dependencies) when every function must link
against a library OS; what operating a fleet loses when the guest has no
shell, no `ps`, and no user/kernel boundary to contain the function's own
bugs; whether the density and boot-time advantages survive the comparison
with the microVM numbers of §B2; and what evidence it is that the
serverless provider in this week's reading, free to choose anything,
built a microVM monitor instead. Identify the deployment where unikernels
*are* the right answer, and conclude with a recommendation and the
conditions that would change it.
