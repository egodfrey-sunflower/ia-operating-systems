# Week 6 — The memory API, and address translation

> **Part I: Virtualization.** Week 6 of 27.

## What you'll learn

Last week ended with the address space — the abstraction of a private, contiguous
memory. This week you get both sides of it: the **interface** a C programmer uses
to fill it (ch. 14), and the first **mechanism** the OS and hardware use to fake
it (ch. 15). The pairing is deliberate: it mirrors weeks 2–3, where the process
API came first and limited direct execution second.

Chapter 14 is short and pointed. The technical content — stack allocation is
automatic, heap allocation is yours to manage, `malloc()` and `free()` are
**library calls** built over `brk`/`sbrk` and `mmap()`, not system calls — takes
a few pages. The chapter's real payload is its catalogue of the seven ways heap
management goes wrong (forgotten allocation, buffer overflow, uninitialized
read, leak, dangling pointer, double free, invalid free) and the disquieting
observation that most of them *don't crash*: the program compiles, runs, and is
still wrong. That is why the chapter ends on tools — `gdb` and `valgrind` — and
why this week's sheet makes you break programs on purpose and read what the
tools say. One conceptual point deserves flagging: memory management is
**two-level**. Even if you never call `free()`, the OS reclaims every page when
the process exits — leaks are a problem for long-running programs (and for the
OS itself), not for `ls`.

Chapter 15 extends limited direct execution to memory. The generic technique is
**interposition**: hardware transforms *every* address the program generates —
instruction fetch, load, store — from virtual to physical, invisibly. The first
incarnation is almost laughably simple: a **base** register (physical = virtual
+ base) and a **bounds** register (fault if the virtual address is too big),
one pair per CPU, sitting in what we now call the MMU. The chapter's structure
is the part to internalize: a table of what the *hardware* must provide
(privileged mode, the register pair, translate-and-check circuitry, privileged
instructions to update it, exceptions) against what the *OS* must do (find free
memory, save and restore the pair at context switches, handle the out-of-bounds
trap — usually by killing the process). Two consequences matter later: a
descheduled process can be **moved** in physical memory just by copying it and
updating the saved base — try that under static relocation — and the unused gap
between heap and stack is wasted, which the chapter names **internal
fragmentation** and which motivates next week's segmentation.

The cross-reading fills a genuine OSTEP gap. OSTEP has essentially nothing on
**linking**, and ch. 15's aside on software-based relocation is exactly where
linking touches the OS story: a loader rewriting addresses at load time *is*
relocation, done once in software rather than per-access in hardware. CS:APP
§7.1–7.7 gives you the vocabulary — relocatable object files, symbols, symbol
resolution, and the relocation step itself — and with it the classic **binding-time** ladder (compile time, load
time, run time) that Cambridge papers examine directly. Base-and-bounds is
run-time binding; you should be able to say precisely what each earlier binding
time gives up.

Finally, the Wahbe paper asks the inverted question: ch. 15 shows protection
*with* hardware help — what if you have none? **Software fault isolation**
confines a program by having the toolchain insert address checks before risky
instructions, then *verifying* the result at load time rather than trusting the
compiler. It is the week's protection story told from the other side, and it is
where sheet 6's closing critique lives.

**Key ideas:** stack vs heap · the seven memory errors · ran ≠ correct ·
gdb and valgrind · library calls over `brk`/`mmap` · two-level memory
management · address translation as interposition · base and bounds (dynamic
relocation) · the MMU · hardware/OS division of labour · internal
fragmentation · static vs dynamic relocation · binding at compile/load/run
time · linking, symbols, relocation · software fault isolation.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 14** | Interlude: Memory API | 12 | 1.7 h |
| 2 | **OSTEP ch. 15** | Mechanism: Address Translation | 15 | 2.1 h |
| 3 | **CS:APP 3rd ed. §7.1–7.7** | Linking: compiler drivers, static linking, object files, symbols, symbol resolution, and relocation | ~20 | 2.9 h *(read through — this is the course's linking material; OSTEP has none)* |

**Paper (required):** Wahbe, Lucco, Anderson & Graham (1993), *Efficient
Software-Based Fault Isolation*, SOSP. Cited by ch. 15 — OSTEP calls it "a
terrific paper." Isolation with no hardware support: the toolchain inserts
address-confining checks, a load-time verifier keeps the compiler out of the
trusted base. Read for the mechanism and the trust argument; skim the
performance evaluation.

**Not yet:** ch. 16 (segmentation) and ch. 17 (free-space management) land in
week 7. This week's sheet stops deliberately at the ch. 15 boundary — you'll
meet the *gap* between heap and stack now, and the algorithms for carving it up
next week.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise06.md`](../exercises/exercise06.md) — budget 3 h. Closed-book first, then [`../exercises/solutions/exercise06-solutions.md`](../exercises/solutions/exercise06-solutions.md) |
| **Lab** | [`../labs/lab02-syscalls-sched/`](../labs/lab02-syscalls-sched/) — **ends this week** · [`../labs/lab03-allocator/`](../labs/lab03-allocator/) — **starts this week** (weeks 6–8). Budget ~5.5 h across the pair (5.0 h finishing lab 2, 0.5 h opening lab 3). Sheet §B1's valgrind work is the natural warm-up for Lab 3 |
| **Past papers** | **None this week.** All four scheduling questions open by now are spent (weeks 3–5). The two remaining unlocked questions are scheduled later, where the load is lighter and the surrounding material supports them better — `y2008p1q8` in week 9, and `y2023p2q4` in week 13 as spaced retrieval through the concurrency block — unlock week is a floor, not a schedule. Week 6 runs at 16.8 h — the heaviest week in the course — and has no room for one anyway |

## Week load

```
OSTEP ch. 14-15     27pp ÷ 7  =  3.9 h
CS:APP §7.1-7.7     20pp ÷ 7  =  2.9 h
Wahbe SFI [M]                 =  1.5 h
Exercise sheet 6              =  3.0 h
Lab 2 ends · Lab 3 starts     =  5.5 h
Past papers          none     =  0.0 h
                                ------
                                16.8 h   — over the 12-14 h band (labs are not
                                          trimmed to fit; heaviest week in the
                                          course)
```

## Notes for the curious

- **Ch. 14 has a "Homework (Code)" section but ships no code.** There is no
  `vm-api` directory in either upstream repo — the chapter expects you to write
  `null.c` and its broken siblings yourself and study them under gdb and
  valgrind. Sheet §B1 supplies that material as a guided sequence. It doubles
  as preparation for Lab 3, where you move to the *other* side of the
  `malloc()` interface and implement it.
- **Why CS:APP ch. 7 and not Silberschatz §2.5.** Both cover linkers and
  loaders; CS:APP is far stronger, and its authors note that linking "is not
  covered in most systems texts" — OSTEP included. This is exactly the kind of
  hole the cross-readings exist to fill.
- **The bounds register comes in two designs** — hold the address-space *size*
  and check before adding the base, or hold the *end* physical address and
  check after. Logically equivalent; exam questions state either. The sheet
  drills both.
- **SFI has a long afterlife.** The verify-don't-trust structure of Wahbe et
  al. resurfaces wherever code must be confined without, or more cheaply than,
  a hardware boundary — in-process sandboxes in browsers being the famous
  descendants. Next week's OSPP §8.4 gives the textbook retrospective; this
  week you read the primary source first.
- Everything ch. 15 needs from the MMU is an adder and a comparator. Keep that
  as a baseline: over the paging weeks the MMU grows into the most intricate
  piece of hardware the course touches.
