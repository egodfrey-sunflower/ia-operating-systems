# Lab 0 — Toolchain & Unix warm-up

**Week 1 · 4 hours · OSTEP ch. 1–2, App. F**

Nothing here requires root except the one `apt install` line in Part 1, and
everything except Part 2 runs without QEMU or a cross-compiler at all. If you
are on a constrained machine, Parts 1, 3 and 4 still work.

## Layout

```
lab00-toolchain/
  README.md          this handout
  starter/           Makefile, three skeletons, toolcheck.sh  <- work here
  fixtures/          generator for the test tree
  tests/run.sh       the autograder for Part 3
  solutions/         SPOILERS. Reference code + the answer key. Later.
```

Copy the three working directories somewhere of your own and work there. Copy
all three, not just `starter/` — the autograder and the fixture generator are
referred to as `../tests/…` and `../fixtures/…` throughout, and those paths only
resolve if the layout comes with you:

```sh
cp -r starter tests fixtures ~/lab0
cd ~/lab0/starter
```

`solutions/` is deliberately left behind.

## What you hand in

| File | Part | Weight |
|---|---|---|
| `TOOLCHAIN.md` | 1 | 25% |
| `GDB-NOTES.md` | 2 | 25% |
| `mycat.c`, `myls.c`, `mygrep.c` building `-Werror` clean and passing `tests/run.sh` | 3 | 37% |
| `STRACE-NOTES.md` | 4 | 13% |

Parts 1 and 3 are machine-checked. Parts 2 and 4 are prose, marked against
the key in `solutions/README.md` — **which you read afterwards, not before.**

---

# Part 1 — Toolchain, proved (~1.0 h)

You need: a host C toolchain, a RISC-V cross-compiler, QEMU for RISC-V, a GDB
that speaks `riscv:rv64`, and `strace`.

On Debian/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential git strace \
    gcc-riscv64-unknown-elf gdb-multiarch qemu-system-misc
```

(`qemu-system-misc` is the package that contains `qemu-system-riscv64`.
On Arch: `riscv64-linux-gnu-gcc qemu-system-riscv gdb`. On macOS with
Homebrew: `brew tap riscv-software-src/riscv && brew install riscv-tools
qemu`, and expect more friction than the effort is worth — a Linux VM is
faster to get right.)

Then build xv6. **The course ships a pinned copy** at `labs/xv6-riscv/` — a
pristine checkout of tag `xv6-riscv-rev5`, the revision the xv6 book this course
cites was written against. Copy it and build the copy; never build in place:

```sh
cp -r /path/to/labs/xv6-riscv ~/xv6
cd ~/xv6
make qemu
```

You are looking for:

```
xv6 kernel is booting

init: starting sh
$
```

Type `ls`, then quit QEMU with **Ctrl-A** then **x** (that is the two-key
QEMU escape, not Ctrl-A-x held together).

> **Why a pinned copy rather than a clone.** Upstream's default branch has moved
> on since rev5 and will keep moving, and Part 2 asks you for concrete addresses
> and symbol names, which are revision- and compiler-specific. Function names
> drift too: the kernel's print routine is `printf()` in `kernel/printf.c` at
> rev5 and `printk()` in `kernel/printk.c` on later commits — which would change
> the answer to question 4. Working from the pinned tree means the answer key
> and your observations describe the same kernel.
>
> Your *addresses* will still differ from the key's if your compiler differs.
> That is expected and fine — the key says so, and marks the explanation rather
> than the number. Record your toolchain versions in `TOOLCHAIN.md`.

## Check it

```sh
./toolcheck.sh /path/to/xv6-riscv
```

`toolcheck.sh` asks two questions and no more: does every tool report a
version we can live with (major numbers only — anything newer is assumed
fine), and does `make qemu` reach a shell prompt inside 90 seconds. It quits
QEMU for you afterwards. With no xv6 path it runs the tool checks and skips
the boot check rather than pretending to have run it.

## Deliverable — `TOOLCHAIN.md`

Fill this in with what *your* machine actually printed. The point is that in
week 8, when something breaks, you have a record of what "working" was.

```markdown
# Toolchain

Machine: <OS and version, e.g. Ubuntu 24.04 x86-64 / macOS 14 arm64 in a VM>
Date:    <when you set this up>

| Tool | Version | Command that proved it works |
|---|---|---|
| host gcc | | `gcc --version` |
| make | | `make --version` |
| RISC-V cross gcc | | `riscv64-unknown-elf-gcc --version` |
| qemu-system-riscv64 | | `qemu-system-riscv64 --version` |
| gdb (multiarch) | | `gdb-multiarch --version` |
| strace | | `strace --version` |

