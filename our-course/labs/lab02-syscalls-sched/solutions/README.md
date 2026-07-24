# Lab 2 — Reference solution and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                                                                   ║
║                          ⚠  SPOILERS  ⚠                           ║
║                                                                   ║
║  Part 1 is worth 11%, is entirely prose, and is marked against    ║
║  the key below. It is also the part that makes Parts 2 and 3 take ║
║  three hours instead of eight: the traversal you do by hand once  ║
║  is the traversal you then edit blind. Reading section 1 of this  ║
║  file first trades that for twenty saved minutes.                 ║
║                                                                   ║
║  Come back when `tests/run.sh` passes, or when you have been      ║
║  stuck on one thing for an hour.                                  ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
```

Every model answer is inside a collapsed `<details>` block, so you can check
them one at a time.

## Applying it

```sh
../setup.sh ~/lab2-ref          # a clean tree
./apply.sh ~/lab2-ref           # the reference kernel over the top
../tests/run.sh ~/lab2-ref
```

`apply.sh` replaces whole files rather than applying a patch: by the time you
read this your own tree has your own edits in it, and every hunk of a patch
would be a conflict. `diff -u ../starter/overlay/ overlay/` shows exactly the
same information.

The files it replaces are `Makefile`, `kernel/syscall.h`, `kernel/syscall.c`,
`kernel/proc.h`, `kernel/proc.c`, `kernel/sysproc.c`, `kernel/kalloc.c`,
`kernel/defs.h`, `user/user.h` and `user/usys.pl`. Everything else — the test
programs, `kernel/sched.h`, `kernel/rand.c`, `kernel/sysinfo.h`,
`kernel/pinfo.h` — is what `setup.sh` already gave you.

## Both ways

There are 42 cases, and one invocation builds three kernels — one per
scheduler policy — and boots each of them more than once.

| tree | mode | result | wall clock |
|---|---|---|---|
| reference | `--fast` | **39 passed, 3 failed** — the three regression cases, which `--fast` declines to run and counts against you on purpose | 2 min 20 s |
| reference | default (`usertests -q` ×3) | **42 passed, 0 failed** | 10–14 min across independent runs |
| starter | `--fast` | **6 passed, 36 failed** | 30 s |
| starter | default | **7 passed, 35 failed**, exit status 1 | 3 min 00 s |

All `[observed]`. `--full` is the same 42 cases with the whole of `usertests`
in place of `usertests -q`, three times over; it is not tabulated here,
because the only thing that changes is about half an hour of regression
suite. Expect a little under an hour, and run it once before handing in.

The starter's seven in the default mode are:

- the `-Werror` tripwire, which is about your `Makefile` and not your kernel;
- the four *"Build: … is unmodified"* tripwires, which are about the four
  given test programs and are supposed to pass until somebody edits one;
- `usertests` **under `rr`**, which is stock xv6 and is *meant* to pass
  before you start — it is there to catch what you break, not to reward what
  you have not yet written. Note that `usertests` under `lottery` and under
  `mlfq` both *fail* on the starter, and legitimately: those kernels do not
  reach a shell;
- *"Part 3: sysinfotest runs to completion"*, which only says the program ran
  and the kernel survived it. On the starter it runs and reports seven
  failures of its own `[observed]` — every call it makes returns −1.

All seven are legitimately unfalsifiable by a student who has written no
kernel code, and all seven are declared as such here. **Every case that
grades a property of `trace`, `sysinfo`, the lottery or the MLFQ fails on the
starter**, which is the property that makes the other thirty-five mean
anything.

The starter's Parts 4 and 5 fail in *seconds* rather than at four successive
deadlines, because an unwritten policy branch chooses nobody and the given
watchdog in `scheduler()` says so at boot: `sched: nothing chosen but 1
runnable`, which the harness treats as fatal.

---

## Part 1 — Trace a system call end to end (11%)

**Marking note.** Nine sections; sections 1–8 have specific right answers and
carry the mark, section 9 is marked only on whether you ran it. Give yourself
credit for a section if you named **the file** and **the function or symbol**,
and answered the "where are the arguments" question with a register name, a
structure field, or an address — not with a paraphrase. A section that names
the right function but says the arguments are "passed to the kernel" has not
answered the question that section exists to ask.

All file and line references below are to this course's pinned tree,
`labs/xv6-riscv/` at `xv6-riscv-rev5`. Two things there differ from the way the
xv6 book's ch. 4 narrates the same path, and both are marked ✱ where they come
up — if you followed the code you will have found them, and if you followed the
book you will not.

<details>
<summary><b>1. The user-side prototype</b></summary>

`user/user.h`:

```c
int write(int, const void*, int);
```

The compiler emits an ordinary function call — `jal`/`call write` — with no
knowledge that anything unusual is about to happen. By the RISC-V C calling
convention the first three integer arguments are in **a0, a1 and a2**: the file
descriptor in `a0`, the buffer address in `a1`, the byte count in `a2`.

That agreement is not a coincidence. xv6 chose the same registers for system
call arguments that C uses for function arguments, which is why the stub in
section 2 does no shuffling at all.
</details>

<details>
<summary><b>2. The stub</b></summary>

Generated by `user/usys.pl` into `user/usys.S` at build time — `usys.S` does
not exist in the source tree until you run `make`, which is why grepping for it
before a build finds nothing. The Makefile rule is `user/usys.S: user/usys.pl`.

The generated symbol is `write`, and the whole of it is:

```asm
.global write
write:
 li a7, SYS_write
 ecall
 ret
