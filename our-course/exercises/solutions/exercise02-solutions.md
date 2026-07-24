> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 2 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> These are model answers and marking notes, not the only correct responses.
> Where a question is open-ended the notes flag the points a supervisor wants to
> see made, rather than prescribing wording.

---

## A. Warm-ups

**A1. FALSE.** A program is a lifeless artifact on disk — instructions and
static data. A process is a *running* program: the program plus machine state
(address space contents, registers including PC and stack pointer, open files).
The distinction is not pedantry: one program can be running as many processes
at once, each with different state.

**A2. FALSE.** The child comes to life *as if it had itself called* `fork()`,
returning from that call with value 0 — it does not start at `main()`. The
chapter's evidence: the pre-fork "hello" message prints once, not twice; if the
child restarted at `main()`, it would print it again.

**A3. FALSE.** I/O completion moves the process from **blocked to ready**, not
to running. Whether it runs next is a separate decision made by the scheduler
— ch. 4's own trace shows the OS choosing *not* to switch back to Process 0
when its I/O completes. Blocked→running with no scheduler involved would mean
I/O completion preempts whatever is running, which is a policy no one has
chosen just by completing an I/O.

**A4. TRUE.** This is the **zombie** state: the process has exited but its exit
status must live somewhere until the parent collects it with `wait()`. The
process-list entry (PCB) is where it lives; only after reaping can the OS free
it. A parent that never waits leaks these entries — you will meet this in
Lab 1.

**A5. FALSE.** A successful `exec()` **never returns**. It does not create a
process; it *transforms* the caller — overwriting code and static data,
re-initialising heap and stack — and the transformed process continues from
the new program's entry point. Only a *failed* `exec()` returns (which is why
code after `execvp()` in the chapter's examples is reachable only on error).

**A6. TRUE.** `fork()` gives the child its own copy of the address space:
identical values at the moment of the fork, thereafter fully private. The
*only* difference visible at the return is the return value — child's PID in
the parent, 0 in the child. (Whether the copying is done eagerly or cleverly
is machinery for a later week; the *semantics* are a private copy.)

**A7. FALSE.** The shell is an ordinary user program: prompt, read a line,
`fork()`, `exec()` the command, `wait()`. It needs nothing from the kernel
beyond the same system calls available to every process — which is exactly why
there are many shells to choose from, and why Lab 1 can ask you to write one.
Credit for noting the kernel is of course *involved* — via those system calls
— but nothing about the shell is privileged.

**A8. FALSE.** The shell does the work in the child, between `fork()` and
`exec()`: close descriptor 1, open the file (which lands at descriptor 1), then
exec. `wc` just writes to descriptor 1 as it always does. See B3(c) for the
two descriptor facts this relies on.

---

## B. Process traces and the process tree

**B1.**
**(a)** Both processes are pure CPU: P1 runs ticks 1–5, P2 ticks 6–10.
**Elapsed 10, utilization 100%.** Uninteresting because nothing ever blocks:
with no I/O there are no state transitions except done, so no switching policy,
ordering, or overlap decision can change anything.

**(b)** `4:100,1:0`: P1 computes t1–4; P2 issues its I/O at t5, is blocked
t6–10, and finishes when the I/O completes. **Elapsed 10, CPU busy 5 ticks →
50%.** Nothing overlaps, because when the I/O finally starts there is no work
left to hide behind it.
`1:0,4:100`: P1 issues at t1; the system switches; P2 computes t2–5 while P1 is
blocked (t2–6); P1 finishes at t6. **Elapsed 6, busy 5 → 83%.**
Order matters because an I/O issued *early* exposes its 5 blocked ticks for
other work to fill; issued *last*, the same I/O is pure serial waiting. The
general lesson: **overlap** — keep the CPU busy during I/O waits — is where
multiprogramming's utilization win comes from (this is ch. 4's "improves
resource utilization" observation, and week 3 builds policy on it).

**(c)** With `SWITCH_ON_END` the system refuses to switch while P1 waits: t1
issue, t2–6 CPU idle, P1 done t6, P2 runs t7–10. **Elapsed 10, busy 5 → 50%.**
The waste relative to (b) is **4 ticks — exactly P2's entire CPU demand**,
which fits inside P1's 5-tick I/O wait and could have been hidden there
entirely.

**(d)** `IO_RUN_LATER` leaves the **I/O device** idle: when P1's I/O completes,
the running CPU job keeps the processor, so P1 — whose only remaining work is
to issue more I/Os — doesn't get to issue the next one until the CPU jobs
drain. Its three I/Os serialise *after* the computation instead of alongside
it. `IO_RUN_IMMEDIATE` runs P1 the moment each I/O completes; P1 immediately
issues the next I/O and blocks again, costing the CPU one tick and buying five
ticks of device work in parallel. Running the just-finished process is a good
bet precisely because a process that just did an I/O is likely in an I/O phase
— one tick of CPU now keeps the slow resource streaming. (Full credit for the
resource-overlap argument; a tick-exact trace is not required.)

