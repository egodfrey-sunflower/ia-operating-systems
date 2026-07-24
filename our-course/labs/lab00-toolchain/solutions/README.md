# Lab 0 — Reference solutions and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  Reference code for Part 3 and the model answers for Parts 2      ║
║  and 4. Read it AFTER you have attempted the lab, not before.     ║
║  The prose parts are the ones that teach; reading the key first   ║
║  trades the whole point of the lab for twenty saved minutes.      ║
╚═══════════════════════════════════════════════════════════════════╝
```

Every model answer below is inside a collapsed `<details>` block, one per
question, so you can check them one at a time.

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g, zero warnings
make test       # ../tests/run.sh on this directory: 38 passed, 0 failed
```

---

## Part 1 — marking note (25%)

There is no key: the answer is whatever your machine printed. Give yourself
the marks if `TOOLCHAIN.md` has (a) a version for **every** row of the table,
(b) the xv6 commit sha you actually built, and (c) a pasted boot transcript
that reaches `init: starting sh` and a `$` prompt with one command run at it.

Do not skip (b). Part 2's answers are revision-specific, and in week 8, when
a page-table bug turns out to be a toolchain difference, this file is what
tells you.

---

## Part 2 — the GDB observation questions (25%)

### ⚠ How to read this key

The **structural** answers below are correct for any xv6-riscv near book
rev5. The **numbers** are not universal: they were observed on

| | |
|---|---|
| xv6 | the course's pinned tree, `labs/xv6-riscv/` — tag `xv6-riscv-rev5`, commit `7d7adbb1` |
| compiler | `riscv64-unknown-elf-gcc` 13.2.0 |
| emulator | QEMU 8.2.2, `make CPUS=1 qemu-gdb` |
| debugger | `gdb-multiarch` 15.1 |

Every value marked **[observed]** was read off that live session. Values
marked **[derived]** follow from the source and were not separately measured.
Addresses inside the kernel (`&main`, the breakpoint PC, `&stack0`) move when
the compiler or the commit changes — **do not mark yourself down for a
different address**, mark yourself down for a different explanation.

These were re-derived against the pinned tree after the course settled on
vendoring rev5 rather than cloning upstream. If you are working from an upstream
clone instead, expect different addresses *and* different names — upstream
renamed `printf` to `printk` after rev5, which changes question 4's answer.

<details>
<summary><b>1. Where does the very first instruction execute?</b></summary>

**[observed]** `$pc = 0x1000`, `$sp = 0x0`. `info symbol $pc` answers
`No symbol matches $pc.`

```
=> 0x1000:      auipc   t0,0x0
   0x1004:      addi    a2,t0,40
   0x1008:      csrr    a0,mhartid
   0x100c:      ld      a1,32(t0)
   0x1010:      ld      t0,24(t0)
   0x1014:      jr      t0
```

**This is not xv6's code.** It is QEMU's built-in reset vector — six
instructions the `virt` machine places at 0x1000 and starts every hart at.
There is no symbol for it because it is not in `kernel/kernel`'s symbol
table; GDB only knows the symbols of the file you loaded.

What it does: put the hart id in `a0`, load the device-tree blob address into
`a1`, load a target address from a word sitting just past the code (at
`t0+24`), and jump to it. That target is 0x80000000, where `-kernel` loaded
xv6.

The point of the question: xv6 is built with `-bios none`, so there is *no*
firmware, no OpenSBI, no bootloader. Those six instructions are the entire
pre-history of the machine. Everything after 0x80000000 is xv6's own doing.

**Full marks for:** identifying that it is QEMU's ROM and not xv6, and
saying how you could tell (no symbol; it is below 0x80000000; it is not in
any xv6 source file).
</details>

<details>
<summary><b>2. What is <code>sp</code> at that point, and what does that tell you?</b></summary>

**[observed]** `$sp = 0x0` at the reset vector, and **still `0x0`** on
arrival at `_entry` (`break *0x80000000; continue`).

There is no stack. Nothing has set one up, because there is nobody to have
set one up — the six-instruction ROM does not, and there is no firmware
underneath it. `sp` is simply whatever the reset state left it as, which on
this machine is zero.

Two consequences worth stating:

- This is why `_entry` exists and why it is **assembly**. Compiled C assumes
  a valid stack pointer from its first instruction (it pushes a frame in the
  prologue). You cannot write the code that creates the stack in the language
  that requires one.
- It is also why `_entry`'s very first act is `la sp, stack0`, before it
  does anything else at all.

