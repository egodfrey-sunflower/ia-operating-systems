> # ⚠️ SPOILER — ANSWERS TO EXAMPLES SHEET 1 ⚠️
> **Do not read until you have attempted the sheet closed-book.**
> These are model answers / marking notes, not the only correct wording.
> For the IA-sheet questions cited by number, work them yourself from
> `../../../cambridge-course/examples_sheets/examples_sheet1.pdf`; the notes here just flag the
> points a supervisor wants to see.

---

# Sheet 1 — Answers

## A. Warm-ups

**A1. FALSE.** Dual-mode is *hardware*-enforced, not a software convention. The
CPU carries a mode bit; privileged instructions (halt, load page-table base, set
timer, mask interrupts, I/O) *trap* if executed in user mode. If it were only a
software flag the kernel checked, malicious user code could simply not check it.

**A2. FALSE.** A system call is a *controlled mode transition*, not an ordinary
call. `printf` calling `write` is a normal call; `write` then executes a trap
instruction (`ecall`/`syscall`/`int 0x80`) that raises an exception, switches to
supervisor mode, and vectors through the kernel's trap handler at a
kernel-chosen address. Control does *not* jump to a caller-chosen address, and
the stack and page table change. The whole point is that the user cannot pick
where in the kernel control lands.

**A3. TRUE.** The hypervisor exposes a narrow virtual-hardware interface and each
guest has its own kernel; the shared trusted layer between two guests is small.
Two containers share *one* Linux kernel — a huge trusted code base (the entire
system-call surface). A single kernel bug reachable from a container can breach
isolation between all containers, whereas breaching VM isolation requires a
hypervisor bug. (Nuance: containers can be adequately isolated with
seccomp/user-namespaces; the claim is about the *size of the shared trusted
computing base*, which is genuinely larger.)

**A4. FALSE-ish (the interesting answer).** Both *store the same matrix* — ACLs
by column, capability lists by row — so a *static snapshot* converts either way.
But they are not operationally equivalent: capabilities carry *delegation and
transfer* semantics (you can hand a right to another subject) and *ambient
authority* differences that ACLs don't, and revocation is easy in one model and
hard in the other. So "always convert losslessly" is false once you include the
operations the two models are *for*, not just the snapshot.

**A5. FALSE.** After reset a RISC-V hart begins in **machine mode (M-mode)** —
*more* privileged than supervisor, not less — running firmware/boot ROM
(on the Lab 0 target, OpenSBI) before any kernel code. The boot flow is a
*descent* in privilege: M-mode firmware sets up and hands off to the S-mode
kernel, which eventually drops to U-mode to run the first user process. You
watched exactly this M→S→U staging in Lab 0, Part B, single-stepping the very
first instructions under gdb: the CPU is at its *most* privileged at reset, and
"kernel (supervisor) mode" is something the boot sequence arrives at, not where
it starts.

## B. Protection and dual-mode

**B1 (IA Sheet 1 Q1–Q2).** Marking notes:
- Q1(a) **True** — preemption needs a *timer interrupt* to wrest the CPU back;
  without hardware you can only cooperatively yield. (b) **False** — a context
  switch saves/restores a whole register + memory-map state, far more than a
  flip-flop's one bit. (c) **False** — without system calls unprivileged code
  could not do I/O or any privileged action; they are the *only* legitimate
  doorway into the kernel.
- Q2(a) the three hardware supports: **(1) dual-mode operation** (privileged vs
  user), **(2) memory protection** (an MMU/base-limit so a process can't touch
  others' or the kernel's memory), **(3) a timer interrupt** (to regain control
  and preempt). (b) via **system calls** (trap instruction). (c) Lacking the
  hardware you fall back to *software* techniques: cooperative scheduling,
  language-level sandboxing / software-fault isolation, interpretation or
  bytecode verification — all weaker, slower, or trust-the-compiler.

**B2. Mechanism vs policy.**
- (a) *Mechanism* = the machinery that makes something possible; *policy* = the
  decision about how to use it. Scheduling: the timer interrupt + context switch
  are mechanism; "run shortest job first" is policy. Memory: the MMU/page-table
  hardware is mechanism; "which page to evict" is policy.
