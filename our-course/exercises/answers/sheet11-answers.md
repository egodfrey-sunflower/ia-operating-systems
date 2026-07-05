# ⚠️ SPOILER — Examples Sheet 11 model answers ⚠️

> **STOP.** Full worked solutions and, for the essay, a marking-points list. Do
> the sheet closed-book first. The nested-paging walk counts were verified with
> Python; checks are noted inline.

---

## A. Warm-ups

**A1. False.** The guest kernel is **deprivileged** — it runs at a *lower*
privilege than the VMM (classic: guest ring 1 or, with VT-x, guest-mode ring 0
which is still below root mode). It only *believes* it is fully privileged; its
privileged operations trap to the VMM, which is the one that actually holds the
most-privileged mode.

**A2. False.** All containers on a host **share the single host kernel**. A
container is just a process (tree) the kernel isolates with namespaces and
limits with cgroups; there is no per-container kernel. (That sharing is exactly
what a VM does *not* do — and the source of containers' weaker isolation.)

**A3. False.** The proof shows the C implementation **refines a formal
specification** and that certain properties hold — but only *under assumptions*:
a correct hardware model, correctness of the small hand-written assembly and the
boot code, the compiler/translation-validation, and that the spec itself says
what you want. Bugs in those assumptions, or side channels outside the model,
remain possible.

**A4. False.** That is precisely what made pre-VT x86 non-virtualizable: there
were ~17 **sensitive but unprivileged** instructions (e.g. `POPF`, `SGDT`,
`SIDT`, `SMSW`, `PUSHF`) that read or depend on privileged state yet execute
**silently** in user mode instead of trapping — so a VMM cannot catch them by
trap-and-emulate.

---

## B. Trap-and-emulate and ISA virtualizability

**(a)** **Popek & Goldberg:** an ISA is (efficiently) virtualizable by
trap-and-emulate iff the set of **sensitive** instructions (those that read or
change privileged machine state, or whose behaviour depends on it) is a
**subset** of the **privileged** instructions (those that trap when executed
outside the most-privileged mode). Trap-and-emulate: the guest kernel runs
**deprivileged** (user mode) directly on the CPU for the common case; when it
executes a privileged/sensitive instruction the CPU **traps** into the VMM,
which **emulates** the instruction's intended effect against the guest's virtual
state and resumes the guest. Most guest instructions run natively; only the
sensitive minority trap.

**(b)** Two sensitive-but-**not**-privileged x86 instructions:
* **`POPF`** — pops flags including the interrupt-enable flag `IF`. In user mode
  it **silently ignores** the change to `IF` rather than trapping. A guest kernel
  doing `POPF` to enable/disable interrupts thus fails silently — the VMM never
  sees it, so the guest's virtual interrupt state desynchronises. No trap ⇒ no
  emulation.
* **`SGDT`/`SIDT`/`SMSW`** — store the (real) GDTR/IDTR/machine-status into a
  user register/memory with **no trap**. A guest reads the *host's* descriptor-
  table base instead of its virtual one, leaking host state and getting wrong
  answers. Again, no trap ⇒ the VMM cannot interpose.

Because these leak/modify privileged state without trapping, pure
trap-and-emulate cannot virtualize them — the sensitive set is *not* a subset of
the privileged set.

**(c)** **Binary translation (VMware):** the VMM scans and **rewrites the
guest's kernel instruction stream** on the fly, replacing the offending
sensitive instructions (and control flow) with safe sequences or calls into the
VMM, caching translated blocks. Cost: translation overhead and complexity, and
managing the translation cache. **Paravirtualization (Xen, paper 24):** the
guest OS is **modified** to replace sensitive operations with explicit
**hypercalls** into the VMM (and to use VMM-friendly interfaces for memory,
interrupts, I/O). Cost: you must **change the guest kernel source**, so it is
**impossible for an unmodifiable guest** (a closed OS you cannot patch) — which
is precisely why hardware support (d) was needed for commodity guests.

**(d)** VT-x/AMD-V add orthogonal **root** (VMM) and **non-root** (guest) modes,
each with its own full ring 0–3. The guest kernel now runs in **non-root ring 0**
— it thinks it is fully privileged — while the VMM runs in **root mode**.
Configurable conditions (sensitive instructions, certain events) cause a **VM
exit** that transfers control to the VMM with the guest state saved in a VMCS;
the VMM handles it and does a **VM entry** to resume. This makes *every*
sensitive operation interposable regardless of the old privileged/sensitive
mismatch, so the Popek–Goldberg gap on x86 disappears — clean trap-and-emulate
is restored in hardware.

---

## C. Shadow vs nested paging

*Walk counts verified in Python:* `(G+1)(H+1)−1` = 8, 15, 24 for (2,2),(3,3),
(4,4).

**(a) Shadow paging.** A TLB miss costs a **normal 4-level walk = 4 memory
accesses**, because the shadow table maps gVA → hPA directly, like an ordinary
(non-virtualized) page table. The price is on **updates**: the VMM must keep the
shadow consistent with the guest's real tables, so it **write-protects the
guest's page tables** and **traps every guest edit** (and page fault), then
propagates the change into the shadow. Guest code that churns page tables — a
`fork`-heavy workload (COW makes many PTE changes), context switches that swap
`CR3`, mmap/munmap storms — causes a storm of these traps, which is shadow
paging's worst case.

**(b) Nested paging.** Walk = `(G+1)(H+1) − 1` memory accesses:

| (G, H) | accesses |
|--------|---------:|
| (2, 2) | **8** |
| (3, 3) | **15** |
| (4, 4) | **24** |

The walk is **two-dimensional** because the guest page-table pointers are
**guest-physical** addresses, not host-physical: the hardware cannot dereference
a gPA directly. So at *each* of the guest's G levels, the gPA of the next guest
table must itself be translated through the **host's H-level** table before it
can be read — and the final gPA (the guest page frame) too. That nests an H-level
host walk inside each step of the G-level guest walk, giving the product.

**(c)**

| Axis | Shadow | Nested (EPT/NPT) |
|------|--------|------------------|
| TLB-miss cost | cheap (4 accesses) | expensive (up to 24) |
| PT-update cost | expensive (trap every guest edit) | free (no traps; guest edits its own tables) |

Modern hardware **prefers nested paging** because eliminating the update traps
matters more for real workloads than the (rarer, TLB-amortised) miss cost, and
it removes a large, subtle piece of VMM software. It asks a lot of the **TLB**:
entries are **tagged** (VMID/ASID) so the TLB survives world switches without
flushing, and the CPU adds **page-walk caches** for intermediate nested entries
to cut the 24 down in practice; big pages (2 MiB/1 GiB) increase reach. **Shadow
can still win** for guests that fault/rewrite page tables rarely but suffer many
TLB misses (the 24-access walk hurts), or on hardware without page-walk caches.

**(d)** 24 accesses × 100 ns = **2400 ns = 2.4 µs** for one uncached TLB-miss
walk — catastrophic if common. It is kept rare by (i) a **large, VMID-tagged
TLB** that avoids flushes on VM entry/exit so most translations hit, and (ii)
**page-walk caches** (nested/paging-structure caches) that hold recently used
intermediate entries so the host sub-walks are short-circuited — the full 24 is
close to a worst case, not the norm.

---

## D. Containers vs VMs (Lab 7)

**(a)**
* (i) **isolation** → **namespaces** (`CLONE_NEWPID`/`NEWNS`/`NEWUTS`/`NEWNET`/
  `NEWIPC`/`NEWUSER`): each virtualizes one global resource (PID space, mount
  table, hostname, network stack, IPC, uid/gid).
* (ii) **resource control** → **cgroups v2**: `memory.max`, `cpu.max`, etc.,
  accounting and capping a set of processes.
* (iii) **root-filesystem image** → **`pivot_root`** onto a supplied tree (Lab
  7); Docker layers **overlayfs** on top — stacking read-only image layers under
  a thin writable layer, giving copy-on-write layered/shareable images, whereas
  Lab 7 just extracts a flat tarball.

**(b)** A container **cannot** isolate: (1) the **kernel itself** — no separate
kernel version, and a kernel bug/`0-day` is reachable from every container (a
VM confines a compromise to one guest kernel); (2) **kernel-global state and
surface** not namespaced — parts of `/proc`, `/sys`, the syscall table, kernel
side channels, some resources (kernel memory, certain sysctls). Security
consequence: the **attack surface is the entire host kernel's syscall
interface**, and a single kernel vulnerability escalates from container to host —
so container escape is a kernel-bug away, whereas VM escape needs a much smaller
VMM/hardware bug.

**(c)** (choose two)
* (i) **`setgroups`/`gid_map`:** the kernel refuses to let an unprivileged
  process write `/proc/<pid>/gid_map` until `"deny"` has been written to
  `setgroups`. Otherwise a user mapped to gid 0 in the namespace could call
  `setgroups(2)` to **drop supplementary groups**, thereby *gaining* access to
  files that were protected by a group-**deny** (files readable by everyone
  *except* a group you are in) — a real privilege escalation. Denying
  `setgroups` first removes that lever.
* (ii) **unmapped-inode / `CAP_DAC_OVERRIDE`:** capabilities in a user namespace
  are only effective against files whose owner **has a mapping** in that
  namespace (`capable_wrt_inode_uidgid`). If a rootfs was extracted by uid 1000
  and root maps only `"0 0 1"`, uid 1000 is unmapped, so "root" in the container
  gets `EPERM` even on `mkdir` — `CAP_DAC_OVERRIDE` is deliberately void against
  unmapped inodes. For `mkdir(put_old)` to succeed, the uid **owning the rootfs
  files** must be mapped: unprivileged → map *yourself* (your files then appear
  owned by container-root); real root → map the **full range**.
* (iii) **`pivot_root` vs `chroot`:** `chroot` only changes where `/` resolves
  for lookups; the process keeps a handle on the old root (via an open dir fd or
  `..`), and a second `chroot` **escapes** it. `pivot_root` **moves the actual
  root mount** and then we **unmount** the old one, so there is nothing left to
  escape to. `put_old` is the directory *under new_root* where `pivot_root`
  parks the old root while it swaps them (then we `umount2(MNT_DETACH)` it).

**(d)** Real runtimes add: **capability dropping** (drop `CAP_SYS_ADMIN` etc., so
even container-root cannot do host-privileged operations) — defends against a
compromised container using root powers; **seccomp** (a syscall allow-list) —
shrinks the kernel attack surface by blocking dangerous/rare syscalls; **LSMs
(AppArmor/SELinux)** — mandatory access control confining what the container's
processes can touch. Lab 7 omits all three, which is why it is a teaching tool,
not a boundary. Paper 25's efficiency argument: container ("OS-level")
virtualization shares one kernel and page cache, so it has **near-native
performance and far higher density** (many more isolated instances per host, near-
zero per-instance memory/boot overhead) than full VMs, at the cost of weaker
isolation.

---

## E. Microkernel vs monolithic; L4 and seL4

**(a)** A **monolithic** kernel runs the whole OS — scheduler, VM, file systems,
drivers, network stack — in a single privileged address space (Linux, xv6). A
**microkernel** keeps only the **minimum** in kernel mode — address spaces,
threads/scheduling, and **IPC** — and pushes file systems, device drivers, and
paging **policy** into **user-space server processes** that communicate by IPC.
The kernel provides mechanism (address spaces + fast messages); servers provide
policy.

**(b)** In a monolithic kernel a call from (say) the VFS to a driver is an
ordinary **function call**. Move the driver to its own address space and that
call becomes **IPC**: a context switch, message copy, and scheduler
involvement. Because a single request may cross several servers, the system's
speed is dominated by **IPC cost**; if IPC is slow the whole microkernel is
slow — which is what doomed first-generation Mach (heavyweight messages, poor
cache behaviour). Liedtke (paper 28) argued this is an *implementation* failure,
not fundamental: **L4** made IPC 10–20× faster by keeping the kernel tiny,
passing short messages in **registers**, minimising the kernel's cache/TLB
footprint, and designing IPC as *the* optimised primitive. Fast IPC makes the
decomposition viable.

**(c)** **seL4's** proof establishes **functional correctness**: the C
implementation **refines** an abstract formal specification (and further proofs
give integrity/confidentiality/authority-confinement properties). It does **not**
prove the absence of all problems: it **assumes** a correct **hardware model**,
correctness of the small amount of **hand-written assembly** and boot code, the
**compiler**/translation step, and that the **spec itself** captures the intended
behaviour; side channels outside the model are not covered. A **small** kernel is
a precondition because the proof effort is enormous and roughly scales with code
size and complexity — seL4 is ~10k lines; verifying a monolithic kernel's
millions of lines (with drivers) is currently infeasible.

**(d)** *(Sheet 1 callback.)* A **capability** is an **unforgeable reference**
held *by the subject* that both **names** an object and **confers** specific
rights — possession *is* authorisation. An **ACL** attaches a list of
(subject → rights) *to the object*, and the subject exercises **ambient
authority** (all its rights apply automatically to any object it names). Ambient
authority causes the **confused-deputy** problem: a privileged service asked to
act on a caller's behalf uses *its own* authority on an object the caller named,
so the caller borrows rights it should not have (the classic compiler-writes-to-
billing-file example). Capabilities avoid this because the caller must **pass the
capability** it holds, so the deputy acts only with authority explicitly
delegated. **Revocation**: ACLs revoke by editing the object's list (immediate,
centralised); capabilities revoke via indirection/derivation trees (seL4's
capability-derivation tree lets you revoke a subtree). Capabilities suit a
verified microkernel because they make **authority explicit and enumerable** —
least privilege is expressible and checkable, which the integrity proofs rely
on.

