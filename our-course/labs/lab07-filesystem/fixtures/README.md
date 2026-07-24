# Fixtures

The fixture tree is **generated, not committed**. Run:

```
./genfixtures.sh            # builds ./tree
./genfixtures.sh /tmp/fix   # or anywhere you like
```

`tests/run.sh` regenerates it into a temporary directory on every run, so you
never need to do this by hand — but it is worth doing once, so you can look at
what you are being tested against, and run `stat`, `tail` and `find` on it
yourself.

Generating rather than committing is deliberate: the tree contains symlinks
(one of them dangling), a hard-linked pair, files carrying the set-user-ID,
set-group-ID and sticky bits, a zero-byte file, an empty directory and a
4 MiB file — none of which git round-trips faithfully. The script is
deterministic — same bytes and same modes on every machine — which is what
makes the diff test against the system tools reliable.

## `meta/` — for `mystat` and `myls -l`

| Path | What it is |
|---|---|
| `reg644`, `exec755`, `priv600` | ordinary files at three permission masks |
| `setuid` (4755), `setgid` (2755), `sticky` (1755) | one file for each of the set-user-ID, set-group-ID and sticky bits |
| `empty` | a regular file of length zero |
| `big` | a fixed 1234-byte file: an exact `size` to reproduce |
| `subdir/` | a directory: its own type, and a link count that is not 1 |
| `hard_a`, `hard_b` | one inode under two names: a link count that is not 1, on a regular file |
| `link_to_reg` | a symlink to a real file — its type, and its size (the length of the target path) |
| `dangling` | a symlink whose target does not exist: it still has an inode to report on |
| `.hidden` | a dotfile `myls` must not list |
| `Upper` | an uppercase name, which sorts before the lowercase ones in byte order — and which `readdir` will not hand you sorted |

`tests/run.sh` also points `mystat` at **`/dev/null`**, a character special
file, which no fixture can create without root.

## `tail/` — for `mytail`

| Path | Bytes | What it is |
|---|---|---|
| `lines20` | — | twenty numbered lines with a trailing newline: the ordinary case at several values of `n` |
| `nonl` | 16 | three lines, the last with **no** trailing newline |
| `oneline_nonl` | 25 | a single line, no trailing newline |
| `empty` | 0 | nothing to print, exit 0 |
| `big` | 4194304 | 4 MiB, trimmed mid-line so it does not end in a newline. Its last few lines are a few hundred bytes; the harness runs `mytail` on it under `strace` and checks how much of the file you read or map. |

## `find/` — for `myfind`

| Path | What it is |
|---|---|
| `foo`, `a/foo`, `a/b/foo` | the same name at three depths — the walk must reach all of them |
| `foobar`, `barfoo` | names that *contain* `foo` but are not equal to it |
| `FOO` | the name `foo` spelled in capitals — a different name, byte for byte |
| `a/b/c/deep_target` | a match four levels down |
| `a/needle_dir/` | a **directory** whose name is a search target |
| `emptysub/` | an empty directory the walk must pass through without complaint |
| `linkfoo` → `foo` | a symlink, matched by its own name |
| `loop` → `.` | a symlink pointing back at its own directory |

`find` without `-L` (and the reference `myfind`) match `loop` by name and never
descend through it.