```

`a7` carries the system call number. `SYS_write` is **16**, defined in
`kernel/syscall.h` — a kernel header, included by the *generated assembly*,
which is the one place user and kernel code share a definition.

a0, a1 and a2 still hold what the caller put there.
</details>

<details>
<summary><b>3. The instruction that changes privilege</b></summary>

`ecall`. It raises an exception, and the hardware:

- switches from user (U) mode to supervisor (S) mode;
- sets `sepc` to the address of **the `ecall` instruction itself**;
- sets `scause` to 8, "environment call from U-mode";
- sets `sstatus.SPP` to 0, recording that the trap came from user mode, moves
  `SIE` into `SPIE` and clears `SIE`, so the kernel starts with interrupts off;
- sets the program counter to `stvec`.

`stvec` was pointed at the trampoline page's `uservec` by `prepare_return()` in
`kernel/trap.c`, which runs on every return to user space.

**a0–a2 and a7 are untouched.** The hardware copies no general-purpose
registers whatsoever. Everything that is saved is saved by software, which is
what section 4 is for — and is why a trap on RISC-V is cheap in hardware and
expensive in code.
</details>

<details>
<summary><b>4. The trap entry point</b></summary>

`kernel/trampoline.S`, symbol `uservec`.

It stashes the user's `a0` in `sscratch` so that `a0` is free, loads `a0` with
the constant `TRAPFRAME`, and then stores all 32 user registers at fixed
offsets in the page at that address — `struct trapframe`, declared in
`kernel/proc.h` with its offsets in comments: `ra` at 40, `sp` at 48, **`a0` at
112, `a1` at 120, `a2` at 128, `a7` at 168**.

Each process has its own trapframe page, but it is mapped at the same virtual
address `TRAPFRAME` in every user page table, which is how a single hard-coded
constant in shared assembly can reach the right one.

Then it loads the kernel stack pointer from `p->trapframe->kernel_sp`, installs
the kernel page table from `p->trapframe->kernel_satp` with `csrw satp`, and
jumps to `p->trapframe->kernel_trap`, which is `usertrap`.

So: the **user's** page table is in force when this code starts, the
**kernel's** when it ends. The switch happens mid-function and survives only
because the trampoline page is mapped at the same virtual address in both — the
one page in the system of which that is true. (How page tables do that is
Lab 4; here it is enough to see why the mapping has to be identical.)
</details>

<details>
<summary><b>5. The C trap handler</b></summary>

`usertrap()` in `kernel/trap.c`. It tells the three cases apart in this order:

- `r_scause() == 8` → a system call;
- otherwise `devintr() != 0` → a device or timer interrupt;
- otherwise `scause` 13 or 15 and `vmfault()` succeeding → a page fault on a
  lazily-allocated page ✱;
- otherwise → an unexpected fault: print `scause`, `sepc` and `stval`, and kill
  the process.

✱ The lazy-allocation case is in this tree and is not in every xv6 revision.
Say what it is for and move on; demand paging is Lab 4.

It saves the user program counter with `p->trapframe->epc = r_sepc()`, and then
for the system call case does

```c
p->trapframe->epc += 4;
```

because `sepc` points at the `ecall` **itself**, and a RISC-V instruction is
four bytes wide. Return to `sepc` unchanged and the process executes the same
`ecall` again, forever. Four is the instruction width, not an arbitrary offset.

Then `intr_on()` — deliberately not before, because an interrupt would
overwrite `sepc`, `scause` and `sstatus` — and `syscall()`.
</details>

<details>
<summary><b>6. The dispatch</b></summary>

`syscall()` in `kernel/syscall.c`:

```c
num = p->trapframe->a7;
```

— the number comes out of the **trapframe**, not out of the register, because
by now the register belongs to the kernel. `uservec` put it there.

Two tables:

- the numbers: `SYS_fork` … `SYS_close` in **`kernel/syscall.h`**;
- the functions: `static uint64 (*syscalls[])(void)` in
  **`kernel/syscall.c`**, a designated-initialiser array indexed by those
  numbers.

The indirection is the point of the whole arrangement: the number is part of
the kernel's ABI and the function address is not, so the kernel can be
recompiled, rearranged and relinked without a single user binary changing.

The return value goes to `p->trapframe->a0`, overwriting the saved first
argument. A system call's result and its first argument share one register, on
purpose.
</details>

<details>
<summary><b>7. The kernel implementation</b></summary>

`sys_write()` in `kernel/sysfile.c`:

```c
argaddr(1, &p);          // the buffer
argint(2, &n);           // the count
if(argfd(0, 0, &f) < 0)  // the fd, via argint(0, &fd)
  return -1;
return filewrite(f, p, n);
```

`argint`, `argaddr` and `argfd` all end at `argraw(n)` in `kernel/syscall.c`,
which is a `switch` returning `p->trapframe->a0` … `a5` — the values `uservec`
saved. `argfd` additionally range-checks the descriptor and indexes
`myproc()->ofile[]`, so a nonsense fd fails here rather than deeper in.

The buffer address is a **user virtual address and stays one**. `sys_write`
never dereferences it; it hands it to `filewrite()` in `kernel/file.c`, which
passes it down to `writei` and eventually to `either_copyin` → `copyin` in
`kernel/vm.c`, which walks *that process's* page table to translate it.

The kernel cannot dereference it directly: it is running on the kernel page
table, where the same number means something else or nothing at all, and a bad
address in the kernel is a panic rather than a signal. This is the fact Part 3
is built on.
</details>

<details>
<summary><b>8. The way back</b></summary>

Three steps.

1. `syscall()` (`kernel/syscall.c`) stores the result in `p->trapframe->a0`.

2. `usertrap()` (`kernel/trap.c`) calls `prepare_return()`, which points
   `stvec` back at `uservec`, refills the `kernel_*` fields of the trapframe
   for next time, clears `SPP` and sets `SPIE` in `sstatus`, and sets `sepc` to
   `p->trapframe->epc` — the ecall's address plus four. `usertrap()` then
   *returns* the user `satp` value.

3. ✱ It returns straight into `userret` in `kernel/trampoline.S`, which is the
   instruction after the `jalr` that called `usertrap`. `userret` installs the
   user page table from `a0`, restores every register from `TRAPFRAME`
   including `a0`, and executes `sret`, which sets the program counter from
   `sepc` and the privilege level from `sstatus.SPP` — back to user mode, at
   the instruction after the `ecall`, which is the `ret` in `usys.S`.

✱ Some xv6 revisions, and the book's narration, have a separate
`usertrapret()` that jumps to `userret` explicitly. This tree splits it: the
register and CSR set-up is `prepare_return()`, and the jump is a plain function
return. `forkret()` in `kernel/proc.c` shows the same pair used by hand, which
is how a brand-new process reaches user space having never trapped out of it.

The C caller then sees the value in `a0` as `write`'s return value, and never
learns that anything else happened.
</details>

<details>
<summary><b>9. Seen under GDB</b></summary>

Marked on having done it, not on the numbers. A run that produces the right
shape:

```
(gdb) break syscall.c:138 if num == 16
(gdb) continue

Breakpoint 1, syscall () at kernel/syscall.c:138
138	  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
(gdb) print num
$1 = 16
(gdb) print p->trapframe->a0
$2 = 1
(gdb) print/x p->trapframe->a1
$3 = 0x3edf
(gdb) print p->trapframe->a2
$4 = 1
```

`[observed]`, on a starter tree, at the first `write` of the boot. `num` — and
`p->trapframe->a7`, which is where it came from — is 16, `SYS_write`. `a0` is 1,
the file descriptor for the console. `a1` is a user virtual address, and yours
will differ from anyone else's. `a2` is the byte count, and **1** is the
expected answer for anything that got here through `printf`: xv6's user-space
`printf` writes one character per `write`. A program that calls
`write(1, buf, n)` directly shows `n`, and that is the version worth catching
if the write-up quotes a count.

A second breakpoint on `sys_write` and a `print n` after `argint(2, &n)` shows
the same count arriving through `argraw`, which is the observation the section
is worth making.

Two practical notes, both `[observed]`, because a transcript that will not
reproduce is worse than none.

**Break on the line, not on the function.** `break syscall` breaks at the
function's opening brace, before `struct proc *p = myproc();` has run, and the
kernel is compiled with optimisation on, so `print p->trapframe->a7` there
answers `value has been optimized out` — and a *condition* on that breakpoint
cannot be evaluated either, which aborts a `-x` command file outright with
`Error in testing condition for breakpoint 1`. Break on the first line *after*
the assignment instead: the `if(num > 0 && ...)` line, which is
`kernel/syscall.c:138` in a starter tree and moves as soon as the student
starts adding to the file. `break syscall` and `next` twice reaches the same
place. The handout says all of this; a student who reports the "optimized out"
message read past it.

**The first stop is not yours.** `init` calls `exec` before the shell exists,
so an unconditional break reports `num` = 15 (`SYS_exec`) first. Keep
continuing until it is 16, or set the condition when you break, as above.
Conditional breakpoints are evaluated by GDB over the remote protocol on every
system call the boot makes, so they are the slower option in general; over the
handful of calls between reset and the first `write` the difference is not
noticeable `[observed]`.

Breaking on `uservec` instead is instructive and awkward: it runs on the
trampoline page under the user page table, so symbol lookup and stack unwinding
both misbehave there. Say so if you tried it.
</details>

---

## Part 2 — Add `trace` (22%)

**Marking note.** Machine-marked. The cases are *"Part 2: tracetest runs to
completion"*, *"…nothing is traced before trace() is called"*, *"…a traced call
prints one line per call"*, *"…the mask selects which calls are traced"*,
*"…a failed call traces its return value, not its argument"*, *"…the trace mask
is inherited across fork"*, *"…one process's mask does not affect another's"*,
*"…trace(0) turns tracing off again"* and *"…trace() itself returns 0"*. All
nine pass or the part is not done.

The seventh is the one that carries the part's actual subject. Blocks A–E of
`tracetest` only ever have one mask in play at a time, so a kernel with a
single global mask — no field in `struct proc`, no copy in `fork` — is
indistinguishable from a correct one there, *including* on the "inherited
across fork" case, because a global mask is trivially inherited. Block F is the
only place two processes hold different masks: the child sets its own to
`SYS_close` after the fork, and the parent's must still be `SYS_getpid`. A
global mask prints the child's `close` line and then the *parent's* `close`
line instead of the parent's `getpid` line, and fails exactly that one case.

The handout also asks for one by-hand run with `SYS_write` in the mask.
Nothing checks it and nothing can: its whole point is watching `printf` turn
into one trace line per character. `tracetest` deliberately does not do it —
its own markers would be unreadable — so the only route is a user program of
the student's own plus a `UPROGS` line, which the handout says outright is
expected. A student who reports being unable to do this read the frozen list
as a freeze on the whole tree.

<details>
<summary><b>The five places a new system call has to be mentioned</b></summary>

1. `user/user.h` — `int trace(int);`
2. `user/usys.pl` — `entry("trace");`, which generates the `li a7, SYS_trace` /
   `ecall` / `ret` stub.
3. `kernel/syscall.h` — `#define SYS_trace 22`.
4. `kernel/syscall.c` — `extern uint64 sys_trace(void);` and
   `[SYS_trace] sys_trace,` in the dispatch table.
