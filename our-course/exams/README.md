# Exams

The course's three closed-book assessments, in Cambridge Tripos house style.

| | When | Covers | Format |
|---|---|---|---|
| **Midterm 1** | Week 10 | Weeks 1–10 — all of virtualization | 90 min, closed book — [`midterm1.md`](midterm1.md) |
| **Midterm 2** | Week 20 | Weeks 1–20, emphasis concurrency / persistence / security | 90 min, closed book — [`midterm2.md`](midterm2.md) |
| **Final** | Week 27 | Everything | 3 h; compulsory Q1 = sealed `y2026p2q3` — [`final.md`](final.md) |

Both midterms sit on clean seams. **Midterm weeks.** Weeks 10 and 20 carry no
timed past paper; the midterm is that week's timed practice. That is what keeps
both weeks inside the 12–14 h budget.

## Sealed material — do not open

- `y2026p2q3` — the final's compulsory question
- `y2026p2q4` — standalone timed mock, week 27

> **Sealed material.** `y2026p2q3` and `y2026p2q4` have never been opened or
> inspected by anyone. Do not read, extract, or infer their contents.

> **Held back for revision.** `y2023p2q3`, `y2024p2q4`, `y2025p2q4` are
> deliberately unspent until week 27.

## Format conventions

Every question on every paper follows the Tripos Paper-2 shape:

- **20 marks, 3–5 parts, ramping.** Roughly: ~4 marks of bookwork (definitions,
  mechanism statements — things a prepared candidate writes without thinking),
  ~8 marks of applied calculation (a trace, a translation, a sizing — the
  chapter's arithmetic on fresh numbers), and ~8 marks that require judgement
  (a design to critique, a mechanism to adapt, a proposal to evaluate). The
  weakest scripts are separated from the strongest almost entirely by the last
  category.
- **1 mark ≈ 1 minute ≈ one distinct point.** A 4-mark prose part wants four
  made points, not one point made four times. A calculation part pays for
  method: full working with an arithmetic slip keeps most of the marks; a bare
  correct number earns few.
- **Timing.** The midterms give 90 minutes for three 20-mark questions —
  30 minutes a question, the marks-as-minutes rule plus a margin for choosing
  and checking. The final runs at the course's 35-minutes-per-question
  convention across four questions.
- **Conventions are stated, not assumed.** Where a trace depends on a tie-break
  or queue-ordering rule, the question states it. A different, clearly stated,
  internally consistent convention earns full marks; an unstated one does not.

## Midterm 1 — blueprint

Four questions, answer **three**. The set spans weeks 1–10; the two most-drilled
Cambridge calculation families — scheduling traces and address translation —
each get a full question.

- **Q1 — Processes, the process API, and limited direct execution** (ch. 4–6,
  xv6 ch. 4; Ritchie & Thompson; Ousterhout 1990). Bookwork on the two
  register-save events of a context switch; an applied `fork()`/`exec()`/`wait()`
  output-enumeration and the shell-redirection derivation that Ritchie &
  Thompson use to justify the split; a syscall-overhead calculation feeding an
  Ousterhout-style critique of "hardware got faster, so OS overhead stopped
  mattering".
- **Q2 — Scheduling** (ch. 7–9; Waldspurger & Weihl). The scheduling-trace
  family: FIFO, STCF and round-robin over a four-job arrival table with
  turnaround/response/waiting averages, then lottery expectation and a stride
  trace, closing with an MLFQ design critique (what the priority boost is for,
  and what deleting it costs).
- **Q3 — Paging, TLBs, and multi-level tables** (ch. 18–20). The
  address-translation family: a full two-level walk of a hex address with the
  physical address of every access, page-table sizing for a sparse address
  space against the linear alternative, an EAT-with-TLB computation, and a
  page-size design critique.
- **Q4 — Memory under pressure** (ch. 17, 21–23; Levy & Lipman). Fragmentation
  bookwork; a replacement trace (LRU against clock, same string, use-bit
  convention stated); page-fault-rate algebra with a dirty-eviction refinement;
  and a paper part on how VAX/VMS approximated LRU on hardware with no
  reference bit.

## Midterm 2 — blueprint

Four questions, answer **three**. Weighted toward weeks 11–20 — concurrency,
persistence, security — with one deliberate reach-back into midterm-1 material,
as the Tripos Q3/Q4 pair habitually does.

- **Q1 — Concurrency** (ch. 26–32; Lu et al.; Lampson & Redell; reaches back to
  ch. 7–8 scheduling). Lu's bug taxonomy as bookwork; a broken
  producer/consumer to diagnose and fix (the `if`-versus-`while` and
  single-CV defects, with interleavings); the semaphore rendering of the same
  buffer; and priority inversion — a locking bug that is *also* a scheduling
  bug, tying week 12's locks to week 3–4's schedulers.
- **Q2 — I/O and RAID** (ch. 36–38; Patterson, Gibson & Katz). The disk
  service-time model on fresh numbers (random vs sequential, the ratio, and
  what it justifies); the RAID capacity/reliability/throughput table derived
  rather than recalled; the RAID-4 small-write bottleneck quantified; a
  choice between two stated configurations under a capacity floor, decided
  by a measured property of the workload.
- **Q3 — File systems** (ch. 39–41; McKusick FFS). The inode-arithmetic family:
  maximum file size under a stated geometry, reads-to-reach-an-offset through
  each pointer chain, path-resolution cost with cold and warm caches; then
  FFS locality reasoning and an is-placement-worth-its-complexity critique
  on a small-file workload.
- **Q4 — Security** (ch. 53–55; Saltzer & Schroeder). The access matrix and its
  two projections on a fresh instance, the revocation asymmetry, Unix 9-bit
  expressiveness; salting arithmetic (what it costs the attacker, and the
  hash-stretching factor that pushes the attack past a deadline); a
  fail-open design to audit against the ch. 53 principles.

## Grading bands (midterms)

Cambridge classes, translated to a 60-mark script (three questions):

- **First (≥ 70%, ~42+/60).** Calculations correct with working shown;
  conventions stated; critique parts deliver a verdict *with the conditions
  under which it flips*, not a survey. Bookwork is complete and fast enough to
  leave time for the judgement parts.
- **2.1 (60–69%).** Methods right throughout with occasional arithmetic slips;
  bookwork solid; critiques make genuine points but stop short of a committed,
  conditioned verdict.
- **2.2 (50–59%).** Bookwork mostly present; calculations started correctly but
  incomplete or convention-muddled; critique parts restate the chapter instead
  of judging.
- **Third (40–49%).** Fragments — definitions without mechanism, traces that
  lose state partway, critique parts that assert without argument.

**Marking yourself honestly.** There is no examiner, which makes the marking
the most corruptible step. Mark the *script*, not the intention: no marks for
points you would have made, half-marks at most for a gesture at the right idea,
and write the per-part marks in the margin as an examiner would. The number is
only useful if it would survive an audit.

## Mock-exam protocol (midterms)

1. **Handwritten, closed book, timed.** One sitting, no pauses, no notes, no
   calculator — the real papers assume none. Put the solutions file somewhere
   you cannot see it.
2. **Choose questions like a candidate.** Read all four, commit to three,
   budget 30 minutes each, and move on when a part stalls — marks-per-minute
   falls fast on a stuck part.
3. **Mark it cold, the next day.** Same-day marking is too generous. Use the
   solutions file's mark scheme part by part, margin-marking as above.
4. **A question under 60% names a revision target.** Re-work the corresponding
   exercise-sheet sections closed-book, then re-sit that question from a blank
   page a few days later. A question re-sat warm proves nothing; the gap is the
   point.

For the final: all four questions, 35 minutes each, out of 80; the same
percentage bands apply. Sitting and marking details are in
[`final.md`](final.md).