- (b) Timer → the kernel chooses the *quantum* and *when* to preempt (RR vs
  priority) in software. Dual-mode bit → the kernel chooses *which* operations
  are privileged and what a syscall is allowed to do. MMU → the kernel chooses
  the *layout* and *protections* of each address space, demand paging, COW —
  none baked into the MMU.
- (c) Baking policy into hardware ages badly because policy needs change faster
  than silicon. Examples: hardware task-switching / segmentation on x86 (the
  386's hardware TSS-based task gates went unused by real OSes, which do context
  switches in software); or elaborate hardware scheduling that OSes bypass. Keep
  hardware to *fast, general mechanism* (Lampson: "use a good idea again" — make
  the mechanism reusable).

**B3. Kernels vs microkernels.**
- (a) Monolithic: file system, drivers, network stack, scheduler all run in
  kernel mode in one address space; the user/kernel boundary is the syscall
  interface; only syscalls and their results cross it. Microkernel: the kernel
  provides only IPC, address spaces, and scheduling; FS, drivers, etc. are
  *user* processes; requests cross the boundary as *messages* relayed by the
  microkernel.
- (b) Monolithic read: one syscall trap in, work done in-kernel, one return —
  **2 crossings**. Microkernel read: app → kernel (send msg), kernel → FS server
  (deliver), FS server → kernel (reply), kernel → app (deliver) — at least **4**,
  often more (FS server itself calls a disk-driver server, adding another
  round-trip). This IPC cost is why first-generation microkernels were slow.
- (c) *For*: a driver or FS crash is a user-process crash, not a kernel panic —
  fault isolation and restartability; smaller trusted kernel is easier to get
  right (→ seL4, Week 16). *Against*: every service interaction is IPC + context
  switches, so first-gen microkernels (Mach) paid heavy overhead. Liedtke
  (Week 16) shows careful design recovers most of it.

## C. System calls

**C1 (IA Sheet 1 Q5).**
- (a) Purpose: give unprivileged code a *safe, controlled* way to request
  privileged services (I/O, process/mem management) the OS must mediate.
- (b) Mechanism: a **trap/software interrupt** — a special instruction
  (`ecall`/`syscall`/`svc`/`int 0x80`) that raises a synchronous exception,
  switching to supervisor mode and vectoring through the trap handler. The call
  number selects the service; arguments in registers; result in a register.
- Lab-2 concrete path (RISC-V): `ecall` with the call number in **a7** and
  args in **a0–a5**; hardware jumps via `stvec` to `uservec` (trampoline), which
  saves user regs to the `TRAPFRAME` and switches to the kernel page table;
  `usertrap()` sees `scause == 8` (environment call) and calls `syscall()`, which
  dispatches on `a7` and writes the result to the saved **a0**; return restores
  state and `sret`s back to user mode.

**C2. Crossing safely.**
- (a) The kernel runs privileged with the *user's* pointer; if it blindly
  dereferences `buf`, a user can pass a *kernel* address and trick the kernel
  into reading/writing kernel memory on its behalf (info leak or arbitrary
  write) — a *confused-deputy* attack. It could also pass an unmapped address and
  fault in kernel mode.
- (b) `copyin`/`copyout` enforce **(1) the address is valid and mapped in the
  *caller's* page table** (not the kernel's), and **(2) it lies in user space
  with the right permissions** (the user actually may read/write it). Together:
  the kernel only touches memory the *user* was already allowed to touch.
- (c) Blocking *inside* a syscall is fine because the kernel *voluntarily
  yields*: it marks the process blocked and schedules another — interrupts stay
  enabled and the timer still fires. A user loop with interrupts disabled would
  (i) not be *able* to disable interrupts (privileged — it would trap), and (ii)
  if it somehow could, it would freeze the whole machine because the timer could
  never preempt it. The difference is *who is in control* and whether preemption
  remains possible.

## D. Access matrix

**D1 (IA Sheet 1 Q6).** Marking notes:
- (a) The four ops are not independent: e.g. *modify* = read + replace (or
  append) of part of a file; *replace* can be seen as append after truncate.
  Credit any coherent reduction; the point is to notice combinations, and to
  note *read* and *append/write* are the natural primitives.
