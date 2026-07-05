# Lab 0 — Toolchain & Unix warm-up

**Weeks:** 1 · **Budget:** 4–6 hours · **Track:** userspace · **Weight:** 5%

## Background

Every later lab depends on two things you build here: a working RISC-V
cross-toolchain with QEMU and `gdb`, and a working mental model of the
system-call boundary you are about to spend a term crossing in both
directions.

This lab has four parts:

- **A. Toolchain** — install and verify the tools (once, for the whole
  course).
- **B. Boot xv6 under the debugger** — watch a machine come up from the first
  instruction to the first user process. This is IA Lecture 1 ("booting")
  made concrete.
- **C. Unix warm-up in C** — reimplement `cat`, `ls -l`, and `grep` using
  **raw system calls only**. You are writing the userspace side of the exact
  interface xv6 implements on the kernel side.
- **D. Observe the boundary with `strace`** — see the syscalls your tools
  actually make, and explain them.

The theory this cashes out (booting, the user/kernel interface, the process
as the thing that makes system calls) is in the week-1 reading; see
`../../reading-list.md`. This handout does not re-explain it.

Throughout: **no stdio.** No `printf`, `fopen`, `fgets`, `getchar`,
`fputs`, … Your tools talk to the kernel directly through the thin `unistd.h`
wrappers (`open`, `read`, `write`, `close`, `lstat`, …) and, where glibc
ships no wrapper, through `syscall(2)`. `malloc(3)`, `strerror(3)`, and the
`str*`/`mem*` functions are not stdio and are allowed.

## Setup

You need Ubuntu 24.04 (or equivalent). Everything runs under emulation — no
RISC-V hardware.

```
sudo apt update
sudo apt install build-essential gdb-multiarch \
    gcc-riscv64-unknown-elf qemu-system-misc strace
```

- `build-essential` → `gcc`, `make`
- `gcc-riscv64-unknown-elf` → the cross compiler `riscv64-unknown-elf-gcc`
- `qemu-system-misc` → `qemu-system-riscv64`
- `gdb-multiarch` → a `gdb` that understands RISC-V
- `strace` → the syscall tracer Part D is built on

Verify with the provided script:

```
starter/toolcheck.sh
```

It prints an `OK`/`MISSING` table and exits nonzero if anything is missing.
Do not proceed until it is all `OK`.

A pristine xv6-riscv tree is vendored at `../xv6-riscv/`. **Never edit it.**
For Part B you copy it to a scratch directory and work there.

## Tasks

Tasks are numbered and weighted; the weights are the rubric.

---

### Part A — Toolchain (10%)

1. Install the packages above and run `starter/toolcheck.sh` until every row
   reads `OK`. **Deliverable:** paste the final table into your writeup.

---

### Part B — Boot xv6 under GDB (25%)

Make a scratch copy so the vendored tree stays pristine:

```
cp -r ../xv6-riscv ~/xv6-scratch
cd ~/xv6-scratch
```

**2. Boot it (5%).**

```
make qemu
```

QEMU builds the kernel, boots it, and drops you at the xv6 shell (`$`). Try
`ls`, `echo hi`, `cat README`. **Exit QEMU with `Ctrl-A` then `x`** (press
Ctrl and A together, release, then press `x`). Note this now — it is the only
way out and it is easy to forget.

**Deliverable:** the boot banner and the output of `ls` at the xv6 prompt.

**3. Single-step the boot (20%).**

xv6 ships a template (`.gdbinit.tmpl-riscv`) from which `make qemu-gdb`
generates a `.gdbinit` that sets the architecture, connects to QEMU's gdb
stub, and loads symbols — so the `.gdbinit` file appears the first time you
run that target. Modern `gdb` refuses to auto-load a `.gdbinit` from the
current directory unless you mark it safe (you can add the entry before the
file exists). Add this line to your `~/.config/gdb/gdbinit` (create the file
if needed), using your real path:

```
add-auto-load-safe-path /home/YOU/xv6-scratch/.gdbinit
```

