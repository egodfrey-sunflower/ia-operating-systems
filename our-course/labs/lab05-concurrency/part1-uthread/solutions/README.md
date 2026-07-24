# Lab 5 Part 1 — Reference thread package and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference uthread.c, the completed swtch.S, and the account   ║
║  of what each case is for. The initial-frame construction in       ║
║  uthread_create is the one genuinely hard idea in this part, and   ║
║  it is four lines long: reading them costs you the whole of it.    ║
║                                                                   ║
║  If you are stuck, the handout's diagram and its "If you get       ║
║  stuck" list say everything this file says, without the code.      ║
╚═══════════════════════════════════════════════════════════════════╝
```

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2, zero warnings
make test       # ../tests/run.sh on this directory: 15 passed, 0 failed
```

The reference is 220 lines of `uthread.c` as shipped, of which 129 are code
and the rest comment, plus 55 lines of `swtch.S`. The whole suite takes about
three seconds, including a memcheck run and a helgrind run.

**The untouched starter scores `0 passed, 13 failed, 2 skipped` of 15.**
Nothing passes by accident. The two skips are the valgrind cases, each of
which is gated on the plain run of the case it re-runs: instrumenting a case
that is already failing produces a wall of consequential errors and no
information.

## The design in one paragraph

A fixed table of `UTHREAD_MAX` slots, a singly linked FIFO ready queue through
a `->next` field, and one saved context for the main stack. Threads switch to
the **scheduler**, never directly to each other: `uthread_yield` and
`uthread_exit` differ only in which state they write before calling
`uthread_swtch(&t->ctx, &sched_ctx)`, and `uthread_run` decides what happens
next. That shape is what makes releasing a dead thread's stack safe — the
scheduler is standing on the main stack when it frees it, not on the stack it
is freeing. A design that switches directly from thread to thread has to defer
the free to somebody else, and the usual answer is never to free at all.

The initial frame is the whole of the difficulty:

```c
top = ((unsigned long)t->stack + UTHREAD_STACK) & ~15UL;
sp  = (unsigned long *)(top - 16);
sp[0] = (unsigned long)uthread_entry;   /* what `ret` pops into %rip */
sp[1] = 0;                              /* uthread_entry's "return address" */
t->ctx.rsp = (unsigned long)sp;
```

`uthread_swtch` loads `%rsp = sp` and executes `ret`, which pops `sp[0]` into
the program counter and leaves `%rsp` at `top - 8` — eight below a multiple of
16, which is where the ABI says a function finds it on entry. `sp[1]` is never
read: `uthread_entry` does not return, because there is nothing below it to
return to.

<details>
<summary><b>Why the fake return address has to be there at all</b></summary>

Only for alignment. If `ctx.rsp` were `top - 8`, the `ret` would leave `%rsp`
at `top`, a multiple of 16, and every function `uthread_entry` calls would
find its stack eight bytes out of phase. The compiler assumes the phase and
uses it when it aligns local buffers for SSE. Nothing complains; things just
occasionally fault inside library code.

The alternative is a two-instruction assembly trampoline that fixes the
alignment itself. That is what several real thread packages do, and it is
worth knowing about, but it is one more file to write for no gain here.

</details>

## Design freedom, and how it was checked

`tests/run.sh` promises that the queue's representation and the source of the
stacks are yours. **That claim was checked by writing a second, independent
implementation** — a fixed-size ring of pointers with no `->next` field at
all, stacks from `mmap` with a `PROT_NONE` guard page below each one, a
rotating slot cursor instead of a scan from zero, and struct-assignment
zeroing instead of `memset` — and running the whole suite against it:
**15 passed, 0 failed**.

That is the only sound way to check a claim of this kind. Simulating the
alternative inside the reference — say, by keeping the linked list and merely
allocating stacks differently — would not test it, because the property at
issue is whether any harness case can *see* the difference, and only a
different implementation can answer that.

What the harness genuinely does fix is the **order threads run in**, which is
in `uthread.h` and is what six of the fifteen cases diff against. That is not
a design choice being taken away; it is the specification.

## What each case is for

Every case below was checked by breaking the reference in a way that should
fail it and confirming that it does. The right-hand column names the mutation.

