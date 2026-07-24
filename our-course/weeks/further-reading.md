# Further reading — after the exam

> **Optional, and deliberately after the final.** Nothing here is examinable,
> and none of it belongs in week 27, whose hours are already spent. Open this
> page when the exam is behind you.

Four papers, each free from its publisher or its author. None of them teaches
you anything new about operating systems; that is the point. Each one re-reads
material you already own — and a paper that would have been fortune cookies in
week 2 lands differently once twenty-six weeks of mechanism are sitting
underneath it. Read them in any order, one an evening.

## 1. Lampson, *Hints for Computer System Design* (1983)

Butler W. Lampson, 9th ACM Symposium on Operating Systems Principles, 1983.
<https://bwlampson.site/33-Hints/Acrobat.pdf>

The canonical paper to read *after* a first systems course, and close to
useless before one. It is a couple of dozen slogans — *do one thing well*,
*don't hide power*, *cache answers*, *use hints*, *log updates*, *make actions
atomic*, *shed load* — organised on two axes (why: functionality, speed,
fault-tolerance; where: interface, implementation) and each illustrated from a
system Lampson helped build. Cold, they read as aphorisms. Now, every one of
them has a week attached: "cache answers" is weeks 9–10 arguing about
replacement policy, "log updates" and "make actions atomic" are week 21's
journal, and the passage on monitors is week 13's Mesa semantics seen from the
designer's side — Lampson explaining why the thing he and Redell shipped was
kept deliberately small and fast. The course taught these mechanisms one at a
time because that is the only way to teach them; this is the one place they are
all held in the hand at once.

## 2. Klein et al., *seL4: Formal Verification of an OS Kernel* (2009)

Gerwin Klein et al., SOSP 2009.
<https://trustworthy.systems/publications/nicta_full_text/1852.pdf>

Week 3 set the monolithic/microkernel comparison out as a trade-off and left it
there: isolation, bought with a round trip through the kernel on every service
call. This is what the microkernel side looks like when someone takes it
completely seriously. seL4 is 8,700 lines of C and 600 of assembler, verified
by machine — from an abstract specification down to the C implementation — to
never crash and never perform an unsafe operation, on the stated assumption
that compiler, assembly code and hardware behave. It also finishes the other
thread the course leaves hanging: seL4's abstractions are address spaces,
threads and IPC, with authorisation by **capability**, which is Dennis & Van
Horn's 1966 idea from week 20 turned into a kernel primitive rather than a
paper design. Read §2 for what the kernel is and §5 for what the proof actually
cost; the Isabelle/HOL mechanics in between are there if you want them.

## 3. Saltzer, Reed & Clark, *End-to-End Arguments in System Design* (1984)

J.H. Saltzer, D.P. Reed and D.D. Clark, ACM Transactions on Computer Systems
2:4, November 1984.
<http://web.mit.edu/Saltzer/www/publications/endtoend/endtoend.pdf>

Week 23 met this as ch. 48's Aside, and the Aside carried what the exam needed.
The full paper is worth an hour now because its examples are mostly not
networking examples — careful file transfer, encryption, duplicate message
suppression, recovery from system crashes, delivery acknowledgement — and they
are the same argument you were already making about `fsync()` in week 17 and
about checksums in week 22, without knowing it had a name and a citation. The
corollary is the half people forget, and the paper is precise about it: a
low-level check is never *sufficient* to establish the guarantee, but it is
often a thoroughly worthwhile *performance* optimisation.

## 4. Waldspurger, *Memory Resource Management in VMware ESX Server* (2002)

Carl A. Waldspurger, 5th USENIX Symposium on Operating Systems Design and
Implementation (OSDI '02).
<https://www.usenix.org/legacy/event/osdi02/tech/full_papers/waldspurger/waldspurger.pdf>

The paper that pays the course back with interest, because four separate weeks
turn out to be one problem in a single system. A virtual machine monitor
(week 25) has to divide physical memory among guest operating systems that each
believe they own it and each run their own replacement policy (weeks 9–10).
ESX's answers are all recognisable and all slightly wrong-footing:
**ballooning**, a module inside the guest that allocates memory purely to
create pressure, so the *guest's* replacement policy picks the victims and the
VMM never has to guess; **content-based page sharing**, finding identical pages
by hashing their contents and mapping them copy-on-write; statistical sampling
to estimate each VM's working set; and an **idle memory tax** on top of
proportional shares, charging a client more for a page it is holding idle than
for one it is using. That last is week 4's shares — from the author of the
lottery-scheduling paper — carried across to a resource where, unlike CPU time,
handing you your share is not the same as it doing you any good.
