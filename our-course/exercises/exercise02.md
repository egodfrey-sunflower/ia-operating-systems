# Exercise Sheet 2 — The process and the process API

**Attempt after Week 2.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise02-solutions.md`](solutions/exercise02-solutions.md).

**This sheet leans on:** OSTEP ch. 3–5; Ritchie & Thompson (1974), *The UNIX
Time-Sharing System*. OSPP's referee/illusionist/glue vocabulary (week 1) is
assumed.

**You will need:** python3 with the OSTEP simulators
`cpu-intro/process-run.py` and `cpu-api/fork.py` (from the `ostep-homework`
repo), and a C compiler for §B3–B4.

---

## A. Warm-ups

*True or false? In each case give a one- or two-sentence justification. A bare
verdict earns nothing — the justification is the answer.*

**A1.** A program and a process are the same thing.

**A2.** After `fork()` returns, the newly created child begins execution at the
start of `main()`.

**A3.** When a blocked process's I/O completes, the process resumes running on
the CPU.

**A4.** A process that has exited but whose parent has not yet called `wait()`
still occupies an entry in the OS's process list.

**A5.** A successful call to `exec()` returns 0 to the calling process.

**A6.** Immediately after `fork()`, parent and child hold identical copies of a
variable `x`, but an update by one is invisible to the other.

**A7.** The shell must be part of the operating system kernel, since it can
launch and control other programs.

**A8.** Redirecting a command's output (`wc p3.c > out.txt`) requires the `wc`
program to contain code supporting redirection.

---

## B. Process traces and the process tree

> **Cost model for B1** (matches the simulator's defaults): executing one CPU
> instruction takes one tick; issuing an I/O occupies the CPU for the tick in
> which it is issued; the I/O then takes **5 ticks**, during which the issuing
> process is *blocked*; when the I/O completes the process becomes *ready* (a
> process whose final instruction was the I/O is then finished). Some simulator
> versions charge one extra tick per I/O completion — if `-c -p` output differs
> from your trace by a constant per I/O, that is why; every comparison below is
> unaffected.

**B1. The state machine under different policies.**
Work each part on paper first — states of each process at every tick, CPU
utilization, total elapsed time — then check with the simulator.
  (a) `python3 process-run.py -l 5:100,5:100`. Predict CPU utilization and elapsed
      time. Why is this the least interesting workload the simulator can run?
  (b) Compare `-l 4:100,1:0` against `-l 1:0,4:100`, both with
      `-S SWITCH_ON_IO`. Give elapsed time and utilization for each. Why does
      the *order* of the two processes matter, and what general scheduling
      lesson does the difference illustrate?
  (c) Re-run `-l 1:0,4:100` with `-S SWITCH_ON_END`. Give the new elapsed time
      and utilization. Exactly how much time is wasted, and what is it equal to?
  (d) With `-l 3:0,5:100,5:100,5:100 -S SWITCH_ON_IO`, contrast
      `-I IO_RUN_LATER` with `-I IO_RUN_IMMEDIATE` (no tick-exact trace needed).
      Which resource does `IO_RUN_LATER` leave idle, and why might running a
      process that *just finished* an I/O again immediately be a good idea?

**B2. The process tree, and what exit does to it.**
`fork.py` describes histories as action strings: `a+b` means "a forks b", `c-`
means "c exits". When a process exits, `fork.py` reparents its children to the
**root process `a`** by default; with `-R` it reparents them to the exiting
process's **own parent**.
  (a) For `python3 fork.py -A a+b,b+c,c+d,c+e,c-`, draw the process tree after each
      action, and give the final tree under the default reparenting rule.
  (b) Give the final tree for the same action string under `-R`.
  (c) Whichever rule is used, *somebody* must adopt an orphan. Using ch. 4's
      account of what happens when a process exits, explain what would go wrong
      if an exiting process's children were simply left with no parent at all.
  (d) `python3 fork.py -t -F` shows only a final tree and asks for the actions. Can two
      *different* action strings produce the same final tree? Either prove they
      cannot, or give the smallest example you can — and state exactly what a
      final tree of `n` processes does let you infer about the number of forks
      and exits in the history.

**B3. The shell's trick.**
Recall ch. 5's `p3.c` (fork, then the child execs `wc` while the parent waits)
and `p4.c` (identical, except the child first does
`close(STDOUT_FILENO); open("./p4.output", O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);`
before the exec).
  (a) For `p3.c`: which output orderings are possible, and which lines are
      *guaranteed* to appear where they do? What single call is responsible for
      the determinism, and what orderings become possible if it is removed?
  (b) The chapter notes the "hello" message printed *once*, not twice. What
      does that tell you about where the child begins execution?
  (c) `p4.c` works because of two separate facts about file descriptors. State
      both, and explain why the `close`/`open` pair must run *in the child,
      between fork and exec* — what breaks if the shell instead did it before
      calling `fork()`?
  (d) Describe, call by call, what the shell does to run
      `grep -o foo file | wc -l`. Why does neither `grep` nor `wc` need to know
      it is in a pipeline — and which of OSPP's three OS roles is the kernel
      playing when it carries bytes between them?

**B4. Fork arithmetic.**
Consider:

```c
int main(void) {
    for (int i = 0; i < 3; i++)
        fork();
    printf("hello\n");
    return 0;
}
```

  (a) How many processes exist by the end (counting the original), and how many
      times is `hello` printed? Generalise to a loop bound of `n`.
  (b) Draw the process tree, and write it as a `fork.py`-style action string
      (invent names; the original process is `a`). How many children does the
      *original* process have, and why is the tree not a chain?
  (c) *(stretch)* Run with output redirected to a file and `hello` can appear
      **8 times even if the `printf` is moved *before* the loop**. Explain,
      given two supplied facts: C stdio buffers output in user memory, flushing
      it at exit (and file output is fully buffered, while terminal output is
      line buffered); and the buffer lives in the memory that `fork()`
      duplicates.

---

## C. Discussion and design critique

*Longer-form. Aim for a few paragraphs each. This week's discussion questions ask
you to **argue the strongest case against a claim the reading makes** — steelman
the opposition, then say what conditions decide the matter.*

**C1.** Chapter 5's aside declares that with `fork()`/`exec()` "the UNIX
designers simply got it right", filing the API under Lampson's *get it right*.
Not everyone agrees. A 2019 HotOS paper (Baumann et al., *A fork() in the
road*) argues — you need only these claims, supplied here — that: `fork()`
logically copies an *entire* address space merely to throw the copy away at the
next `exec()`, so making it fast demands elaborate machinery behind the scenes;
the child silently inherits *everything* — open descriptors, signal
dispositions, environment — so a privileged program that forks leaks state
unless it remembers to scrub each item; the copied-but-unflushed I/O buffers of
§B4(c) are one symptom; and a single-call `spawn()`-style API (as Windows and
POSIX `posix_spawn` provide) creates a new program directly, taking explicit
parameters for exactly what the child should inherit.

Make the strongest case you can *against* OSTEP's claim. Then make the
strongest reply on OSTEP's behalf — what does the fork/exec gap buy that a
spawn API must reproduce parameter by parameter? Finish with a judgement:
*under what conditions* is each side right? (A verdict without conditions
earns little.)

**C2.** Chapter 4 presents the separation of **policy** from **mechanism** as
"a form of modularity" — change the policy without rethinking the mechanism.
Argue the strongest case against *strict* separation: what information does a
clean policy/mechanism interface hide, and when does hiding it produce worse
decisions or slower systems? Give at least one concrete setting (from this
course so far, the lab, or systems you know) where you would deliberately blur
the boundary — and state what property of that setting justifies it.

**C3.** In their conclusions, Ritchie & Thompson attribute much of UNIX's
success to the fact that its designers built it *for themselves* — for their
own convenience, not to a customer's specification. Argue the strongest case
against "design for yourself" as a general method for building systems: whom
does it fail, and what does it systematically miss? Then identify the
conditions that made it work so well in 1974 — and say whether those conditions
hold for a team building, say, a phone OS today.
