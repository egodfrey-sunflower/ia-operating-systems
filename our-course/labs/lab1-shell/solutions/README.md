```
╔══════════════════════════════════════════════════════════════════╗
║  ⚠  SPOILERS — REFERENCE SOLUTION FOR LAB 1  ⚠                    ║
║                                                                    ║
║  This directory contains a complete working `msh`.  Do not read   ║
║  it until you have made a genuine attempt at the lab yourself.     ║
║  Looking at the pipe/dup2/close choreography before you've fought  ║
║  the leaked-fd hang on your own defeats the entire point.          ║
╚══════════════════════════════════════════════════════════════════╝
```

# Lab 1 — Reference Solution

`msh.c` is a complete implementation of all five tasks (~470 lines,
commented at the "why" level). Build and grade it:

```
make                       # -Wall -Wextra -Werror -std=c11, builds ./msh
../tests/run.sh .          # -> 100% PASS
```

## Design notes for graders

**Structure.** A line is tokenized (provided lexer), parsed into a
`struct pipeline` of `struct cmd`s (each with its `struct redir`), then
`run_pipeline` forks the stages. Builtins (`cd`, `exit`, `wait`) run in the
shell process; everything else is `fork`+`execvp`.

**Signals.** `SIGINT` is caught (never `SIG_IGN`) and forwarded to the
foreground process group via `kill(-fg_pgid, SIGINT)`; children are placed in
their own group with `setpgid` and reset `SIGINT` to `SIG_DFL`. The handler
is installed *without* `SA_RESTART` so a `SIGINT` during `getline`/`waitpid`
yields `EINTR` and the loop reprompts. `SIGCHLD` reaps background children;
it is *blocked* around the foreground fork+wait so it can't steal the
foreground child's status. `wait` uses the block-then-`sigsuspend` idiom to
avoid a lost-wakeup race.

## Model answers to the writeup questions

**Why `cd`/`exit` must be builtins.** `chdir` and process exit affect only
the process that calls them. If `cd` were an external program, the shell
would `fork` a child, the child would `chdir`, and the child would exit —
the shell's own working directory would be unchanged. `exit` likewise must
terminate the *shell*, not a child.

**The leaked-pipe hang.** A pipe's read end returns EOF only once *all* write
ends are closed. If the parent shell `dup2`s a pipe's write end into a child
but forgets to close its own copy, that write end stays open in the shell
forever, so the reading stage never sees EOF and blocks in `read` — the
pipeline hangs. The fix is to close every pipe fd in the parent immediately
after forking the stage that needs it (and in each child, close the
originals after `dup2`).

**Terminal vs. forwarded `SIGINT`.** Interactively, pressing Ctrl-C makes the
terminal line discipline send `SIGINT` to *every* process in the terminal's
**foreground process group** — that's why the whole running pipeline dies,
not just one process. The shell arranges for the job to be that foreground
group via `tcsetpgrp`. In our non-interactive tests there is no controlling
terminal doing this, so the shell emulates the semantic: it catches the
`SIGINT` delivered to itself and forwards it to the job's process group.

## Model `a | b > f` fd choreography (the 1-page deliverable)

Three processes: the shell, `a`, `b`. One pipe `P` with ends `P[0]` (read)
and `P[1]` (write). `f` is the output file.

1. **Shell** calls `pipe(P)` → now holds `P[0]`, `P[1]`.
2. **Shell** `fork()`s **a**. In **a**: `dup2(P[1], 1)` (stdout → pipe
   write), then `close(P[0])` and `close(P[1])` (originals no longer needed;
   the dup'd copy on fd 1 remains). `a` has no redirection, so it execs with
   stdout = the pipe.
3. **Shell** (parent) `close(P[1])` — it must drop the write end so that
   when `a` finishes, *no* write end remains and `b` sees EOF. Shell keeps
   `P[0]` to hand to `b`.
4. **Shell** `fork()`s **b**. In **b**: `dup2(P[0], 0)` (stdin ← pipe read),
   then `close(P[0])`. Then apply `> f`: `fd = open("f", O_WRONLY|O_CREAT|
   O_TRUNC, 0644); dup2(fd, 1); close(fd)`. `b` execs with stdin = pipe,
   stdout = `f`.
5. **Shell** (parent) `close(P[0])` — drops its last pipe fd, so the only
   holders are `a` (write) and `b` (read).
6. **Shell** `waitpid`s both children; pipeline status = `b`'s status.

Invariant: after step 5 the shell holds no pipe fds at all. `a`'s write end
closes when `a` exits → `b`'s `read` returns EOF → `b` finishes. No fd leaks,
no hang.