5. `Makefile` — `$U/_tracetest\` in `UPROGS`, or the program is not in the
   disk image and the shell says `exec tracetest failed`.

Only (1) and (3) fail at compile time. (2) fails at *link* time with
"undefined reference to `trace`". (4) missing — or present at a number that
is not the one in (3) — builds and boots and prints `unknown sys call 22` at
the moment you call it. (5) gives you nothing at all until you try to run the
program, and then only `exec tracetest failed`.
</details>

<details>
<summary><b>The kernel side</b></summary>

`kernel/proc.h`, in `struct proc`:

```c
uint64 trace_mask;           // bit N set => print a line for syscall N
```

`uint64` rather than `int` because syscall numbers keep being added — this lab
takes them to 25, Lab 7 adds more — and `1 << 31` on an `int` is undefined
behaviour. Test it with `(1UL << num)`.

**Locking.** `proc.h` says `p->lock` protects the fields this lab adds, and
`trace_mask` is the one exception, stated as such in both copies of the header:
it is written only by the process it belongs to (in `sys_trace`) and read only
by that same process (in `syscall()`). The one moment a second process touches
it is `fork`'s copy, which happens under the child's lock while the child
cannot yet run. Every other field — `tickets`, `ticks`, `prio`, `allot` — is
touched by `scheduler()` on whichever CPU is running it, and does need the
lock. Taking `p->lock` around `trace_mask` anyway is not wrong, only
unnecessary; what would be wrong is holding it across the `printf`.

`kernel/proc.c`, in `allocproc()` — every process in the system is born here,
including `userinit()`'s, so this is the only place the default has to be
right:

```c
p->trace_mask = 0;
```

**This particular line is not enforced by the Part 2 cases; mark it by eye.**
Every process except `userinit`'s is created by `fork`, which immediately
overwrites the child's mask from its parent, so a kernel that omits
`p->trace_mask = 0;` still passes all nine Part 2 cases, and no user-visible
failure can be constructed for `trace` alone.

The *block* is enforced, from Part 4 onwards. `tickets`, `ticks`, `prio` and
`allot` are initialised here too and are deliberately **not** copied by
`fork`, so a process born without them holds zero tickets and can never win a
lottery — and `schedtest` looks at a bare fork's child through `getpinfo`
before anything has set its tickets, which is the case *"Part 4: a newly
forked process starts with the default tickets"*. Deleting any of those four
lines fails that case with the observed number in the message. Only the trace
mask's own initialisation remains a by-eye check.

The second edit is in `fork()`, after `safestrcpy(np->name, ...)`:

```c
np->trace_mask = p->trace_mask;
```

`fork` copies named fields one at a time; it cannot copy `struct proc`
wholesale, because most of it — the lock, the page table, the trapframe
pointer, the kernel stack — is the child's own.

`kernel/sysproc.c`:

```c
uint64
sys_trace(void)
{
  int mask;

  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}
```

And the hook, in `syscall()` in `kernel/syscall.c`, **after** the dispatched
function has returned:

```c
p->trapframe->a0 = syscalls[num]();

if((p->trace_mask & (1UL << num)) != 0)
  printf("%d: syscall %s -> %d\n",
         p->pid, syscall_names[num], (int)p->trapframe->a0);
```

plus a second table in the same file, indexed exactly like `syscalls[]`:

```c
static char *syscall_names[] = {
[SYS_fork]    "fork",
...
[SYS_trace]      "trace",
};
```

Two details the tests are specifically looking for.

The return value must be printed **signed**. The trapframe field is a
`uint64`, and a failing call has put −1 in it, so a `%lu` prints
`18446744073709551615`. `(int)` with `%d` is one way; `%ld` with no cast is
another, and prints the same thing, because xv6's own `printf` treats `%ld`
as signed (`kernel/printf.c`). What is graded is the number, not the
conversion.

And *after* the call, not before: the line contains the return value, and this
is the only point in the kernel that knows both the number and the result.
</details>

<details>
<summary><b>Why the hook belongs in <code>syscall()</code> and not in a wrapper</b></summary>

One hook covers all 25 system calls, including the ones added later in this
lab, for free. Anything else — a line in each `sys_*` function, a wrapper table
— costs an edit per call and gets one wrong.

That is the payoff of the dispatch table as an *indirection*: the table gives
you one place through which every call passes. OSTEP ch. 6 draws that as a
diagram; this is the diagram with a line of code in it.
</details>

---

## Part 3 — Add `sysinfo` (19%)

**Marking note.** Machine-marked. The cases are *"Part 3: sysinfotest runs to
completion"*, *"…sysinfo fills the struct with plausible numbers"*, *"…freemem
tracks allocation and freeing"*, *"…nproc counts processes exactly"*, *"…a user
pointer the kernel may not write to is rejected"* and *"…sysinfotest reports no
failures at all"*. A kernel that assigns through the user's pointer fails the
whole part at once, because it panics rather than returning −1 — that is the
correct outcome and the reason the case is worded that way.

<details>
<summary><b>Counting free memory</b></summary>

`kernel/kalloc.c`, and it has to be there because `kmem` is `static` to that
file:

```c
uint64
kfreemem(void)
{
  struct run *r;
  uint64 n = 0;

  acquire(&kmem.lock);
  for(r = kmem.freelist; r; r = r->next)
    n++;
  release(&kmem.lock);
  return n * PGSIZE;
}
```

The free list is the allocator's only record of what is free — there is no
counter to read — so walking it is the only honest answer. It is O(free pages),
about 32,000 links on a 128 MiB machine. Fine for a diagnostic call; not fine
for anything on a hot path, and worth noticing that the cheap-looking system
call is the linear one.

Declared in `kernel/defs.h` under `// kalloc.c`.
</details>

<details>
<summary><b>Counting processes</b></summary>

`kernel/proc.c`, because `proc[]` is `static` to that file:

```c
uint64
knproc(void)
{
  struct proc *p;
  uint64 n = 0;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state != UNUSED)
      n++;
    release(&p->lock);
  }
  return n;
}
```