xv6 commit: <sha>  (`git describe`: <e.g. xv6-riscv-rev5-36-gXXXXXXX>)

## Boot proof

Paste the last ~10 lines of `make qemu`, through `init: starting sh` and the
`$` prompt, plus the output of one command you typed at that prompt.

## Anything that did not just work

<Empty is a fine answer. If you fought something — a missing package name, a
Homebrew formula that had moved, QEMU too old to know `-machine virt` — write
down the fix. Future you will hit it again on another machine.>
```

---

# Part 2 — xv6 under the debugger (~1.0 h)

You are **reading, not modifying**. Do not edit xv6 in this lab.

## Getting attached

Two terminals, both in the xv6 tree.

Terminal 1:

```sh
make CPUS=1 qemu-gdb
```

**Use `CPUS=1`.** The default is 3, and with three harts every breakpoint
fires three times, GDB switches threads under you mid-command, and `stepi`
lands somewhere you did not expect. Set it to 1 and the machine behaves like
the single processor the questions assume. (You *should* look at the 3-CPU
case once — see the stretch goal — but not while answering these.)

That command prints something like `*** Now run 'gdb' in another window.` and
then waits: QEMU has started with the CPU halted before its first
instruction, listening for GDB on a port derived from your user id — **not
1234**. xv6's Makefile computes it as `id -u % 5000 + 25000`, so everyone on a
shared machine gets a different one. Ask it rather than guessing:

```sh
make -C /path/to/xv6-riscv print-gdbport   # or: expr $(id -u) % 5000 + 25000
```

Terminal 2:

```sh
gdb-multiarch
```

The xv6 Makefile writes a `.gdbinit` that connects for you. Modern GDB
refuses to auto-load it and tells you so:

```
warning: File "/path/to/xv6-riscv/.gdbinit" auto-loading has been declined ...
```

Do what it says — add the suggested `add-auto-load-safe-path` line to
`~/.config/gdb/gdbinit` — or connect by hand:

```
(gdb) set architecture riscv:rv64
(gdb) target remote 127.0.0.1:PORT
(gdb) file kernel/kernel
```

using the port from above. Connecting to 1234 gives you `could not connect:
Connection timed out.` and no hint as to why — this is the single most common
way to lose twenty minutes in Part 2. (The generated `.gdbinit` gets the port
right on its own, which is why the auto-load path does not hit this.)

Useful commands for what follows:

```
info registers pc sp        print specific registers
p/x $pc                     one register, in hex
x/12i $pc                   disassemble 12 instructions at the PC
info symbol <addr>          which symbol, if any, is at that address
stepi                       execute exactly one machine instruction
break *0x80000000           breakpoint on an address rather than a symbol
break main                  breakpoint on a symbol
continue / bt / info line   run on / backtrace / where is this source line
```

## The questions

Answer all six in `GDB-NOTES.md`. Where a question asks for an address or a
register value, **give the value you observed** and say how you got it. Your
addresses will not match anyone else's exactly — a different xv6 commit or a
different compiler moves them — and that is fine. What must be right is the
*structure* of the answer.

> **You are not expected to already know RISC-V assembly, what a CSR is, or
> what `satp` does.** None of it has been taught — this is week 1. These
> questions ask you to *observe* the machine and give a one-line reason, not to
> explain it from theory. Where a question reaches past what you know, say what
> you saw and what you infer, and move on. The mechanisms arrive later:
> page tables and `satp` in Lab 4, the console device in Lab 6, multiple harts
> in Lab 5. Looking at the real thing before you can fully explain it is the
> point of the exercise, not an accident of it.
>
> Two hints that save a search: `csrr`/`csrw` read and write **control and
> status registers**, the privileged machine state ordinary programs cannot
> touch; and `satp`'s top four bits are a MODE field, where **0 means address
> translation is off** and every address is physical.

1. **Where does the very first instruction execute?**
   Before you `continue` anything, what is `$pc`? Does `info symbol $pc`
   find a symbol for it? Disassemble a dozen instructions there. Whose code
   is this — xv6's, or something else's — and how can you tell?

2. **What is `sp` at that point, and what does that tell you?**
   Read `$sp` at the first instruction. Then set `break *0x80000000`,
   `continue`, and read `$sp` again. Explain the value.

3. **Where does xv6's own first instruction live, and what does it do?**
   Find the address of `_entry` (`p/x &_entry`) and read
   `kernel/entry.S`. Single-step through it with `stepi`, watching `$sp`.
   What is `sp` set to, and what is the formula? Look up `stack0` — where is
   it, how big is it, and why is it that size? (`p/x &stack0`,
   `p sizeof(stack0)`, and `grep -n stack0 kernel/start.c`.)

4. **Which function prints the banner?**
   `xv6 kernel is booting` appears on the console. Find the function that
   contains the call that produces it, and the function it calls to do the
   printing. Name the file and line. Then say — one sentence — how those
   characters get from that call to your terminal.

5. **What does the machine look like at a breakpoint on `main`?**
   `break main`, `continue`. Report `$pc`, `$sp` and `$satp`. Why is the
   `$pc` GDB stops at not the same as `p/x &main`? What does the value of
   `$satp` tell you about whether paging is on yet — and, given that xv6 is
   about to spend the next ten lines setting up memory, why must it be off?

6. **The OSTEP claim, cashed out.**
   OSTEP ch. 2 says the operating system is "just a program". You have now
   looked at its first instruction, its stack, and a breakpoint in its
   `main`. In three or four sentences: in what concrete sense is that claim
   true, and what did you see in this session that an ordinary program
   *cannot* do?

## Deliverable — `GDB-NOTES.md`

```markdown
# GDB notes

