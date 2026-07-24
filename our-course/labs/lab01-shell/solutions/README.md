# Lab 1 — Reference solution and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference msh.c, the answer to the Part 1 comment, and the   ║
║  key to the Part 6 checklist. Read it AFTER you have attempted    ║
║  the lab. Part 6 in particular is worth 24% and is self-marked;   ║
║  reading the key first trades the part that teaches most for      ║
║  twenty saved minutes.                                            ║
╚═══════════════════════════════════════════════════════════════════╝
```

Every model answer below is inside a collapsed `<details>` block, so you can
check them one at a time.

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g, zero warnings
make test       # ../tests/run.sh on this directory: 43 passed, 0 failed
```

The reference is 576 lines as shipped, of which 401 are code and the rest
comment. The empty starter skeleton scores **0 of 43** — no
case in the harness passes on a shell that does nothing, which is the property
that makes the numbers mean something.

---

## Part 1 — REPL and builtins (16%)

**Marking note.** Give yourself the marks if the ten `Part 1:` cases pass *and*
you wrote the `cd` comment. The cases cover: a command running at all, blank
and whitespace-only lines, end of input on a line with no newline, `exit`
stopping the shell immediately, `exit N`, `cd` to a relative directory, `cd ..`,
`cd` with no argument, a failed `cd` reporting and returning 1, and a failed
`cd` leaving the working directory alone.

<details>
<summary><b>Why <code>cd</code> cannot be an external program</b></summary>

The working directory is **per-process** state: the kernel keeps it in the task
structure, and `chdir(2)` changes it for the calling process and for nobody
else. An external command runs in a child made by `fork(2)`. `fork` *copies*
the parent's working directory into the child — it does not share it — so the
child would `chdir`, then exit, and the shell, which never moved, would carry on
exactly where it was. Every command after it would still resolve relative paths
against the old directory.

There is no system call that lets one process set another's working directory,
and that is not an oversight. It is the same isolation that stops any program
you run from rearranging the shell that launched it. So the change has to happen
*in the shell's own process*, which means the shell has to implement `cd`
itself.

`exit` is a builtin for the same shape of reason — only the shell can end the
shell — and `jobs` because the job table is the shell's own memory, which a
child could not see even if it wanted to.

**Marking.** Full marks for naming (a) that the working directory is per-process
kernel state and (b) that `fork` gives the child its own copy, so the child's
`chdir` cannot propagate back. Half marks for "the child would exit and nothing
would change" without saying why nothing can propagate back. No marks for
efficiency arguments — `cd` is not a builtin because forking is slow, it is a
builtin because forking makes it *impossible*.
</details>

---

## Part 2 — fork, exec, wait (24%)

**Marking note.** Eight cases. The one that matters most is *the shell waits for
the child before the next command*: it runs `slowprint.sh` (sleep, then print
`A`) followed by `echo B`, and a shell that forgets to wait prints `B` first.
The status cases require `WIFEXITED`/`WEXITSTATUS` decoding, 127 for a command
that is not found, and 126 for a file that exists and is not executable.

<details>
<summary><b>The three details that catch people</b></summary>

- **`fflush(NULL)` before `fork`.** `fork` copies the stdio buffers, so anything
  buffered but not yet written exists in both processes. It only *doubles* output
  if a child then flushes that copy — and in this shell no child does: every child
  either `execvp`s (which discards the buffer) or `_exit`s (which never flushes it).
  So a missing `fflush(NULL)` is **not observable in this harness** and no case
  tests it (a mutant without it scores identically). It is good hygiene and a real
  general hazard — a shell that flushed in a child, or printed more of its own
  output before forking, would double it — so the requirement stands, but do not
  award or deduct on it here. (The doubling it prevents is the same failure the
  `_exit`-not-`exit` detail below *does* cause and *is* tested.)
- **`_exit`, not `exit`, in a child whose `exec` failed.** `exit` runs the
  atexit handlers and flushes the (copied) stdio buffers a second time.