One lock at a time. Acquiring the whole table at once deadlocks against any
process holding its own lock and waiting for something this loop would have to
wait for. The snapshot is therefore not atomic across the table — by the time
slot 40 is read, slot 3 may have changed — which is acceptable for a
measurement and would not be for anything that made decisions on the result.

Declared in `kernel/defs.h` under `// proc.c`.
</details>

<details>
<summary><b>The system call, and the direction of the copy</b></summary>

`kernel/sysproc.c`:

```c
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  uint64 addr;

  argaddr(0, &addr);
  info.freemem = kfreemem();
  info.nproc = knproc();

  if(copyout(myproc()->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}
```

The whole part is in the last three lines.

`addr` is a **number a user process chose**. It may be unmapped, above `MAXVA`,
mapped read-only, or belong to a mapping the process is not allowed to write.
`copyout` walks *that process's* page table, checks the PTE's valid, user and
write bits, and returns −1 instead of faulting — because a fault in the kernel
is a panic, not a signal. Validating the address any other way means writing a
second, worse copy of the page-table walk, and getting it subtly wrong is how
kernels acquire privilege-escalation bugs.

Note also that the struct is built on the kernel stack and copied out in one
call. Filling it field by field through the user pointer would be three
chances to fault instead of one, and would let a concurrent `sbrk` change the
mapping halfway through.

The three addresses `sysinfotest` tries are each a different way of being
wrong: `0xffffffffffff0000` is above the top of the address space, so the
walk fails on the first level; `0x3f00000000` is below `MAXVA` (`1L << 38` =
`0x4000000000`, `kernel/riscv.h`) and far above any `p->sz`, so the walk finds
an invalid PTE and `vmfault` declines to create one; and `0` is the process's
text page, which is mapped and readable and **not writable**, which is the one
a length check or a "is it below `p->sz`" test would wave through.

A near miss worth knowing about, since it is the obvious address to reach for:
`0x3fffffe000` is **not** unmapped. It is exactly `TRAPFRAME` —
`TRAMPOLINE - PGSIZE` = `MAXVA - 2*PGSIZE` (`kernel/memlayout.h`) — which is
mapped in every user page table and holds the very registers Part 1 had you
read. `copyout` still refuses it, but on `(*pte & PTE_U) == 0` in `walkaddr`
(`kernel/vm.c`), not on validity: a *mapped* page the user may not reach, not
an absent one. Different mechanism, different lesson.
</details>

---

## Part 4 — A lottery scheduler (30%)

**Marking note.** Machine-marked, ten cases. Eight come out of the one
`schedtest` run under `POLICY=lottery`: *"Part 4: schedtest runs to
completion"*, *"…settickets rejects a ticket count outside [1, MAX_TICKETS]"*,
*"…getpinfo rejects a pointer the kernel may not write through"*, *"…a newly
forked process starts with the default tickets"*, *"…getpinfo reports the
scheduler's per-process tick counts"*, *"…the CPU share the children get
matches their tickets"*, *"…two processes holding one ticket each are both
selected"* and *"…schedtest reports no failures at all"* — and two more,
*"…getpinfo counts selections under POLICY=rr too"* and *"…under POLICY=mlfq
too"*, are graded from the kernels Parts 2–3 and Part 5 already build. All of
them pass or the part is not done.

The share case is the part's actual subject and the widest statistical margin
in the lab. The equal-ticket case beside it is **half exact and half
statistical**, and the halves are worth keeping apart: *"both children were
selected"* is exact and is what the case is for (below); the ±200 per mille
around 500 that rides along with it is the tightest statistical margin in the
lab, and the only one a correct kernel can plausibly miss.

Its window is `dur/4` with a floor of 40 ticks, so at the graded `schedtest`
default it is 100 selections — sd 50 per mille, band ±4 sd, `[observed]` 460,
490 and 440 across three runs. At `schedtest 100`, which is what a student
iterating will actually type, the window falls to its 40-tick floor: sd 79 per
mille, band ±2.5 sd, and `[observed]` 275, 575, 350 and 575 across four runs
of the reference. The 275 is the first run after the boot and so is
*reproducible*: a correct kernel iterated on with `schedtest 100` shows the
same out-of-band 275 every time. Nothing is graded at that length — the case
runs only at the default — but a student who reports it as a bug has found a
handout problem, not a kernel one, and the handout says so under **the other
assertion, half of which is exact**. If the graded case is ever the single red
line on an otherwise clean run, re-run it before reading anything into it.

The newborn case is the only one
anywhere that can see whether `allocproc` was given its defaults, which is why
it exists. The two under the other policies exist because the specification
says `ticks[]` is counted under every policy and this is where that is
checked — without them a tick counter written into the lottery branch alone
scores full marks here and then produces a `SCHED-RESULTS.md` table with two
empty rows, which the student discovers at the write-up and cannot explain.

Note that *"getpinfo reports the scheduler's per-process tick counts"* cannot
fail on its own: if the counter never moves, `schedtest` reports a share of
−1 and the share case fails too. It is a diagnostic — it names *which* of the
two failures happened — rather than an independent check, and no kernel makes
it the only red line.

**What these cases catch.** A lottery that ignores the ticket counts is
caught by the share case alone, at 478 per mille `[observed]`; one that always
runs the highest-ticket process, by the same case alone, at 1000; a policy
flag that builds the round-robin code under `POLICY=lottery`, by the same
case, at 500; a lottery that selects nobody, by all of them, in seconds,
through the watchdog; a tick counter maintained only in the lottery branch, by
the two cases under the other policies (`[observed]`: 0 selections in a
100-tick window under each, against 100 for the reference); one maintained
only in the round-robin branch, by the tick-count case and then the share
case; an `allocproc` that does not set the ticket count, by all of them — a
process born with no tickets can never be chosen, so that kernel does not
reach a shell; a `fork` that copies the ticket count, by the newborn case
alone; a `getpinfo` that writes straight through the user's pointer, by all of
them, through `panic: kerneltrap`; one that calls `copyout` and ignores what
it returns, by the trust-boundary case alone. All `[observed]`.

**`acc >= winner`, and why the equal-ticket window is there.** The off-by-one
in the ticket walk is the one bug in this part that the 30∶10 measurement
genuinely cannot see: every process wins on one ticket's worth too much of the
draw, the first process in the table gaining what the last loses, which at
30∶10 is 25 per mille — 780 against the correct kernel's 755, both `[observed]`
— or about one standard deviation of a 400-tick run. No statistical check of
that window can separate them, and one tight enough to try would fail correct
kernels constantly.

The size of the error is not the point, though, and that is the thing worth
teaching here. The *shape* of it is: the first runnable process in table order
wins on one extra value of the draw and the last wins on one fewer. Give two
processes one ticket each and the first wins on both of the two possible draws
and the second wins on none. `[observed]`, two children holding one ticket
each over a 100-tick window:

| kernel | C | D | share |
|---|---|---|---|
| reference | 46 | 54 | 460 per mille |
| `acc >= winner` | 100 | **0** | 1000 per mille |

D is never selected once. *"Both children were selected"* is an exact
assertion that a correct lottery cannot fail and this bug cannot pass, it
costs ten seconds of emulated time, and it is also the honest statement of
what the bug does: under `>=`, any process holding one ticket that sits last
in the table starves outright. That is a great deal worse than a 3% error, and
it is why the case exists rather than a by-eye note.

<details>
<summary><b>The fields, and the two places they are and are not touched</b></summary>

`kernel/proc.h`, in the same block as `trace_mask`:

```c
int tickets;                 // Part 4: lottery tickets held
int ticks;                   // Part 4: times scheduler() has chosen this proc
```