- (b) With a small chosen set of tuples, e.g. `{(a,f0,read),(a,f0,replace),
  (b,f0,read),(b,f3,append),(c,f7,read)}`:
  - **Matrix**: users as rows, files as columns, cell = set of rights. Mostly
    empty (sparse) — the key observation.
  - **ACLs**: attach to each *file* a list of (user, rights): `f0: {a:[read,
    replace], b:[read]}`, `f3: {b:[append]}`, `f7: {c:[read]}`. Empty files have
    empty ACLs — storage proportional to non-empty cells.
  - **Capabilities**: attach to each *user* a list of (file, rights):
    `a: {f0:[read,replace]}`, `b: {f0:[read], f3:[append]}`, `c: {f7:[read]}`.
  Both avoid storing the sparse matrix's empty cells.

**D2. Choosing a representation.**
- (a) (i) "who can access f7?" — **cheap with ACLs** (read f7's column
  directly), expensive with capabilities (scan every subject). (ii) "revoke b's
  access to everything" — **cheap with capabilities** (delete b's row), expensive
  with ACLs (scan every object's list). (iii) "pass exactly the read-right on f3
  to a helper" — **cheap with capabilities** (copy/delegate the capability),
  awkward with ACLs (must edit f3's ACL and name the helper).
- (b) Compression to owner/group/other × rwx loses *per-user* granularity: you
  cannot grant one specific extra user access without abusing groups. **POSIX
  ACLs** add back arbitrary named-user and named-group entries plus a mask,
  restoring per-subject precision.
- (c) A capability must be **unforgeable** and **tamper-proof**: a subject must
  not be able to fabricate or widen one. (i) Tagged-memory hardware marks
  capability words with a tag bit that only privileged instructions can set, so
  ordinary arithmetic can't forge one. (ii) Kernel-managed handles (file
  descriptors) are just small integers *indexing a kernel table*; the authority
  lives in the kernel, and the integer is meaningless without the kernel's
  table — the user can't forge authority by making up a number.

## E. Authentication vs authorization

**E1.**
- (a) **Authentication** verifies a *claimed identity* — it proves you are who you
  say you are. **Authorization** decides whether an *already-identified* subject
  may perform a requested operation on an object. OS mechanism for
  authentication: the `login`/PAM password check (or SSH public-key
  verification), which establishes the session's UID. OS mechanism for
  authorization: the kernel's permission check inside `open()`/`read()`, which
  tests that UID against the file's mode bits / ACL (or a capability) and permits
  or denies. Authentication runs *once* at the door; authorization runs on
  *every* access.
- (b) `login` (or `sshd`) runs privileged and is trusted with two things. It
  *sees* the secret: it receives the cleartext the user types, hashes it, and
  compares against the stored hash. And it *sets* the credentials of the
  resulting session — it `setuid`/`setgid`s the shell to the authenticated
  UID/GID. Everything downstream (the kernel's permission checks) then trusts
  that UID *without re-authenticating*. So `login` sits in the trusted computing
  base: it sees the secret for an instant and asserts the identity permanently, and
  a bug or backdoor in it forges identity for the whole machine.
- (c) What the thief of the database gets:
  - **Cleartext:** every password directly — instant total compromise, and any
    passwords reused on other systems fall too.
  - **Unsalted hash:** `H(password)`. Passwords are not readable directly, but the
    attacker can precompute one table of `H(guess)` (a rainbow table) and match
    *every* account against it at once; identical passwords hash identically, so
    equal passwords across accounts are visibly equal, and one precomputation
    cracks the whole file.
  - **Salted hash:** each row stores `(salt, H(salt ‖ password))` with a unique
    random salt. Precomputation is now useless — the same password hashes
    differently under different salts — so the attacker must attack **each account
    separately**, and identical passwords are no longer visibly identical. The
    salt denies *amortisation across accounts*.
  - What salting does **not** protect: a *weak or guessable* password. An attacker
    targeting one account can still hash dictionary/brute-force guesses against
    that account's salt. Salt defeats sharing work across accounts, not the
    per-account guess — for that you additionally use a deliberately *slow* hash
    (bcrypt/scrypt/Argon2).
- (d) The object consulted is the **access matrix** of Section D — concretely its
  ACL slice (the file's mode/ACL) or the subject's capability list. The
  authenticated identity (UID/GID, or the process's capability set) is the *row*
  selector: it picks *which subject's* rights apply; the target object picks the
  column; the cell says permit or deny. So the two halves of "subjects and
  objects" join up: **authentication binds a process to a subject (fills in which
  row you are); the access matrix says what that subject may do to each object.**

## F. VMs vs containers

**F1.**
- (a) Type-1 hypervisor: hardware → hypervisor → {guest OS kernel + apps} × 2;
  *shared* = the hypervisor only; *duplicated* = a full OS kernel per guest.
  Containers: hardware → one Linux kernel → {container userspace} × 2; *shared* =
  the entire kernel; *duplicated* = only userspace (libraries, root fs view).
- (b) **cgroups** partition resources (CPU, memory, I/O bandwidth);
  **namespaces** isolate/virtualise names (PIDs, mounts, network, users, UTS,
  IPC). (Plus overlayfs for the filesystem view, seccomp to shrink the syscall
  surface — Week 15.)
- (c) A VM isolates the *kernel* itself and the *hardware interface*: a guest can
  run a different OS/kernel version, and a guest kernel bug can't reach the host
  kernel. Containers share one kernel, so they can't run a different kernel and a
  kernel exploit is shared. Containers are preferred for many instances because
  they are far lighter — no duplicated kernel, near-instant start, higher density,
  shared page cache.

**F2.** True (as A3). The shared *trusted* layer for containers is the whole
kernel system-call interface — thousands of syscalls, huge attack surface — so a
single kernel vulnerability reachable from a container compromises the isolation
of every container on the host. For VMs the shared trusted layer is the much
smaller virtual-hardware / hypercall interface; breaching it needs a hypervisor
bug. Fewer lines of trusted, attacker-reachable code ⇒ stronger isolation. (One
exploited bug in the shared layer buys the attacker every co-tenant on that
layer.)

## G. Covert channels

**G1.**
- (a) A *covert channel* is a communication path not intended for information
  transfer, used to move data in violation of the security policy. *Storage
  channel*: signal via a shared stored attribute — e.g. High sets/clears a lock
  file or fills a disk quota that Low can observe. *Timing channel*: signal via
  *when* something happens — e.g. High modulates its CPU usage and Low infers
  bits from how fast its own work runs.
- (b) High and Low time-share a core. To send a `1`, High spins for a full
  quantum; to send a `0`, High sleeps immediately. Low continuously does a fixed
  chunk of work and reads a clock: a slice that took long ⇒ it was contended ⇒
  bit `1`; a fast slice ⇒ bit `0`. Bandwidth is on the order of *one bit per
  scheduling quantum* — very roughly 1–1000 bits/s depending on quantum and clock
  resolution (order-of-magnitude answer; credit any reasoned estimate).
- (c) The access-matrix model controls *access to named objects* but implicitly
  assumes information only flows through those authorised accesses. A covert
  channel carries information through a *shared resource's observable side
  effects* (timing, cache state, contention) that the matrix never modelled as an
  "object" — so every (subject, object, right) entry can be correct while
  information still flows.
- (d) Mitigations & costs: *add noise / jitter* to timing (costs performance and
  only reduces, not eliminates, bandwidth); *quantise the clock* Low can read
  (breaks legitimate high-resolution timing needs); *partition the resource*
  statically in time or space (e.g. fixed per-domain time slices, cache
  partitioning) — strongest, but wastes capacity and hurts utilisation. All
  trade performance/utilisation for reduced channel bandwidth.

**G2.** A *container escape* gives the attacker code execution *outside* the
sandbox (host-level access / other containers) — a breach of the isolation
mechanism. A *covert channel* leaks *information* across a boundary that stays
mechanically intact — no escape, just signalling. "The access control policy is
correct" defends against neither: an escape exploits an *implementation bug* in
the mechanism enforcing the policy, and a covert channel exploits an *unmodelled
shared resource* the policy never covered.

## Past paper notes
y2015p2q3, y2011p2q3(a) and y2014p2q3 test the access-matrix/ACL/capability
and protection-mechanism material of Sections B and D. y2007p1q8(a)–(c) tests
dual-mode operation — the status register's supervisor/user and
interrupt-enable bits — the Section B material. Model answers follow directly
from the notes above.