- **The child reports the exec failure, not the parent.** The parent cannot
  know: `fork` has already returned successfully, and the failure happens
  afterwards, in another process. This is why 127 and 126 have to be *encoded*
  as exit statuses — they are the only channel back.

The zombie observation is not decoration. A process that has exited but has not
been waited for still has an entry in the process table, holding its exit
status, because that status belongs to the parent and the kernel cannot know
whether the parent still wants it. `ps` shows `Z`. That entry is exactly what
`wait` collects, and the whole of Part 5 is about collecting it without
blocking.
</details>

---

## Part 3 — Redirection (12%)

**Marking note.** Ten cases. Two of them exist only to catch the classic bug —
*`>` does not leak into the next command* and *`<` does not leak into the next
command* — and one checks that a redirection that cannot be opened stops the
command running and gives status 1.

<details>
<summary><b>Why it has to be the child, and what happens if it is not</b></summary>

The four lines that do the work sit between `fork` and `exec`, in the child:

```c
fd = open(s->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
if (fd < 0) { report; _exit(1); }
dup2(fd, STDOUT_FILENO);
close(fd);
```

That is ch. 5's argument made concrete. `fork` and `exec` are two calls rather
than one precisely so that there is a moment, in the new process, where the
shell's code still runs and can rearrange the descriptor table the new program
is about to inherit. `wc` cannot tell whether its standard input is a file, a
pipe or a terminal, and does not have to be told.

Do it in the parent and:

- the `>` half redirects the *shell's* stdout. The first command works, because
  the child inherits it; the second command writes into the same file; and
  everything the shell itself prints goes there too.
- the `<` half replaces the shell's own source of commands. Depending on how
  much of your script stdio has already buffered, the shell either exits early
  or starts executing the contents of the redirected file.

`close(1); open(...)` in place of `dup2` works too, by the lowest-free-descriptor
rule, and is what the original shells did. It is not safe if a signal handler
could open a file in between; `dup2` is atomic, which is the reason it exists.
</details>

---

## Part 4 — Pipelines (16%)

**Marking note.** Eleven cases, including three-, four- and six-stage
pipelines. A two-stage pipeline can pass with descriptor handling that a
four-stage one hangs on, which is why the harness does not stop at two. A case
that reports `TIMED OUT` is not a slow machine — it is a leaked write end.

<details>
<summary><b>The close rule, and the loop that obeys it</b></summary>

The rule:

> A read end reaches EOF only when **every** copy of the matching write end is
> closed.

Copies appear in two ways: the parent gets both ends from `pipe()`, and every
child forked after that inherits both. So the reference does this, for stage
*i* of *n*:

```
if (i < n-1) pipe(p[i]);
fork();
  child:  if (i > 0)   dup2(p[i-1][0], 0);
          if (i < n-1) dup2(p[i][1], 1);
          close every p[…] descriptor still open here   <- both ends, both pipes
          apply < and > redirections
          execvp
  parent: if (i > 0) { close(p[i-1][0]); close(p[i-1][1]); }
```

The parent's close is placed *after* forking stage *i*, because pipe *i*−1 is
wanted by exactly two children — stage *i*−1, which writes it, and stage *i*,
which reads it — and both now exist. Do it any earlier and the second child
never gets it; do it any later, or not at all, and the reader waits forever for
an EOF the parent is holding back.

Note that the children only ever inherit **two** pipes, because the parent
closes each one as it goes. A perfectly good alternative creates all *n*−1
pipes up front and has each child close all 2(*n*−1) descriptors; it is easier
to reason about and easier to get wrong by one.

The pipeline's status is the **last** stage's — `false | echo ok` is 0 — and
the parent must wait for every stage, not only the last, or the earlier ones
become zombies.

Diagnosing a hang: run `ls -l /proc/self/fd` as a command inside your own
shell. `ls` inherits the shell's descriptor table, so anything past 0, 1, 2 and
`ls`'s own directory handle is your leak, and the symlink target names which
pipe it is.
</details>

---

## Part 5 — Background jobs and reaping (8%)

