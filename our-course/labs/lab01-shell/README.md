# Lab 1 — Build a shell

**Weeks 2–4 · 12.5 hours · OSTEP ch. 4–6 · TLPI ch. 24–28, 20–22, 34 as reference**

Everything here is userspace C on Linux. No root, no QEMU, no cross-compiler.
You will need `bash`, `gcc`, `make`, and the ordinary Unix tools the tests use
(`cat`, `sort`, `uniq`, `head`, `wc`, `sleep`, `ls`).

## Layout

```
lab01-shell/
  README.md          this handout
  starter/           Makefile, msh.c with the REPL and tokeniser done  <- work here
  fixtures/          generator for the files the tests run against
  tests/run.sh       the autograder for Parts 1–5
  solutions/         SPOILERS. Reference msh.c + the answer key. Later.
```

Copy the three working directories somewhere of your own and work there. Copy
all three, not just `starter/` — the autograder and the fixture generator are
referred to as `../tests/…` and `../fixtures/…`, and those paths only resolve if
the layout comes with you:

```sh
cp -r starter tests fixtures ~/lab1
cd ~/lab1/starter
```

`solutions/` is deliberately left behind.

## What you hand in

| File | Part | Weight |
|---|---|---|
| `msh.c` — builtins | 1 | 16% |
| `msh.c` — fork/exec/wait | 2 | 24% |
| `msh.c` — redirection | 3 | 12% |
| `msh.c` — pipelines | 4 | 16% |
| `msh.c` — background jobs | 5 | 8% |
| `JOBCONTROL.md` — the Part 6 checklist, with transcripts | 6 | 24% |

Parts 1–5 are machine-checked by `tests/run.sh`. Part 6 is a manual checklist
you run at a real terminal and mark yourself against the key in
`solutions/README.md` — **which you read afterwards, not before.**

---

# ⚠ Scope — read this before you write a parser

The command language is **exactly** this and nothing more:

```
words                       an ordinary command and its arguments
< file                      stdin from a file
> file                      stdout to a file, created and truncated
|                           between stages, any number of them
&                           at the very end: run in the background
```

**No quoting. No globbing. No variable expansion. No subshells. No comments.**
No `;`, no `&&`, no `||`, no `>>`, no `2>`, no here-documents, no `$?`, no
backslash escapes, no tilde expansion.

This is not laziness, it is the budget. Students reliably spend an hour
gold-plating the tokeniser — which is *given to you already written* — and then
run out of time before Part 6, which is the part with no OSTEP coverage and the
most to teach. If you find yourself writing a quote-state machine, stop.

The tokeniser you are given treats `<`, `>`, `|` and `&` as one-character tokens
of their own, so `echo hi>out` and `echo hi > out` tokenise the same way. Take
that as given and move on.

---

# The specification

The autograder compares your shell against `bash` on scripts fed to both on
standard input. Anything below that says "must" is something it checks.

**Observability.** There is no `$?` in this language, so the only way to see a
command's exit status from outside is the shell's own: **`msh` exits with the
status of the last command it ran**, both at end of input and on `exit` with no
argument. The starter already returns `last_status` from `main`; keep it
accurate and the tests can see everything they need.

**The prompt.** Printed only when standard input is a terminal. The starter
does this already (`isatty`), and it matters: the tests drive the shell down a
pipe, and a prompt written into that pipe would land in the diff. The same rule
governs everything terminal-related in Part 6.

**Exit statuses**, matching `bash`:

| Situation | Status |
|---|---|
| command ran | its own exit status |
| command killed by signal *n* | 128 + *n* |
| command not found (`execvp` gives `ENOENT`) | 127 |
| found but not executable (anything else) | 126 |
| a redirection could not be opened | 1, and the command does not run |
| `cd` failed | 1 |
| a malformed line | 2 |
| a pipeline | the status of its **last** stage |

**Diagnostics.** Every error must put *something* on stderr, but the wording is
yours: the autograder compares stdout and exit status, never the text of an
error. Use `strerror(errno)`, and prefix with `msh: ` by convention.