`p->lock` protects both — unlike `trace_mask`, these are written by
`scheduler()` on whichever CPU is running it, not by the process itself.

`kernel/proc.c`, in `allocproc()`, beside `p->trace_mask = 0;`:

```c
p->tickets = DEFAULT_TICKETS;
p->ticks = 0;
```

and in `fork()`, **nothing**. That is deliberate and is specified: the trace
mask is inherited, the ticket count is not. A student who adds
`np->tickets = p->tickets` fails no case here — `schedtest`'s parent holds a
thousand tickets and its bare-fork child would then hold a thousand too,
which the newborn case catches immediately. That is what that case is for.

The `allocproc` line is now genuinely enforced, which it was not in Part 2:
deleting `p->tickets = DEFAULT_TICKETS;` leaves every process with whatever
was in the slot, and `schedtest` reports it in one line.
</details>

<details>
<summary><b><code>settickets</code> and <code>getpinfo</code></b></summary>

`kernel/sysproc.c`:

```c
uint64
sys_settickets(void)
{
  int n;

  argint(0, &n);
  if(n < 1 || n > MAX_TICKETS)
    return -1;

  acquire(&myproc()->lock);
  myproc()->tickets = n;
  release(&myproc()->lock);
  return 0;
}

uint64
sys_getpinfo(void)
{
  struct pinfo *pi;
  uint64 addr;
  int r;

  argaddr(0, &addr);

  // struct pinfo is 1280 bytes and a kernel stack is one 4096-byte page,
  // most of it already spoken for by the trap path. Borrow a page instead.
  if((pi = (struct pinfo *)kalloc()) == 0)
    return -1;

  kgetpinfo(pi);
  r = copyout(myproc()->pagetable, addr, (char *)pi, sizeof(*pi));
  kfree((void *)pi);              // including on the error path
  return r < 0 ? -1 : 0;
}
```

Three things worth marking by eye.

`struct pinfo` on the kernel stack is a real bug that does not always show:
1280 bytes of a 4096-byte stack that the trap path has already spent much of.
It survives a quiet test and corrupts something under `usertests`. The
borrowed page is the fix, and `kfree` has to be on the error path too.

`kgetpinfo` goes in `kernel/proc.c`, because `proc[]` is `static` to that
file, and takes one `p->lock` at a time — the same rule as `knproc`, and for
the same reason. The snapshot is therefore not atomic across the table; that
is fine for a measurement and would not be for a decision.

`settickets` returning −1 for `n < 1` is not fussiness. A process holding
zero tickets can never be chosen, and one holding a negative count corrupts
the total — a negative or wrapped modulus makes the draw meaningless and can
leave the walk selecting nobody while runnable processes wait, which the
`scheduler()` watchdog reports as `sched: nothing chosen but N runnable`.
</details>

<details>
<summary><b>The lottery</b></summary>

`kernel/proc.c`, the `SCHED_LOTTERY` branch of `scheduler()`:

```c
int total = 0;
for(p = proc; p < &proc[NPROC]; p++){
  acquire(&p->lock);
  if(p->state == RUNNABLE)
    total += p->tickets;
  release(&p->lock);
}

if(total > 0){
  int winner = rand() % total;
  int acc = 0;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state == RUNNABLE){
      acc += p->tickets;
      if(acc > winner){
        p->state = RUNNING;
        p->ticks++;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
        found = 1;
        release(&p->lock);
        break;
      }
    }
    release(&p->lock);
  }
}
```

About forty lines with the comments, and `swtch` is untouched. That is the
part's real content: the *policy* changed completely and the *mechanism* did
not change at all.

Four details, each of which is a way to get this wrong:

- **`total > 0`.** Nothing is runnable fairly often — every time the machine
  is idle at the shell prompt — so the draw must be guarded. `rand() % 0` is
  undefined in C, and there is nobody to select in any case. On this RISC-V
  kernel a missing guard is *not* a crash — the hardware's remainder-by-zero
  returns the dividend rather than trapping, and with nothing runnable the
  garbage draw selects nobody and the core idles correctly — so no case
  catches its absence `[observed]`: omitting the guard scores an identical
  run to the reference. Mark it as correctness, not as a bug the harness
  finds; it is the empty case of the algorithm, stated in the handout without
  a promised symptom because there is none.
- **`acc > winner`, not `>=`.** With `winner` drawn from `[0, total)` and
  `acc` accumulated *before* the test, a process holding *t* tickets must
  win on exactly *t* values of `winner`. `>=` shifts every range by one and
  gives the first process one ticket too many and the last one too few —
  which at 30∶10 is a 3% error, invisible to the share case, and at 1∶1 is
  the difference between 500 and 1000. The equal-ticket window is what
  catches it.
- **Only `RUNNABLE` processes in the total.** Counting sleeping or zombie
  processes leaves ticket ranges nobody occupies, so some draws select
  nothing, `found` stays 0, and the machine idles a tick that a runnable
  process wanted. The watchdog says so.
- **The two passes must agree.** Interrupts are off from the `intr_off()` at
  the top of the loop and this kernel is single-CPU, so nothing can change
  between them. On a multiprocessor it could, and this code would be wrong —
  worth saying out loud, because it is the first time in the course that
  "single CPU" is doing load-bearing work in an argument.

Counting the win — `p->ticks++` where the process is set `RUNNING` — belongs
in **all three** branches, including round robin. Part 5 compares the three,
and a counter that only exists under `lottery` makes the comparison
impossible.

Both halves of that are enforced. *Which* branches, by the two
*"getpinfo counts selections under POLICY=…​ too"* cases. *Where inside the
branch*, by *"Part 4: ticks[] counts scheduling decisions, not whole ticks
consumed"*, which is graded from `mlfqtest`'s output because `mlfqtest`'s
sampling parent is the only process in the lab that is chosen far more often
than it consumes a whole tick: it wakes, samples and blocks again on every
tick of the run, so it never reaches `yield()`. `[observed]`: the reference
reports it selected about once per sample; the same kernel with all three
`p->ticks++` removed and one added to `yield()` reports **0**. Every other
process any case reads through `ticks[]` spins, and for a process that spins
"was chosen" and "used a whole tick" are the same event, which is why that
mutant used to score a clean run.
</details>

<details>
<summary><b>Why the check is statistical, and what the numbers actually are</b></summary>

The case asserts that A's share of the two children's selections lands in
**650–850 per mille**, against a target of 750. The handout states that band,
and a student is entitled to ask why it is so wide.

Because the share is a *sample*. Over a window of *n* scheduling decisions
between two processes with a 3∶1 ticket ratio, the number A wins is binomial
with p = 0.75, so the share has a standard deviation of
√(0.75 × 0.25 / n) — about 22 per mille at the 400-tick default. A band of
±100 is therefore ±4.6 standard deviations. Tightening it to ±20 per mille,
which looks like the "obviously right" 750 ± a bit, would fail a correct
kernel about a third of the time.

The band is nonetheless narrow enough to catch what actually goes wrong,
because the wrong answers are not near-misses:

| kernel | share it lands on | distance from the band |
|---|---|---|
| correct lottery | ~750 | inside, by 4.6 sd |
| ignores tickets (round robin under another name) | 500 | 11 sd below |
| always runs the highest-ticket runnable process | 1000 | 11 sd above |
| inverts the comparison | 250 | further still |

`[observed]`, reference kernel, separate boots of the same binary: **755 per
mille every time** (302 selections to 98, over 400 ticks). The *first*
`schedtest` after a boot is identical on every boot, because the generator is
seeded to a constant and the schedule is deterministic — so the graded case,
which always runs it on a fresh boot, does not flake at all.

