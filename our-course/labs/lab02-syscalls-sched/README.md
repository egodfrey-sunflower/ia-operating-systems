# Lab 2 — xv6 system calls & scheduler

**Weeks 4–6 · 13.5 hours · OSTEP ch. 6–10 · xv6 rev5 ch. 4 (Traps and system calls) and ch. 8 (Scheduling)**

This is the first lab that edits a kernel. Everything runs on xv6-riscv under
QEMU, with the RISC-V cross-compiler you installed in Lab 0; if `make qemu`
worked there it will work here. No root.

You will add two system calls and thread each of them through every layer
between a user program and the kernel function that does the work. Get one
layer wrong and there is no compiler error and no message — the call simply
returns the wrong thing, or the kernel dies. Part 1 exists so that you know
what all the layers are before you start adding to them.

## Layout

```
lab02-syscalls-sched/
  README.md          this handout
  setup.sh           makes you a fresh xv6 tree to work in   <- run this first
  starter/overlay/   the files setup.sh lays over stock xv6
  tests/run.sh       the autograder: builds your tree, boots it, scrapes the console
  solutions/         SPOILERS. Reference kernel + the answer key. Afterwards.
```

The pinned xv6 checkout at `../xv6-riscv/` is never modified. `setup.sh` copies
it and lays `starter/overlay/` on top; the copy is what you edit, so starting
again from a clean kernel is one command:

```sh
./setup.sh ~/lab2          # refuses if ~/lab2 exists; -f replaces it
cd ~/lab2
make                       # POLICY=rr by default
make qemu                  # Ctrl-A then x to quit
```

Build **single-job**. `make -j` on a machine that is also about to run a 128 MB
emulator buys seconds and costs the run.

`make` on its own builds the kernel and stops there: the file system image the
user programs live in is `make qemu`'s dependency, not `make`'s. So the first
emulator session after a change to any user program spends a few extra seconds
building `fs.img` before QEMU starts, and a compile error in a user program
surfaces there rather than at the `make` you thought was the build.

The tree you get builds and boots as it stands. It is stock xv6 plus this lab's
stubs, headers and test programs, and every system call the lab adds returns
−1 until you write it.

Leave this lab directory where it is; the autograder takes your working tree as
an argument:

```sh
tests/run.sh ~/lab2
```

`solutions/` is deliberately left alone until you are done.

## What you hand in

| File | Part | Weight |
|---|---|---|
| `SYSCALL-TRACE.md` — the annotated call chain | 1 | 11% |
| the patched tree — `trace` | 2 | 22% |
| the patched tree — `sysinfo` | 3 | 19% |
| the patched tree — lottery scheduling | 4 | 30% |
| the patched tree — MLFQ — plus `SCHED-RESULTS.md` | 5 | 18% |

Parts 2 to 5 are machine-checked by `tests/run.sh`, which builds your kernel
once under each of the three scheduler policies and boots each of those
kernels several times — one boot per test program. The two prose
deliverables — Part 1's `SYSCALL-TRACE.md` and Part 5's `SCHED-RESULTS.md` —
you mark yourself against the key in `solutions/README.md`, **which you read
afterwards, not before.**

---

# The specification

Anything below that says **must** about the behaviour of your kernel is
something `tests/run.sh` checks. Part 1 is prose: the **must**s in its skeleton
are marking criteria you apply by hand, not machine checks.

## `trace(int mask)` — Part 2

Sets the calling process's trace mask and returns 0. Bit *N* of the mask means
"print a line for system call number *N*", where the numbers are the ones in
`kernel/syscall.h`, so `trace(1 << SYS_getpid)` traces `getpid` and nothing
else.

While a bit is set, the kernel must print exactly one line **after** each
matching call completes, on the console, in exactly this form:

```
<pid>: syscall <name> -> <return value>
```

so a traced `getpid` in process 3 prints

```
3: syscall getpid -> 3
```

and a `close(99)`, which fails, prints `-1` — the value the call **returned**,
not the value it was passed. The return value is printed as a signed 32-bit
decimal.

The mask must be:

- **selective** — calls whose bit is clear print nothing;
- **per process** — one process's mask must not affect another's;
- **inherited by `fork`** — a child starts with its parent's mask, and a
  process that has never called `trace` has an empty one;
- **clearable** — `trace(0)` turns it off again.

Nothing else this lab adds is inherited across `fork`.

## `sysinfo(struct sysinfo *info)` — Part 3

Fills in the caller's `struct sysinfo` (declared in `kernel/sysinfo.h`, which
you are given) and returns 0:

| field | meaning |
|---|---|
| `freemem` | bytes of free physical memory — free pages × `PGSIZE` |
| `nproc` | process-table slots whose `state` is not `UNUSED` |

`freemem` is a byte count, not a page count, so it must be a multiple of
`PGSIZE`, and it must fall when a process allocates memory and rise when that
memory is freed.

`sysinfo` **must return −1, and must not take the kernel down,** when the
pointer is one the kernel may not write through: above the top of the address
space, unmapped, or mapped read-only. You do not test the address yourself —
`copyout` answers that question, and answering it any other way is the bug this
part exists to prevent.

## `settickets(int n)` — Part 4

Sets the calling process's lottery ticket count and returns 0. It **must
return −1** for an `n` outside `[1, MAX_TICKETS]`; `MAX_TICKETS` is in
`kernel/sched.h`. It should change nothing when it does — nothing checks that
half, because no later call can tell a rejected write from one that never
happened; it is the honest implementation, and the damage a stored
out-of-range count does downstream *is* checked, hard.

