> # ⚠️ SPOILER — MODEL ANSWERS TO EXERCISE SHEET 6 ⚠️
>
> **Do not read until you have attempted the sheet closed-book.**
>
> Model answers and marking notes, not the only correct responses. For open
> questions the notes flag the points a supervisor wants to see made.

---

## A. Warm-ups

**A1. FALSE.** The interface works precisely *because* the allocator records
the size itself at `malloc()` time and looks it up when handed the pointer
back. The user never passes a size to `free()`; the library's own bookkeeping
supplies it. (Ch. 14 deliberately doesn't say how — that design question is
C1, and Lab 3.)

**A2. FALSE.** Memory management is **two-level**: the library manages the
heap inside the process, and the OS manages the pages given to the process.
When the process exits, the OS reclaims every page — code, stack, heap, leaked
or not. Nothing survives to reboot. Leaks matter for *long-running* programs
(servers, and the OS itself), where unreclaimed heap accumulates until the
process dies of memory exhaustion.

**A3. FALSE.** `sizeof` is (in this use) a compile-time operator applied to
the *type* of `x`, which is `int *` — so the result is the size of a pointer,
8 on a 64-bit machine, regardless of what was allocated. To get 40 you need a
true array declaration (`int x[10]`), where the compiler statically knows the
extent. Credit for noting this is exactly the confusion ch. 14 warns about.

**A4. FALSE — and this is the sheet's most important warm-up.** The write
lands one element past the block; depending on allocator rounding and what
sits there, the program frequently runs to apparent completion (see B1c). The
overflow may silently corrupt a neighbouring value instead of crashing —
which is *worse* than a crash, and is why buffer overflows are a classic
source of security vulnerabilities. Ran ≠ correct.

**A5. FALSE.** `malloc()` and `free()` are **library calls** that manage heap
space within the address space; the library itself is built on system calls —
`brk`/`sbrk` to move the end of the heap, or `mmap()` for anonymous regions —
which it invokes only when it needs more memory from the OS. (Ch. 14's advice:
never call `brk`/`sbrk` yourself.)

**A6. FALSE.** Translation happens in **hardware** — the MMU adds the base and
checks the bounds on *every* reference, at full speed, with no OS
involvement. The OS's role is confined to the exceptional and the occasional:
it sets the register pair (privileged instructions) when dispatching a
process, and it handles the trap when a bounds check fails. This is week 1's
mechanism/policy split with named hardware: if the OS were consulted per
reference, the machine would be unusably slow.

**A7. FALSE.** Rewriting the addresses *in the executable* does nothing to
constrain the addresses the program *computes at run time* — a stray pointer
can still read or write any other process's memory. Protection requires
checking every access, which needs hardware. Static relocation also makes the
placement permanent in practice: once rewritten, the process is hard to move
(see B3b).

**A8. FALSE.** The compiler sees one translation unit and cannot know where
anything outside it will live; it emits a **relocatable object file**
containing symbol references left unresolved. The **linker** resolves symbols
across object files and fixes up the references. Final *physical* placement is
later still — at load time or, under dynamic relocation, per access at run
time (B3).

---

## B. Break it, diagnose it, translate it

**B1.**
**(a)** (i) The program dies with a **segmentation fault** — the OS's
response to an illegal reference. You learn *that* it died, not where or why.
(ii) With `-g`, gdb runs the program, catches the signal, and reports
`Program received signal SIGSEGV` **at the exact source line** of the
dereference, with variable names inspectable — turning "it crashed" into "this
line dereferenced `p`, which is 0x0". (iii) Valgrind reports something like
**"Invalid read of size 4"** and that **"Address 0x0 is not stack'd, malloc'd
or (recently) free'd."** It knows because it instruments every memory access
and checks it against its own map of what memory the program validly owns —
the same interposition idea as ch. 15, done in software per access. The
compiler said nothing because the program is type-correct; the error exists
only at run time. ("Size 4": an `int` was read through the pointer.)