**Marking note.** Four cases, and they are not diffs against `bash`: a `[1]
12345` notice has no non-interactive `bash` equivalent and a pid is not
reproducible. They check that `&` returns long before the job finishes, that
`jobs` names the running command, that two jobs get distinct numbers, and that
after the job has finished and the shell has been idle for three seconds it has
no child in state `Z`.

<details>
<summary><b>Sweep or handler — and why the reference sweeps</b></summary>

The reference reaps with a `waitpid(-1, &st, WNOHANG)` sweep at the top of the
main loop, not with a `SIGCHLD` handler. Both are allowed; the sweep is chosen
because everything that can go wrong with the handler is real:

- A handler runs between any two instructions of the main loop, so it may only
  call **async-signal-safe** functions. `printf`, `malloc` and `free` are not
  on that list; a handler that prints a "done" notice with `printf` can deadlock
  in the allocator or corrupt a partially-written line.
- A handler that reaps `-1` in a loop will happily reap the **foreground** job
  too, and then the blocking `waitpid` in your main path returns `-1`/`ECHILD`
  and you have lost the status you were waiting for. This is a *race*, so it
  shows up in about one run in thirty — which is worse than a bug that shows up
  every time. If you use a handler, either block `SIGCHLD` around the
  foreground wait (`sigprocmask`), or have the handler skip pids it does not
  recognise as background jobs.
- With a handler installed, a slow `read` in the main loop can return `EINTR`.
  `signal()` in glibc installs with `SA_RESTART`, so it usually does not; if
  you use `sigaction` without that flag, it will, and your shell will exit at
  the first background job that finishes.

The sweep has none of these problems, and the cost is only that a job is
noticed at the next prompt rather than the instant it dies. The harness's
margins are seconds precisely so that both designs pass.
</details>

---

## Part 6 — Signals and process groups (24%)

**Marking note.** Self-marked from `JOBCONTROL.md`. Give yourself the full 24%
only if all seven checks pass with a pasted transcript each; give yourself
nothing for a check you asserted without pasting. If you got 1, 2 and 4 but not
5, 6 and 7, you have `SIGINT` handling but not terminal control, which is about
half the part.

The transcripts below were produced by the reference shell driven from a
pseudo-terminal. Pids will differ; the *relationships* between them will not.

<details>
<summary><b>1, 2, 4 — Ctrl-C reaches the job and nothing else</b></summary>

```
msh> sleep 30
^C
msh> /bin/echo still-alive
still-alive
msh> sleep 20 &
[1] 1369151
msh> ^Cjobs
[1] running   sleep 20 &
msh> exit
```

Three separate facts here:

- The foreground `sleep` died and the shell did not. `Ctrl-C` makes the
  terminal driver send `SIGINT` to the terminal's **foreground process group**,
  not to a process. The shell is not in that group — it put the job in a group
  of its own — and it ignores `SIGINT` anyway, so it survives twice over.
- At an idle prompt, `Ctrl-C` reaches the shell's own group and is ignored.
- The background job is in a *third* group, which is not the foreground group,
  so `Ctrl-C` never reaches it at all. That is the check that fails if you
  forgot `setpgid` for background jobs: they stay in the shell's group, and
  since the shell ignores `SIGINT` but its children do not, they die.
</details>

<details>
<summary><b>3 — Ctrl-C kills every stage of a pipeline</b></summary>

Because all stages were put in the *same* new group, one `SIGINT` reaches all
of them:

```
msh> sleep 40 | sleep 40 | sleep 40
```

From the second terminal, **before** the interrupt:

```
$ ps --ppid $(pgrep -n msh) -o pid,pgid,stat,comm
    PID    PGID STAT COMMAND
1369352 1369352 SN   sleep
1369353 1369352 SN   sleep
1369354 1369352 SN   sleep
```

Three children, all sharing one PGID — the job's group, distinct from the
shell's. Then `^C` in the shell, and again:

```
$ ps --ppid $(pgrep -n msh) -o pid,pgid,stat,comm
    PID    PGID STAT COMMAND
```

