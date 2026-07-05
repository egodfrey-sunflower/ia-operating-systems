# Examples Sheet 10 — Unix case study & crash consistency

**Attempt after Week 14.** Budget 2–4 hours. Work closed-book first; self-mark
against `answers/sheet10-answers.md` (spoilers — attempt first).

*Partly extension material. Covers: the Unix shell and
process-management syscalls; signals; pipes; and **crash consistency** — fsck,
write-ahead logging, ext3 journaling modes, `fsync` guarantees, and
log-structured file systems.*

Reading: Kerrisk ch. 20–22 (signals) and ch. 44 (pipes) for the Unix case study
(week 13); OSTEP ch. 42 (crash consistency & journaling) and reading-list
papers 22 (Rosenblum & Ousterhout, LFS) and 23 (Prabhakaran et al., journaling
analysis) for crash consistency (week 14). This sheet pairs with **Lab 1 (the
shell)** and **Lab 6 Task 3 (the write-ahead log experiment)** — several
questions refer to them directly.

---

## A. Warm-ups (true / false, with a one-line justification)

**A1.** `fsync(fd)` on a newly created file guarantees that, after a crash, the
file *and* its name in the directory will both be present.

**A2.** Metadata-only ("ordered") journaling writes every data block twice.

**A3.** After `fork()`, the child inherits the parent's installed signal
handlers; after a successful `exec()`, those handlers survive unchanged.

**A4.** A `write()` to a pipe is atomic regardless of its size.

---

## B. Bookwork from the IA sheet (do by citation)

**B1.** Do **IA Examples Sheet 3, Q7**
(`../../cambridge-course/examples_sheets/examples_sheet3.pdf`) — describe the operation of the
Unix shell with reference to the process-management system calls it uses
(`fork`, `exec`, `wait`, `exit`, and file-descriptor manipulation). *Note:* keep
your pseudo-code precise about the *order* of `fork` → child `dup2`/`close`/
`exec` versus parent `wait`; C(a) below makes it exact for a pipeline.

---

## C. Shell syscall sequences (Lab 1)

In **Lab 1** you built a shell that handles redirection and pipes. Work out the
exact syscall sequence.

**(a)** For the command

```
  sort < in.txt | uniq > out.txt
```

give the full sequence of `fork`, `pipe`, `dup2`, `close`, `exec*`, and `wait`
calls the shell makes, and say **which process** makes each. Be explicit about
*every* `close` — including why each end of the pipe must be closed in both
children and the parent, and what deadlock or hang results if a pipe end is left
open.

**(b)** Explain why the redirection (`< in.txt`, `> out.txt`) is done with
`open` + `dup2` **in the child, after `fork` and before `exec`**, and not in the
parent shell. What property of `exec` makes this the natural place?

**(c)** The shell `wait`s for the pipeline. If it forks both stages and then
`wait`s, does the order in which the two children finish matter for
correctness? What does the shell learn from each `wait`'s return status, and how
does this relate to `$?`?

---

## D. Signals across `fork` and `exec`

Read Kerrisk ch. 20–22.

**(a)** Make a table: for each of **signal dispositions (handlers)**, the
**signal mask**, and **pending signals**, state what happens across (i) `fork`
and (ii) a successful `exec`. Note the one disposition that is treated specially
by `exec` (ignored vs handled).

**(b)** A program installs a `SIGCHLD` handler to reap children, then `fork`s a
child that immediately `exec`s. Which of the parent's handlers is the child
running before `exec`, and after? Why can a signal-handler function pointer
**not** meaningfully survive `exec`?

**(c)** Why must a signal handler restrict itself to *async-signal-safe*
functions, and what does this have to do with the fact that a signal can
interrupt the main flow at an arbitrary instruction? Give one concrete unsafe
example (e.g. `malloc`, `printf`).

---

## E. Crash consistency: ordering the writes

A process **appends** one block to a file. This dirties, at least: the file's
**inode** (new size + new block pointer), the **free-block bitmap** (block now
in use), and the new **data block**. Consider three schemes.

**(a)** Under **xv6-style write-ahead logging** (Lab 6, `kernel/log.c`), give
the order in which blocks reach disk: the log copies, the **header (commit)**
write, and the install (checkpoint) of home blocks. Identify the single write
that is the atomicity point. If power is lost **just before** it, what does the
next mount's `recover_from_log` do? **Just after** it?

**(b)** Under **ext3 data journaling** (`data=journal`), what is written to the
journal and in what order relative to the final in-place writes? Under
**metadata-only (ordered) journaling** (`data=ordered`, the ext3 default),
what is journaled, what is *not*, and what ordering constraint between the data
write and the metadata commit prevents a metadata pointer from ever pointing at
**stale/garbage** data? (Reading-list paper 23.)

**(c)** Compare the three on **write amplification** (how many times the data
block is written) and on the **strength of the guarantee** (can a post-crash
file ever contain garbage in the appended block?). When is data journaling worth
its cost?

**(d)** State precisely what **`fsync(fd)`** guarantees and what it does **not**.
In particular: after `fsync` on a *new* file returns, is the file guaranteed
recoverable by name? What else must be `fsync`ed, and why is this a famous source
of application bugs?