**(b)** (i) Nothing visible — it runs and exits 0. (ii) Valgrind's heap
summary: **40 bytes in 1 block "definitely lost"**, with the allocation stack
trace. (iii) For this program: not operationally a bug — at exit the OS
reclaims all pages, so nothing is lost (the ch. 14 principle: **two-level
memory management**, as in A2). For a web server: a real bug — the process
never exits, the leak compounds per request, and the server eventually
exhausts memory and crashes. Credit for stating the principle rather than
just the two verdicts.

**(c)** Prediction to beat: "it crashes." Usually it doesn't — allocators
round request sizes and keep their own data adjacent, so `data[100]` often
lands in slack space and the run looks clean. Valgrind reports **"Invalid
write of size 4"** at an address **"0 bytes after a block of size 400
alloc'd"** — a precise diagnosis of an off-by-one the program itself never
exhibits. This is A4 made concrete.

**(d)** The print frequently *succeeds*, often showing the old value — the
freed block hasn't been reused yet. That is the worst outcome because the
program appears correct while depending on memory it no longer owns; the day
the allocator recycles that block (a later `malloc()`), the value silently
changes. Valgrind: **"Invalid read of size 4 … inside a block of size 400
free'd"**, with both the free and the allocation stack traces. This is the
**dangling pointer**.

**(e)** Typically an immediate abort from the C library itself —
`free(): invalid pointer`. The allocator *can* detect this one unaided
because the failure is visible in its own bookkeeping: `data + 50` was never
a value `malloc()` returned, so whatever the allocator keeps about
allocations fails its consistency check the moment `free()` examines it. In
(c) and (d) the program abused memory *without asking the allocator
anything*, so the allocator had no opportunity to object — only a tool that
watches every access (valgrind) sees those.

**(f)** The lesson: **a memory bug is not a compile-time event and often not a run-time one either — it is a latent corruption that surfaces later, elsewhere, or never.** (a)–(d) compile silently and (b)–(d) frequently run to completion looking fine.
Why (e) is different: the invalid free passes a pointer the compiler can *see* is not the result of an allocation on that path, so `-Wfree-nonheap-object` fires. That is a purely local, syntactic judgement. The others need knowledge the compiler does not have — whether a pointer is still live, how much was allocated at a site far away, whether a value was ever written. Those are run-time properties, which is exactly why a tool like valgrind, watching actual execution, catches what the compiler cannot.

**B2.**
**(a)** Base = 32 KB = 32768; bounds (size) = 4096. Valid virtual addresses
are `0 … 4095`; physical = virtual + 32768.

| VA | Result |
|----|--------|
| 0 | 32768 (= 32 KB) |
| 1024 | 33792 (= 33 KB) |
| 3000 | 35768 |
| 4095 | 36863 — last legal byte |
| 4096 | **Fault** — VA ≥ bounds; equal-to-bounds is already out |
| 5000 | **Fault** |

The off-by-one at 4096 is the deliberate trap: the bounds check is
`VA ≥ bounds ⇒ fault`.

**(b)** **Size form:** bounds holds **4096**; the hardware checks the
*virtual* address first (`VA ≥ 4096 ⇒ fault`), then adds the base. **End
form:** bounds holds **36864** (= 32768 + 4096, first illegal physical
address); the hardware adds the base first, then checks the *physical* result
(`PA ≥ 36864 ⇒ fault`). Equivalent because the base is a constant during the
process's run: the check is the same inequality shifted by 32768 — each form
accepts exactly the virtual addresses `0 … 4095`.

**(c)** The 4 KB space must lie entirely inside 64 KB of physical memory:
**base ≤ 65536 − 4096 = 61440 (= 60 KB)**. Any higher and the top of the
address space would translate past physical memory.

**(d)** A generated VA is valid iff it is `< l`, and VAs are uniform over
[0, 4096), so the valid fraction is **l / 4096** — linear from 0 (at `l = 0`)
to 1 (at `l = 4096`). The sketch is a straight line through the origin with
slope 1/4096. This is the chapter's homework question 5, answered analytically
rather than empirically.