**`>`** creates with `O_CREAT | O_TRUNC` and mode `0666`. `<` opens read-only.
A redirection operator may appear anywhere among the command's words.

**Builtins** are recognised only when the whole command is a single stage in the
foreground. `cd x | wc` is not special-cased — it goes down the ordinary path,
where it will fail to find a program called `cd` and report 127. Real `bash`
*does* run builtins in a pipeline, each stage in its own subshell, so
`sleep 0.1 | exit 3` yields 3 there and 127 here. That divergence is deliberate
and untested: implementing it means every stage becoming a fork of the shell
itself, which is a lot of machinery for no lesson this lab is teaching.
Redirection on a builtin is not required.

**Do not add builtins** beyond `exit`, `cd` and `jobs`. `echo`, `true`, `false`
and `pwd` are external programs here; making one a builtin risks diverging from
`bash` in a way the diff will notice.

---

# Part 1 — REPL and builtins (~2.0 h, week 2, 16%)

The REPL is written for you. What is missing is `execute()`.

Implement three builtins, in the shell's own process:

- **`exit [N]`** — end the shell. With no argument, exit with the status of the
  last command; with an argument, with `N`. If `N` is not a number, exit **2**
  after a diagnostic on stderr — `bash` does the same, and "silently treat it as
  zero" is the tempting wrong answer. Nothing after `exit` on later lines
  is read.
- **`cd [DIR]`** — `chdir(2)`. No argument means `$HOME`. On success the status
  is **0**, like any other command — it matters, because a script may end in
  `cd` and the shell exits with the last status. On failure, report to stderr,
  set the status to 1, and **stay where you were**.
- **`jobs`** — comes in Part 5. Leave it for now.

Blank lines, whitespace-only lines, trailing whitespace and end of input
(`Ctrl-D`) must all be handled without complaint. The starter loop does this;
check that it still holds once `execute()` does something.

**Write the comment this part exists for.** In `execute()`, above the builtins,
explain in two or three sentences **why `cd` cannot be an external program**.
The answer is a fact about a specific system call and about what `fork(2)`
copies — not "because it would be slow". This is marked; write yours before you
read the key.

---

# Part 2 — fork, exec, wait (~3.0 h, week 2, 24%)

Launch external commands. `fork()`, then `execvp()` in the child, then
`waitpid()` in the parent, and record the child's status.

Three details that all matter; the last two the harness checks directly:

- **`fflush(NULL)` before you fork.** A child inherits the parent's unflushed
  stdio buffers; flush before you fork.
- **The child reports its own exec failure**, then calls **`_exit`**, not
  `exit` — 127 if `errno == ENOENT`, 126 otherwise. `exit` would flush the
  parent's buffers a second time.
- **Decode the status properly**: `WIFEXITED`/`WEXITSTATUS`, and `128 +
  WTERMSIG(status)` when `WIFSIGNALED`.

**Do the thing the part is named for.** Before you write the `waitpid`, leave it
out on purpose. Generate the fixtures once —

```sh
../fixtures/genfixtures.sh /tmp/fix
```

— and then, at your own shell's prompt, run

```sh
/tmp/fix/slowprint.sh
echo B
```

in your shell and watch `B` come out first. Then, from another terminal, find
your shell and look at its children:

```sh
ps -o pid,ppid,stat,comm --ppid $(pgrep -n msh)
```

The `Z` in the `STAT` column is a zombie: a process that has exited, whose exit
status the kernel is still holding because nobody has asked for it. That entry
is the whole reason `wait` exists. Now put the `waitpid` back and watch it go.

---

# Part 3 — Redirection (~1.5 h, week 3, 12%)

Add `< file` and `> file`.

Do it **in the child, between `fork` and `exec`**: open the file, `dup2` it onto
0 or 1, close the original descriptor. The launched program needs no
cooperation and cannot tell — `wc` has no idea whether its standard input is a
terminal, a file or a pipe, and that is the entire point of ch. 5's argument for
`fork` and `exec` being two calls instead of one.

If the `open` fails, the child reports it and `_exit(1)`; the command must not
run.

You may use `dup2`, or the older `close(1); open(...)` — the lowest-free-
descriptor rule puts the new file at 1 either way. Sheet 2 argues about which
is better; here, either passes.