**B2.**
**(a)** Step by step (children in braces):

```
a+b     a{b}
b+c     a{b},  b{c}
c+d     a{b},  b{c},  c{d}
c+e     a{b},  b{c},  c{d,e}
c-      default: d,e reparent to root a
final:  a{b,d,e},  b{}
```

**(b)** With `-R`, d and e go to c's own parent, **b**:
`a{b}, b{d,e}`.

**(c)** Ch. 4: an exiting process becomes a zombie until some process — 
normally its parent — calls `wait()` to collect its status, at which point the
OS can free its process-list entry. A parentless process has **no possible
waiter**: when it eventually exits, its zombie entry could never be reaped, and
the process list leaks an entry per orphan, forever. Reparenting preserves the
invariant that every process has someone responsible for burying it.

**(d)** **Yes.** Smallest example: the single-node tree `a` is produced both by
the empty history and by `a+b,b-`. (One size up: `a{b}` from `a+b`, from
`a+b,a+c,c-`, and from `a+b,b+c,c-`.) The invariant: every fork adds exactly
one node and every exit removes exactly one (its children survive via
reparenting), so a final tree of `n` nodes tells you exactly that
**forks − exits = n − 1** — the *difference* is determined, the individual
counts are not. The minimum history is `n − 1` forks and no exits; nothing
bounds the counts above. Credit requires the invariant, not just an example.

**B3.**
**(a)** With `wait()` the output order is **fully determined**: `hello` (the
only pre-fork print), then the child's `child (pid:…)`, then `wc`'s counts
(same process, sequential, after the exec), and the parent's `parent of …`
line **last** — `wait()` does not return until the child has fully exited.
Remove the `wait()` and the parent's line may appear anywhere after `hello`:
before the child's line, between the child's lines, or last, at the scheduler's
whim. `hello` is always first in either case, since only one process existed
when it ran.

**(b)** That the child does **not** begin at `main()` — it begins as if it had
called `fork()` itself and received 0. One "hello" is the observable proof
(see A2).

**(c)** The two facts: **(1) the lowest-free-descriptor rule** — UNIX
allocates the smallest unused descriptor number, so after
`close(STDOUT_FILENO)` the very next `open()` lands at descriptor 1; **(2)
open descriptors survive `exec()`** — the new program inherits the descriptor
table unchanged, so `wc`'s writes to descriptor 1 go to the file. It must
happen *in the child, between fork and exec*, because that is the only place
where the change affects exactly one command: done in the parent before
`fork()`, the **shell itself** loses its standard output — every subsequent
prompt and every future child's output lands in `p4.output`. The fork/exec gap
exists precisely to give per-command environment surgery a private place to
happen; this is the chapter's core argument for the API.

**(d)** The shell: calls `pipe()`, getting a kernel-managed queue with a read
end and a write end; `fork()`s the first child, which installs the **write
end as its descriptor 1** and execs `grep`; `fork()`s the second child, which
installs the **read end as its descriptor 0** and execs `wc`; the shell closes
its own copies of both ends and `wait()`s for both children. (Installing an
existing descriptor at a chosen slot is done with `dup2()` — TLPI reference
material and Lab 1's job; the principle is the same descriptor surgery as
p4.c.) Neither program needs modification because each just reads descriptor 0
and writes descriptor 1, as always — the plumbing was arranged before exec.
The kernel is acting primarily as **glue** (a common communication service),
with a **referee** element: the pipe's buffer must be synchronised and
flow-controlled between the two processes (this mirrors week 1's C1(d);
either emphasis earns credit if justified).

**B4.**
**(a)** Each of the three iterations doubles the process count: **2³ = 8
processes** (7 created), and `hello` prints **8 times** — every process
reaches the `printf`. General: **2ⁿ processes, 2ⁿ prints.**

**(b)** Tracking iterations (i0, i1, i2): after i0: a forks b. After i1: a
forks c, b forks d. After i2: a forks e, b forks f, c forks g, d forks h.

```
a ── b ── d ── h
│    └─── f
├─── c ── g
└─── e
```

Action string (one valid order): `a+b,a+c,b+d,a+e,b+f,c+g,d+h`.
The original has **3 children** — one per iteration. It is not a chain because
*every* live process forks in every iteration: b, itself a child, goes on to
create d and f. (The tree is the binomial tree B₃; any correct tree drawn in
any orientation earns full marks.)

**(c)** *(stretch)* To a file, stdio output is fully buffered: `hello\n` is
written into a user-memory buffer and **not yet flushed** when the forks
happen. `fork()` duplicates the address space — *including the buffer* — so
all 8 processes exit holding a copy of the unflushed line, and each flushes it
at exit: 8 hellos. On a terminal, line buffering flushed the newline before
any fork, so the line is out of the address space and prints once. This is a
concrete instance of C1's "fork copies everything, whether you wanted it or
not."