The variation the band exists to absorb is between *implementations*: a
student whose scheduler makes a slightly different number of draws lands on a
different window of the same stream, and the second and third `schedtest` at
the same prompt land on different windows for exactly the same reason.
`[observed]`, three consecutive `schedtest 400` in one boot: **755, 712, 750**
— and only the first of those three is reproducible. Runs two and three start
wherever the stream has got to, which depends on every draw the kernel has
made since boot, so a run-three number is not a property of the policy and a
student quoting a different one is not to be marked down for it. The handout
quotes 755 alone for that reason. Ten *correct* lottery kernels — the reference, one scanning `proc[]`
backwards, one starting each scan at a rotating cursor, one drawing from the
high 32 bits, one doing weighted reservoir sampling in a single pass, and five
differing only in the generator's seed — give 755, 752, 785, 712, 735, 740,
770, 760, 747 and 730 `[observed]`: mean 749, sample sd 21, range 712–785,
none within 62 per mille of an edge. That sd is the √(0.75 × 0.25 / 400) =
21.7 the band is derived from, which is the point: the effective number of
independent draws in a 400-tick window really is about 400, so ±100 really is
about 4.6 sd. Four thousand simulated windows of the same generator and the
same draw pattern give mean 750, sd 22, range 668–828, and **none** outside
650–850.

The half-way sample `schedtest` also prints is not graded. It is there for
Part 5's write-up, where the point is precisely that it is noisier — and note
what the write-up needs: a single `schedtest 40` beside a single
`schedtest 400` cannot show convergence, because each is one draw and the
first draw of a boot is fixed. Convergence shows up only in the *spread* of
several runs at each length, which is why the handout tells the student to run
each length three or four times at one prompt and the rubric asks for the
spread rather than for one error.
</details>

<details>
<summary><b>The generator, and why it is the one it is</b></summary>

`kernel/rand.c` is given, and it is splitmix64 rather than the two-line
xorshift that any search for "smallest usable PRNG" returns first. The
reason is worth knowing, because it is a trap that measures as *correct on
average* and is *wrong every time you run it*.

The scheduler takes the value **modulo the ticket total**, so the draw is
decided by the generator's LOW bits. Xorshift's low bits are visibly
correlated; its high bits are fine. Multiplying the output by an odd constant
— the usual "xorshift\*" patch — does not help here at all, because
multiplication by an odd number is a permutation of the low *k* bits and
therefore preserves exactly the structure that is causing the trouble.

The symptom is not a biased average. Over 400,000 draws the plain xorshift
gives 750 per mille, correct to a fraction of a per mille. Over the *few
hundred* draws a test makes, from a *constant seed*, it reproducibly gives
something else. Swap this `rand()` for the plain two-line xorshift, leave the
rest of the kernel alone, and `schedtest 400` reports **672 to 685 per mille,
on every boot** `[observed]` — a correct lottery scheduler apparently a third
short of its own target, with no run-to-run variation to suggest that noise
was involved. It is still inside the band, and it would still have a student
hunting a bug in code that does not have one. Try it; it is ten minutes and
it is the most surprising thing in the lab.

If a student replaces `rand()` with something of their own, this is the thing
to check: draw a few hundred values modulo 40 and count how many land below
30.
</details>

---

## Part 5 — MLFQ behind a policy switch (18%)

**Marking note, two halves.** The kernel is machine-marked. Seven cases come
out of `mlfqtest`:
*"Part 5: mlfqtest runs to completion"*, *"…a process that never blocks is
demoted to the bottom queue"*, *"…the periodic boost lifts every process out
of the bottom queue again"*, *"…the allotment at each level is 1, 2 and 4
ticks"*, *"…a process that blocks before its tick is up is not demoted"*,
*"…a newly forked process enters at the top level"* and *"…mlfqtest reports
no failures at all"*. An eighth, *"Part 5: MLFQ round-robins equal-priority
processes fairly"*, is read from the `schedtest`-under-`mlfq` boot instead and
is discussed with Rule 2 below (the per-level scan cursor). Two preconditions:
a kernel with no demotion at all is
not awarded the boost case, because there was nothing to lift anything out
of, and one with no boost is not awarded the allotment case, because with a
single descent there is nothing to time.

The middle three are the ones that make the difference between a kernel that
implements the five rules and one that merely reaches the bottom queue and
comes back:

- **The boost lifts every process.** `mlfqtest` runs **two** spinning
  children, and a boost that lifts only the process that happens to be
  running when it fires lifts whichever child is running, which is then the
  child that goes on running — so the other one never leaves the bottom queue
  again. `[observed]`, boost moved into `yield()` and applied to the running
  process only: across boots the first child is above the bottom queue in 0
  to 29 samples of 200 — but the two children are **never both** above it,
  against 55 for the reference, which is why the case owns two failure
  messages and not one. A boost that lifts only the first runnable process it
  finds is caught the blunter way `[observed]`: the process it finds is the
  parent, so neither child ever comes back up at all.
- **The allotments are 1, 2 and 4.** A process that never blocks owns 7 of
  its own ticks after each boost before it reaches the bottom; one demoted on
  every tick regardless of `MLFQ_ALLOTMENT` owns 3. With two children sharing
  the CPU that is the difference between being above the bottom in **28** per
  cent of the samples and in **8** `[observed]`; the case asks for 15.
- **Blocking is not charged.** The measuring parent blocks in `pause()` on
  every tick of the run, so it is never charged an allotment and never leaves
  the top level: `[observed]` 169 to 200 samples of 200 at level 0. A kernel
  that charges the allotment where a process is *selected* rather than where
  it gives the CPU up demotes it instead — `[observed]` **0** of 200 — which
  is why this is a case rather than a by-eye marking note.

**What these cases still cannot see, stated plainly.** They do not pin down
*where* a working Rule 5 is hooked, only what it does. A boost moved out of
`scheduler()` and into `yield()`, still triggered off the global tick counter
and still sweeping every process, scores a clean run `[observed]` — and that
is the right answer, because on a uniprocessor that kernel does not starve
anybody. A process is starved only while something else is consuming the CPU;
anything consuming the CPU is preempted by the timer and therefore reaches
`yield()`; and if nothing is CPU-bound, the scheduler reaches the bottom queue
on its own. The only behavioural difference is that boosts pause while the
machine is idle, which nothing can observe from inside the machine.

So the rule to mark by eye is not "not in `yield()`". It is two-part:

- **the trigger must be the global tick counter**, not anything a process
  controls. A boost hooked into `sys_write`, or into `fork`, fires often for
  a process that is running and never for one that is not, and no case here
  would see it;
- **the sweep must touch every process**, not just the current one. That half
  *does* have a case, and it is the one the second spinner is there for.

`scheduler()` is still the right place, and worth saying why: its loop turns
on the tick counter and runs whether or not any process does, so it needs
neither half of that argument.

`SCHED-RESULTS.md` is prose, and is marked against the rubric below.

<details>
<summary><b>Rubric for <code>SCHED-RESULTS.md</code></b></summary>

Two questions, and they are the whole rubric: **did you measure, and does the
explanation match your own numbers?** A write-up that reasons beautifully
from numbers it did not take is worth less than one with the numbers and a
hesitant explanation.