Boot xv6 halted, waiting for the debugger. To make single-stepping sane, run
with **one CPU** so breakpoints do not fire on three harts at once:

```
make CPUS=1 qemu-gdb
```

It prints `*** Now run 'gdb' in another window.` and waits. In a **second
terminal, in the same directory**:

```
gdb-multiarch kernel/kernel
```

The `.gdbinit` connects you to the frozen machine. Now walk the boot:

```
(gdb) break _entry
(gdb) break start
(gdb) break main
(gdb) break kvminithart
(gdb) break userinit
(gdb) continue
```

Step with `stepi`/`step`, print registers with `p/x $pc`, `p/x $sp`,
`p/x $satp`, `p/x $mstatus`, and CSRs with `info registers`. Relevant source:
`kernel/entry.S`, `kernel/start.c`, `kernel/main.c`, `kernel/vm.c`,
`kernel/proc.c`.

**Answer these in writing** (this is the graded deliverable). Back every
answer with the GDB command you used and its output.

Don't worry if `mstatus`, privilege modes, and `satp` feel ahead of the
theory — they are: modes arrive with week 2's reading and paging with weeks
7–8. Today you only need to *observe* the values and match them against the
named definitions in `kernel/riscv.h`; the understanding comes later.

1. **Load address & first instruction.** At `_entry`, what is `$pc`? Why that
   address specifically (see the comment at the top of `entry.S` and
   `kernel/kernel.ld`)? What is the *first* thing `_entry` does, and why must
   it happen before the `call start`?
2. **Privilege mode at entry.** `qemu -kernel` enters in **machine mode**.
   In `start()`, which single instruction drops the CPU to a *less*
   privileged mode, and which mode does it land in? (Look at the end of
   `start.c`.)
3. **Preparing the mode switch.** `start()` writes `mstatus` near the top.
   Print `$mstatus` before and after that write. Which field changes, and
   what value in it encodes "Supervisor"? (Cross-check `MSTATUS_MPP_S` /
   `MSTATUS_MPP_MASK` in `kernel/riscv.h`.)
4. **`satp` before vs after paging.** Put a breakpoint at `kvminithart` and
   another one line *after* the `w_satp(...)` inside it. Print `$satp` at
   both. What is its value before, and what does that mean for address
   translation? After the write, decode the top nibble of `$satp` — which
   Sv39 mode value is it, and what do the low bits hold?
5. **Ordering of "booting" vs paging.** The banner `xv6 kernel is booting`
   is printed by `main()`. By reading `main()`, determine: is paging on or
   off at the instant that banner is printed? Justify from the *order* of
   calls in `main()`.
6. **The first process.** Break at `userinit`. What `state` does the first
   `proc` get, and what is its `cwd`? What is its pid? Note that in this xv6
   the first user program is **not** an embedded `initcode` blob — set a
   breakpoint at `forkret` (`kernel/proc.c`) and find the exact call that
   loads and runs the first real user program. What path does it exec?

---

### Part C — Unix warm-up (45%)

Work in `starter/`. Skeletons compile and exit 1 (`not implemented`). Build
with `make` (flags are fixed: `-Wall -Wextra -Werror -std=c11 -g`; your code
must compile clean). Test with `make test` from inside `starter/`
(equivalently, `tests/run.sh starter/` from the lab root); it will FAIL until
you implement the tools and, when done, it must all PASS.

**Contract for all three:** raw syscalls only; diagnostics to fd 2; a tool
that hits an error on one operand prints a message, keeps going with the
rest, and exits nonzero.

**4. `mycat` (12%).** `mycat [FILE...]`

- Copy each `FILE` to standard output (fd 1), in order.
- With no `FILE`, or when a `FILE` is `-`, copy standard input (fd 0).
- Must be **binary-safe** (NUL bytes, no assumption of trailing newlines):
  copy exactly the bytes read.
- A file that cannot be opened → error to fd 2, continue, exit status 1.
- Handle short `write()`s and `EINTR`.