---

## C. Discussion and design critique

*This week's discussion questions ask for the strongest case against a claim the
reading makes. In all three, marking follows the same rule: a verdict without
conditions earns little; the marks are in the steelman and in stating precisely
what would settle the question.*

**C1.** A strong answer prosecutes, defends, and then judges.

**The case against "they simply got it right":**
- **Wrong default.** `fork()` copies *everything* — descriptors, signal
  dispositions, environment, unflushed buffers (B4c). Inheritance is opt-out,
  and the opt-out list grows with every kernel feature. For a privileged
  program, forgetting one item is a security leak; an API whose safe use
  requires enumerating an open-ended set of things to scrub is not "right".
- **Wasted work by construction.** The common case is fork-then-immediately-
  exec: logically duplicating an entire address space in order to discard it.
  Making that fast requires elaborate hidden machinery, which the rest of the
  memory system must then accommodate forever — the API's apparent simplicity
  is subsidised elsewhere.
- **Poor composition.** The B4(c) buffer duplication is the small symptom of a
  general problem: fork interacts badly with any library state, because
  libraries cannot know a copy of their world was just made.
- **The alternative exists and is honest.** A `spawn()`-style call creates the
  new program directly and takes *explicit* parameters for what the child
  inherits — auditable, no copy, no scrubbing.

**The strongest reply:** the fork/exec *gap* is a programmable hook. The child
runs arbitrary code to build its own environment — redirection, pipes (B3),
resource limits, and mechanisms not yet invented — with no change to the API.
`spawn` must reproduce each capability as a parameter (POSIX `posix_spawn`'s
file-actions and attributes objects are exactly that enumeration, made
manifest — and perpetually incomplete). Two small orthogonal primitives beat
one wide call; and R&T's shell is the existence proof of how much that bought.

**Conditions.** For shell-like programs, exploratory systems code, and 1974's
machines (small address spaces, no threads, single-digit-person systems),
OSTEP's claim stands. For today's large processes, security-sensitive
privileged code, and environments where the copy is expensive or the implicit
inheritance is dangerous, the critics are right. The mature judgement: fork
was *right for its context and remains right for shells*; as the universal
process-creation primitive, the claim overreaches — which is why real systems
now offer both. Credit any conclusion that is conditional in this way;
penalise unconditional verdicts in either direction.

**C2.** The strongest case against strict separation is an information
argument: a clean interface *hides* exactly the facts a good policy needs.
Examples worth credit: a "which process next?" policy that cannot see the
mechanism's costs (how expensive is the switch it is about to order?) will
happily make decisions whose overhead exceeds their benefit; a generic
mechanism must serve every conceivable policy, so it exposes a
lowest-common-denominator interface and forecloses optimisations that need
both halves (the policy that *knows* it will never migrate work can skip
state the mechanism dutifully maintains). Interfaces also ossify: the split
freezes a guess about which decisions are "policy" — and the guess outlives
its era.

A good concrete setting: the week-1 C3 embedded device — one workload, one
policy, forever. There, separation buys evolvability that will never be used
and costs indirection on every decision; merging is simpler *and* faster. The
property that justifies blurring: **the policy is fixed and known at design
time** (or the policy/mechanism boundary is the performance-critical path).
Conversely, where policies must evolve or several coexist — a general-purpose
OS — separation is what makes ch. 4's "change policies without rethinking
mechanism" real. Marking note: any well-argued instance earns the marks; "the
separation is always good" or "always bad" earns few.

**C3.** The case against *design for yourself*: it systematically serves
people like the designers and fails everyone else. Concretely: non-programmer
users (nothing in the method surfaces their needs); requirements the designers
never personally feel — internationalisation, accessibility, security against
*hostile* users (early UNIX's famously trusting protection model reflects a
building full of colleagues, not the open internet); and operational needs of
people who cannot patch the source. Add survivorship bias: UNIX is the method's
great success, but the method also produced countless self-satisfying systems
nobody else could use; citing the winner is not evidence.

Conditions that made it work in 1974: the designers *were* the target market
(a system by programmers for programmers, so self-design was user research);
the team was tiny and iteration cheap, so taste could stay coherent; there was
no installed base or compatibility burden; and the surrounding institution
tolerated years of unpromised work. For a phone OS today, essentially none of
these hold — the users are emphatically not the designers, hostile input is
the default, and the ecosystem imposes requirements no designer personally
feels. What survives of the method is its residue: *dogfooding* as one input
among many, rather than the design method itself. Credit answers that reach a
different balance if they argue from the same conditions; the failure mode is
treating "it worked for UNIX" as sufficient.
