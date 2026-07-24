# Week 1 — What an operating system is

> **Part 0: Introduction.** The first of 27 weeks.

## What you'll learn

This week is the whole course in miniature, plus the toolchain you'll build on
for the next six months.

OSTEP opens by claiming an operating system does three things: it **virtualizes**
the CPU and memory (making one processor look like many, and a small physical
memory look like a large private one), it provides the machinery for
**concurrency** (so those illusions survive many things happening at once), and
it delivers **persistence** (so data outlives the power supply). Those three
pieces are the book's three parts and this course's spine. Chapter 2 demonstrates
each with a running C program — you should compile and run all four, because
their output is the evidence for claims the rest of the course assumes.

Alongside that, OSPP gives a complementary framing that OSTEP lacks entirely: the
OS as **referee** (arbitrating between competing processes), **illusionist**
(creating abstractions that don't physically exist), and **glue** (offering common
services). It is a useful lens for what follows, even where the course doesn't
name it again: scheduling is the referee role in its purest form, and virtual
memory is the illusionist's.

By the end of the week you should be able to say what an OS *is* without
gesturing vaguely at "it manages resources", and you should have a working
RISC-V cross-compiler, QEMU, and GDB.

**Key ideas:** virtualization · concurrency · persistence · the OS as
referee/illusionist/glue · design goals (abstraction, performance, protection,
reliability) · why abstraction always costs performance.

## Read

| # | Source | What | pp | Time |
|---|--------|------|----|------|
| 1 | **OSTEP ch. 1** | A Dialogue on the Book | 2 | 0.2 h |
| 2 | **OSTEP ch. 2** | Introduction to Operating Systems | 19 | 2.8 h |
| 3 | **OSPP §1.1–1.2** | OS as referee/illusionist/glue; evaluation criteria | ~15 | 2.1 h |
| 4 | **OSTEP App. F** | Laboratory: Tutorial — C, Makefiles, gdb, man pages | 13 | ~1.0 h *(reference — consult during the lab, don't read cover to cover)* |

**Optional:** Lavington (1978), *The Manchester Mark I and Atlas: A Historical
Perspective*, CACM. Where paging and virtual memory came from. OSTEP recommends
it over the original Atlas papers.

> **Skip OSPP §1.3** — its history/future material duplicates OSTEP ch. 2's
> history section.

**Not yet:** Ritchie & Thompson's UNIX paper is cited by ch. 2, but it lands in
**week 2** where the process abstraction gives it something to attach to.

## Do

| | |
|---|---|
| **Exercises** | [`../exercises/exercise01.md`](../exercises/exercise01.md) — budget 3 h. Work it closed-book, then self-mark against [`../exercises/solutions/exercise01-solutions.md`](../exercises/solutions/exercise01-solutions.md) |
| **Lab** | [`../labs/lab00-toolchain/`](../labs/lab00-toolchain/) — **starts and ends this week.** Budget 4.0 h. Toolchain setup, boot xv6 under GDB, Unix warm-up tools |
| **Past papers** | **None this week.** The earliest usable Cambridge paper is week 3 — nothing is examinable until limited direct execution and scheduling are taught. This is an expected consequence of following OSTEP's order |

## Week load

```
OSTEP ch. 1–2      21pp ÷ 7   =  3.0 h
OSPP §1.1–1.2      15pp ÷ 7   =  2.1 h
App. F (reference)            =  1.0 h
Exercise sheet 1              =  3.0 h
Lab 0                         =  4.0 h
                                ------
                                13.1 h   ✅ within 12–14 h target
```

## Notes for the curious

- OSTEP ch. 2's history section and OSPP §1.3 disagree slightly in emphasis —
  OSTEP tells the story through batch systems and UNIX, OSPP through the
  economics of hardware. Neither is wrong.
- Appendix F is a *tooling* appendix with no conceptual dependencies, which is
  why it can be pulled forward without violating the course's OSTEP-order rule.
- If C feels rusty, Appendix F plus K&R chapters 1–5 is the fastest repair. Lab 0
  is deliberately designed to surface gaps early rather than at week 8.
