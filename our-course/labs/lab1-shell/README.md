# Lab 1 — Build a Shell

**Weeks:** 2–3 · **Budget:** 8–12 hours · **Track:** userspace · **Weight:** 5%

You will build `msh`, a small Unix shell, in C. By the end it runs external
programs, redirects I/O, connects commands with pipes, manages background
jobs, and handles `Ctrl-C` correctly. This is the IA *Unix case study*
(week 13 material, L11–12) done rather than described: `fork`, `execvp`,
`wait`, file descriptors, `pipe`, process groups, and signals are all here.

---

## Background

A shell is a read–eval–print loop over programs. Each iteration it reads a
line, splits it into a command and arguments, and runs that command as a
*new process*. The three system calls that make this possible are the spine
of the whole lab:

- **`fork()`** duplicates the calling process. It returns twice: the child
  gets `0`, the parent gets the child's pid.
- **`execvp(file, argv)`** replaces the current process image with a new
  program, searching `$PATH` for `file`. It only returns on failure.
- **`waitpid(pid, &status, opts)`** lets the parent collect a child's exit
  status and release its kernel resources (otherwise the child lingers as a
  *zombie*).

Everything else in the lab is a variation on "set up file descriptors in the
child between `fork` and `exec`". A child inherits the parent's open file
descriptors; redirection and pipes are just rearrangements of fds 0/1/2
using `open`, `pipe`, `dup2`, and `close` *before* the `exec`.

For the theory, see `../../reading-list.md`: the process/`fork`/`exec` model
(OSTEP "Process API"), pipes and redirection, and signals. Don't re-derive
it here — read it there, then build.

### Scope

**In scope:** simple commands with `$PATH` search; the `<`, `>`, `>>`, `2>`
redirections; pipelines of arbitrary length; `&` background jobs with zombie
reaping; the `cd`, `exit`, and `wait` builtins; `SIGINT` handling.

**Out of scope** (do *not* implement — the tests don't need them, and they'd
triple the code): quoting and escaping, globbing (`*`), environment-variable
expansion (`$VAR`, `$?`), command substitution, `&&`/`||`/`;` sequencing, and
interactive job control (`fg`/`bg`). A provided tokenizer already splits the
metacharacters for you, so you never parse quotes.

---

## Setup

```
cd starter
make            # builds ./msh with -Wall -Wextra -Werror -std=c11
./msh           # type commands; Ctrl-D (EOF) to leave
```

The starter compiles and runs but executes nothing yet — `run_pipeline()` is
a stub. You will replace it. The provided `tokenize()` turns an input line
into tokens, treating `< > >> 2> | &` as standalone tokens even without
surrounding spaces (so `echo hi>f` tokenizes as `echo` `hi` `>` `f`). Build
your parser on top of it.

Run the autograder any time:

```
../tests/run.sh .        # from inside starter/, grades ./msh
```

It prints a PASS/FAIL table with per-task subtotals and an overall score,
and exits nonzero if anything fails.

---

## Tasks

Weights are the fraction of the lab mark. Implement in order — each builds on
the last.

### Task 1 — REPL and simple commands (15%)

- Print the prompt `msh> ` **to stderr** (so a test capturing stdout sees
  only program output, never the prompt), read a line with `getline()`,
  tokenize it, and `parse` the tokens into your command structure.
- For an external command: `fork`, have the child `execvp` the program
  (which searches `$PATH`), and have the parent `waitpid` for it. Report
  "command not found" style errors to stderr and keep going.