**5. `myls` (20%).** `myls [PATH...]` — a minimal `ls -l`. With no `PATH`,
list `.`.

The output format is **defined here and checked byte-for-byte**, so it must
be deterministic:

- For each `PATH` argument, in the order given:
  - if `PATH` is a **directory**: read its entries, **discard `.` and `..`**
    (nothing else — dotfiles like `.bashrc` *are* listed), **sort the
    remaining names ascending by raw byte value** (i.e. `strcmp`/C locale),
    and print one line per entry. `NAME` is the bare entry name.
  - otherwise (regular file, symlink, device, …): print **one** line for
    `PATH` itself. `NAME` is `PATH` verbatim.
  - Directories are **not** recursed into.
- Each line is exactly:

  ```
  MODE SP SIZE SP NAME LF
  ```

  - `MODE` — 10 characters, exactly like `ls -l`: one **type** char
    (`d` dir, `-` regular, `l` symlink, `c` char dev, `b` block dev,
    `p` fifo, `s` socket, `?` other) followed by **9 permission** chars —
    `rwx` triples for owner/group/other, `-` where the bit is clear. Render
    **plain `rwx` only**: do *not* show setuid/setgid/sticky (`s`/`t`).
  - `SP` — a single ASCII space (`0x20`).
  - `SIZE` — `st_size` in bytes, decimal, no leading zeros, no padding.
  - `NAME` — as above.
  - `LF` — a single newline (`0x0A`).
- `stat` entries with `lstat(2)` / `fstatat(2)` + `AT_SYMLINK_NOFOLLOW` so a
  symlink is reported as a symlink, not its target.
- A `PATH` that cannot be `lstat`'d → error to fd 2, continue, exit 1.

**You may not use `readdir(3)`** — it is libc *buffered* directory I/O, and
this exercise is about the raw interface. Read directories with
**`getdents64(2)`**. glibc ships no wrapper for it, so call it via
`syscall(2)` and parse the raw records yourself (see Hints).

**6. `mygrep` (13%).** `mygrep PATTERN [FILE...]` — fixed-string search
(like `grep -F`, *not* a regex).

- Print every input line that contains `PATTERN` as a substring.
- With no `FILE`, or `FILE == -`, read standard input.
- Always terminate a printed line with `\n`, **even if** the matching input
  line had no trailing newline (this matches `grep`).
- When **more than one** `FILE` is given, prefix each printed line with
  `FILE:` (also matching `grep -F`). One file, or stdin: no prefix.
