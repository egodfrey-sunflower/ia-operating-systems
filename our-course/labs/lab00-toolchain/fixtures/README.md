# Fixtures

The fixture tree is **generated, not committed**. Run:

```
./genfixtures.sh            # builds ./tree
./genfixtures.sh /tmp/fix   # or anywhere you like
```

`tests/run.sh` regenerates it into a temporary directory on every run, so you
never need to do this by hand — but it is worth doing once, so you can look at
what you are being tested against.

Generating rather than committing is deliberate: the tree contains a binary
file, a zero-byte file and directories whose contents are only dotfiles, and
git does not round-trip all of those faithfully. The script is deterministic —
same bytes on every machine — which is what makes the diff test reliable.

## What each fixture is for

| Path | Bytes | Exists to catch |
|---|---|---|
| `hello.txt` | 12 | the base case |
| `colours.txt` | 28 | `green` is a prefix of `greener`/`greenest`, so line-equality passes as substring matching until you look closely |
| `empty.txt` | 0 | `read()` returning 0 on the very first call; also `myls` on nothing |
| `nonewline.txt` | 24 | `mycat` must not invent a trailing newline; `mygrep` must add one when it prints the line |
| `binary.bin` | 17 | embedded NUL and high bytes: anything using `strlen`/`strstr` on the buffer truncates |
| `large.txt` | 200000 | 3 × 65536 + 3392, so a 64 KiB buffer needs four `read()`s and the last comes back short. One `read()` cannot do it. |
| `tree/` | — | sorting: `Middle` (uppercase) must sort before `alpha.txt` in byte order, and `readdir` will not hand them to you sorted |
| `tree/.hidden` | 2 | `myls` must hide it, like `ls -1` |
| `tree/sub/`, `tree/sub/deeper/` | — | nesting; `deeper/` contains only a dotfile, so it *lists* as empty while not being empty on disk |
| `emptydir/` | — | print nothing, exit 0 — not an error |

`tests/run.sh` additionally feeds `large.txt` down a pipe in two bursts with a
pause between them, which makes the *first* `read()` return short with data
still to come. On a regular file you only ever see the short read at the end;
on a pipe you see it in the middle, and that is the one that breaks naive code.
