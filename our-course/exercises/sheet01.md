# Examples Sheet 1 — OS Structure, Protection, and System Calls

**Attempt after Week 2.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet01-answers.md` (spoilers — attempt first).

**Reading this sheet leans on** (see `../reading-list.md`): OSTEP ch. 1–2, 4–6
(the process abstraction and *limited direct execution*) and ch. 53–55
(security: intro to OS security, authentication, access control — week-1
reading); Anderson & Dahlin ch. 1–2 (protection, the kernel/user boundary);
the monolithic/microkernel/modules overview (Liedtke, item 28, as a week-16
read-ahead — Silberschatz §2.8 is an optional backstop) for §B3, and Microsoft
Learn's *"Containers vs. virtual machines"* + Julia Evans's *"What even is a
container"* for §F (VMs vs containers; deep treatment weeks 15–16); Ritchie & Thompson (1974), *The UNIX Time-Sharing System* (reading item 8);
Saltzer & Schroeder §I.A–B (item 10 — ACLs vs capabilities, least privilege,
for §D/§E); Wahbe et al. §1–3 (item 9 — software fault isolation, for §B1/Q2c — confining code without protection hardware);
Lampson (1973), *A Note on the Confinement Problem* (item 11 — covert channels,
for §G). The access-matrix material for §D is OSTEP ch. 55 (ACLs vs
capabilities) + Saltzer & Schroeder + A&D ch. 2.

Where a question is a pointer into the original Cambridge IA sheet, do that
question in `../../cambridge-course/examples_sheets/examples_sheet1.pdf`. Those questions are
part of this sheet — they are cited, not reprinted.

---

## A. Warm-ups (true/false with one or two sentences of justification)

> For each statement, say whether it is true or false and *why*. A bare verdict
> earns nothing; the justification is the point.

**A1.** "Dual-mode operation (user/supervisor) is a software convention that a
well-written kernel enforces by checking a flag before each privileged action."

**A2.** "A system call is just a function call into the kernel: control
transfers the same way `printf` calls `write`."

**A3.** "A hypervisor running several guest OSes gives stronger isolation
between two workloads than running those same two workloads as containers on
one Linux kernel."

**A4.** "Access control lists and capability lists are two ways of storing the
*same* access matrix, so a system can always convert losslessly from one to the
other."

**A5.** "The first instruction the CPU executes after reset is already in
supervisor (kernel) mode."

---

## B. Protection and dual-mode operation

**B1. (IA foundations — do by citation.)**
Do **IA Examples Sheet 1, Q1** and **Q2** in
`../../cambridge-course/examples_sheets/examples_sheet1.pdf`.
- Q1 is three true/false warm-ups on preemptive scheduling needing hardware
  support, whether a context switch is "a flip-flop", and whether system calls
  are optional.
- Q2 asks which *three* kinds of hardware support let an OS stop an application
  crashing or monopolising the machine, how applications ask the OS to act for
  them, and what you could do lacking that hardware.

Frame your Q2 answer around the three mechanisms A&D ch. 2 identifies
(privileged mode, memory protection, timer interrupts) and connect each to a
concrete failure it prevents.

**B2. Mechanism vs policy.**
The course's organising slogan (README; OSTEP ch. 2's design goals — and, in
week 3, Lampson's *Hints*, reading item 12) is *separate mechanism from policy*.
- (a) Define the two terms and give one example pair from CPU scheduling and one
  from memory protection.
- (b) The dual-mode bit, the timer, and the MMU are *mechanisms*. Name a
  *policy* decision each one enables the kernel to make without the hardware
  itself hard-coding that policy.
- (c) Why does mixing mechanism and policy in hardware tend to age badly? Give a
  historical example (e.g. a scheduling or protection decision baked into a CPU).

**B3. Kernels vs microkernels.**
- (a) Sketch the structure of a monolithic kernel and a microkernel, marking
  the user/kernel boundary and what crosses it.
- (b) In a microkernel the file system is an ordinary user process. Trace the
  messages exchanged when an application reads a file, and count how many
  user↔kernel boundary crossings occur versus the monolithic case.
- (c) Give one robustness argument *for* microkernels and one performance
  argument *against* first-generation ones. (You will revisit this with
  Liedtke's paper in Week 16; here answer from the Week 2 overview.)

---

## C. System calls

**C1. (IA foundations — do by citation.)**
Do **IA Examples Sheet 1, Q5** in `../../cambridge-course/examples_sheets/examples_sheet1.pdf`:
the purpose of a system call and the mechanism typically used to implement one.

Extend it with the concrete RISC-V path you will implement in **Lab 2**
(`labs/lab2-syscalls/README.md`): name the instruction that raises the trap,
where the call number and arguments live, and how the kernel returns a result.
Your answer to "what mechanism" should mention the trap/exception, the trap
vector, the switch to supervisor mode, and the switch of stack and page table.
*(Address translation is week-7 material and the multi-level page-table walk
week 8; the Lab 2 implementation only touches them in passing — here, answer at
the level of "the kernel swaps the protected translation state", not the
multi-level walk itself.)*

**C2. Crossing the boundary safely.**
- (a) Why must the kernel *never* dereference a pointer passed by a user program
  directly? Describe the attack a naive `read(fd, buf, n)` implementation that
  trusts `buf` would enable.
- (b) xv6 copies user data in and out with `copyin`/`copyout`, which validate
  the address against the caller's page table. What two distinct properties do
  these checks enforce? *(The mechanism detail comes in week 8 — here, say
  *what* must be checked, not *how* the hardware stores it.)*
- (c) A system call that takes a long time (e.g. a disk read) blocks. Explain
  why blocking *inside* a system call is fine, whereas an ordinary application
  spinning in a loop with interrupts disabled would be a disaster.

---

## D. The access matrix: ACLs vs capabilities

**D1. (IA foundations — do by citation.)**
Do **IA Examples Sheet 1, Q6** in `../../cambridge-course/examples_sheets/examples_sheet1.pdf`:
three users, ten files, the four operations *read / append / replace / modify* —
whether all four are primitive, and a worked representation of one access set as
(i) an access matrix, (ii) access control lists, (iii) capability sets.

**D2. Choosing a representation.**
Building on D1's example:
- (a) ACLs store the matrix by *column* (per object); capability lists store it
  by *row* (per subject). For each of these operations, say which representation
  makes it cheap and which makes it expensive, and why:
  (i) "who can access file `f7`?"; (ii) "revoke user `b`'s access to everything";
  (iii) "let a process pass exactly its read-right on `f3` to a helper process".
- (b) Unix file permissions are a compressed ACL (owner/group/other × rwx).
  What expressiveness is lost by that compression, and what did POSIX ACLs add
  back?
- (c) Capabilities behave like unforgeable tickets. What must the system
  guarantee about a capability for the scheme to be secure, and how is that
  guarantee provided in (i) a tagged-memory/hardware-capability machine and
  (ii) a system where capabilities are kernel-managed handles (like file
  descriptors)?

---

## E. Authentication vs authorization

Authentication asks *who are you?*; authorization asks *what are you allowed to
do?* The IA syllabus lists both under "subjects and objects" — this question
pulls them apart. (Reading: OSTEP ch. 54, authentication — week-1 reading.)

**E1. Two questions, two mechanisms.**
- (a) Distinguish authentication from authorization in one sentence each, and
  give one concrete OS mechanism that performs each. (Contrast what `login` does
  with what the permission check on `open()` does.)
- (b) The login program (or the SSH daemon) is part of the trusted computing
  base. State what it must be trusted with: what does it *see*, and what does it
  *set* about the resulting process that the rest of the system thereafter takes
  on faith?
- (c) A system stores, per account, a **salted hash** of the password rather than
  the cleartext. Say what an attacker who steals the password database obtains
  under each of three designs — cleartext, an *unsalted* hash, a *salted* hash —
  and what specifically the salt denies them. What does salting *not* protect
  against (think about a weak password attacked one account at a time)?
- (d) Once `login` has authenticated a subject and stamped it with an identity
  (its UID/credentials), authentication is finished — yet the subject still may
  do nothing until *authorization* rules on each access. Which object from
  Section D is consulted then, and what does the subject's identity index into
  it? Tie the two halves of the syllabus line back together.

---

## F. Virtual machines vs containers

**F1. Two kinds of isolation.**
- (a) Draw the layering for (i) a type-1 hypervisor running two guest OSes and
  (ii) a single Linux kernel running two containers. Mark, in each, what code is
  shared between the two workloads and what is duplicated.
- (b) A container is "a process (group) with a restricted view of the machine",
  not a virtual machine. Which kernel mechanisms provide that restricted view?
  (Name the resource-partitioning mechanism and the naming/isolation mechanism;
  you will meet namespaces and cgroups properly in Week 15, but the Week 2
  overview names them.)
- (c) State two things a VM isolates that a container does *not*, and one reason
  containers are nonetheless preferred for deploying many instances of a service.

**F2. Where the trust boundary sits.**
Argue, in a short paragraph, whether the sentence in warm-up **A3** is true.
Ground your answer in *how large the shared, trusted code base is* in each case
(the guest–hypervisor interface versus the container–kernel system-call
interface) and what a single exploitable bug in that shared layer buys an
attacker.

---

## G. Covert channels

**G1. Leaking without an authorised channel.**
Two processes, *High* (holds a secret) and *Low* (may talk to the outside), run
on one machine. The access-control policy forbids *High* from sending data to
*Low*.
- (a) Define a *covert channel*, and distinguish a *storage* channel from a
  *timing* channel with one concrete example of each.
- (b) *High* and *Low* share a CPU. Describe a timing channel by which *High*
  can signal bits to *Low* using nothing but its own scheduling behaviour and a
  clock *Low* can read. Estimate, in order of magnitude, the bandwidth.
- (c) Covert channels defeat the access matrix even when every (subject, object,
  right) entry is correct. Explain *why* — what does the access-matrix model
  assume about information flow that a covert channel violates?
- (d) Suggest one mitigation and state its cost. (Consider: adding noise,
  quantising the clock, partitioning the resource in time or space.)

**G2. Connecting the ideas.**
A container escape and a covert channel are different threats. In one or two
sentences each, say what an attacker gains from each, and why "the access
control policy is correct" is a defence against neither.

---

## Past paper questions

Per this directory's `README.md`, attempt these under time pressure (~35 min each,
closed-book) after finishing this sheet (files in `../../cambridge-course/exam_questions/`):

- **y2025p2q3** — the access matrix and its ACL/capability-list slicings, plus
  Unix users, groups and the root UID.

(The protection parts of **y2019p2q4** overlap this sheet, but its central
IPC-comparison part is week-3 material, so that question is allocated to
**Sheet 2** instead.)

For extra, untimed drill on this sheet's protection and access-control
material, these pre-2016 questions fit (files in `../../cambridge-course/exam_questions/`):

- **y2015p2q3** — ACLs vs capabilities, how Unix implements file access
  control, and the principle of minimum privilege — Section D material.
- **y2011p2q3 (a)** — access-control definitions (access control, ACLs,
  capabilities) and how Unix and Windows NT manage file access — Section D
  again. (Part (b), on page replacement, is Sheet-6 memory material — save it
  for then.)
- **y2014p2q3** — the mechanisms a multiuser OS uses to protect one user's
  processes and stored information from another's, and the hardware each
  relies on — Sections B and D.
- **y2007p1q8 (a)–(c)** — the status register's supervisor/user and
  interrupt-enable bits: dual-mode operation, Section B material. (Parts
  (d)–(e), on inodes and NTFS, belong with the file-system sheets.)