xv6 commit: <sha>
Built with: <cross gcc version>   Debugged with: <gdb version>
Started with: `make CPUS=1 qemu-gdb`

## 1. The first instruction
Observed: $pc = <...>
info symbol $pc said: <...>
Disassembly:
    <paste>
Whose code this is, and why:
<...>

## 2. sp at the first instruction
At reset:        $sp = <...>
At _entry:       $sp = <...>
(These two may well be the same value. If they are, that is the answer, not a
mistake — say why.)
What that means:
<...>

## 3. _entry and the boot stack
&_entry = <...>
sp after entry.S sets it up = <...>
Formula: <...>
&stack0 = <...>, sizeof(stack0) = <...>, and why that size:
<...>

## 4. The banner
Printed from: <file:line>, inside function <...>
Which calls: <...>
Route to my terminal:
<...>

## 5. At main
$pc = <...>   (p/x &main = <...>, and they differ because <...>)
$sp = <...>
$satp = <...>, which means <...>
Paging must still be off here because:
<...>

## 6. "Just a program"
<3-4 sentences>
```

When you have written all six, and **not before**, read the answer key in
`solutions/README.md`.

---

# Part 3 — Unix tools without stdio (~1.5 h)

Three tools, in `mycat.c`, `myls.c`, `mygrep.c`. Build with the supplied
`Makefile`, which uses `-Wall -Wextra -Werror`: a warning is a build failure,
on purpose.

## The rules

- **Allowed:** `open`, `read`, `write`, `close`, `stat`/`lstat`, and libc that
  is not stdio — `malloc`, `free`, `realloc`, `strlen`, `strcmp`, `strdup`,
  `memchr`, `memmem`, `qsort`, `strerror`.
- **Banned:** all of stdio. No `printf`, no `fprintf`, no `puts`, no `FILE *`,
  no `fopen`, no `perror`. Diagnostics are assembled by hand and pushed out
  with `write(2)` to fd 2.
- **The one exception:** `opendir` / `readdir` / `closedir` in `myls`. There
  is no portable lower-level way to read a directory, so `<dirent.h>` is
  permitted there and nowhere else.
- `tests/run.sh` enforces the stdio ban by inspecting each binary's undefined
  symbols, including the `err.h`/`asprintf` family that formats for you without
  being stdio proper. It is a symbol check, not a proof: the rule is the spec,
  and the gate only catches the common ways of breaking it.

Two things that will bite you, both deliberately tested:

- **`read()` may return fewer bytes than you asked for** without being at end
  of file. Only a return of **0** means end of input. A negative return with
  `errno == EINTR` means "try again", not "error".
- **`write()` may write fewer bytes than you gave it.** Loop until the whole
  buffer is out.

## The specifications

Each tool is specified as "byte-for-byte identical to the system tool" —
stdout, stderr *and* exit status. That is what the autograder actually
compares, so the spec cannot drift from the test.

**Pick a read-buffer size and write it down.** Nothing here constrains it, and
any sensible value passes — but Part 4 asks you to account for the number of
`read` calls `strace` shows, and that answer is a function of the number you
chose here. Choosing 64 KiB and choosing 512 bytes are both defensible; not
knowing which you chose is not.

### `mycat [FILE...]` — reference: `cat`

- No operands, or an operand that is exactly `-`: copy standard input.
- Otherwise copy each operand to stdout in order.
- A file that will not open: write `mycat: <path>: <strerror(errno)>` to
  stderr, **carry on with the remaining files**, and exit 1 at the end.
- A file that opens but fails to *read* — `mycat somedirectory` is the case you
  will meet, which opens fine and then fails with `EISDIR` — is reported the
  same way, with the same message shape and the same exit 1.
- Otherwise exit 0.
- Binary safe: the fixtures contain NUL bytes. Never treat the buffer as a
  C string.
- Never add or remove a trailing newline.

### `myls [PATH]` — reference: `LC_ALL=C ls -1 PATH`

- No operand means `.`. More than one operand is a usage error, exit 2 —
  this `myls` takes at most one path. (Real `ls` accepts many and interleaves
  headers between them; reproducing that is a week's worth of formatting rules
  for no lesson. Because there is no reference behaviour to diff against, the
  autograder does not test this case and the exact wording is yours — the exit
  status is what matters.)
- If `PATH` is not a directory, print it back verbatim and exit 0. (That is
  what `ls -1 somefile` does.)
- If it is a directory: print the entry names, one per line, **sorted by raw
  byte value** (`qsort` with a `strcmp` comparator), **omitting every name
  that begins with `.`** — which covers `.`, `..` and dotfiles, exactly as
  `ls -1` does. No sizes, no modes, no colours, just names.
- Cannot access it: `myls: cannot access '<path>': <strerror(errno)>` to
  stderr, exit 2.

  The sort is not decoration. `readdir` returns entries in whatever order the
  filesystem chose, which differs between machines and between runs; without
  a defined order the diff test is a coin flip.

### `mygrep PATTERN [FILE...]` — reference: `grep -F PATTERN [FILE...]`

- **Plain substring matching. No regular expressions** — no `.`, no `*`, no
  anchors. `grep -F` is the reference precisely because it does the same.
- No FILE operands, or an operand that is exactly `-`: read stdin. (`grep -F`
  treats `-` as stdin just as `cat` does.)
- An **empty pattern** matches every line — `grep -F "" file` prints the file.
  Do not special-case it into matching nothing.
- Print every line that contains the pattern anywhere, always terminated with
  `\n` even if the source line was not.
- With **two or more** FILE operands, prefix each printed line with
  `<file>:`. With one file, or with stdin, no prefix.
- A file that will not open: `mygrep: <path>: <strerror(errno)>` to stderr,
  carry on.
- Exit status, in this order of precedence: **2** if any file could not be
  read, else **0** if anything matched, else **1**.
- Fewer than two arguments: usage line to stderr, exit 2.
- Lines are not NUL-terminated. Match with `memmem(hay, hlen, needle, nlen)`,
  not `strstr`.
- The input does not arrive in one `read()`, and the fixtures include one large
  enough (200000 bytes) that a single `read()` will not swallow it. Your line
  handling has to cope with input that spans more than one read.

## Running the tests

```sh
make                              # must be warning-free
../tests/run.sh .                 # or: make test
```

`run.sh` builds your tools, checks the stdio ban, generates the fixture tree
into a temp directory, and then for every case runs your tool and the system
tool with identical arguments and identical stdin and diffs stdout, stderr
and exit status. It prints a PASS/FAIL table and exits non-zero if anything
failed; a failing case shows you both outputs.

Only one thing is normalised: the leading program name in a diagnostic
(`mycat: ` becomes `cat: `) so that the *wording* is compared and not your
executable's name. Everything else must match exactly. The one exception is
the `mygrep` no-arguments case, where both programs must fail with status 2
and say *something* on stderr, but the usage wording is each program's own.

Look at the fixtures before you start:

```sh
../fixtures/genfixtures.sh /tmp/fix && ls -la /tmp/fix
```

`fixtures/README.md` says what each one is there to catch. Reading that table
first is worth ten minutes of debugging later.

---

# Part 4 — Watch the syscalls (~0.5 h)

Now look at the boundary you have been calling across.

```sh
../fixtures/genfixtures.sh /tmp/fix