**Full marks for:** noticing it is 0/garbage rather than a plausible address,
and connecting that to why `entry.S` must be assembly.
</details>

<details>
<summary><b>3. Where does xv6's own first instruction live, and what does it do?</b></summary>

**[observed]** `p/x &_entry` → `0x80000000`. That address is not an accident:
`kernel/kernel.ld` places `.text` there, and QEMU's `-kernel` loads the image
there, and the ROM in question 1 jumps there. All three have to agree.

`kernel/entry.S`:

```asm
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        call start
```

**The formula is `sp = stack0 + 4096 * (mhartid + 1)`.**

**[observed]** `p/x &stack0` → `0x80007860`; `p sizeof(stack0)` → `32768`.
Stepping: after the `la` (which assembles to two instructions, `auipc` +
`addi`) `sp` is exactly `0x80007860`; after the `add` it is `0x80008860` on
hart 0 — one 4 KiB slot higher. **[observed, CPUS=3]** hart 2 gets
`0x8000a860`, i.e. `0x80007860 + 3 × 4096`, which is the formula again.

Why 32768: `kernel/start.c` declares

```c
__attribute__((aligned(16))) char stack0[4096 * NCPU];
```

and `kernel/param.h` has `#define NCPU 8`. So it is one 4 KiB stack per
*possible* hart, 8 of them, reserved statically in the kernel's BSS. It has
to be static: there is no allocator yet — `kinit()` is a dozen lines away in
`main()` and cannot run without a stack of its own.

The `+ 1` is because RISC-V stacks **grow downwards**, so hart *n* must start
at the *top* of its slot, not the bottom.

**Full marks for:** the formula, the `NCPU` explanation of the size, and the
"grows downwards, hence +1" point. A common wrong answer is that the stack is
4096 bytes total.
</details>

<details>
<summary><b>4. Which function prints the banner?</b></summary>

`main()`, in `kernel/main.c`, on the `cpuid() == 0` path:

```c
if (cpuid() == 0) {
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");     // <- kernel/main.c:17
```

**[observed]** `info line kernel/main.c:17` → *starts at address 0x80000e92
`<main+86>`*, and the call itself is at `0x80000e9a`.

Note the ordering: `consoleinit()` and `printfinit()` are called *first*, on
purpose. The banner is the first thing printed precisely because it is the
proof that the console works.

The route to your terminal **[derived, from the source]**:

`printf()` (`kernel/printf.c`) formats the string a character at a time and
calls `consputc()` (`kernel/console.c:35`), which calls `uartputc_sync()`
(`kernel/uart.c`), which busy-waits for the UART's transmit-holding register
to be free and stores the byte into it. The UART is memory-mapped at
`UART0 = 0x10000000` (`kernel/memlayout.h:21`) — so "printing" is, at the
bottom, a single store instruction to a magic address. QEMU is emulating a
16550 UART there and forwards whatever is written to your terminal.

`_sync` in the name means "no interrupts, no sleeping" — the interrupt-driven
path does not exist yet this early, and would need a scheduler that has not
been started.

**Full marks for:** naming `main()` and the print function, and getting the
chain down to *a store to a memory-mapped device register*. That last step is
the whole point of the question.
</details>

<details>
<summary><b>5. What does the machine look like at a breakpoint on <code>main</code>?</b></summary>

**[observed]**

```
Breakpoint 2 at 0x80000e44: file kernel/main.c, line 13.
$pc   = 0x80000e44
$sp   = 0x80008840
$satp = 0x0
```

and from `kernel/kernel.sym`, `main` is at `0x80000e3c`.

**Why `$pc != &main`:** GDB deliberately breaks *after* the prologue, not at
the entry point — `0x80000e44` is `main+8`. Until the prologue has run, the
frame pointer and the saved registers are not in place, so arguments and
locals would print as garbage and a backtrace would be wrong. GDB uses the
line table to find the first address of the first *statement*, which here is
line 13 (`if (cpuid() == 0)`), and puts the breakpoint there.

**Why `$sp = 0x80008850`:** 32 bytes below the `0x80008860` that `entry.S`
installed for hart 0 — the frames of `start()` and `main()`. Same stack;
nothing has switched away from it.

**`$satp = 0x0` means paging is off.** `satp`'s top four bits are the MODE
field; MODE = 0 is *Bare*, i.e. no translation at all — every address the CPU
issues is a physical address. (Sv39, which xv6 uses, is MODE = 8.)