---

# Part 4 — Pipelines of arbitrary length (~2.0 h, weeks 3–4, 16%)

Support `a | b | c | …` for any number of stages.

The shape: *n* stages, *n*−1 pipes. Pipe *i* joins stage *i*'s stdout to stage
*i*+1's stdin. Fork every stage, wire each child's ends, wait for all of them,
and take the **last** stage's status as the pipeline's.

The part everyone gets wrong is closing. State it as a rule:

> A read end sees EOF only when **every** copy of the matching write end is
> closed — and the parent holds a copy of both ends of every pipe it creates,
> and every child forked afterwards inherits a copy too.

Get the descriptor bookkeeping wrong and your shell does not print the wrong
answer — it never returns. The harness gives every case a timeout for exactly
this reason, and reports `TIMED OUT` rather than hanging.

A two-stage pipeline can work while the descriptor handling is wrong. The tests
run three, four and six stages for that reason. When something hangs, the
fastest diagnosis is to run, inside your own shell:

```sh
ls -l /proc/self/fd
```

`ls` inherits whatever the shell had open. You should see 0, 1, 2 and the one
`ls` opened for itself — no pipes — however many pipelines have run.

Redirection composes with pipes: `sort < in | uniq -c > out` applies the file
redirections to the first and last stages, after the pipe wiring.

---

# Part 5 — Background jobs and reaping (~1.0 h, week 4, 8%)

A trailing `&` means: do not wait. Instead

- print `[n] pid` — job number and the pid of the process group leader;
- record the job in a table of your own (job number, pgid, the command as
  typed);
- **reap it asynchronously** when it finishes;
- and implement `jobs`, listing the ones still running. The format is yours,
  but each line must contain the command as typed, so that the test can find it.

Two ways to reap, and the spec allows either:

1. **A sweep at the prompt** — `while (waitpid(-1, &st, WNOHANG) > 0)` at the
   top of the loop, before the prompt is printed. Simple, and everything it
   touches is ordinary code.
2. **A `SIGCHLD` handler.** More responsive, and much easier to get wrong. A
   handler runs between any two instructions of your main loop, so it may only
   call async-signal-safe functions — `printf`, `malloc` and `free` are not on
   that list — and it must not swallow the status of the *foreground* job that
   the main path is waiting for. If you go this way, block `SIGCHLD` around the
   foreground `waitpid`, or reap only pids you know are background jobs.

The test asserts that after a background job has finished and the shell has been
idle for a while, the shell has **no child left in state `Z`**. It scrapes
`/proc` for that; the margins are seconds, so a prompt sweep passes comfortably.

---

# Part 6 — Signals and process groups (~3.0 h, weeks 3–4, 24%)

This part has no OSTEP chapter. **TLPI ch. 34** is the source; read the sections
on process groups, sessions and the controlling terminal before you start, and
budget for the looking-up.

What to build:

- Put the shell itself in a process group of its own, and give that group the
  terminal (`setpgid`, `tcsetpgrp`) — **only when `isatty(0)`**.
  `setpgid(0, 0)` fails with `EPERM` if the shell is already a session leader,
  which happens whenever it is launched directly under a PTY rather than from
  another shell. That is not an error: **ignore it and carry on**. Real shells
  do. Treating it as fatal, or printing a diagnostic every startup, is the
  usual first bug here.
- Make the shell **ignore** `SIGINT`, `SIGQUIT`, `SIGTSTP`, `SIGTTIN` and
  `SIGTTOU`. Every child restores them to `SIG_DFL` before `exec`.
- Put each job — the whole pipeline — in a **new process group of its own**.
  Call `setpgid` in both the child and the parent: whichever runs first wins,
  and neither ordering is guaranteed.
- For a foreground job, hand the terminal to the job's group before waiting and
  take it back afterwards.

Two things worth knowing before they confuse you:

- `Ctrl-C` does not send a signal to a process. The terminal driver sends
  `SIGINT` to the **foreground process group** of the terminal. That is why
  putting the job in its own group and pointing the terminal at it is the whole
  mechanism — and why a shell that leaves its children in its own group dies
  with them.