- Implement two **builtins**, run in the shell process itself:
  - `exit [n]` — leave the shell (default status = last command's).
  - `cd [dir]` — change directory (`chdir`); default to `$HOME`.
- At **EOF** (Ctrl-D on an empty line; `getline()` returns −1) the shell
  exits with the last command's exit status — exactly like `exit` with no
  argument. The tests grade this.
- **Writeup question:** explain *why* `cd` and `exit` must be builtins and
  cannot be external programs. (Hint: what process would a forked `cd`
  change the directory of?)

### Task 2 — I/O redirection (20%)

Support, in any order relative to the command and its arguments:

| Operator | Effect | `open` flags | Mode |
|----------|--------|--------------|------|
| `< file`  | stdin from `file`            | `O_RDONLY`                        | —    |
| `> file`  | stdout to `file`, truncate   | `O_WRONLY\|O_CREAT\|O_TRUNC`       | 0644 |
| `>> file` | stdout to `file`, append     | `O_WRONLY\|O_CREAT\|O_APPEND`      | 0644 |
| `2> file` | stderr to `file`, truncate   | `O_WRONLY\|O_CREAT\|O_TRUNC`       | 0644 |

Apply redirection **in the child, after `fork`, before `exec`**, using
`open` then `dup2(fd, 0/1/2)` then `close(fd)`. `> a.txt echo hi` must work
as well as `echo hi > a.txt`.

### Task 3 — Pipelines (25%)

Run `a | b | c` for pipelines of arbitrary length, and let redirection
appear at the ends (`a < in | b | c > out`). Requirements:

- One `pipe()` per interior connection; `dup2` the correct end into each
  child's stdin/stdout.
- The **exit status of the pipeline is the status of the last command**.
- **Reap every child**, and **leak no file descriptors**.
- **Writeup question:** explain the classic bug where the parent forgets to
  close a pipe's write end, and *why* that makes the reader hang forever.
  (Hint: when does `read` return EOF on a pipe?)

### Task 4 — Background jobs (20%)

- A trailing `&` runs the pipeline in the background: print a `[pid]` line to
  **stderr** and *do not* wait for it.
- Reap finished background children so they don't accumulate as zombies —
  either with a `SIGCHLD` handler or an opportunistic `waitpid(..., WNOHANG)`
  loop.
- Add a `wait` builtin that blocks until all background jobs have finished.

### Task 5 — Signals (20%)

- The shell must **not die** on `SIGINT` (Ctrl-C). While a foreground job
  runs, a `SIGINT` delivered to the shell must **kill the job, not the
  shell**, and the shell must then print a fresh prompt. Background jobs
  must be unaffected.
- Designing the delivery is the task. If you get stuck, hold on to the
  required outcome — the shell survives, the whole foreground job dies —
  and know that `setpgid(2)` and `kill(2)`'s negative-pid form are the
  tools built for exactly this.
- **Writeup question:** contrast this with interactive delivery. When you
  press Ctrl-C at a terminal, who sends `SIGINT` to whom, and why does the
  terminal deliver it to the whole foreground process group rather than just
  the shell? (Hint: controlling terminal, foreground process group,
  `tcsetpgrp`.)

---

## Hints

- **One command first.** Get `fork`/`execvp`/`waitpid` working for a single
  command before touching pipes. Print the exec error with
  `strerror(errno)` and `_exit(127)` in the child.
- **Redirection is fd surgery.** The open/`dup2`/`close` sequence from
  Task 2 is the whole trick. Apply it *after* wiring pipes so an explicit
  `>` at a pipeline end overrides the pipe.
- **Pipes: close aggressively.** After `dup2`ing a pipe end onto 0 or 1,
  close the original. In the parent, close *both* ends you handed to each
  child. Leaked ends are the number-one cause of hung pipelines — the
  Task 3 writeup question asks you to explain exactly why.
- **Draw `a | b > f` on paper** before coding it — the deliverable asks for
  exactly this. Track every fd from `pipe()` to `close()` in each of the
  three processes.
- **SIGCHLD vs the foreground wait.** If a `SIGCHLD` handler reaps *every*
  child with `waitpid(-1, ...)`, it can steal your foreground child's status
  out from under the main loop. Block `SIGCHLD` around the foreground
  fork+wait, or otherwise keep the handler from touching foreground jobs.
- **Interrupting blocking calls.** Install the `SIGINT` handler *without*
  `SA_RESTART` so a signal that arrives during `getline()` or `waitpid()`
  returns `EINTR` and you can react (fresh prompt / notice the killed job).
- **`man` pages are the reference:** `fork(2)`, `execvp(3)`, `waitpid(2)`,
  `dup2(2)`, `pipe(2)`, `open(2)`, `sigaction(2)`, `setpgid(2)`, `kill(2)`.

---

## Deliverables

1. `msh.c` (and `Makefile`) building clean under
   `-Wall -Wextra -Werror -std=c11` and passing `tests/run.sh`.
2. A **design note (≈1 page)**: the exact fd choreography for `a | b > f` —
   the full sequence of `pipe`, `fork`, `dup2`, and `close` calls in *each*
   of the three processes (the shell, `a`, and `b`), and which fd each
   `close` is releasing and why.
3. Short answers to the three writeup questions embedded in the tasks
   (builtins, the leaked-pipe hang, terminal vs. forwarded `SIGINT`).

Submit the code, the design note, and the writeup answers.

---

## Rubric

| Component | Weight |
|-----------|--------|
| Task 1 — REPL, simple commands, `cd`/`exit` builtins | 15% |
| Task 2 — redirection (`<`, `>`, `>>`, `2>`)          | 20% |
| Task 3 — pipelines, correct status, no fd/zombie leaks | 25% |
| Task 4 — background jobs, reaping, `wait` builtin    | 20% |
| Task 5 — `SIGINT` handling, process groups           | 20% |

The task percentages above are the autograder weights (`tests/run.sh`
reports them). On top of that, the writeup is self-marked against the solution
notes: the 1-page `a | b > f` design note and the three short answers are
required for full credit — a shell that passes every test but ships no design
note is incomplete. Clean, warning-free code is expected, not rewarded separately;
warnings under `-Werror` mean it doesn't build.

---

## Stretch goal — interactive job control

Out of scope for the mark, but the part of the Unix case study most notes
skip. Make `msh` a real interactive job-control shell:

- Give the shell its own process group and make it the terminal's foreground
  group with `tcsetpgrp` before running a job, then reclaim the terminal
  after.
- Handle the fact that a background process writing to the terminal gets
  `SIGTTOU`/`SIGTTIN`; the shell must ignore `SIGTTOU` around `tcsetpgrp`.
- Add `jobs`, `fg`, and `bg`, tracking stopped (`SIGTSTP`) as well as
  running jobs via `WUNTRACED` / `WCONTINUED`.

Pointers: `tcsetpgrp(3)`, `SIGTTOU`/`SIGTTIN`/`SIGTSTP`, `waitpid` with
`WUNTRACED|WCONTINUED`, and the "Implementing a Job Control Shell" section of
the glibc manual.