Every process is born holding `DEFAULT_TICKETS` (also in `kernel/sched.h`),
including the first user process and every child of a `fork` whose parent
never called `settickets`. Ticket counts are **not** inherited: a child of a
process holding a thousand tickets holds `DEFAULT_TICKETS`, like everybody
else.

## `getpinfo(struct pinfo *info)` — Part 5's evidence, written in Part 4

Fills in the caller's `struct pinfo` (declared in `kernel/pinfo.h`, which you
are given) and returns 0, or −1 for a pointer the kernel may not write
through — the same rule, and the same mechanism, as `sysinfo`.

`struct pinfo` has one entry per slot of the process table, in table order:

| field | meaning |
|---|---|
| `inuse[i]` | 1 if slot *i*'s state is not `UNUSED` |
| `pid[i]` | the process id, or 0 |
| `tickets[i]` | lottery tickets held |
| `ticks[i]` | how many times `scheduler()` has chosen this process |
| `prio[i]` | MLFQ level, 0 = highest; 0 under the other two policies |

`ticks[i]` **must be counted under every policy**, including round robin —
the whole point of Part 5 is comparing the three, and a counter that only
works under one of them measures nothing. Count it in the one place that
knows a choice has been made: where `scheduler()` sets a process `RUNNING`.
`tests/run.sh` runs `schedtest` under all three policies for exactly this. Under
`rr` and `mlfq` it asks only the question that makes sense there — did the
counter move at all? — and there is a case for each. A counter that lives in
one branch scores both of them zero, and would otherwise leave two of the
three rows of your own `SCHED-RESULTS.md` table empty.

It is a counter of *scheduling decisions*, not of time. On this kernel they
are almost the same thing, because a process that does not block runs until
the timer interrupt takes it away, but they are not the same thing, and Part
5's write-up is a better piece of work if it says so. They come apart for a
process that gives the CPU up **before** its tick is over, and `mlfqtest`'s
own sampling parent is one: it is chosen once a sample and blocks again
immediately, so its `ticks[]` rises once a sample under a counter of
decisions and not at all under a counter incremented in `yield()`. There is a
case on that, and it is the one that tells the two apart.

## Traps that apply to both

> **The trap.** Several places have to agree about a new system call, and the
> build catches only some of a disagreement. Get one of them wrong and the
> tree can still compile, link, boot and run a shell — and go wrong only at the
> moment something actually makes the call. Part 1 is the map of which places
> those are.

> **The trap.** A system call's arguments are fetched with
> `argint`/`argaddr`/`argstr`, never by reading anything the user handed you.
> `argaddr` gives you a **number that a user process chose**. It is not a
> pointer the kernel may follow. `copyout` is how the kernel writes to it, and
> `copyin` is how it reads from it.

**Locking, given.** xv6's locking rules are a later chapter, and deriving them
is not this lab's exercise, so the pattern you need is handed over outright: to
walk the process table, take `p->lock`, look, release it, and move on — one
lock at a time, never two at once, and never while already holding another
process's lock. `kernel/sysproc.c` repeats this where you need it.

## Things that are given, and that the autograder checks you left alone

**What follows is the whole list.** Everything else in the tree is yours,
including files that are not in it yet: writing your own user programs and
adding them to `UPROGS` is expected, and several parts of this lab are easier
if you do. Nothing checks the tree for files it has not seen before.

Three kinds of frozen thing. The first two have a case each; the third the
harness enforces at boot, before any case runs:

- **`user/tracetest.c`, `user/sysinfotest.c`, `user/schedtest.c` and
  `user/mlfqtest.c`.** Every Part 2–5 verdict is scraped from what these four
  print, and they sit in your tree where you can edit them, so the grader
  compares them byte for byte against the copies in `starter/overlay/`. Edit
  one and its *"Build: … is unmodified"* case fails, whatever your kernel
  does. If you have already changed one, copy it back or re-run `setup.sh`
  into a fresh directory.
- **`-Werror` in the `Makefile`.** The lab is graded with it on, so turning it
  off to get past a warning fails a case rather than solving one.
- **`kernel/main.c`'s `sched: policy=…` line.** `tests/run.sh` reads it during
  boot to confirm it is talking to the kernel it just built, and refuses to
  start a test session without it. It reports the build flag, not your
  scheduler. Leave it in. (If every kernel case suddenly reports that the
  kernel did not survive, and the evidence line mentions waiting for
  `sched: policy=rr`, this is what happened — look at `main.c`, not at your
  system call.)

One more thing is given, and it is there to help rather than to police you.
The bottom of `scheduler()`'s idle branch, just before the `wfi`, counts the
`RUNNABLE` processes it declined to run and prints

```
sched: nothing chosen but 2 runnable
```

the first time it finds any. Both Part 4 and Part 5 begin from a branch that
chooses nobody at all, and that much is at least loud: the boot stops at
`init: starting sh` and no prompt ever appears.

The case the line really earns its keep on is the other one, and it is the
reason to leave it in. A scheduler that declines to choose only *sometimes* —
a total that included a `SLEEPING` process, so the drawn ticket occasionally
belongs to nobody; a level loop that stops one level short — **still boots,
still gives you a prompt, and still runs everything you type.** It loses the
odd tick to `wfi` and is otherwise indistinguishable from a working kernel by
eye. Nothing about it looks wrong, the tests that fail are the statistical
ones, and the failure you go and investigate is a share that is slightly off.
This line is the only thing in the system that says what actually happened,
and it says it once, the first time. `tests/run.sh` treats it as fatal and
fails the part in seconds rather than at the far end of a deadline. Read it
when it appears: it means the choosing code above it did not choose — not
that the machine hung, and not that the machine is fine.