- Ignoring `SIGTTOU` in the shell is not optional. `tcsetpgrp` called by a
  process that is not in the terminal's foreground group generates `SIGTTOU`,
  and the default action stops the shell.

## The checklist — Part 6's deliverable

Part 6 is not auto-graded. Process groups and `Ctrl-C` are terminal behaviour
and cannot be driven down a pipe; testing them properly means driving a PTY,
which is more harness engineering than one part of one lab is worth.

Instead: run these seven checks **at a real terminal** (generate the fixtures
first — `../fixtures/genfixtures.sh /tmp/fix` — for the one that needs a file),
paste the transcript of each into `JOBCONTROL.md`, and mark yourself. All seven
must pass.

1. **The shell ignores `Ctrl-C` at an idle prompt.** Press `Ctrl-C` with nothing
   running. You should get a fresh prompt and the shell should still be there.
2. **`Ctrl-C` kills the foreground job and not the shell.** Run `sleep 30`,
   press `Ctrl-C`. The sleep dies, you get a prompt, the shell lives. Run
   something afterwards to prove it.
3. **`Ctrl-C` kills the *whole* foreground pipeline.** Run
   `sleep 40 | sleep 40 | sleep 40` — three stages that are *all* still alive
   when you interrupt, which is the point: a pipeline whose early stages have
   already exited cannot tell you whether you killed the group or only the last
   stage. Before pressing `Ctrl-C`, from another terminal run

   ```sh
   ps --ppid $(pgrep -n msh) -o pid,pgid,stat,comm
   ```

   and confirm **three** children. Press `Ctrl-C`, run it again, and confirm
   **none**.

   > Use `--ppid` (or `ps -e`). A bare `ps` lists only processes on *your own*
   > terminal, so run from a second terminal it prints nothing at all — which,
   > on a check for absence like this one, looks exactly like success.
4. **A background job survives `Ctrl-C`.** Run `sleep 60 &`, press `Ctrl-C`,
   then `jobs`. The job must still be listed. This is the check that fails if
   you forgot to put the job in its own process group.
5. **Each job really is its own process group.** With `sleep 60 &` running,
   from another terminal run `ps -e -o pid,pgid,ppid,comm` (the `-e` matters —
   see check 3) and paste the lines for your shell and the sleep. The `PGID` values must differ, and the sleep's
   `PGID` must equal its own `PID` — it is its own group leader.
6. **The terminal follows the foreground job.** Run
   `ps -o pid,pgid,tpgid,comm` as a foreground command in your shell. `TPGID`
   is the terminal's foreground process group; on that line it must be the
   `ps` process's own `PGID`, not the shell's. Then run it again with `&` and
   see the difference.
7. **A background job that reads the terminal is stopped, not served.** Run
   `cat &` with no arguments. It should stop almost immediately (`ps` shows
   state `T`) rather than eating the input you type at the prompt. That is
   `SIGTTIN`, and it is the proof that `tcsetpgrp` is actually pointing the
   terminal at the right group.

### `JOBCONTROL.md`

```markdown
# Job control — Part 6 checklist

Terminal used: <e.g. gnome-terminal / tmux / ssh session>

## 1. Ctrl-C at an idle prompt
    <paste>
Pass / fail, and what I observed:

## 2. Ctrl-C kills the foreground job, shell survives
    <paste>

## 3. Ctrl-C kills every stage of a foreground pipeline
    <paste, including the ps output showing nothing left>

## 4. A background job survives Ctrl-C
    <paste, including jobs>

## 5. Each job is its own process group
    <paste the ps lines; say which PGID is which>

## 6. The terminal follows the foreground job
    <paste both the foreground and the background ps line, and explain TPGID>

## 7. A background reader is stopped by SIGTTIN
    <paste, including the T state>

## What I got wrong first
<Empty is a fine answer, but it usually is not. The two classic ones are
forgetting tcsetpgrp entirely, and forgetting that the shell must ignore
SIGTTOU before it calls tcsetpgrp.>
```

Then, and not before, read the Part 6 key in `solutions/README.md`.

---

# Running the tests