Empty: all three died together, because `SIGINT` went to the *group*.

Three stages that are all still alive is the whole point of the command — a
pipeline whose early stages have already exited cannot distinguish "killed the
group" from "killed the last stage".

If one or two `sleep`s survive, you called `setpgid(0, 0)` in each child instead
of `setpgid(0, pgid_of_first_stage)`, so each stage became its own group leader —
three groups, only one of which the terminal was pointing at. Note also that
`--ppid` (or `-e`) is required: a bare `ps` lists only processes on your own
terminal, so from a second terminal it prints nothing at all, which on a check
for absence looks exactly like success.
</details>

<details>
<summary><b>5, 6, 7 — the process table, read properly</b></summary>

```
msh> ps -o pid,pgid,ppid,tpgid,stat,comm
    PID    PGID    PPID   TPGID STAT COMMAND
1369347 1369347 1369346 1369352 SNs  msh
1369349 1369349 1369347 1369352 SN   sleep      <- background job
1369351 1369351 1369347 1369352 TN   cat        <- background reader, stopped
1369352 1369352 1369347 1369352 RN+  ps         <- foreground job
```

- **5.** Every job's `PGID` equals its own `PID`: it is the leader of a group
  that contains only it (or, for a pipeline, only its stages). All three
  differ from the shell's `1369347`.
- **6.** `TPGID` is the terminal's foreground process group, and every line
  shows `1369352` — the `ps` job's own group. The shell handed the terminal
  over with `tcsetpgrp` before waiting and takes it back afterwards. Run the
  same command with `&` and `TPGID` is the shell's pgid instead, because the
  shell never gave the terminal away.
- **7.** `cat &` is in state `T`: **stopped**. A process in a background group
  that reads from the controlling terminal is sent `SIGTTIN`, whose default
  action is to stop it. That is the kernel refusing to let two process groups
  compete for your keystrokes — and it only happens if the terminal's
  foreground group is genuinely the shell's, which is what the check proves.
  (The reference's `jobs` still calls it `running`; it has no stopped state,
  because `fg`/`bg`/`SIGTSTP` are the stretch goal, not the part.)

**The two mistakes that produce most of the lost marks:**

- Forgetting `tcsetpgrp` entirely. Symptom: checks 1–4 pass, 6 and 7 fail, and
  `cat &` silently eats the characters you type at the prompt.
- Forgetting that the shell must **ignore `SIGTTOU`** before it calls
  `tcsetpgrp`. A process that is not in the terminal's foreground group and
  calls `tcsetpgrp` is sent `SIGTTOU`, and the default action stops it. Symptom:
  your shell freezes the first time it hands the terminal *back*, and you have
  to `kill -CONT` it from another window.
</details>

---

## Notes on the reference implementation

Not marked; read them if you want to compare designs.

- **The tokeniser is shared with the starter, unchanged.** It is given away on
  purpose: string parsing is not what this lab teaches and reliably eats an
  hour.
- **`struct stage` holds a fully-parsed stage** — `argv`, `infile`, `outfile` —
  and parsing finishes completely before any `fork` happens. Parsing and forking
  in one pass is possible and considerably harder to get right, because the
  error paths then run in a process that has already created children.
- **Builtins run only as a single foreground stage.** `cd x | wc -l` goes down
  the ordinary path and fails to find a program called `cd`. Real shells run
  builtins in pipeline stages in a subshell, where the `chdir` is as useless as
  it would be in a child — the scope rule at the top of the handout exists to
  keep that out of the lab.
- **The shell's exit status is the last command's**, both at end of input and
  from a bare `exit`, matching `bash`. Since the language has no `$?`, that is
  the only channel through which the harness can see any command's status, and
  it is why so many test cases end with a deliberately failing command.
- **`setpgid` is called in both the child and the parent**, with the same
  arguments. Either process may run first after `fork`; if only the parent did
  it, a fast child could `exec` and get its first signal before being moved, and
  if only the child did it, the parent could `tcsetpgrp` to a group that does
  not exist yet.