It **must** be off, because `main()` is about to *build* the thing that would
otherwise be doing the translating:

```c
kinit();          // physical page allocator -- there are no pages yet
kvminit();        // create the kernel page table -- it does not exist yet
kvminithart();    // write satp: NOW paging is on
```

You cannot run through a page table you have not allocated the pages for.
`kvminithart()` is the exact line where `satp` changes; break there and print
`$satp` before and after if you want to see it flip.

**Full marks for:** the prologue-skipping explanation, and recognising
MODE = Bare *and* giving the ordering argument for why it has to be that way.
</details>

<details>
<summary><b>6. The OSTEP claim, cashed out</b></summary>

Model answer (yours will differ in wording; the content is what matters):

> It is true in every mechanical sense. The kernel is an ELF file built by
> the same `gcc` from ordinary `.c` files; it has a `main()`; it has a stack
> that a few instructions of assembly had to set up; GDB debugs it with the
> same `break`, `stepi` and `bt` I would use on `mycat`. Nothing about the
> instructions is special — `add`, `ld`, `jr` are the same instructions.
>
> What is different is not the program but the *privilege* it runs at and the
> fact that nothing runs underneath it. In this session I read `mhartid`,
> `mstatus` and `satp` — machine- and supervisor-level control registers that
> a user program cannot even name, let alone write. I watched it print by
> storing a byte into a device register at 0x10000000, with no `write()` and
> no kernel to ask. And I saw it start with `sp = 0`, which no ordinary
> program ever does, because for an ordinary program the kernel has already
> built the stack, the address space and the file descriptors before `main()`
> is reached. The OS is just a program — it is the program that has to do all
> of that for itself, and then for everyone else.

**Full marks for:** at least one *concrete thing observed in this session* on
each side of the claim. An answer that only paraphrases OSTEP without citing
something seen in GDB has not done the exercise.
</details>

---

## Part 3 — reference implementations (37%)

Marked entirely by `tests/run.sh` — 38 cases, and it exits non-zero if any
fail. Validated both ways: **38 passed, 0 failed** against this directory,
and **32 failed** against the unimplemented `starter/` skeletons. (The six
that "pass" against the skeletons are the three no-stdio symbol gates and the
three cases whose correct answer really is "print nothing and exit 0".)

### Shared conventions

Each tool is a single translation unit with no shared header, so `write_all`
and `warn` are repeated in all three. That duplication is deliberate: the
short-write loop is the thing you must not get wrong, and having written it
once is not the same as having it available.

- **No stdio.** Everything goes out through `write(2)`. `write_all()` loops
  over short writes and retries `EINTR`. `warn()` formats
  `prog: context: strerror(errno)` on fd 2 by hand. `strerror`, `malloc`,
  `qsort` and `memmem` are libc but not stdio, so they are fair game.
- **Error contract.** A per-operand failure is reported to fd 2, does *not*
  abort the run, and forces a non-zero exit at the end. This is what coreutils
  does, and matching it is what makes the diff test possible at all.

### `mycat.c`

Streams in 64 KiB chunks straight from the source fd to fd 1 — never buffers
a whole file, so it is memory-flat on a 4 GB input and binary-safe by
construction (nothing is ever treated as a C string). `-` and the
no-arguments case both route to fd 0.

The loop is the lab: `read` returning less than the buffer is *normal*, only
`0` is EOF, and `-1` with `EINTR` means "go round again".

### `myls.c`

`opendir`/`readdir`/`closedir` are used — the one permitted exception. Names
are `strdup`'d (the `struct dirent` readdir returns may be reused on the next
call), collected into a `realloc`'d array, and `qsort`'d with a `strcmp`
comparator, which gives ascending raw-byte order — exactly what `LC_ALL=C ls`
does, and why `Middle` sorts before `alpha.txt`.

`d_name[0] == '.'` drops `.`, `..` and dotfiles in one test, matching
`ls -1`. A non-directory operand is echoed back verbatim, which is the
behaviour of `ls -1 somefile` and easy to forget.

The `stat()` before `opendir()` is not redundant: it is what distinguishes
"not a directory" (print it) from "does not exist" (error, exit 2), with the
right `errno` for the message either way.

### `mygrep.c`

Slurps the whole input into a `realloc`'d buffer and only then splits on
`\n`. Streaming with a fixed buffer is possible but you then own the
straddling-line problem, and for a warm-up that is a distraction from the
syscall lesson. The `slurp` loop is still where short reads have to be
handled correctly.

