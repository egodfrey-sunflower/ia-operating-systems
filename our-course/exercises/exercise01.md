# Exercise Sheet 1 — What an operating system is

**Attempt after Week 1.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise01-solutions.md`](solutions/exercise01-solutions.md).

**This sheet leans on:** OSTEP ch. 1–2; OSPP §1.1–1.2.

**You will need:** a C compiler, and the four demo programs from OSTEP ch. 2
(`cpu.c`, `mem.c`, `threads.c`, `io.c` — available in the `ostep-code` repo under
`intro/`, or retypeable from the chapter in a few minutes each).

---

## A. Warm-ups

*True or false? In each case give a one- or two-sentence justification. A bare
verdict earns nothing — the justification is the answer.*

**A1.** An operating system's main purpose is to make programs run faster.

**A2.** Virtualizing the CPU means the operating system gives each running
program its own physical processor core.

**A3.** If a program is running, the operating system is not.

**A4.** The difference between memory virtualization and persistence is that
persistent data must survive a loss of power.

**A5.** Because the OS is a resource manager, it must be involved in every memory
access a running process makes.

**A6.** OSPP's "illusionist" role and OSTEP's "virtualization" describe
essentially the same idea.

**A7.** An operating system is a program like any other, so it could be written
in any language and run as an ordinary application.

---

## B. Evidence from the demo programs

*OSTEP ch. 2 makes four claims and backs each with a program. Predict the output
before running, then run it. Where you were wrong, explain why.*

**B1.** `cpu.c` takes a string and loops forever printing it.
  (a) Run four instances simultaneously (`./cpu A & ./cpu B & ./cpu C & ./cpu D &`).
      Before running: how many CPUs does your machine have, and what do you
      predict the output ordering will look like?
  (b) Run it again. Is the interleaving identical? What does your answer tell you
      about who is choosing the order?
  (c) The machine has (say) 8 cores and you ran 4 processes. Does this experiment
      actually demonstrate CPU *virtualization*? How would you change it so that
      it does?

**B2.** `mem.c` allocates an integer, prints its address, then increments and
prints it in a loop. Run it as `./mem 0` — the `ostep-code` copy takes the
starting value as an argument (the book's Figure 2.3 copy ignores it, so
`./mem 0` works with either).
  (a) Run two instances at once (`./mem 0 & ./mem 0 &`). What do you observe
      about the two addresses?
  (b) Now run both again under `setarch -R` (e.g. `setarch -R ./mem 0 &
      setarch -R ./mem 0 &`), which disables **address-space layout randomisation**.
      What changes? OSTEP's own text assumes this setting.
  (c) In the `setarch -R` case the two processes report the *same* address yet
      never see each other's values. Explain, in one sentence, what must be true
      about that address.
  (d) ASLR deliberately breaks the tidy result in (c). What is it defending
      against, and why does it not undermine the point the experiment is making?
  (e) Predict what would happen on a machine with no memory virtualization at all
      — what would the two programs do to each other?

**B3.** `threads.c` creates two threads that each increment a shared counter `N`
times.
  (a) Run it with `N = 1000`. Then with `N = 100000`. What differs, and why does
      the loop count matter?
  (b) Give the shortest interleaving of machine-level operations you can that
      produces a final count of exactly `2N - 1`.
  (c) The chapter says this is a problem the OS must help solve. Name the two
      distinct things a program would need in order to fix it.

**B4.** `io.c` writes to a file and calls `fsync()`.
  (a) What guarantee does `fsync()` provide that a plain `write()` does not?
  (b) Why can't the OS simply make every `write()` durable and drop `fsync()`
      from the interface?

---

## C. Discussion and design critique

*Longer-form. Aim for a few paragraphs each; these rehearse the open-critique
part that closes every recent Cambridge paper.*

**C1.** For each of the following, say whether the OS is acting as **referee**,
**illusionist**, or **glue** in OSPP's sense — and justify borderline cases,
because several are genuinely more than one:
  (a) the CPU scheduler deciding which process runs next;
  (b) `malloc()` returning a pointer into a heap that grows on demand;
  (c) the file system presenting a directory tree over a flat array of blocks;
  (d) two processes communicating through a pipe;
  (e) killing a process that has exceeded its memory limit;
  (f) a device driver exposing a printer through the same `write()` call used for
      files.

**C2.** OSTEP claims the OS pursues abstraction, performance, protection and
reliability, and notes these conflict. Pick **two** of those goals and describe a
concrete mechanism, from anywhere in computing you know, where improving one
measurably damages the other. Be specific about the cost.

**C3.** *An intrepid engineer proposes the following.* "Operating systems are
overhead. For our new embedded device we run exactly one application, forever, on
a single core, with a fixed amount of memory and no user logins. We will therefore
ship no operating system at all — the application will boot directly on the
hardware and talk to devices itself. We will get every cycle and every byte."

Evaluate this proposal. Which of the OS's roles genuinely disappear under these
assumptions, and which are being quietly reinvented inside the application? What
would you need to know about the device before agreeing or refusing? Conclude
with a recommendation.