```sh
make                              # must be warning-free: -Wall -Wextra -Werror
../tests/run.sh .                 # or: make test
```

`run.sh` builds your shell, then for every case feeds the **same script** to
your shell and to `bash`, each in its own private copy of the fixture tree, and
compares stdout byte for byte and the exit status exactly. It prints a
PASS/FAIL table and exits non-zero if anything failed; a failing case shows you
the script and both outputs.

What it does **not** compare is stderr — your diagnostics are your own. Cases
marked `(error)` additionally require that you wrote *something* there.

Part 5's cases are not diffs: `[1] 12345` has no `bash` equivalent when `bash`
is not interactive, and the pid is not reproducible. They assert on elapsed
time, on the text `jobs` prints, and on the process table instead. Part 6 is
not tested here at all — it is the checklist above.

**Every case runs under a timeout**, and a case that hits it is reported as
`TIMED OUT`. That is almost always Part 4: a pipe write end left open somewhere,
so a stage is waiting for an EOF that cannot arrive. It is not a slow machine.

Look at the fixtures before you start:

```sh
../fixtures/genfixtures.sh /tmp/fix && ls -la /tmp/fix
```

`fixtures/README.md` says what each one is there to catch.

---

# Stretch goals

Unweighted. Do them if the six parts came easily.

- **`>>` append and `2>` stderr redirection.** Both are a few lines once the
  parse is right: `O_APPEND` instead of `O_TRUNC`, and `dup2` onto 2. Add
  test cases for them to your own copy of `tests/run.sh` — the reference for
  `>>` is `bash`'s `>>`, so the diff harness will take them unchanged.
- **`fg` and `bg`, and `SIGTSTP` (`Ctrl-Z`) suspension.** The natural
  completion of Part 6 and a genuine job-control implementation: you need
  `WUNTRACED` in the foreground `waitpid`, a `stopped` state in the job table,
  `SIGCONT`, and `tcsetpgrp` in `fg`. TLPI ch. 34 covers all of it.
- **A `$?` variable**, and with it the ability to test statuses from inside a
  script rather than only at the shell's exit. Note what this costs: the moment
  you have one variable, you have variable expansion, and the scope rule at the
  top of this handout was there to stop you needing it.

---

# If you get stuck

- `make test` says it cannot find the autograder: you copied only `starter/`.
  Copy `tests/` and `fixtures/` alongside it, or pass
  `make test TESTS=/path/to/lab01-shell/tests/run.sh`.
- **A case reports `TIMED OUT`**: a pipe descriptor is still open somewhere.
  Run `ls -l /proc/self/fd` inside your shell after a pipeline; anything beyond
  0, 1, 2 and `ls`'s own is your leak. Remember the parent holds both ends of
  every pipe it created.
- **Your output appears twice**: a child flushed stdio buffers it inherited from
  the shell — it called `exit` instead of `_exit`, or returned instead of
  `exec`ing — and `fflush(NULL)` before the `fork` is what would have left the
  buffers empty so there was nothing for it to write a second time.
- **The shell exits after the first command**: something has replaced the
  shell's own standard input. Think about which process a `<` should affect, and
  which process you changed it in.
- **The second command writes into the first one's output file**: the `>` half
  of the same thing — a redirection that outlived the command that asked for it.
- **`sleep 5 &` returns immediately but `jobs` shows nothing**: you recorded the
  job after the `fork` returned in the *child* as well. Only the parent adds to
  the table.
- **`Ctrl-C` kills your shell too**: the shell is still in the same process
  group as the job, or it never called `signal(SIGINT, SIG_IGN)`.
- **Your shell stops itself the moment it runs a job**: `tcsetpgrp` sent it
  `SIGTTOU`. Ignore `SIGTTOU` in the shell.
- **A test passes for you and fails in `run.sh`**: the harness runs your shell
  with stdin as a *file*, not a terminal, so `isatty` is false and no prompt is
  printed. If you print the prompt unconditionally it lands in the diff.
- **Zombies pile up during development**: they are reaped when the shell exits,
  so `exit` and start again. If a stray `sleep` survives a killed test,
  `pkill sleep`.