**(e)** **Internal fragmentation** — waste *inside* the allocated unit.
Base-and-bounds allocates one contiguous slot per process and translates with
a single base; there is no way to name "the middle 1 KB" separately, so the
gap between heap and stack cannot be given to anyone else. This is precisely
what motivates splitting the address space into independently-placed pieces —
segmentation, week 7.

**B3.**
**(a)** Base-and-bounds is **run-time binding**: every reference is translated
at the moment it executes. It needs per-CPU base and bounds registers,
add-and-compare circuitry in the MMU on *every* access, privileged
instructions to load the pair, and an exception path for failed checks — none
of which compile-time or load-time binding require, since both fix all
addresses before execution.

**(b)**
- **Compile time:** coexistence only by prior arrangement (every program
  compiled for a disjoint fixed range) and with no enforcement — a bad
  address still lands anywhere; moving requires recompiling. Verdicts: no
  and no.
- **Load time (static relocation):** load anywhere — the loader offsets every
  address once — so arbitrary programs *coexist by placement*, but nothing
  polices run-time addresses, so they do not coexist *safely*; and once
  rewritten the program is effectively immovable, because addresses baked
  into the image are indistinguishable from ordinary integers, so no one can
  reliably find and rewrite them again. Verdicts: only unsafely, and no.
- **Run time (dynamic relocation):** safe coexistence (bounds check every
  access) and movable (see (c)). Verdicts: yes and yes — paid for with
  per-access hardware.

**(c)** 1. **Deschedule** the process. 2. **Copy** its address space from the
old physical location to the new. 3. **Update the saved base** in the
per-process structure (PCB) — bounds is unchanged, since the size didn't
change. 4. On next dispatch the OS loads the new base into the hardware and
the process resumes, oblivious. The move is possible *because* nothing inside
the image encodes its physical position — the base register is the single
point of truth.

**(d)** The linker's two jobs: **symbol resolution** — bind every reference to
exactly one definition across the set of relocatable object files — and
**relocation** — fix up each reference once the pieces have been assigned
positions in the output. Under dynamic relocation the linker can lay every
program out from (virtual) address 0, because *physical* placement is no
longer the linker's or the loader's problem: the OS chooses it at run time by
setting the base. This is why ch. 15 says each program is "written and
compiled as if it is loaded at address zero." (This binding-time vocabulary —
compile/load/run — is exactly how Cambridge phrases it.)

---

## C. Discussion and design critique

**C1.** Any two coherent designs, honestly compared. The canonical pair:

- **A header before each block.** `malloc(n)` actually allocates `n + h`
  bytes and stores the size (and perhaps a magic number) in the first `h`
  bytes, returning a pointer just past it. `free(p)` finds the size at
  `p − h` in O(1). Space cost: `h` (≈ 8–16 bytes) per allocation — painful
  for many tiny allocations. Against B1(e): `data + 50` points into the
  middle of a block, so the "header" read there is garbage; a magic-number
  check catches it *probabilistically* (this is roughly how the C library
  produced its `invalid pointer` abort), but a plausible-looking garbage
  header can defeat it.
- **A separate table** (e.g. a hash map from pointer → size) kept by the
  allocator. Space cost: one entry per live allocation, away from user data,
  so an overflowing write (B1c) can't trample the metadata. `free(p)` costs a
  lookup — slower than p − h, still amortized O(1). Against B1(e): *robust* —
  `data + 50` simply isn't a key in the table, so the invalid free is
  detected with certainty, not probabilistically.

Marking note: full credit needs the trade-off stated on all three axes and a
committed choice with a reason (headers for speed and simplicity is the
defensible default; the table where robustness or tiny allocations dominate).
Any answer asserting the allocator "just knows" the size earns nothing — that
was A1.

**C2.** Sorting the catalogue is the skeleton of the answer:

- **Eliminated:** dangling pointers, double free, invalid free — there is no
  `free()` to misuse, and the collector only reclaims what is provably
  unreachable.
