# Exercise Sheet 6 — The memory API, and address translation

**Attempt after Week 6.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise06-solutions.md`](solutions/exercise06-solutions.md).

**This sheet leans on:** OSTEP ch. 14–15; CS:APP §7.1–7.7; Wahbe et al. (1993),
*Efficient Software-Based Fault Isolation*.

**You will need:** a C compiler, `gdb`, and `valgrind` (§B1); optionally
python3 with `relocation.py` from `ostep-homework/vm-mechanism/` to check §B2,
which is otherwise pen-and-paper.

> **Note.** OSTEP ch. 14 has a "Homework (Code)" section but ships **no code**
> in either upstream repo — you are expected to write the broken programs
> yourself. §B1 below supplies that missing material. It is also your warm-up
> for Lab 3, where you implement the allocator side of this interface.

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns nothing —
the justification is the answer.*

**A1.** `free()` cannot work as specified, because it is not told how many
bytes to release.

**A2.** A C program that exits without calling `free()` leaks that memory
until the machine is rebooted.

**A3.** After `int *x = malloc(10 * sizeof(int));`, the expression
`sizeof(x)` evaluates to 40.

**A4.** A program that writes one element past the end of a `malloc`'d array
will crash when that write executes.

**A5.** `malloc()` is a system call.

**A6.** Under base-and-bounds, the operating system is invoked on each memory
reference to add the base register to the virtual address.

**A7.** Static relocation — a loader rewriting every address in the executable
when the program is placed in memory — provides the same protection as
base-and-bounds, since both ensure the program's addresses land in its own
region.

**A8.** When compiling `a.c`, the compiler assigns final addresses to the
functions `a.c` calls, including those defined in other files.

---

## B. Break it, diagnose it, translate it

**B1. The missing ch. 14 homework.**
Write each program below (a few lines each), compile it with `-g`, and run it
three ways where asked: directly, under `gdb`, and under
`valgrind --leak-check=yes`. **Predict before you run.**

  (a) `null.c`: create an `int *`, set it to `NULL`, dereference it.
      (i) What happens when you run it directly? (ii) What does `gdb` add?
      (iii) What does valgrind report, and how does valgrind know address 0 is
      invalid when the compiler said nothing?
  (b) `leak.c`: `malloc` room for 10 ints, exit without freeing.
      (i) Does anything visibly go wrong? (ii) What does valgrind report?
      (iii) Is the missing `free()` actually a bug? Answer once for this
      program and once for a long-running web server, and name the ch. 14
      principle your two answers rest on.
  (c) `overflow.c`: `int *data = malloc(100 * sizeof(int));` then
      `data[100] = 0;`. Predict, run, then valgrind. Explain why this program
      can run to completion without any visible error.
  (d) `uaf.c`: allocate the same array, `free(data)`, then print `data[50]`.
      Same drill. What might the print show, and why is "it printed the old
      value" the *worst* outcome?
  (e) `badfree.c`: allocate the same array, then call `free(data + 50)`.
      This one usually announces itself even without valgrind — why can the
      allocator detect this case itself, when it silently missed (c) and (d)?
  (f) Of the programs above, only (e) draws a compiler warning (gcc 13 emits
      `-Wfree-nonheap-object`); (a)–(d) compile silently, and (b)–(d) can run
      without visible error. State the general lesson in one sentence — and say
      why the compiler could catch (e) but not the others.

**B2. Base-and-bounds, worked.**
A machine has 64 KB of physical memory, all of it available to user processes.
Each process has a 4 KB address space. Process P is loaded with **base =
32 KB** and a bounds register holding the address-space **size**.

  (a) For each virtual address, give the physical address or say **fault**:
      0, 1024, 3000, 4095, 4096, 5000.
  (b) The bounds register can instead hold the **end physical address** of the
      region. For P, state the value each design holds, and where in the
      translation each design performs its check. Why are the two logically
      equivalent?
  (c) What is the maximum value the base register could be set to such that
      P's address space still fits entirely in physical memory?
  (d) A test harness generates virtual addresses uniformly at random in
      [0, 4 KB). If the bounds (size form) is set to `l` bytes, `l ≤ 4096`,
      what fraction of the generated addresses is valid, as a function of `l`?
      Sketch the function. (Check with
      `python3 relocation.py -a 4k -p 64k -n 10 -l <l> -c` if you like.)
  (e) Inside its 4 KB slot, P uses 1 KB of code, 1 KB of heap and 1 KB of
      stack. What is the remaining 1 KB called, why can't base-and-bounds
      hand it to another process, and what design direction does this waste
      motivate?

**B3. Binding times.**
An address can be bound to its final physical location at **compile time**
(the executable contains absolute physical addresses), at **load time** (a
loader rewrites every address once, when the program is placed — ch. 15's
static relocation), or at **run time** (hardware translates every reference —
dynamic relocation).

  (a) Which binding time does base-and-bounds implement, and what hardware
      does it need that the other two schemes do not?
  (b) For each of the three schemes, give a one-sentence verdict on each of:
      can two arbitrary programs safely coexist in memory, and can a placed
      program later be moved?
  (c) Under dynamic relocation, list the exact steps the OS takes to move a
      process's address space to a different physical location.
  (d) The compiler sees one `.c` file at a time, so it cannot know where
      functions defined in other files will live. Name the two jobs the
      **linker** must therefore do, and explain why, under dynamic relocation,
      the linker can lay out *every* program as if it starts at address 0.

---

## C. Discussion and design critique

**C1. Where does `free()` get the size?**
Chapter 14 states that the size of an allocated region "must be tracked by the
memory-allocation library itself" — and says nothing about how. Before Lab 3
makes you choose for real: propose **two** different ways an allocator could
record, for each outstanding allocation, how large it is. Compare them on
per-allocation space overhead, the cost of finding the size inside `free(p)`,
and how each behaves when handed the invalid pointer from B1(e). Commit to one.

**C2. The case against C.**
A colleague watches you work through §B1 and concludes: "Garbage collection
eliminates dangling pointers, double frees and invalid frees outright. Manual
memory management is an indefensible historical accident; every new system —
kernels included — should be written in a garbage-collected language."
Sort the ch. 14 error catalogue into errors GC *eliminates*, errors it merely
*transforms*, and errors it *leaves untouched* — ch. 14 itself flags one of
the third kind. Then give your judgement on the claim, with the conditions
under which you would and would not accept it, treating the kernel as a
separate case.

**C3.** *An intrepid engineer proposes the following.* "Base-and-bounds
hardware is wasted silicon. Wahbe et al. showed that a toolchain can insert a
few instructions before each store and jump to confine a program to its
assigned region — software fault isolation. I propose our new OS drop the MMU
entirely: every program runs in one shared physical address space, built by
our compiler, which masks every risky address. We save the hardware, a
'context switch' becomes a jump, and calls between programs become function
calls — orders of magnitude faster than trapping into a kernel."

Evaluate this proposal. Address specifically: what the inserted checks cost
and where that cost lands; whether the compiler must now be trusted, and what
the paper's answer to that is; what happens to programs the engineer's
toolchain did not build; what a single bug in the isolation machinery costs
when everything shares one address space; and whether "we save the hardware"
is a real saving on a commodity CPU. Identify the settings in which this
design is genuinely the right answer. Conclude with a recommendation and the
conditions that would change it.