---

## F. Synthesis essay — marking points

A strong answer to *"a microkernel is just a hypervisor with better marketing"*
should engage with **most** of:

* **What each multiplexes/isolates.** Microkernel: OS *services* (FS, drivers,
  pagers) as user processes over a minimal kernel. Hypervisor: whole *guest
  OSes* over virtual hardware. Both isolate mutually-distrusting components in
  separate address spaces — the kernel of the analogy.
* **The interface exposed.** Microkernel: IPC + capabilities + address-space/
  thread primitives (an *OS-construction* interface). Hypervisor: a **virtual
  hardware** / hypercall interface (guests see CPU, MMU, devices). The
  granularity and abstraction level differ.
* **The performance-critical path.** Microkernel: **IPC latency** (Liedtke's
  point). Hypervisor: **world switches** and the **2-D page walk** / VM
  exits. Recognise both are "make the cross-domain transition cheap" problems.
* **Where the TCB line is drawn.** seL4: a small **verified** trusted core.
  VMM: the hypervisor + its emulation is the TCB; type-1 vs type-2 differ. Good
  answers compare TCB size and assurance.
* **Where the analogy genuinely holds.** A microkernel **can** host
  paravirtualized guests; **L⁴Linux** runs Linux as a user process on L4; seL4
  and Xen share the "small privileged core, everything else deprivileged"
  worldview; NOVA/microhypervisors blur the line deliberately.
* **Where it breaks.** Different **threat models** and interfaces: containers
  (shared kernel, no guest OS) are neither; **exokernels** (paper 30) are a
  *third* answer — expose hardware, let applications manage resources — showing
  "what should a kernel be?" has more than two answers. A hypervisor's job is
  faithful hardware emulation for unmodified OSes; a microkernel's is a clean
  OS-construction substrate — same *mechanism* (deprivilege + interpose),
  different *goal*.
* **A judgement.** Best answers conclude that the quip is a useful half-truth:
  the underlying mechanism (privilege separation + fast controlled crossing) is
  shared, but the *interface, granularity, and purpose* differ enough that the
  two are convergent-but-distinct, not identical. Cite papers 24, 28, 29, 30.

---

*Python verification summary:* nested-paging walk `(G+1)(H+1)−1` = **8** (2,2),
**15** (3,3), **24** (4,4); shadow-paging TLB miss = 4 accesses (4-level). A
24-access uncached walk at 100 ns/access = 2.4 µs.
