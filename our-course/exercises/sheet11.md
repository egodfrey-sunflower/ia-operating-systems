# Examples Sheet 11 — Virtualization & kernel architecture

**Attempt after Week 16.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet11-answers.md` (spoilers — attempt first).

*All extension material. Covers: trap-and-emulate and ISA
virtualizability; shadow vs nested paging; containers vs VMs (namespaces,
cgroups, overlayfs); microkernels vs monolithic kernels and IPC cost; seL4
capabilities vs ACLs. Ends with a synthesis essay.*

Reading: OSTEP appendix on virtual machine monitors; reading-list papers 24
(Barham et al., Xen), 25 (Soltesz et al., containers), 28 (Liedtke, µ-kernel
construction), 29 (Klein et al., seL4), 30 (Engler et al., exokernel), and
paper 27 (Agesen et al., "The Evolution of an x86 VMM") for the
hardware-virtualization core — VT-x/AMD-V (§B) and nested / two-dimensional
paging, EPT/NPT (§C) — which the OSTEP appendix and the 2003 Xen paper both
predate; **A&D §10.2 (Virtual Machines)** gives the shadow-page-table mechanism
§C(a) builds on. This sheet pairs with **Lab 7 (containers from scratch)** — the container questions
refer to it, including the user-namespace pitfalls its handout documents.

---

## A. Warm-ups (true / false, with a one-line justification)

**A1.** To run a guest OS, a hypervisor must let the guest kernel execute in the
CPU's most-privileged mode.

**A2.** Each container runs its own copy of the operating-system kernel.

**A3.** seL4's formal proof means it can never be exploited.

**A4.** On classic (pre-VT) x86, any instruction that reads or writes
privileged state also causes a trap when executed in user mode.

---

## B. Trap-and-emulate and ISA virtualizability

**(a)** State the Popek & Goldberg condition for an ISA to be efficiently
virtualizable by **trap-and-emulate**, in terms of *sensitive* and *privileged*
instructions. Explain what "trap-and-emulate" means: what runs the guest kernel,
at what privilege level, and what happens when it executes a sensitive
instruction.

**(b)** Classic 32-bit x86 was **not** classically virtualizable. Give two
concrete examples of instructions that are **sensitive but not privileged** (they
read or depend on privileged state yet execute silently in user mode instead of
trapping), and explain why each breaks pure trap-and-emulate.

**(c)** Two pre-hardware-support answers were **binary translation** (VMware) and
**paravirtualization** (Xen, reading-list paper 24). Explain the essential idea
of each and their main cost: what does binary translation rewrite, and what does
paravirtualization change about the guest? Why is paravirtualization impossible
for an unmodifiable guest OS?

**(d)** Hardware virtualization (Intel VT-x / AMD-V) added a new **guest/root
mode** distinction. Explain how this restores clean trap-and-emulate — where the
guest kernel now runs, and how a sensitive operation transfers control to the
VMM — and why this made the Popek-Goldberg gap on x86 disappear.

---

## C. Shadow vs nested paging (translation cost)

A guest runs under a hypervisor. The guest maintains its own page tables
(gVA → gPA); the hypervisor must ultimately produce host physical addresses
(gPA → hPA). Assume a hardware page-table walk and a TLB miss.

**(a)** With **shadow paging** the VMM builds a *shadow* page table mapping
gVA → hPA **directly**. How many memory accesses does a TLB miss cost on a
4-level scheme, and why? What is the price the VMM pays elsewhere — i.e. what
must it trap and do every time the **guest** edits its own page tables, and why
is `fork`-heavy or page-table-churning guest code its worst case?

**(b)** With **nested (two-dimensional) paging** (EPT / NPT) the hardware walks
*both* the guest table and the host table. For a guest with **G** levels and a
host with **H** levels, the number of memory accesses for one TLB-miss walk is

```
  (G + 1) × (H + 1) − 1