---

# Part 1 — Trace a system call end to end (~1.5 h, week 4, 11%)

**No code.** Follow one existing system call — `write` — from the user program
that calls it to the kernel function that does the work, and back, and write
down what you find.

Read it in the source first, then watch it happen under GDB. `make qemu-gdb` in
one terminal and `riscv64-unknown-elf-gdb` (or `gdb-multiarch`) in another is
the setup from Lab 0. Two practical notes before you start, both of which cost
a detour otherwise:

- the `.gdbinit` that connects GDB to the emulator is *generated*, so it does
  not exist in a fresh tree until `make .gdbinit` or `make qemu-gdb` has run.
  Modern GDB then declines to auto-load it and says so; Lab 0 covers the
  `add-auto-load-safe-path` line that fixes it, and the alternative of
  connecting by hand.
- **break on a line, not on the function.** `break syscall` stops at the
  opening brace, *before* `struct proc *p = myproc();` has run, and the kernel
  is compiled with `-O`, so every `print p->trapframe->...` there answers
  `value has been optimized out` — and a *conditional* breakpoint on that
  address fails its condition, which aborts a whole `-x` command file with
  `Error in testing condition for breakpoint 1`. Break on the dispatch line
  instead. In a fresh tree that is `break syscall.c:138`, the
  `if(num > 0 && ...)` line; check the number, because it moves as soon as you
  start adding to the file. (`break syscall` followed by `next` twice gets you
  to the same place.) From there `print num` works, and so does everything
  through `p`.

A breakpoint on that line and one on `sys_write` is enough to see everything
below.

You are not expected to already understand the page-table switch or the
trampoline page — virtual memory is Lab 4, and xv6 ch. 4 is your week-5
reading. For those steps, say what happens and give a one-line reason; the
mechanism arrives later.

## The deliverable

`SYSCALL-TRACE.md`, following this skeleton. Every heading below must name
**the file and the function or symbol**, and answer **where the arguments are**
— meaning: in which registers, at which offsets in which structure, or at which
addresses. "In `a0`" is an answer. "Passed along" is not.

```markdown
# write() end to end

`printf("hi\n")` in a user program, down to the disk-free part of the kernel
and back.

## 1. The user-side prototype
file, symbol. What the compiler emits at the call site, and which registers
the three arguments are in when the stub is entered.

## 2. The stub
file (and the file it is generated into), symbol. The two instructions before
`ret`. Which register carries the system call number, and where that number is
defined.

## 3. The instruction that changes privilege
Which instruction. What the hardware does to the privilege level, to `sepc`,
to `scause`, and to the program counter — and which register tells it where
to jump. What happens to a0–a2: are they copied, saved, or untouched?

## 4. The trap entry point
file, symbol. Where the 32 user registers are put — name the structure and say
how the kernel finds it. Which page table is in force when this code starts,
which when it ends.

## 5. The C trap handler
file, function. How it tells a system call from a device interrupt from a
fault. What it does to the saved user program counter, and why the number it
adds is what it is.

## 6. The dispatch
file, function. Which field of which structure the system call number is read
out of. Name the two tables involved and the file each lives in. Where the
return value is put.

## 7. The kernel implementation
file, function. Each of the three arguments in turn: which helper fetches it,
which register that helper reads, and — for the buffer pointer — whose address
space that address belongs to and what the kernel is allowed to do with it.

## 8. The way back
The three steps that get the return value into the caller's `a0` and the
program counter back to the instruction after the trap. Name the file and
symbol for each.

## 9. Seen under GDB
One transcript. Break somewhere in the path, print the system call number and
the arguments out of the trapframe, and show the values. Say which breakpoint
and which commands you used.
```

Sections 1–8 have specific right answers and are marked against the key.
Section 9 is marked on whether you actually ran it: your addresses will differ
from anyone else's and are not the point.

---

# Part 2 — Add `trace` (~3.0 h, weeks 4–5, 22%)

Implement `trace(mask)` to the specification above.

Do the user-side plumbing in week 4 — the prototype, the `usys.pl` entry, the
system call number, the `UPROGS` line — and the kernel side after you have read
xv6 ch. 4 in week 5. The starter marks each place with a `TODO Part 2`.

The kernel side is one new field and three edits:

- a field in `struct proc` (`kernel/proc.h`), initialised in `allocproc` and
  copied in `fork` (`kernel/proc.c`);
- `sys_trace` in `kernel/sysproc.c`, which records the mask and returns;
- the print itself in `syscall()` in `kernel/syscall.c`.

> **The trap.** The print goes in `syscall()`, not in `sys_trace`, and it goes
> **after** the dispatched function returns. It has to: the line contains the
> return value, and nowhere else in the kernel knows both the system call
> number and its result. One hook in the dispatch path covers every system call
> there is, which is the whole reason a dispatch table is worth having. You
> will also need a second table — the call names — indexed exactly like the
> first one, in the same file. Add a name whenever you add a number, or a
> traced call reads off the end of the array.

Test it by hand with `tracetest`, and once with a mask containing `SYS_write`:
xv6's user-space `printf` calls `write` once per character, so the console
fills with trace lines. That is worth seeing exactly once, because it is proof
the hook really is in the dispatch path and not somewhere special-cased.

