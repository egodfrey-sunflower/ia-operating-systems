# Fixtures

The fixture tree is **generated, not committed**. Run:

```sh
./genfixtures.sh            # builds ./tree
./genfixtures.sh /tmp/fix   # or anywhere you like
```

`tests/run.sh` regenerates it into a temporary directory on every run, so you
never need to do this by hand — but it is worth doing once, so you can see what
your shell is being compared against.

Generating rather than committing keeps it deterministic: the same bytes and
the same permission bits on every machine. A transcript diff against `bash` is
only meaningful if both shells see identical input.

## What each fixture is for

| Path | Exists to catch |
|---|---|
| `words.txt` | 12 lines, unsorted, with duplicates — so `sort`, `uniq -c` and `head` in one pipeline each change the data. A 4-stage pipeline whose output equals its input proves nothing. |
| `lines.txt` | 200 numbered lines. `wc -l` on it is a real answer, and it is small enough to fit in a pipe buffer — so a hanging pipeline is hanging on a missing EOF, not on a full pipe. |
| `marker.txt`, `sub/marker.txt`, `home/marker.txt` | `cd` must change the **shell's own** working directory. The only way to observe that is to name a file relatively afterwards, from three different directories. |
| `home/` | used as `$HOME` for both shells during the tests, so `cd` with no argument has a fixed, symlink-free answer. |
| `slowprint.sh` | sleeps 0.4 s, then prints `A`. A shell that forks and forgets to `wait` prints the *next* command's output first — visible in a diff, no stopwatch needed. |
| `bgjob.sh` | sleeps briefly, then creates the marker `out/bg.done`. This is what makes the Part 5 zombie assertion real: the harness waits for the marker before scanning the process table, so it is checking a job that has genuinely finished rather than one that never started. |
| `noexec.txt` | a plain file with mode 644. `execvp` fails with `EACCES` **after** the fork, so the diagnostic must come from the child and the shell must survive. |
| `latewrite.sh` | sleeps 0.6 s, then writes the marker `out/late.txt` — to a *file*, not stdout, so `SIGPIPE` stays out of it. Used as the first stage of a pipeline whose last stage exits at once: a shell that waits only for the last stage gets back to the prompt before the marker exists, and the next command cannot read it. |
| `out/` | scratch directory for the `>` cases. Emptied on every generation. |

The tests also use `no_such_dir`, `no_such_file` and `no_such_command_xyz` —
names that deliberately do not exist, for the `cd`, redirection and `execvp`
failure cases. Those need no fixture: their
whole point is being absent.