strace -f ./mycat /tmp/fix/hello.txt              2>&1 | less
strace -f ./myls  /tmp/fix/tree                   2>&1 | less
strace -f ./mygrep green /tmp/fix/colours.txt     2>&1 | less

strace -f cat /tmp/fix/hello.txt                  2>&1 | less
```

Useful narrowing:

```sh
strace -c ./mycat /tmp/fix/large.txt              # count per syscall
strace -e trace=read,write ./mycat /tmp/fix/large.txt
strace -e trace=openat,getdents64,close ./myls /tmp/fix/tree
```

## The questions

Four short paragraphs in `STRACE-NOTES.md` — a paragraph each, not an essay.

1. **`mycat`.** Separate the syscalls *your code* caused from the ones
   *process startup* caused. Where exactly does your program's own behaviour
   begin? Name the syscall that marks the boundary and say why.

2. **`myls`.** You called `opendir`/`readdir`, which are library functions,
   not system calls. What syscall(s) does `strace` show underneath them? How
   many `readdir` calls did you make, and how many of that syscall appeared —
   and what does the difference tell you about what the library is doing on
   your behalf?

3. **`mygrep`.** Compare the `read` pattern on `/tmp/fix/large.txt` against
   the one on `/tmp/fix/hello.txt`. What determines the number and size of
   the reads, and what would change if you halved your buffer?

4. **`mycat` vs `cat`.** Run both on `/tmp/fix/large.txt` with `strace -c`
   and account for the difference. It goes in **two directions at once**:
   real `cat` makes *more* syscalls in total than yours, but *fewer*
   `read`/`write` calls while actually copying the file. Explain both.
   (Look at `strace -e trace=read,write,fstat,fadvise64` for the copying
   half; the totals differ mostly before the first byte moves.)

## Deliverable — `STRACE-NOTES.md`

```markdown
# strace notes