| | full marks | half | none |
|---|---|---|---|
| **1. Three-way comparison** | `schedtest 400` run under all three policies, with the selection counts, and an explanation of why `rr` and `mlfq` ignore the tickets for *different* reasons — `rr` has no concept of them; `mlfq` has one but both children sink to the same level and take turns there | numbers for all three, explanation that stops at "they ignore tickets" | numbers for one policy, or numbers that do not appear |
| **2. Convergence** | at least three run lengths, each repeated three or four times at one prompt, the *spread* shrinking, and the observation that a tenfold run gives roughly a threefold improvement — the √n of an average of independent draws | shorter runs are noisier, no rate; or three lengths run once each, which cannot show a spread | asserted without numbers |
| **3. MLFQ behaviour** | `mlfqtest`'s output, plus the ticks-to-bottom after a boost checked against 1 + 2 + 4 = 7 — and the observation that two children are sharing the CPU, so it takes about twice that many ticks of the clock | the output, quoted, not interrogated | absent |
| **4. A workload MLFQ wins** | a named workload, the rule that does the work, and what the lottery would need — the honest answer is "a way to notice that a process blocks", which tickets alone cannot express | a plausible workload, no mechanism | "MLFQ is adaptive" |
| **5. What could not be measured** | notices that `ticks[]` counts *decisions*, not CPU time, and that the two diverge exactly for processes that block early — which is precisely the population MLFQ is designed around, so the instrument is weakest where the policy is most interesting | notices the distinction, does not follow it | absent |

Row 3 asks for a time in ticks that `mlfqtest` never prints. It prints a
histogram of samples per level, and the time has to be *derived*: the
allotments give 7 of the child's own ticks, two spinners halve its share of
the clock, so about 14 of the 40 between boosts — which the histogram then
corroborates, at 28 per cent of samples above the bottom against 14/40 = 35
per cent predicted. Deriving it and checking it against the histogram is the
exercise, and the handout says so; full marks wants the arithmetic and the two
numbers side by side, not a figure read off a line of output, and a write-up
that also accounts for the gap between 35 and 28 — a boosted child does not
start spending its allotment until it is next chosen — is doing more than the
row asks.

Full marks does not require agreeing with any of this. A write-up that argues
the comparison is unfair because all three policies were measured on a
workload of two identical spinners is *right*, and saying so is worth more
than another table.

The numbers below are `[observed]` on the reference kernel and are what a
correct write-up's table looks like. A student's will differ in the third
digit and should not differ in the first.

| policy | A (30 tickets) | B (10 tickets) | A's share |
|---|---|---|---|
| `rr` | 200 | 200 | 500 |
| `lottery` | 302 | 98 | 755 |
| `mlfq` | 200 | 200 | 500 |

`mlfqtest` on the same kernel reports both children reaching level 3, 55 or
56 of 200 samples above the bottom queue after the first sank, and the two of
them above it together in 55 `[observed]`. Unlike `schedtest`'s share, this
number moves a little from boot to boot — see below — so a write-up quoting
50 or 60 is quoting the same behaviour.

`rr` and `mlfq` both land on 500, and for genuinely different reasons: round
robin never looks at a ticket count, while MLFQ looks at behaviour instead —
both children are compute-bound, both are demoted at the same rate, both end
up at the bottom level, and Rule 2 then shares that level round-robin. The
ticket counts are not overridden; they are not consulted.
</details>

<details>
<summary><b>The selection loop</b></summary>

`kernel/proc.c`, the `SCHED_MLFQ` branch:

```c
for(int lvl = 0; lvl < NPRIO && !found; lvl++){
  for(int i = 0; i < NPROC; i++){
    p = &proc[(rr_next[lvl] + i) % NPROC];
    acquire(&p->lock);
    if(p->state == RUNNABLE && p->prio == lvl){
      rr_next[lvl] = ((p - proc) + 1) % NPROC;
      p->state = RUNNING;
      p->ticks++;
      c->proc = p;
      swtch(&c->context, &p->context);
      c->proc = 0;
      found = 1;
      release(&p->lock);
      break;
    }
    release(&p->lock);
  }
}
```

with `static int rr_next[NPRIO];` declared at the top of `scheduler()`.

Rule 1 is the outer loop, lowest level number first. Rule 2 is `rr_next`:
without it the inner scan always restarts at slot 0 and the lowest-numbered
table slot at each level wins every tie for ever, which is a starvation bug
that `mlfqtest` does not catch and `usertests` sometimes does. Starting the
scan where the last one stopped costs one integer.

**One cursor per level — the eighth Part 5 case.** A single shared cursor
looks equivalent and is not: every selection at a high level moves it, so the
scan at every lower level restarts from wherever the high-level process
happened to sit in the table. In this lab that high-level process is the
measuring parent, which wakes every tick, so the effect is not subtle. On
kernels differing only in that one declaration `[observed]`: one shared cursor
serves the two identical spinners **326 to 74** over a `schedtest 400` under
`mlfq` — which reads as MLFQ honouring a 3∶1 ticket ratio it never even looked
at — and one cursor per level gives an even split, 200 to 200.

This is what the case *"Part 5: MLFQ round-robins equal-priority processes
fairly"* pins down. It is graded from the same `schedtest`-under-`mlfq` boot
that already grades *"getpinfo counts selections under POLICY=mlfq too"*, so it
costs no extra boot: `schedtest` gives its two children 30 and 10 tickets,
which MLFQ disregards, so a per-level kernel splits the 100-tick window near
**500** per mille each `[observed]` and a shared-cursor kernel lands at **820**
`[observed]` (82 selections to 18) — well outside the 300-to-700 band. The
`mlfqtest`-driven cases do **not** see this: with one shared cursor `mlfqtest`
still passes every one of them, its first child merely above the bottom queue
in 45 samples of 200 against the reference's 55 `[observed]`. It took a case
that reads the *share* of two same-level spinners to catch it. If a student's
MLFQ numbers show a strong split between two identical spinners, this is it.

Marking: a solution that keeps four explicit linked-list queues instead of
scanning the table is *better* and is fine — the per-queue turn order is
structural there rather than something you have to remember. Check the boost
still touches every process, including the ones on no queue.
</details>

<details>
<summary><b>Rule 4 — the allotment, in <code>yield()</code></b></summary>

```c
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;

#if SCHED_POLICY == SCHED_MLFQ
  p->allot++;
  if(p->allot >= MLFQ_ALLOTMENT(p->prio)){
    p->allot = 0;
    if(p->prio < NPRIO - 1)
      p->prio++;
  }
#endif

  sched();
  release(&p->lock);
}
```

`yield()` is reached from exactly two places, both in `kernel/trap.c`, and
both of them mean "the timer interrupt went off while this process was
running". So one call here is one whole tick of CPU consumed without
blocking, and a process that blocks in a system call before its tick is up
never arrives. That single fact is the entire mechanism by which an
interactive process stays near the top and a compute-bound one sinks; there
is no separate detection of "interactivity" anywhere, and there does not need
to be.

Two things to mark:

**Where it is charged.** Charging in `scheduler()`, where the process is
*selected*, looks equivalent and is backwards: a process that blocks
immediately would be demoted for having been picked, and the gaming behaviour
Rule 4 exists to punish would be rewarded. This is a case, not a marking note.
`mlfqtest`'s own parent blocks on every tick of the run, so it must never be
charged; move the charge to the selection and it is at the top level in 0 of
200 samples instead of 169 or more `[observed]`, and *"a process that blocks before
its tick is up is not demoted"* fails.

**The granularity.** The charge is a whole tick even for a process that used
a sliver of one. That is what the hardware gives us, and it is also exactly
the gap that lets a process game the scheduler by sleeping just short of a
tick — which is what Rule 5 is there to bound rather than to prevent. A
write-up that notices this deserves the credit.
</details>

<details>
<summary><b>Rule 5 — the boost, and where it must not go</b></summary>

At the top of `scheduler()`'s loop, before the choice:

```c
extern uint ticks;              // kernel/trap.c
static uint last_boost = 0;

if(ticks - last_boost >= MLFQ_BOOST){
  last_boost = ticks;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state != UNUSED){    // a free slot is not a process
      p->prio = 0;
      p->allot = 0;
    }
    release(&p->lock);
  }
}
```

The `UNUSED` guard is not cosmetic, and a submission without it should be
marked: boosting a free slot writes level 0 into a process-table entry nobody
owns, and the next `fork` into that slot then reports level 0 whether or not
`allocproc` reset it. That is Rule 3's case quietly disarmed — see *What
`mlfqtest` is actually looking at*, below.

Two things matter, and they are separable. **The clock has to be one the
running process cannot influence**: a boost hooked into a system call or into
`fork` fires often for a process that is running and never for one that is
not. And **the sweep has to touch every process**: a boost that lifts only
the process running when it fires keeps lifting whichever process is running,
which is then the process that goes on running, and everything else stays at
the bottom for ever. That second half is what `mlfqtest`'s two spinning
children are for, and it is the case *"the periodic boost lifts every process
out of the bottom queue again"*.

`scheduler()`'s loop satisfies both without an argument: it turns on the
global tick counter, and it runs whether or not any process does.

`ticks` is read without `tickslock`. A stale read moves a boost by one tick
and nothing else, and taking `tickslock` here would nest it inside `p->lock`
on one path and outside it on another, which is a lock-order inversion — a
real deadlock in exchange for a cosmetic improvement. Say so if a student
asks; it is a good question.

Note that boosting resets `allot` as well as `prio`, which is what Rule 5
says: a fresh allotment, not just a fresh level.

Worth knowing, and worth not over-claiming: on *these* constants that reset is
not observable. `MLFQ_ALLOTMENT(0)` is one tick, and the demotion in `yield()`
zeroes `allot` whenever it fires, so a boosted process leaves level 0 after
exactly one tick whether or not the boost reset the count. A kernel identical
to the reference but for the missing `p->allot = 0;` produces
**byte-identical** `mlfqtest` output `[observed]`. It is an equivalent
program, not a bug that slipped past the tests, and no test could separate the
two. It stops being equivalent the moment the top level's allotment is more
than one tick — which is the first thing anyone changes when they experiment
with the constants — so mark it as a point about the rule rather than as a bug
that happens to be invisible.
</details>

<details>
<summary><b>What <code>mlfqtest</code> is actually looking at</b></summary>

Two children that never block, sampled once a tick through `getpinfo`, and
the sampling parent, which blocks in `pause()` on every one of those ticks and
is therefore the interactive process the five rules exist to serve. A child
must be **seen at the bottom level** — that needs Rule 4 — and must then be
**seen above it again without ever having blocked**, which nothing but Rule 5
can produce; **both** children must be above it in the same sample, which
needs a Rule 5 that sweeps the whole table; a child must be above the bottom
for at least 15 per cent of the run, which needs allotments that double rather
than a demotion on every tick; and the parent must **stay** at the top,
which needs the allotment charged where the CPU is given up. Afterwards it
forks once more and checks Rule 3.

The second child is worth its one `fork`. With a single spinner, a boost that
lifts only the running process is indistinguishable from a correct one: the
one child *is* the running process. With two, the boost lands on whichever
child is running, that child then goes on running, and the other never comes
back up — `[observed]` 0 to 29 samples above the bottom queue across boots
against 55, and never once both children above it together.

The Rule 3 check is worth its ten lines. `allocproc` hands out the lowest free
process-table slot, so the process forked after the sunk child has been
reaped takes that child's slot back — and the child left it at the bottom
level. A kernel that never resets `prio` in `allocproc` therefore reports a
brand-new process at level 3. Nothing else in the lab can see it: under
`rr` and `lottery` nothing ever writes `prio` at all, and `struct proc` is a
zero-initialised global, so a slot that has never been used reads 0 anyway.
It only shows on a **recycled** slot under `mlfq`, which is exactly the
situation this constructs. (For the same reason the reference's boost skips
`UNUSED` slots — boosting a free slot is meaningless, and doing it would hide
this.)

The arithmetic: after a boost a child spends 1 tick at level 0, 2 at level 1
and 4 at level 2 before reaching the bottom — 7 of **its own** ticks out of
every `MLFQ_BOOST` = 40. Two children share the CPU, so that is about 14 ticks
of the clock, and sampling once a tick about one sample in three catches it.

That number is not reproducible the way `schedtest`'s is, and it is worth
knowing which kind of number you are looking at: the sampling loop's phase
relative to the boost is not fixed from boot to boot, so the reference gives
**55 or 56 samples of 200** above the bottom queue after sinking across the
boots in hand, and the parent's own level is at the top in 169 to 200 of them.
Both cases are set far below that — 3 samples for the boost, 15 per cent for
the allotments, 25 per cent for the parent — precisely because the quantity
wanders. A student's numbers will not match to the digit and are not supposed
to.

The measuring parent is itself the demonstration of the other half of MLFQ:
it sleeps every tick, so it never reaches `yield()`, so it is never charged
an allotment, so it never leaves level 0 — and it therefore pre-empts the
compute-bound child every single tick. That is the interactive process the
five rules exist to serve, and it is running in the same test.
</details>

---

## Notes on the reference implementation

- The reference kernel adds about 120 lines across nine files for Parts 2 and
  3. If yours is much larger, the likely cause is validating user addresses by
  hand.
- `kernel/syscall.c`'s `syscall_names[]` is a second table that has to be kept
  in step with the first by hand. That is a real defect in this design, and the
  usual fix — one table of `{function, name}` structs — is a reasonable thing
  to do and costs nothing.
- Parts 4 and 5 add about 130 lines: forty of lottery, sixty of MLFQ across
  `scheduler()` and `yield()`, two system calls and one process-table walk.
  Part 5 is the largest single change and almost all of it is in one
  function.
- Three kinds of thing `tests/run.sh` checks that are not about your kernel at
  all, and all of them are stated in the handout: `-Werror` is still in the
  `Makefile`; `user/tracetest.c`, `user/sysinfotest.c`, `user/schedtest.c` and
  `user/mlfqtest.c` still match the copies in `starter/overlay/` (every Part
  2–5 verdict is scraped from what those four print, and they sit in an
  editable tree); and `kernel/main.c` still prints its `sched: policy=…` line,
  which the harness waits for at boot to confirm it is talking to the kernel
  it just built. A tree that has lost that line reports every kernel case as
  *"the kernel stopped responding"*, with the evidence line naming
  `^sched: policy=rr$` — look at `main.c`, not at the system calls.
- The `sched: nothing chosen but N runnable` watchdog at the bottom of
  `scheduler()` is given, is the same under all three policies, and is the
  reason a starter tree fails Parts 4 and 5 in seconds rather than at four
  successive deadlines. It cannot report a false alarm: interrupts have been
  off since the top of the loop and `found == 0` means no `swtch` happened,
  so on a single-CPU kernel nothing can have changed state since the choice
  was made. The harness treats the line as fatal and names it as such
  (`SchedulerStall`) rather than calling it a panic.
- The one thing the boot line does **not** catch is a policy branch that
  compiles under the right flag and runs the wrong code — swap the bodies of
  the `SCHED_LOTTERY` and `SCHED_RR` branches and the kernel still announces
  `sched: policy=lottery`. What catches that is Part 4's share case, at 500
  per mille, and Part 5's demotion case; the boot line proves which *build*
  is running, not which *policy* it implements.
- Every observed number in this file is `[observed]` on the pinned tree with
  `riscv64-unknown-elf-gcc` and `qemu-system-riscv64` at one CPU. Addresses,
  free-memory figures and process counts will differ on your machine; the
  explanation, not the number, is what is marked.