Matching is `memmem(3)` over the length-bounded line, not `strstr`: lines are
not NUL-terminated and the fixtures contain NULs. Printed lines always get a
`\n` even when the source had none, and the `FILE:` prefix appears only with
two or more file operands — both of those are `grep`'s behaviour, and both
are separately tested.

Exit status precedence is **2 (unreadable) > 0 (matched) > 1 (no match)**;
`grep -F pattern missing present` exits 2 even though it matched, which is
the case people get wrong.

---

## Part 4 — the strace paragraphs (13%)

### ⚠ How to read this key

The counts below are **[observed]** on Ubuntu 24.04 / glibc 2.39 /
coreutils 9.4 / strace 6.8, x86-64, against the generated fixtures. Exact
numbers move with libc and coreutils versions. Mark yourself on the
*explanation*, and on whether your numbers are internally consistent with the
trace you actually pasted.

<details>
<summary><b>1. mycat: your syscalls vs process startup's</b></summary>

**[observed]** `strace ./mycat /tmp/fix/hello.txt` — 36 lines, of which the
last six are yours:

```
openat(AT_FDCWD, "/tmp/fix/hello.txt", O_RDONLY) = 3
read(3, "hello\nworld\n", 65536)        = 12
write(1, "hello\nworld\n", 12)          = 12
read(3, "", 65536)                      = 0
close(3)                                = 0
exit_group(0)                           = ?
```

Everything above that is the dynamic loader: `execve`, then `access` on
`/etc/ld.so.preload` (failing, which is normal), `openat`+`fstat`+`mmap` of
`/etc/ld.so.cache` and `libc.so.6`, `mprotect` to apply relocation
protections, `arch_prctl` to set up thread-local storage, `set_tid_address`,
`set_robust_list`, `rseq`, `prlimit64`, `munmap`.

**The boundary syscall is `execve`** — it is the first line of the trace and
the only one that is not a consequence of something else. Everything between
it and your first `openat` was done *for* you, by `ld.so`, before `main()`
was entered. A useful sharper answer: the boundary you can *see* is the last
`mprotect`/`munmap` pair, after which the loader is done; but `execve` is the
principled answer, because it is where this process stopped being the shell.

**Full marks for:** correctly partitioning the trace, naming `execve`, and
noting that the startup half is the dynamic loader's work and not the
kernel's whim. (Building static — `gcc -static` — and re-running is a nice
way to prove it: the startup half almost vanishes.)
</details>

<details>
<summary><b>2. myls: what opendir/readdir become</b></summary>

**[observed]** `strace -e trace=openat,getdents64,close ./myls /tmp/fix/tree`:

```
openat(AT_FDCWD, "/tmp/fix/tree", O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY) = 3
fstat(3, {st_mode=S_IFDIR|0775, st_size=4096, ...}) = 0
getdents64(3, ... /* 8 entries */, 32768) = 232
getdents64(3, ... /* 0 entries */, 32768) = 0
close(3)                                = 0
```

- `opendir` is `openat` with `O_DIRECTORY|O_NONBLOCK|O_CLOEXEC`, plus an
  `fstat` to size the buffer it is about to allocate.
- `readdir` is **`getdents64`** — and the numbers do not match. The fixture
  directory has 8 entries (`.`, `..`, `.hidden`, `Middle`, `alpha.txt`,
  `beta.txt`, `sub`, `zeta.bin`), so `readdir` was called **9 times** (eight
  entries plus the final `NULL`), but only **2** `getdents64` calls appear.

**That is the answer.** `readdir(3)` is a *buffered* reader. One
`getdents64` pulled all 8 entries into a 32 KiB userspace buffer in a single
232-byte reply; the next eight `readdir` calls were pure pointer arithmetic
inside that buffer with no kernel involvement at all, and the ninth call
issued the second `getdents64`, which returned 0 and meant end-of-directory.

The general lesson, which is the one the course keeps returning to: a library
call is not a system call, the ratio between them is a *design decision*
about buffering, and `strace` is how you find out what that decision was. If
`readdir` had been a syscall each time this directory would have cost 9
crossings instead of 2.

**Full marks for:** the syscall name, the mismatch in counts, and the word
"buffered" (or an equivalent explanation of amortisation).
</details>

<details>
<summary><b>3. mygrep: the read pattern</b></summary>

`hello.txt` is 12 bytes: one `read` returning 12, one returning 0. Two calls
regardless of buffer size, because the file is smaller than any buffer.