`tracetest` will not do that one for you — it deliberately leaves `write` out
of its masks, because a test whose own output is traced cannot be read. So
write yourself a five-line user program that calls `trace(1 << SYS_write)` and
then prints something, and add it to `UPROGS` beside the others. That is a
normal thing to do to this tree, not a transgression: the frozen list above is
the whole of it, and a program of your own is not on it. You will want the
same trick again in Parts 3 to 5 whenever you would rather ask the kernel one
question than run a whole measurement.

`user/tracetest.c` is given and must not be edited — the autograder matches its
markers line by line, and checks the file itself against the shipped copy.

---

# Part 3 — Add `sysinfo` (~2.5 h, week 5, 19%)

Implement `sysinfo(struct sysinfo *)` to the specification above. The system
call number, the prototype and the `usys.pl` entry are given this time: you did
that traversal in Part 2 and doing it again teaches nothing. The kernel side is
yours.

You need two counts the kernel does not currently keep, and both live behind a
`static` in the file that owns the data:

- **free memory.** `kmem` is `static` to `kernel/kalloc.c`, so the count has to
  be a new function there. The free list is the allocator's only record of what
  is free, so walking it is the only honest answer.
- **processes.** `proc[]` is `static` to `kernel/proc.c`, so the count has to
  be a new function there, using the locking pattern given above.

Declare both in `kernel/defs.h`, next to the other functions from their file.

> **The trap.** This is the part about the *direction* of the copy. Everything
> the kernel has handed a user process so far has gone back in `a0` — a single
> register the hardware moves for you. A structure does not fit in a register,
> so the kernel has to write into the user's address space, at an address the
> user chose. It cannot simply assign through that pointer: the kernel is
> running on the kernel page table, where user addresses mean something else or
> nothing at all, and a bad address in the kernel is a panic, not a signal.
> `copyout(p->pagetable, addr, src, len)` walks the process's own page table,
> which is what makes it able to say no.

`user/sysinfotest.c` is given and must not be edited — the autograder checks it
against the shipped copy too. It checks itself and
prints `sysinfotest: OK` when everything passed; the autograder reports each of
its groups of checks as a separate case.

---

# Part 4 — A lottery scheduler (~4.0 h, weeks 5–6, 30%)

Replace round robin with lottery scheduling, as in the Waldspurger paper:
every runnable process holds some tickets, the scheduler draws a ticket at
random on each scheduling decision, and the holder of the drawn ticket runs.
Over a long enough run each process gets the fraction of the CPU its tickets
bought.

**Read the paper before you start.** The lab builds precisely what it
describes, and the interesting part of it is the argument about why a
randomised policy needs no accounting to stay fair — which is also why the
check on your work has to be statistical.

## Choosing the policy

The policy is a **compile-time** flag, not a run-time one:

```sh
make clean && make POLICY=rr        # round robin, stock xv6, the default
make clean && make POLICY=lottery   # Part 4
make clean && make POLICY=mlfq      # Part 5
```

`kernel/sched.h` turns those into `SCHED_POLICY`, and `scheduler()` in
`kernel/proc.c` selects between the three with `#if`. All three branches are
in the same function and all three are compiled from the same source file;
only one of them exists in any given kernel.

> **The trap.** A changed compiler flag does not make anything look out of
> date. `make POLICY=lottery` on a tree that was built as `rr` relinks nothing
> and boots the round-robin kernel, and the boot line still says
> `sched: policy=lottery`, because that line comes from the flag. Always
> `make clean` first. `tests/run.sh` does, three times.

## What to write

- **Fields in `struct proc`** (`kernel/proc.h`): the ticket count, and the
  count of times this process has been chosen. `p->lock` protects both.
  Initialise them in `allocproc` — every process in the system is born there,
  including the first — and do **not** copy them in `fork`.
- **`sys_settickets` and `sys_getpinfo`** (`kernel/sysproc.c`), to the
  specification above. `getpinfo` needs a helper in `kernel/proc.c` to walk
  `proc[]`, for the same reason `sysinfo` did: `proc[]` is `static` to that
  file.
- **The lottery itself** (`kernel/proc.c`, the `SCHED_LOTTERY` branch of
  `scheduler()`). Two passes over the process table:
  1. add up the tickets held by every `RUNNABLE` process;
  2. draw `rand() % total`, then walk the table again accumulating tickets
     until you pass the drawn number. That process wins.

  `rand()` is given, in `kernel/rand.c`, because xv6 has none and writing one
  is not the exercise. Take `p->lock` one entry at a time in both passes,
  exactly as the round-robin loop does.

Notice what you are **not** touching: `swtch`, the context, the trapframe, the
trap path, `sched()`, `yield()`. Choosing is policy; switching is mechanism,
and the whole of the mechanism is already there and stays there. That
separation is the reason this part is forty lines rather than four hundred.

> **The empty case.** `total` can be zero — nothing is runnable at some
> moments, which is normal — so the draw only happens when `total > 0`.
> `rand() % 0` has no defined answer, and there is nobody to select anyway.

## The measurement, and exactly what is asserted

`user/schedtest.c` is given, and must not be edited. It gives itself a large
ticket count (a measuring instrument should not be starved by the thing it
measures), forks two children that do nothing but spin, gives one **30**
tickets and the other **10**, and reports how many times each was chosen —
as the difference between two `getpinfo()` snapshots, so that the ticks the
children spend starting up are outside the window.

It then does the same thing again with two children holding **one** ticket
each, over a quarter of the window, and reports that too.