Command lines used:
    <paste>

## 1. mycat: my syscalls vs startup's
<paragraph>
Boundary syscall: <...>, because <...>

## 2. myls: what opendir/readdir become
readdir calls made: <n>
<syscall> calls seen:  <m>
<paragraph>

## 3. mygrep: the read pattern
On hello.txt:  <n> reads of <...>
On large.txt:  <n> reads of <...>
<paragraph>

## 4. mycat vs cat
    $ strace -c ./mycat <file>   -> <total> calls
    $ strace -c cat <file>       -> <total> calls
<paragraph accounting for both directions>
```

Then read the Part 4 key in `solutions/README.md`.

---

# Stretch goals

Unweighted. Do them if Part 3 came easily.

- **`mycat -n`** — number the output lines, like `cat -n`. Match `cat -n`'s
  format exactly (six-wide right-aligned number, a tab, the line). Add a test
  for it to your own copy of `tests/run.sh`.
- **`mygrep -i`** — case-insensitive substring matching, matching `grep -iF`.
  You cannot use `memmem` for this; write the scan yourself and think about
  what "case" even means outside ASCII.
- **Boot with `CPUS=3`** and watch three harts arrive at `_entry`. Set
  `break *0x80000000`, `continue` three times, and print `$sp` and
  `$mhartid` each time. Explain the three different stack pointers from the
  formula in `entry.S`. This is also a good way to learn how much GDB's
  thread switching can confuse you.
- **Find where xv6 installs its first page table.** Break on `kvminithart`,
  step to the instruction that writes `satp`, and note the address that gets
  written. You meet this properly in Lab 4; the point here is only to find
  it. Record it in `GDB-NOTES.md` under a "stretch" heading.

---

# If you get stuck

- `make qemu` hangs with no output: almost always a QEMU too old for
  `-machine virt` with `-bios none`. Check `qemu-system-riscv64 --version`.
- GDB says `Remote 'g' packet reply is too long`: you attached without
  `set architecture riscv:rv64`. Use the xv6 `.gdbinit`, or set it by hand.
- A stray `qemu-system-riscv64` left running after a killed test:
  `pkill qemu-system-riscv64`.
- `tests/run.sh` reports a stdout mismatch you cannot see: it is whitespace
  or a missing trailing newline. `cmp` the two files, or pipe both through
  `od -c | tail`.
- Your `myls` output looks right but the order is wrong on one machine only:
  you sorted with the locale's collation instead of `strcmp`. `LC_ALL=C`.
- `make test` says it cannot find the autograder: you copied only `starter/`.
  Copy `tests/` and `fixtures/` alongside it, or pass
  `make test TESTS=/path/to/lab00-toolchain/tests/run.sh`.
- GDB gives `could not connect: Connection timed out`: wrong port. It is not
  1234 — see Part 2.
- Building xv6 prints `warning: kernel/kernel has a LOAD segment with RWX
  permissions`: expected with current binutils, and harmless. It is the only
  warning in an otherwise clean build; ignore it.