```

Compute it for **(G,H) = (2,2)**, **(3,3)**, and **(4,4)**. Explain *why* the
walk is two-dimensional — i.e. why each of the guest page-table pointers must
itself be translated through the host table.

**(c)** Contrast the two on the two axes that matter: **TLB-miss cost** and
**page-table-update cost**. Which does modern hardware (EPT/NPT) prefer and why,
and what does this ask of the TLB (hint: tagging, larger reach)? When can shadow
paging still win?

**(d)** A guest process suffers a TLB miss on a 4-level/4-level nested system.
Using your (b) number, and assuming each memory access is 100 ns with **no**
intermediate caching, how long is the walk? Give one hardware mechanism that
keeps this from being the common case.

---

## D. Containers versus virtual machines (Lab 7)

In **Lab 7** you built `mycontainer` from namespaces, cgroups and `pivot_root`.

**(a)** Match each container property to the **kernel mechanism** that provides
it: (i) *isolation* (own PID space, mounts, hostname, network), (ii) *resource
control* (memory/CPU caps), (iii) *root-filesystem image*. Name the specific
mechanism for each (namespaces / cgroups v2 / `pivot_root` + overlayfs) and, for
overlayfs, what Docker gets that Lab 7's flat tarball does not.

**(b)** A VM virtualizes the **hardware** (each guest has its own kernel); a
container virtualizes the **OS interface** (all containers share one host
kernel). List two things a container therefore **cannot** isolate that a VM can,
and explain the security consequence of a shared kernel (attack surface; a
single kernel bug).

**(c)** The Lab 7 handout is emphatic that `mycontainer` is *not a security
boundary*. Using its documented pitfalls, explain **two** of the following
precisely:
  (i) the **`setgroups`/`gid_map`** rule — why the kernel forbids writing
  `gid_map` until `setgroups` is denied, and the escalation it prevents;
  (ii) the **unmapped-inode** rule — why `CAP_DAC_OVERRIDE` is void against a
  file whose owner has no mapping in the user namespace, and whose uid must be
  mapped for `mkdir(put_old)` to succeed;
  (iii) why `pivot_root` is used instead of `chroot`, and what `put_old` is for.

**(d)** Name two isolation/hardening mechanisms real runtimes (Docker/runc) add
on top of namespaces+cgroups that Lab 7 deliberately omits, and say what each
defends against (hint: capability dropping, seccomp, LSMs). Reading-list paper
25 argues the *case for* container-based virtualization — give its main
efficiency argument over full VMs.

---

## E. Microkernel vs monolithic; L4 and seL4

**(a)** Define a **monolithic** kernel and a **microkernel**. In a microkernel,
where do the file system, device drivers and paging *policy* live, and what is
the kernel's minimal job (address spaces, threads, IPC)?

**(b)** First-generation microkernels (Mach) were **slow**. Liedtke's "On
µ-Kernel Construction" (paper 28) argues the slowness was **not fundamental**.
Explain the **IPC-cost argument**: why does putting subsystems in separate
address spaces turn former function calls into IPC, and why does IPC performance
therefore determine whether the whole design is viable? What did L4 do to make
IPC fast (small kernel, register-based message transfer, cache/TLB-conscious
design)?

**(c)** **seL4** (paper 29) is a formally *verified* microkernel. State what the
proof does and does **not** cover (functional correctness against a spec vs the
assumptions: hardware model, the small amount of assembly, spec correctness).
Why is a *small* kernel a precondition for such a proof?

**(d)** *(Callback to Sheet 1.)* seL4 mediates all authority through
**capabilities**. Contrast a capability with an **ACL**: where the authority is
named (an unforgeable reference held by the subject vs a list attached to the
object), how *ambient authority* leads to the **confused-deputy** problem, and
how each handles **revocation**. Why do capabilities suit a verified
microkernel's goal of least privilege?

---

## F. Synthesis essay

**F1.** *"A microkernel is just a hypervisor with better marketing."* Discuss.

Write ~1.5 pages. A good answer should engage with: what each actually
multiplexes and isolates (OS services in separate address spaces vs whole guest
OSes); the interface each exposes (IPC + capabilities vs a virtual
hardware/hypercall interface); where the performance-critical path is (IPC vs
the 2-D page walk / world switch); how each draws the trusted-computing-base
line (seL4's verified core vs a VMM's TCB); and the cases where the analogy
genuinely holds (a microkernel *can* host paravirtualized guests; L4 has run
Linux as a user process — L⁴Linux) versus where it breaks (containers,
exokernels, and the different threat models). Reference papers 24, 28, 29 and,
for the "other answer", 30 (exokernel).

---

## Past paper questions

This sheet is entirely extension material with **no IA Tripos equivalent** —
virtualization, containers and microkernels are not on the Part IA syllabus, so
this directory's `README.md` allocates no past-paper set here. The two sealed
y2026 papers are reserved for week 17 per `../exams/README.md` — `y2026p2q4`
as the timed standalone mock, `y2026p2q3` inside the final itself — so open
neither early. Use the essay F1 as open-ended revision that ties weeks 15–16
back to the protection material of Sheet 1.