`large.txt` is 200000 bytes. The reference `mygrep` asks for *the space left
in its buffer*, and doubles the buffer when it fills, so the requested sizes
wander — **[observed]** `65536, 65536, 131072, 62144`, four calls. The
reference `mycat`, with its fixed 64 KiB buffer, shows the underlying pattern
much more cleanly — **[observed]**:

```
read(3, ..., 65536) = 65536
read(3, ..., 65536) = 65536
read(3, ..., 65536) = 65536
read(3, ..., 65536) = 3392
read(3, ..., 65536) = 0
```

Four reads plus the terminating zero: `200000 = 3 × 65536 + 3392`. **The
number of reads is `ceil(size / buffer) + 1`, and the size of each is
`min(buffer, bytes remaining)`.** On a regular file, `read` gives you
everything you asked for until the file runs out — so the only short read is
the last one.

Halving the buffer to 32 KiB doubles the count: `ceil(200000/32768) = 7`
reads plus the zero. Each crossing costs a mode switch, so a bigger buffer is
strictly fewer crossings — with diminishing returns, since the kernel is
already reading ahead, and at some point you are just wasting memory. (That
trade-off is the whole of OSTEP's later I/O chapters in miniature.)

**But regular files are the easy case.** `tests/run.sh` also pipes the file
in two bursts with a pause between; then the *first* read comes back short
with data still to come — **[observed]** `read(0, ..., 65536) = 24576` with
175424 bytes still on the way. Code that treats a short read as EOF passes
every file test and fails the moment its input is a pipe, a socket or a
terminal. That is exactly why the harness has that case.

**Full marks for:** the `ceil(size/buffer) + 1` relationship, the mode-switch
cost argument, and — for full credit — noticing that the file case never
shows you a *mid-stream* short read.
</details>

<details>
<summary><b>4. mycat vs cat</b></summary>

**[observed]** `strace -c` on `/tmp/fix/large.txt` (stdout to `/dev/null`):

| | `./mycat` | `cat` |
|---|---|---|
| total calls | 40 | 50 |
| `read` | 6 | 4 |
| `write` | 4 | 2 |
| `openat` | 3 | 4 |
| `fstat` | 2 | 5 |
| `mmap` | 8 | 10 |
| `brk` | 1 | 3 |
| `fadvise64` | 0 | 1 |
| `getrandom` | 0 | 1 |

**Direction one — `cat` does more in total (50 vs 40), and all of it before
the first byte moves.** It opens and `fstat`s `/usr/lib/locale/locale-archive`
(coreutils is internationalised; yours is not), calls `getrandom` for glibc's
hash-table seeding, and needs more `brk`/`mmap` because it is a bigger
program with more static data. None of that is the copying; it is the price
of being a real, translated, hardened utility.

**Direction two — `cat` does fewer `read`/`write` calls while copying (4 and
2 against 6 and 4), which is the interesting half.** With
`strace -e trace=read,write,fstat,fadvise64`:

```
fstat(1, {st_mode=S_IFCHR|0666, ...})      = 0     <- inspects stdout
openat(AT_FDCWD, "/tmp/fix/large.txt", O_RDONLY) = 3
fstat(3, {st_mode=S_IFREG, st_size=200000, ...}) = 0   <- and the input
fadvise64(3, 0, 0, POSIX_FADV_SEQUENTIAL)  = 0     <- tells the kernel
read(3, ..., 131072)  = 131072
write(1, ..., 131072) = 131072
read(3, ..., 131072)  = 68928
write(1, ..., 68928)  = 68928
read(3, ..., 131072)  = 0
```

`cat` `fstat`s **both ends** and derives its buffer size from `st_blksize`
(and the file size) rather than hard-coding one — 128 KiB here against your
64 KiB, so 200000 bytes takes 2 reads instead of 4. It also issues
`fadvise64(POSIX_FADV_SEQUENTIAL)`, which is not a copy at all: it is a
*hint* to the kernel's readahead, costing one syscall to make the remaining
ones cheaper.

So the two directions have the same shape: `cat` spends syscalls up front —
on locale, on stat, on a hint — to spend fewer of them per byte. Yours is
smaller and simpler and pays a little more per byte. Neither is wrong; the
lab's point is that you can now *see* the difference instead of guessing.

**Full marks for:** getting both directions, and identifying the `fstat`-then-
size-the-buffer mechanism as the reason for the read/write difference.
Half marks for noticing the totals differ without explaining the copy half —
that is the direction that actually teaches something.
</details>