**(e)** Suppose there were **no** journal or log (raw V7). What must **`fsck`**
do at mount after an unclean shutdown, and give two inconsistencies it **can**
repair and one thing it **cannot** recover. Why is `fsck` both slower and weaker
than replaying a log? (Lab 6 Task 3 closing question.)

---

## F. Log-structured file systems (segment cleaning)

Read reading-list paper 22 (Rosenblum & Ousterhout, 1991) — the course reads
this paper deliberately *in place of* OSTEP ch. 43, so the paper *is* the
assigned reading here, not a supplement to the chapter.

**(a)** In one paragraph, explain why LFS writes *all* new data and metadata
sequentially to a log, and what problem this creates over time that **segment
cleaning** (garbage collection) must solve.

**(b)** A cleaner reclaims space by reading whole **segments**, writing back the
still-live blocks compactly, and freeing the rest. If a segment being cleaned
has live fraction (utilisation) **u**, a standard analysis gives the **write
cost**

```
  write_cost = 2 / (1 - u)
```

(one unit read + one unit of copies written, amortised over the `(1−u)` freed).
Compute the write cost for **u = 0.5, 0.75, 0.9**, and state the write cost as
`u → 1`. What does this say about which segments the cleaner should prefer?

**(c)** LFS therefore wants a **bimodal** distribution of segment utilisations
(mostly-empty or mostly-full). Explain why cleaning *cold* (rarely-updated)
segments even at moderate utilisation, and leaving *hot* segments to empty
themselves, achieves this — and connect the whole idea to how an **SSD FTL**
does garbage collection over erase blocks (Sheet 7, §D).

---

## G. TOCTOU file races (fds vs paths)

A **setuid-root** helper wants to append to a log file *on behalf of the calling
user*, but only if that user is allowed to write it. It is tempting to write:

```
  if (access(path, W_OK) == 0) {   /* check with the REAL uid */
      int fd = open(path, O_WRONLY | O_APPEND);   /* use */
      write(fd, line, n);
  } else {
      /* refuse */
  }
```

This is the canonical **time-of-check-to-time-of-use (TOCTOU)** bug.

**(a)** `access` deliberately tests the **real** uid (the invoking user), while
`open` in a setuid program acts with the **effective** (root) uid — that is *why*
the check is there. Explain why this program still has a **race** even though it
is single-threaded and makes no `pthread` call. Name the two operations that must
be atomic-with-respect-to-each-other but are not, and say who plays the role of
"the other thread" from Sheets 5 and 9.

**(b)** Give the concrete attack. Between the `access` and the `open`, an
unprivileged attacker who controls the directory swaps what `path` resolves to
(e.g. `path` is a **symlink** the attacker re-points, or a name they `rename`).
Sketch the interleaving as a two-column trace (victim | attacker) that makes the
helper `open` a file the real uid could **not** write — say, `/etc/passwd`. Why
does the attacker not need to win the race on the first try?

**(c)** The root defect is that `access` and `open` each **re-resolve the path
string from scratch**; the name is late-bound and the binding can change under
you. Contrast this with operating on an **already-open fd**. Using the
fd → open-file → inode picture of **Sheet 8 §E**, explain why an fd is immune to
path re-binding, and why the fix is to resolve the name **once** and then do both
the check and the use against that fixed object — e.g. `open` first, then
`fstat`/`fchmod`/`faccessat` on the fd, or use `openat(dirfd, …)` and
`O_NOFOLLOW`. What does `O_NOFOLLOW` specifically defeat?

**(d)** A colleague objects: "our access-control **matrix** is correct — the real
uid genuinely lacks write on `/etc/passwd`, so how can it be exposed?" Answer
them by callback to **Sheet 1**: the setuid helper is a **confused deputy**
wielding root authority on the caller's behalf, and TOCTOU is a way to make it
*misapply* a correct policy — the same class of failure as a covert channel,
where the policy is right but the mechanism leaks around it. State the two
mitigations that remove the deputy's power to be confused: **drop privilege**
(set the effective uid to the real uid before touching the file, so the kernel's
own permission check does the work and no `access` is needed) and **avoid the
check entirely** (attempt the `open` and handle `EACCES`, turning a check-then-use
into a single atomic syscall). Why is "try it and handle failure" fundamentally
race-free here?

---

## Past paper questions

Per this directory's `README.md`, attempt this after the sheet (~35 min, closed-book).
It is the Unix-case-study paper, a direct fit for this sheet's shell and
file-descriptor material:

* **`y2022p2q4`** (`../../cambridge-course/exam_questions/y2022p2q4.pdf`) — what the kernel does
  for `stdout` redirection vs pipes; UNIX file descriptors *as* capabilities
  (and how they differ from pure capabilities — callback to Sheet 1); and a
  critique of running filesystem defragmentation in the idle process (Sheet 7
  revision).

For fresh memory drill before the final, `y2023p2q3`, `y2024p2q4`
and `y2025p2q4` are held back as exam-revision papers — leave them unread until
then.

For mechanical drill on the crash-consistency material, the OSTEP homework
simulators (<https://github.com/remzi-arpacidusseau/ostep-homework>) for the
FSCK and journaling chapter (ch. 42) step through exactly the
write-ordering/recovery reasoning of section E.