| Case | Isolated by |
|---|---|
| one thread runs and the scheduler returns | `%rbx` not saved in `swtch.S`; `uthread_exit` leaving the thread runnable |
| three threads take turns in round-robin order | `%rbp` not restored; `enqueue` pushing to the head |
| threads with uneven lifetimes leave the queue cleanly | `dequeue` leaving a stale tail pointer — **the case that isolates it most cleanly; three others fail with it: nested creation, slot reuse, and yielding from the main context** |
| a thread that returns ends like one that calls `uthread_exit` | `enqueue` pushing to the head; `uthread_exit` leaving the thread runnable |
| a running thread can create more threads | stale tail pointer; head-insertion |
| twelve threads over five rounds keep their order | head-insertion; `%rbp` not restored |
| `uthread_self()` matches the id create() returned | `uthread_self` returning 1 rather than 0 in the main context |
| the callee-saved registers survive a switch | any one of the six dropped from `swtch.S`, save half or restore half. Named individually |
| each thread's stack survives every other thread | the initial frame built at `top - 8` instead of `top - 16` — an eight-byte alignment error, which nothing else in the suite notices |
| a finished thread's slot is reused | `uthread_run` freeing the stack but not marking the slot free. The id case below fails with it, and only with it: both fill the table twice |
| a thread id is never reused, even when a slot is | `t->tid = i + 1` — the id taken from the slot index rather than from a counter. **The only case that catches it**: within one batch a slot index is already distinct, so it takes two batches through the same slots to see the repeat |
| creating past `UTHREAD_MAX` is refused, not fatal | a slot search that hands out an occupied slot when the table is full |
| yielding from the main context is harmless | `uthread_yield` dereferencing `current` without a null check |
| memcheck finds nothing while eight stacks interleave | writes past the end of a stack allocation |
| helgrind is clean | the baseline: Part 1 has one kernel thread and no races to find, so anything reported here is about the harness or the C library |

Every mutation named above was compiled `-Wall -Wextra -Werror` and run against
the whole suite. None of them scored full marks, and every case in the suite is
isolated by at least one of them — which is the only evidence that a case is
checking anything.

<details>
<summary><b>Why the register probe is written in assembly</b></summary>

`tests/regprobe.S` loads a distinct constant into each of `%rbx`, `%rbp`,
`%r12`–`%r15`, calls `uthread_yield`, and returns a bitmask of the ones that
changed.

In C the compiler decides which registers hold which values. A C version with
six `volatile long` locals keeps them all in memory, and memory lives on the
thread's own stack, which the switch preserves whether or not it saves a
single register. The case would pass against a `uthread_swtch` that moved only
`%rsp`. That is the exact shape of a test that cannot fail, and the only fix
is to name the registers.

</details>

<details>
<summary><b>Why the transcripts can be exact, and what that is worth</b></summary>

Cooperative threads over a FIFO queue have one legal interleaving, so six
cases are `diff -u` against a stored file. No other part of this lab can do
that.

It is worth more than it looks. A diff detects an ordering that is merely
*different*, not just one that is wrong in a way somebody anticipated — a
queue that happens to run threads in the right order for three threads and the
wrong order for twelve is caught by `p1_order` without anyone having had to
predict that failure mode. Assertion-based cases only catch what they were
told to look for.

The cost is that the transcripts have to be regenerated if `cases.c` changes,
and a change to `cases.c` that nobody notices silently invalidates six cases.
They are checked in and treated as specification.

</details>

## What is on your honour

Nothing here can be enforced by the harness:

- **the switch is only the seven registers.** Saving the whole register file,
  or calling `getcontext`/`swapcontext` instead of writing `swtch.S`, passes
  every case. The point of the part is the seven and why it is seven.
- **an exited thread's stack is released.** memcheck runs with
  `--leak-check=no`, because the question it is asked here is whether a stack
  is written outside its allocation, not whether it is freed. A package that
  never frees a stack passes every case: the slot-reuse case forces slots to
  come back, and nothing forces the memory to.
- **the queue is a queue.** An implementation that scans the table for the
  next runnable slot in index order produces the same transcripts when ids and
  slots happen to line up. The handout describes a queue; the cases can only
  see the order.
