# Lab 5 Part 1 — A user-level thread package

**Weeks 11–12 · 3.5 hours · 19% of Lab 5 · OSTEP ch. 26; OSPP §4.6–4.8**

x86-64 Linux, userspace C: one source file of about 130 lines, plus seven
instructions of assembly. You will need `gcc`, `make` and `valgrind`.

Chapter 26 says a thread is a stack, a set of registers and a scheduling
entry. This part makes that literal. By the end you will have `thread_create`,
`thread_yield` and `thread_exit` that the kernel knows nothing about: one
kernel thread, several stacks, and a context switch you wrote that decides
which one the processor is standing on.

The threads are **cooperative**. They switch on `uthread_yield()` and at no
other time — no timer, no signal, no preemption. That has a consequence worth
noticing early: **there are no races in Part 1.** A thread that does not yield
cannot be interrupted, so a critical section is any run of code with no yield
in it. Part 2 takes that away.

## Layout

```
part1-uthread/
  README.md          this handout
  starter/           Makefile, uthread.h (the contract), uthread.c, swtch.S  <- work here
  fixtures/          the six expected transcripts
  tests/run.sh       the autograder
  solutions/         SPOILERS. Reference package and answer key. Later.
```

```sh
cp -r starter tests fixtures ~/lab5/p1
cd ~/lab5/p1/starter
make test
```

Copy all three: `make test` runs `../tests/run.sh`, and the transcript cases
read `../fixtures/`.

## What you hand in

`uthread.c` and `swtch.S`. `uthread.h` is the contract and does not change.

Fifteen cases, all machine-checked, and there is no prose deliverable in this
part — Part 1 is the one place in Lab 5 where the harness marks everything.
**The untouched starter scores `0 passed, 13 failed, 2 skipped`.** Nothing
passes by accident, including the two valgrind cases, which skip while the
cases they re-run are failing.

## The contract

`starter/uthread.h` is the specification and the harness includes *that*
header. Read it before you write anything. The parts worth saying twice:

**The ready queue is FIFO, and the harness can see it.** `uthread_create`
appends to the tail. `uthread_yield` sends the caller to the tail. The thread
at the head runs next. Six of the fifteen cases are byte-for-byte diffs of a
program's output against a stored transcript, and those transcripts are what
that policy produces. Any implementation with that behaviour passes — a linked
list through `->next`, an array of pointers, a ring buffer — and the harness
cannot tell which you chose.

**Thread ids start at 1 and are not reused within a run.** Slots are reused;
ids are not, and the harness fills the table twice to check it: an id that is
really the slot index repeats in the second batch and is caught there.
`uthread_self()` returns 0 in the main context, which is not a thread.

**A thread whose function returns must end exactly as if it had called
`uthread_exit()`.** There is nowhere for it to return to.

**`uthread_create` past `UTHREAD_MAX` returns −1.** Refusing is required;
crashing is not an acceptable substitute.

**`uthread_yield()` from the main context does nothing.** Not an error, not a
crash. There is nothing to switch to.

## The one hard idea: the first switch into a new thread

Everything else in this part is bookkeeping. This is the part that is worth
drawing.

`uthread_swtch(save, load)` saves seven registers into `*save`, loads seven
from `*load`, and executes `ret`. Notice what is *not* in the seven: the
program counter. The return address is not in a register — `call` pushed it
onto the stack — so swapping `%rsp` swaps the return address too, and the
`ret` at the bottom returns into whichever thread last saved `*load`.

For a thread that has run before, that is fine: it saved its context inside
`uthread_swtch`, and it resumes inside `uthread_swtch`. But a brand-new thread
has never been inside `uthread_swtch` and has no saved return address. So you
write one:

```
    high addresses
                          +----------------------+
    top      ------------>|                      |   one past the allocation,
                          |                      |   rounded DOWN to a multiple of 16
    top - 8  ------------>|         0            |   a fake return address:
                          |                      |   uthread_entry never returns
    top - 16 ------------>|   &uthread_entry     |   <--- ctx.rsp points HERE
                          |                      |
                          |    ... unused ...    |
    stack    ------------>|                      |   the malloc'd block
                          +----------------------+
    low addresses                                    (the stack grows downwards)
```

`uthread_swtch` loads `%rsp = top - 16` and executes `ret`. `ret` pops
`&uthread_entry` into the program counter and leaves `%rsp` at `top - 8`. The
new thread is now running inside `uthread_entry`, which calls its function and
then calls `uthread_exit`.