- Exit status: **0** if at least one line matched, **1** if none matched,
  **2** on an error (again, `grep`'s convention).

---

### Part D — Observe the boundary with `strace` (20%)

`strace` prints every system call a process makes. Use it on your own tools
and on a shell.

**7. Profile and trace (8%).**

```
strace -f -c ./mycat mycat.c                    # -c: per-syscall summary
strace -f -e trace=openat,read,write,close ./myls .
```

(Both commands are run from inside `starter/`, where your built tools live.)

`-f` follows `fork`ed children; `-c` prints a count/time summary;
`-e trace=...` restricts to named calls. Run these against all three tools.

**8. Explain a redirection (12%).** Run

```
printf 'hello\nworld\n' > f
strace -f -e trace=openat,read,write,close,dup2,execve \
    bash -c './mycat < f > g'
```

Then write a note that **explains every distinct syscall observed for
`mycat < f > g`** — what each one is for. In particular, answer:

- Which process opens `f` and `g`, and *when* relative to `execve` of
  `mycat`? (Trace `bash`, not just `mycat`, to see it.)
- Why does `strace ./mycat < f > g` (tracing `mycat` alone) show **no**
  `open` of `f` or `g`, even though `mycat` reads `f` and writes `g`? What
  does this tell you about how shell redirection works and what a freshly
  `exec`'d program inherits?
- Identify which syscalls are libc/loader start-up (e.g. `execve`, `brk`,
  `mmap`, `arch_prctl`, the `openat` of the dynamic linker) versus the
  actual work of `mycat` (`read` from fd 0, `write` to fd 1, `exit_group`).

## Hints

- **Ctrl-A x** exits QEMU. `Ctrl-A c` toggles the QEMU monitor if you get
  curious.
- **GDB, multiple harts.** Use `make CPUS=1 qemu-gdb` for stepping. With the
  default 3 CPUs a breakpoint fires on whichever hart reaches it, which is
  confusing when single-stepping one boot path.
- **GDB CSRs.** `p/x $satp`, `p/x $mstatus`, `p/x $pc`, `info registers`.
  The `satp` layout (mode field vs. the low bits) is drawn in
  `kernel/riscv.h` and RISC-V privileged-spec §4.1.11.
- **`getdents64` layout.** Define the record yourself; glibc does not expose
  it:

  ```c
  struct linux_dirent64 {
      unsigned long long d_ino;
      long long          d_off;
      unsigned short     d_reclen;   // total length of THIS record
      unsigned char      d_type;
      char               d_name[];   // NUL-terminated
  };
  ```

  Loop: `long n = syscall(SYS_getdents64, fd, buf, sizeof buf);` returns the
  number of bytes filled (0 = end, <0 = error). Walk `buf` record by record,
  advancing your offset by `d_reclen` each time (records are variable
  length — do **not** assume `sizeof(struct)`). `man 2 getdents64` has the
  canonical example. Open the directory with `open(path, O_RDONLY |
  O_DIRECTORY)`.
- **Sorting.** Collect names into an array of `char*` (`strdup` each), then
  `qsort` with a `strcmp` comparator. `strcmp` is exactly "ascending by byte
  value".
- **`stat`ing directory entries.** You have the directory fd already —
  `fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW)` avoids rebuilding path
  strings.
- **The mode string.** Type char from `S_ISDIR`/`S_ISREG`/`S_ISLNK`/…; the 9
  permission bits are `st_mode & 0777`, tested MSB-first (`0400`, `0200`,
  `0100`, `0040`, …).
- **Line splitting for `mygrep`.** Reading the whole input into a
  `malloc`/`realloc` buffer, then scanning for `\n`, is far easier than
  splitting across `read()` boundaries. Lines are not NUL-terminated, so use
  `memmem(3)` (needs `#define _GNU_SOURCE`) for the substring test, not
  `strstr`.
- **No stdio, even for errors.** Write diagnostics with `write(2, ...)`.
  `strerror(errno)` gives the message text; `malloc` it into place or write
  it in pieces.
- **Robust `write`.** `write` may write fewer bytes than asked, or fail with
  `EINTR`. Wrap it in a loop that retries until all bytes are out.

## Deliverables

Submit a single directory containing:

1. `writeup.md` (or `.txt`) with:
   - the Part A `toolcheck.sh` table;
   - the Part B boot banner + `ls`, and written answers to all six Part-B
     observation questions (with the GDB commands/output that back them);
   - the Part D syscall note (tasks 7–8).
2. Your completed `mycat.c`, `myls.c`, `mygrep.c` (they must build clean with
   the provided `Makefile` and pass the autograder — `make test`).

## Rubric

| Part | Item | Weight |
|------|------|-------:|
| A | Toolchain installed; `toolcheck.sh` all `OK` | 10 |
| B | xv6 boots; banner + `ls` captured | 5 |
| B | Six GDB observation questions, correct & evidenced | 20 |
| C | `mycat` — files, stdin, `-`, binary-safe, error/exit behaviour | 12 |
| C | `myls` — exact format, `getdents64`, sort, lstat, no `readdir` | 20 |
| C | `mygrep` — fixed-string, multi-file prefix, newline & exit rules | 13 |
| D | `strace` profile/trace run on all three tools | 8 |
| D | Correct explanation of every syscall for `mycat < f > g` | 12 |
| | **Total** | **100** |

Automated checks (`make test`) gate the Part C code: a tool that does
not pass its tests scores 0 for that tool regardless of how it looks. Using
any stdio function, or `readdir(3)` in `myls`, is an automatic 0 for that
tool even if the output matches.