```sh
schedtest        # 400 ticks for the 30:10 window, 100 for the 1:1 one
schedtest 100    # shorter, and visibly noisier
```

It also forks one child that does nothing at all and prints what that child
was born holding, which is how the grader sees whether `allocproc` was given
its defaults, and passes `getpinfo` an address the kernel may not write
through, which must come back −1 and not 0.

**The assertion.** Over the default 400-tick window, A's share of the
selections the two children received between them must land within **100 per
mille** of the 750 its tickets bought — that is, between **650 and 850** per
mille, a ratio between about 1.9∶1 and 5.7∶1.

That band is wide, and deliberately: a lottery is random, so over a finite run
the share is a sample, not a value. At 400 ticks its standard deviation is
about 22 per mille, so the band is about four and a half standard deviations
either side — a correct kernel lands outside it roughly once in a million
runs. A band tight enough to insist on 750 ± 20 would fail correct kernels
about a third of the time, which is worse than not checking at all.

It is still a narrow enough band to catch what actually goes wrong. A
scheduler that ignores ticket counts lands on 500, eleven standard deviations
low. One that simply always runs the highest-ticket runnable process — which
sounds proportional, and is not — lands on 1000, and starves everything else
in the system while it does it.

**The other assertion, half of which is exact.** In the 1:1 window, **both**
children **must** be selected at least once, and the first one's share must
land within 200 per mille of 500.

The first half of that is not statistics at all — "both of them ran" is exact,
and it is the half worth having, because the interesting off-by-one in the
ticket walk is invisible to any statistical check. An off-by-one in how the
walk hands out the ticket ranges shifts every range by one. At 30:10 that
moves the share by 25 per mille and nothing could ever see it. At 1:1 it can
give the first child **every** draw and the second **none** — a process holding
a ticket, starved outright, for as long as it lives.

The *share* half is the opposite: it is the noisiest number this lab prints,
and knowing that will save you an evening. The 1:1 window is a quarter of the
run length with a floor of 40 ticks, so it is **100 selections** at the graded
default and **40** at any run of 160 ticks or fewer. At 100 selections one
standard deviation is 50 per mille and the ±200 band is four of them; at 40 it
is 79, the band is two and a half, and a correct kernel is outside it about
one run in a hundred. On the reference kernel, three `schedtest 400` at one
prompt give a 1:1 share of 460, 490 and 440; four `schedtest 100` give
**275**, 575, 350 and 575. The 275 is the first run after the boot, and so —
by the reproducibility below — it is the one you get back every time you boot
and type `schedtest 100` first: well outside 500 ± 200, on a kernel that
passes every case in the lab, and no amount of rebooting will shake it loose.
It is not a bug. It is 11 selections against 29 out of 40. So:

- iterate with `schedtest 100` — but read only *"were both C and D selected?"*
  from its 1:1 window, and ignore the share;
- the share there becomes worth reading at the graded length, `schedtest 400`
  or longer, which is also the only length the case is ever run at;
- and if the graded 1:1 case is the single thing that failed, re-run it before
  you go looking. The 30:10 share, at 400 selections and 4.6 standard
  deviations of band, is the one that does not do this.

One thing that will surprise you: **the first `schedtest` after every
`make qemu` gives exactly the same answer**, to the selection. The generator
is seeded to the same constant at every boot and the schedule is
deterministic, so a fresh boot of a given kernel is reproducible — if the
number is a little away from 750, that is a property of your kernel and not
of your luck, and re-booting will not shake it loose. On the reference kernel
that first number is **755** per mille, and it is the only number in this
section you should expect to reproduce exactly.

The **second** and third runs at the same prompt are a different matter: they
draw from a later stretch of the same sequence, and they wobble. How far along
that stretch they start depends on how many draws the kernel made in between,
which depends on everything else that ran — so their values are yours, not a
property of the policy, and a reference number for them would be a number you
could not check. That is not non-determinism and there is no race to go
looking for: each run draws from a later stretch of the same fixed sequence,
and what you are watching is the sampling error the band exists to absorb —
the only way to see that error at all on a kernel whose first run is fixed. So
run each length three or four times at one prompt: it is the *spread* of
those numbers, not any single one of them, that shrinks as the run
lengthens, and Part 5's write-up asks you for exactly that.

If the case fails, the message prints your observed share and ratio next to
the expected ones, and the raw selection counts they were computed from.

**Watch the run get more accurate as it lengthens.** `schedtest` prints a
sample half way through as well as the total, and `schedtest 40` three or
four times over, next to `schedtest 400` three or four times over, is the
clearest thing in this lab: the same policy, the same tickets, and a spread
that shrinks. Keep those numbers — Part 5's write-up asks for them.

---

# Part 5 — MLFQ behind a policy switch (~2.5 h, week 6, 18%)

Add a multi-level feedback queue as the third policy, selected by
`make POLICY=mlfq`. The lottery code stays exactly where it is; the two live
side by side in the same `#if`.

`kernel/sched.h` gives you the constants, and the tests assume these exact
numbers: `NPRIO` = 4 levels, level 0 highest; `MLFQ_ALLOTMENT(prio)` =
`1 << prio`, so 1, 2, 4 and 8 ticks; `MLFQ_BOOST` = 40 ticks between
priority boosts.

Level 0 is the **highest** priority here, and stays so throughout this lab.
Supervision sheet 4 numbers its queues the other way up — Q2 highest, in
OSTEP's convention — so a trace you drew there reads upside down against a
`prio` printed by this kernel. Same policy, opposite labels.

The five rules, in the form this kernel needs them:

1. Higher priority runs first.
2. Equal priority takes turns — round robin **within** a level.
3. A new process enters at the top level. (That is `allocproc` again:
   `prio = 0`, allotment used = 0.)
4. A process that uses up its allotment at its level is demoted one level.
   At the bottom level it stays there.
5. Every `MLFQ_BOOST` ticks, every process goes back to level 0 with a fresh
   allotment.

## Where each rule goes

**Rules 1 and 2** are the selection, in the `SCHED_MLFQ` branch of
`scheduler()`: run the first `RUNNABLE` process at the highest-priority
non-empty level, and carry the scan on from where it stopped last time rather
than restarting it at slot 0, or the low-numbered table slots quietly win
every tie.

**Rule 4** is the accounting, and it belongs in `yield()`. (Rule 3 is
`allocproc`'s, as above.)
`yield()` is reached from exactly two places, both in `kernel/trap.c`, and
both mean "the timer interrupt went off while this process was running". So
one call is one whole tick of CPU consumed — and a process that blocks in a
system call before its tick is up never reaches that line at all. That single
fact is the entire mechanism by which an interactive process stays near the
top and a compute-bound one sinks, and it is worth staring at until it is
obvious.

**Rule 5** belongs at the **top of `scheduler()`'s loop, before the choice**,
driven by the global tick counter (`extern uint ticks;`, defined in
`kernel/trap.c`): when `MLFQ_BOOST` ticks have passed since the last boost,
put every process back at level 0 with its allotment reset.

## The check

`user/mlfqtest.c` is given, and must not be edited. It forks **two** children
that never block and samples their levels once a tick through `getpinfo` —
and, because it blocks in `pause()` on every one of those ticks, it is itself
the interactive process the five rules exist to serve. Five things have to
show up in the samples, and `tests/run.sh` has a case for each:

- a child **sinks** — it is seen at the bottom level, which needs Rule 4;
- and then, **without ever having blocked**, it is seen above the bottom
  level again, and so is the other one, in the *same* sample. Nothing but
  Rule 5 can do the first, and nothing but a Rule 5 that sweeps **every**
  process can do the second;
- a child stays above the bottom for **at least 15 per cent of the run**: it
  owns 1 + 2 + 4 = 7 of its own ticks after each boost before it reaches the
  bottom, and a kernel that demotes on every tick without consulting
  `MLFQ_ALLOTMENT` gets there in 3;
- the **sampling parent stays at the top level**, because a process that
  blocks before its tick is up is never charged an allotment at all;
- and a process forked *afterwards*, into the process-table slot a sunk
  child has just given back, starts at the **top** level rather than
  inheriting the level that slot was left at. That is Rule 3, and it is the
  one moment in the lab where a field `allocproc` forgot to reset is
  visible at all;
- and one thing that belongs to Part 4 and can only be seen here: the
  sampling parent's own `ticks[]` rises about once a sample. It is the only
  process in the lab that is *chosen* far more often than it *consumes a
  whole tick*, so it is the only place a `ticks[]` counted in `yield()` looks
  different from one counted where `scheduler()` sets a process `RUNNING`.

With two children sharing the CPU a child is above the bottom for about 14
ticks of every 40, so roughly one sample in three should catch it. The threshold
and the reference are both **percentages of the samples taken**, not counts:
the case asks for 15 per cent, and the reference kernel comes in at about 28
per cent — 55 or 56 samples of the default 200. Anything from the low forties
**of samples** upwards — about 21 per cent — is the same behaviour; a kernel
that demotes on every tick without consulting `MLFQ_ALLOTMENT` lands near 8
per cent and fails.

## `SCHED-RESULTS.md` — the deliverable

Run the **same** workload under all three policies and write up the
difference. Two or three pages of numbers and prose; the marking is a rubric,
in `solutions/README.md`, and it asks two things: did you measure, and does
your explanation match your own numbers?

```sh
make clean && make POLICY=rr      && make qemu    # then: schedtest 400
make clean && make POLICY=lottery && make qemu    # then: schedtest 400
make clean && make POLICY=mlfq    && make qemu    # then: schedtest 400, mlfqtest
```

Report at least:

1. **The three-way comparison.** `schedtest 400` under each policy: the two
   children's selection counts and A's share. The two children hold 30 and 10
   tickets in every case. Say what each policy does with that fact, and why
   — the answer for two of the three is "nothing", and the interesting part
   is that they ignore it for *different* reasons.
2. **Convergence.** Your lottery numbers at several run lengths — 40, 400 and
   something longer — each length run **three or four times at the same
   prompt**, since the first run after a boot is reproducible and one number
   on its own says nothing about spread. Does the spread shrink? By roughly
   what factor when you multiply the run length by ten, and does that match
   what you would expect of an average of independent draws?

   Budget for this one before you start it. A tick is a tenth of a second of
   emulated time, and `schedtest n` waits through rather more than *n* of
   them — the 1:1 window and the newborn check are on top — so `schedtest 400`
   is about a minute and three runs at 1200 ticks is around **eight minutes**
   of doing nothing in a single boot, with the emulator up for all of it. Do
   the 40s and
   the 400s first, decide from those what the longest length needs to be, and
   run it once, deliberately.
3. **MLFQ's behaviour.** What `mlfqtest` reports, and the shape of a child's
   level over time. How long after a boost does a compute-bound process take
   to reach the bottom again, in ticks, and does that match 1 + 2 + 4? Two
   children are sharing the CPU, so say what that does to the answer measured
   in ticks of the clock rather than ticks of its own.

   Note that `mlfqtest` does not print that time and cannot: it prints a
   histogram of how many samples found the child at each level. The answer has
   to be *derived* — from the allotments, from how many of the 40 ticks
   between boosts a child owns when it is one of two spinners, and from the
   histogram as the check on the arithmetic. That is the more interesting
   exercise of the two, and it is the one being asked for; a number read
   straight off a line of output would not have told you anything.
4. **One workload where MLFQ beats both others**, argued rather than
   necessarily measured: name it, say which rule does the work, and say what
   you would have to add to the lottery to get the same effect.
5. **What you could not measure, and why.** There is at least one thing —
   `ticks[]` counts scheduling decisions, not CPU time, and the two differ
   exactly when a process blocks before its tick is up. Say what that costs
   the comparison.

Answer 4 honestly. "MLFQ was better because it is adaptive" is not an answer;
naming the workload, the rule and the metric is.

---

# Running the tests

```sh
tests/run.sh ~/lab2
```

The policy is a compile-time flag, so this is not one kernel with three
behaviours — it is three kernels, and the grader builds all three:

| phase | built as | runs |
|---|---|---|
| 1 | `POLICY=rr` | `tracetest`, `sysinfotest`, `schedtest 100`, `usertests` |
| 2 | `POLICY=lottery` | `schedtest`, `usertests` |
| 3 | `POLICY=mlfq` | `mlfqtest`, `schedtest 100`, `usertests` |

The short `schedtest` in phases 1 and 3 is not grading the lottery under a
policy that has none. It asks the one thing the specification demands of all
three policies — that `ticks[]` is counted at all — and it is the row of your
own three-way table that phase is standing in for.

Every build is from clean, because a changed compiler flag makes nothing look
out of date and a half-rebuilt tree is a kernel nobody wrote. Each test
program then gets its own boot, so a kernel that panics in one of them still
gets graded on the others.

Parts 2 and 3 are graded under `rr` on purpose: they are not about
scheduling, and grading them under a scheduler you have just written would
report your scheduler's bugs as system-call bugs.

`usertests` dominates the clock — three times over — and how badly depends
entirely on your machine, since QEMU is emulating a RISC-V processor
instruction by instruction on one core. Three ways to run it:

```sh
tests/run.sh --fast ~/lab2     # Parts 2-5 only; no usertests at all
tests/run.sh ~/lab2            # + usertests -q under each policy  <- usual
tests/run.sh --full ~/lab2     # + the whole of usertests under each policy
```

Iterate with `--fast`; it counts all three regression checks as failures so
that a `--fast` transcript can never be mistaken for a finished run. Run
`--full` once before you hand in.

For scale, on one ordinary two-core Linux box the reference solution takes
about **2.5 min** with `--fast` — of which more than two minutes is
`schedtest` and `mlfqtest` simply waiting, so a faster machine will not save
you much of it — **10 to 14 min** in the default mode and about **50 min** with
`--full`. Take those as an order of magnitude, not a promise. The waiting is
irreducible: those two programs measure a scheduler over hundreds of timer
ticks, and a tick is a tenth of a second of emulated time whatever your
machine costs. Every case has a generous timeout on top, so a slow machine
fails nothing that a fast one passes.

Every case has a timeout, and a kernel that panics fails immediately with the
console tail rather than at the end of one. The harness runs one emulator at a
time and kills it on every exit path, including the ones you take with Ctrl-C.
If you ever suspect one has been left behind:

```sh
ps -eo pid,comm | awk '$2 ~ /^qemu-sys/'
```

and `kill` the pid. Match on the command *name* like that, not on the whole
command line — a pattern like `pkill -f qemu-system-riscv64` also matches the
shell you typed it into. Match `qemu-sys`, not `qemu`: a machine running the
QEMU guest agent has a `qemu-ga` on it that has nothing to do with this lab.

The output ends with a `N passed, M failed` line, and the script exits non-zero
if anything failed. There are **42** cases. Part 1 and Part 5's
`SCHED-RESULTS.md` are not counted there; they are prose.

---

# If you get stuck

- **GDB says `value has been optimized out` for everything through `p`, or
  `Error in testing condition for breakpoint 1`.** You broke on `syscall`
  rather than on a line inside it, so `p` is not assigned yet. Break on the
  dispatch line — see Part 1.
- **`schedtest 100` reports a 1:1 share nowhere near 500.** Expected: that
  window is only 40 selections at that run length, where one standard
  deviation is 79 per mille. Read *"were both C and D selected?"* from it and
  nothing else. The share is worth reading at `schedtest 400` and longer,
  which is the only length it is graded at.
- **`exec tracetest failed`.** The program is not in the disk image. Add
  `$U/_tracetest\` to `UPROGS` in the `Makefile`.
- **`undefined reference to 'trace'` at link time.** The prototype in
  `user/user.h` is there but the `entry("trace")` line in `user/usys.pl` is
  not. Nothing generates the stub, so nothing defines the symbol.
- **`unknown sys call 22`.** The user side is complete and the kernel side is
  not: `syscall.c`'s dispatch table has no entry at that number, or the number
  in `kernel/syscall.h` is not the one the table entry uses.
- **A traced call prints the wrong number.** If it prints the first argument,
  the hook is reading `a0` before the call instead of after it. If it prints
  `18446744073709551615` for a call that failed, it is printing the trapframe's
  `uint64` through an unsigned conversion; the value has to come out signed.
- **Every case times out and the console is a wall of trace lines.** The hook
  is firing regardless of the mask, so `init` and the shell are being traced
  too — and since the shell writes one character per `write`, the kernel never
  gets far enough to give you a prompt. Check the mask test in `syscall()`.
- **The child of a `fork` is not traced.** `fork` copies named fields one at a
  time; it does not copy `struct proc` wholesale, and it cannot — most of the
  struct is the child's own.
- **Every block passes except *"one process's mask does not affect
  another's"*.** That block is the only one in which two processes hold
  *different* masks, so it is the only one that can tell a per-process mask from
  a single kernel-global one. Think about where a per-process mask has to live.
- **`freemem` is a few pages out.** Take the two readings around the
  allocation, not one before it and one at the end. `sbrk` is not the only
  thing allocating: the console, the shell and the file-system log all move.
- **The kernel panics with a store page fault inside `sysinfo`.** You assigned
  through the user's pointer. Use `copyout`.
- **`sysinfo` returns 0 for an address that should have been rejected.** You
  checked the address yourself instead of letting `copyout` fail, or you
  ignored its return value.
- **The kernel deadlocks while counting processes.** Two `p->lock`s held at
  once, or one taken while the caller already holds another. Take one, look,
  release.
- **Everything worked and now nothing does, after a `POLICY=` change.** A
  changed compiler flag does not make anything look out of date. `make clean`.
- **The kernel prints `sched: nothing chosen but N runnable` and stops.**
  Your policy branch declined to select any of `N` runnable processes. Under
  `lottery`, the two commonest causes are a total that includes processes
  that are not `RUNNABLE` — so the drawn ticket sometimes belongs to nobody —
  and an off-by-one in the walk that hands out the ticket ranges. Under `mlfq`,
  it is usually a level loop that stops one level short.
- **The boot gets as far as `init: starting sh` and then hangs, under
  `mlfq`.** Nothing is selecting the shell. If the watchdog line has not
  appeared either, the scheduler *is* choosing somebody — look for a process
  stuck at a level nothing scans, which usually means a `prio` that has been
  allowed past `NPRIO - 1`.
- **`usertests` passes under `rr` and fails under one of the others.** That
  is the check earning its keep. A scheduler bug that only shows up under
  load is nearly always the selection loop releasing the wrong lock, or
  keeping a lock across `swtch`; compare your branch line by line against the
  round-robin one above it, which is stock xv6 and is known good.
- **`schedtest`'s share is nowhere near 750, but the two children do both
  run.** The tickets are not reaching the draw. Check that `settickets`
  writes the field the scheduler reads, that the first pass totals only
  `RUNNABLE` processes, and that `getpinfo` reports the counter the scheduler
  increments rather than one that is never written.
- **`schedtest`'s share is 500 exactly, under `POLICY=lottery`.** The kernel
  running is the round-robin one. `make clean`, then rebuild — and note that
  the `sched: policy=` boot line will *not* have caught this, because it
  reports the flag, not the code the flag selected.
- **`mlfqtest` says the child was never demoted.** The allotment is not being
  charged, or is being charged somewhere a process reaches by blocking. It
  belongs in `yield()`, which a process reaches only by being interrupted.
- **`mlfqtest` says it never came back up.** Rule 5 is not firing. If it is
  written inside the selection loop but after a `break`, or in a branch only
  reached when something was chosen, it never runs on the boot where it
  matters. Put it at the top of the loop, unconditionally.
- **`mlfqtest` says the two children were never both above the bottom
  queue.** Rule 5 is firing, but it is lifting one process rather than all of
  them — most likely the one that is running, or the first one the sweep
  finds. The sweep runs to the end of `proc[]`.
- **`mlfqtest` says a process that blocks was demoted anyway.** The allotment
  is being charged where a process is chosen instead of where it gives the
  CPU up. `mlfqtest` itself blocks on every tick, so it is that process.
- **`mlfqtest` says it sinks faster than its allotments allow.** The
  demotion is unconditional: it is not comparing the ticks used at this level
  against `MLFQ_ALLOTMENT(prio)`, or it is not resetting the count when it
  demotes.
- **`schedtest` says one of the one-ticket children was never selected.**
  An off-by-one in the ticket walk can hand one child every draw and the other
  none. Look hard at the exact boundary of the range each process wins on.
- **A test times out and the machine feels slow afterwards.** Check for a
  leftover emulator with the `ps` line above. Each one holds 128 MB.

---

# Stretch goals

Neither is marked, and neither is small. Both are the natural next question.

**Stride scheduling, alongside lottery.** Give each process a stride
inversely proportional to its tickets and a running pass value; on each
decision run the process with the lowest pass and add its stride to it. It
achieves the same long-run share as a lottery with no randomness at all.
Build it as a fourth policy and compare the *variance* of achieved share at
equal run lengths — 40 ticks, 100, 400 — against your lottery's. That
comparison is the point: the lottery's error shrinks as the square root of
the run length, and stride's is bounded by a constant. Then find the thing
stride is bad at, which is what a process joining late does to a pass value
that everyone else has been advancing for a minute.

**A `nice`-like interface over the ticket counts.** Map a small signed
priority — say −20 to +19, as Unix does — onto ticket counts, and write down
what the mapping makes hard to express. Two questions worth answering
concretely: what does "twice as nice" mean, given that the ticket counts are
what actually compose; and what happens to a process's share when a hundred
other processes arrive, under `nice` and under raw tickets. That second one
is the difference between an interface that describes a share and one that
describes a priority, and it is why the two keep getting confused.