**`top - 8` is not slack and it is not an off-by-one.** The System V ABI says
a function finds `%rsp` eight bytes below a multiple of 16 when it is entered,
because its caller's `call` instruction pushed a return address there. A
thread whose first frame starts at a multiple of 16 instead runs perfectly
well for a while, and then dies inside something that uses an aligned SSE
store — `printf` is a good candidate — a long way from the mistake. The
harness's `each thread's stack survives every other thread` case is where an
eight-byte error shows up, and it will name the thread and the byte.

## The register list

`swtch.S` comes with the save half written and the restore half as a TODO. It
is a mirror image: seven `movq` instructions, the same displacements, `%rsi`
as the base, the register as the destination.

Seven, and not more, because of the ABI:

| Register | Who saves it | In `uthread_swtch`? |
|---|---|---|
| `%rbx`, `%rbp`, `%r12`–`%r15` | the callee | yes — the code either side of the call is entitled to assume they survive |
| `%rsp` | — | yes — swapping it is the whole trick |
| `%rax`, `%rcx`, `%rdx`, `%rsi`, `%rdi`, `%r8`–`%r11` | the caller | no — already spilled into the caller's frame, which comes back when the stack does |
| the vector and x87 file | the caller | no — same argument |

If one of the six goes missing, `the callee-saved registers survive a switch`
names it. That case is written in assembly, because in C the compiler decides
which registers hold which values and would happily pass the case against a
switch that saves nothing.

## Suggested order

1. `enqueue` and `dequeue`, and think about what happens when the queue
   empties. A tail pointer left pointing at a thread that is no longer on the
   queue is the single most common bug here: threads vanish, or the queue
   becomes a loop and the scheduler never returns.
2. `swtch.S`'s restore half.
3. `uthread_create` — the stack, then the initial frame, then the bookkeeping.
4. `uthread_run` — the scheduler loop.
5. `uthread_yield` and `uthread_exit`, which are four lines each and differ in
   one field.

Nothing runs until all five are there. Expect the first run to crash.

## Running the tests

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g -O2, zero warnings
make test       # ../tests/run.sh on this directory
```

The graded build is not your Makefile. `run.sh` compiles `uthread.c` and
`swtch.S` with its own `gcc -Wall -Wextra -Werror -std=gnu11 -g -O2` line into
its own temporary directory, so a Makefile that drops `-Werror` changes
nothing. Your Makefile still has to work — it is checked separately, because
`make` is the workflow.

Six cases are diffs. When one fails you get a `diff -u`: `-` is what the
transcript says, `+` is what your package printed.

The whole suite takes about three seconds.

## If you get stuck

1. **`TIMED OUT after 10s`** — nothing in Part 1 blocks, so this is the queue.
   A scheduler that never dequeues, a running thread that is also on the
   queue, or a stale tail pointer that made the queue circular.
2. **`killed by signal 11`** — a crash. In order of likelihood: the initial
   frame's top word is not `&uthread_entry`; `ctx.rsp` was set to `top` or
   `top - 8` rather than `top - 16`; a thread is running on a stack that has
   already been freed.
3. **`the run order is not the one uthread.h specifies`** with everything
   shifted — you are switching directly between threads instead of back to the
   scheduler, or `uthread_create` puts the new thread at the head.
4. **`threads with uneven lifetimes leave the queue cleanly` fails, and so do
   `a running thread can create more threads`, `a finished thread's slot is
   reused` and `yielding from the main context is harmless`** — that group of
   four, and nothing else, is `dequeue` emptying the queue without clearing the
   tail pointer.
5. **`%rbp did not survive uthread_yield()`** — a missing or wrong
   displacement in `swtch.S`'s restore half. The case names the register; the
   offsets are in `struct uthread_ctx` in the same order.
6. **`stack byte N is 0x..`** — a stack that two threads share, a stack
   pointer that comes back off by a few bytes, or the 16-byte alignment. Check
   that `top` is rounded *down*.
7. **`a finished thread's slot is reused` fails, and `a thread id is never
   reused, even when a slot is` fails with it, complaining that a create
   returned −1** — `uthread_run` frees the stack but does not set the slot's
   state back to free. Both cases fill the table twice, so both stop at the
   same place; those two and nothing else is this bug.
8. **`thread N of 128 and thread M were both given id ...`** — the id is the
   slot index, or some other number derived from where the thread landed. The
   slot comes back when a thread finishes; the id must not. One counter,
   incremented on every successful create, is the whole of it.
9. **`uthread: a dead thread was rescheduled`** — the scheduler switched into
   a thread that had exited. Its stack is gone; anything after this is
   nonsense. The scheduler must only requeue threads that yielded.
10. **`%d creates succeeded before the table filled`** — `uthread_create` is
   handing out a slot that is in use, or refusing one that is free. A slot is
   free when its state is free, not when its stack pointer is null.
11. **`yielding from the main context is harmless` fails alone** —
    `uthread_yield` or `uthread_self` is dereferencing the current-thread
    pointer without checking it is not null.
12. **memcheck reports an invalid write near a stack** — the initial frame is
    being written past the end of the allocation. `top` is `stack +
    UTHREAD_STACK` rounded down, and the two words go *below* it.
13. **`could not build the case driver against your library`** — `uthread.h`
    has been changed. It is the contract; put it back.

## Stretch goals

- **Preemption.** Deliver `SIGVTALRM` on a timer and switch from the handler.
  Then find out which of your own functions are not async-signal-safe, and
  what that means for `malloc` inside `uthread_create`.
- **Grow the stacks.** Map them with `mmap` and put an unmapped guard page
  below each one, so a stack overflow is a segfault at the moment it happens
  rather than a corrupted neighbour discovered later.