- **Transformed:** forgetting to allocate and buffer overflow don't vanish —
  memory-*safe* languages catch out-of-bounds accesses, but that is a
  property of bounds checking, not of garbage collection; a GC'd but unsafe
  language still overflows. Uninitialized reads are typically transformed
  (fields default to zero) rather than made meaningful. Credit for
  separating "GC" from "memory safety" — the colleague conflates them.
- **Untouched:** **leaks** — ch. 14 says it directly: "if you still have a
  reference to some chunk of memory, no garbage collector will ever free it."
  A growing cache or forgotten list leaks in Java exactly as in C.

Judgement: for most new *application* code the claim is close to right — the
eliminated class is the dangerous one, and the costs (collector CPU time,
pause behaviour, memory headroom) are usually affordable. The conditions
under which it fails: hard latency bounds, tight memory, and — the **kernel**
case — code that must run *beneath* the runtime a collector assumes: the
kernel's own allocator can't be built on a facility that needs an allocator,
and week 1's A7 point recurs — the implementation language must not assume an
OS underneath. Accept answers noting that systems languages now offer a third
way (compile-time ownership rather than run-time collection) if kept at that
level; the course returns to this in week 26.

**C3.** A strong answer concedes what is real, then locates the failures.

**What the checks cost and where.** Every store and every computed jump gains
a few instructions (mask or check against the region; the paper dedicates
registers to keep the sequence short and unforgeable). The cost lands on
*all* confined code *all* the time — a continuous tax on execution, in the
low single-digit percent range on the paper's workloads — in exchange for
making cross-domain transfer nearly free. Base-and-bounds prices things the
other way: translation is free-ish per access (hardware adder), and the cost
concentrates at domain crossings (trap, register save/restore). So the
proposal is really a bet that **crossings are frequent relative to ordinary
execution**. The engineer's speed claim for cross-program calls is genuine —
it was the paper's own motivation (cheap communication between fault
domains).

**Trust.** The compiler must *not* be trusted, and the paper's structure says
so: the transformation may be done by a compiler, but a small **verifier
checks the binary at load time** — only the verifier is in the trusted base.
An answer that leaves the compiler trusted has missed the paper's central
move. (Thompson's argument for why you'd want this arrives in week 20; no
credit expected for anticipating it.)

**Programs the toolchain didn't build.** They can't run as-is: unverified
code in a shared address space is unconfined by construction. Everything must
pass the verifier — so legacy and third-party binaries must be rewritten,
recompiled, or refused. The MMU has no such adoption problem: it confines
code it has never seen.

**Blast radius.** With hardware protection deleted, one bug in the verifier
(or one unverified escape path) compromises *the entire machine* — there is
no second fence. Under an MMU, an SFI bug costs one process's address space.
Defence in depth argues for hardware underneath software isolation, not
instead of it.

**"We save the hardware."** Illusory on commodity CPUs: the MMU ships whether
or not you use it, so nothing is saved; the claim only has content on parts
genuinely without memory management (small embedded cores).

**Where the design is right.** (1) MMU-less hardware, where it is the *only*
option; (2) confining extensions *inside* a process that already exists —
plugins, JIT'd or downloaded code, module sandboxes — where a hardware
boundary is too coarse and a per-call trap too slow; deployed browser
sandboxes are this design's descendants. In both, SFI complements rather than
replaces the OS's protection.

**Recommendation.** Reject as the machine's sole protection mechanism on
commodity hardware: keep the MMU as the base isolation layer, and deploy
SFI selectively where crossing costs dominate or inside processes. Conditions
that would change the verdict: hardware with no MMU; a closed world where
every binary provably passes one verifier; or a workload so
communication-dominated that trap costs are the bottleneck — and even then,
the blast-radius argument should make you want the hardware fence too.

*Marking note: the strongest answers concede the cheap-crossing point and the
embedded case rather than dismissing the proposal, correctly place the
verifier (not the compiler) in the trusted base, and price "save the
hardware" honestly. An answer that only says "software is slower than
hardware" has both the economics and the paper wrong.*
