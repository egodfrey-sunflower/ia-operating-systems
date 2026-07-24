# Lab 4 — reference solution and answer key

> **SPOILERS.** This directory is the worked solution. Read it after you have a
> serious attempt, or when you are properly stuck on one part and want that part
> only. Reading it first trades the whole point of the lab for a few hours.

Apply the reference over a tree made by `../setup.sh`:

```sh
../setup.sh ~/lab4-ref
./apply.sh ~/lab4-ref
cd ~/lab4-ref && make qemu
```

`apply.sh` lays whole files over your tree; `diff -u` against `../starter/overlay/`
or the pinned `../../xv6-riscv/` shows exactly what changed. `trap.c` and
`sysproc.c` are not among them: the pinned tree already routes a page fault
(trap.c) and the lazy branch of `sbrk` (sysproc.c) into `vmfault()`. Part 3 is
the body of `vmfault()`, not that wiring.

## What each part touches, and the marking note

### Part 1 — vmprint (`kernel/vm.c`, called from `kernel/proc.c`)

`vmprint()` prints the header, then a small recursive helper prints one page
of PTEs — a ` ..` group per level, the index, the raw PTE, `PTE2PA`, and the
five flag characters — and recurses into any entry that is an interior node
(none of R/W/X set). It is called once, in `forkret()`, right after the first
process execs `/init`.

**Exact expected tree for `/init`.** The shape is deterministic; the physical
addresses vary with the kernel's size (this reference builds a 128 KB reference-
count array into the kernel, so its addresses differ from a stock build — that
is expected and does not matter). What must match, line for line, is the
`(depth, index, flags)` of each entry:

```
page table 0x....
 ..0:        -----   root  -> middle table
 .. ..0:     -----   middle-> leaf table
 .. .. ..0:  r-xu-   /init text     (R, X, U)
 .. .. ..1:  rw-u-   /init data     (R, W, U)
 .. .. ..2:  rw---   stack guard    (R, W, but NOT U -- uvmclear cleared it)
 .. .. ..3:  rw-u-   user stack
 ..255:      -----   root  -> middle table
 .. ..511:   -----   middle-> leaf table
 .. .. ..507:r--u-   USYSCALL       (R, U, but NOT W -- Part 2's read-only page)
 .. .. ..510:rw---   trapframe      (R, W, NOT U)
 .. .. ..511:r-x--   trampoline     (R, X, NOT U)
```

`tests/run.py` parses this out of the boot console and compares it to
`P1_EXPECTED`, ignoring the addresses, so an off-by-one in the indentation, a
missing or extra leaf, or a misdecoded flag fails Part 1 automatically.

Leaf 507 (USYSCALL) is Part 2's page, not Part 1's, and the grader **ignores it**
in the Part 1 shape check — present or absent. A student who completes Part 1
before Part 2 prints a correct tree without leaf 507 and still passes Part 1;
`P1_EXPECTED` omits it, and `part1()` strips any `(3, 507)` entry before
comparing. The other ten entries are matched exactly, and no walk bug can drop
507 alone, so tolerating it costs no coverage.

**Marking `PGTBL-NOTES.md` (by hand).** The prose should say: leaf 0 is text
because it is the only leaf with `x` and no `w`; the guard page and trapframe and
trampoline have no `u` because they must be unreachable from user mode; the two
subtrees hang off root indices 0 (low addresses: code, data, stack) and 255 (the
top of the address space: trampoline, trapframe, USYSCALL, at virtual addresses
just under 2^38); and the "a page table is itself pages" idea is visible in that
every `pa` printed, including the interior ` .. ` ones, is an ordinary physical
page the allocator handed out. Full marks do not require every one of these, but
should show the student read the tree rather than pasted it.

### Part 2 — the read-only pid page (`kernel/proc.c`, `user/ulib.c`)

`proc_pagetable()` kallocs a page, stores `p->pid` into it as a `struct
usyscall`, and maps it at `USYSCALL` with `PTE_R | PTE_U` — **not** `PTE_W`.
`proc_freepagetable()` unmaps it with `do_free = 1`. `ugetpid()` casts `USYSCALL`
to `struct usyscall *` and returns `->pid`.

The address `USYSCALL` is `MAXVA - 5*PGSIZE`, not the more usual page directly
below the trapframe, because `usertests`' `lazy_copy` probes the top four pages
of the address space as addresses that must be inaccessible to user code; a
readable page among them would break that check.

**Marking.** `pgtbltest` gives four cases, and checks the **page** directly
rather than trusting `ugetpid()`: (1) the USYSCALL page, read straight from its
fixed address, holds this process's pid — proving it exists, is mapped and is
correct with no dependence on student code; (2) `ugetpid()` returns that pid;
(3) the page is per-process (a child reads its own pid there); (4) the page is
read-only (a user store to it faults). The read-only case is credited only if
case 1 passed — an unmapped page faults on a store too, so "the store faulted"
proves read-only only once the page is known mapped.

Note (1) and (4) do not go through `ugetpid()`: case 1 dereferences `USYSCALL`
itself, and if the page is unmapped that load faults and kills the program, so
the completion gate force-fails all of Part 2 — a `ugetpid(){ return getpid(); }`
fake over an unmapped page scores nothing. What the grader **cannot** see is
whether `ugetpid()` avoided the trap: a wrapper that calls `getpid()` is
indistinguishable from user space. That is the one hand-marked property of
Part 2 — read `ulib.c` and confirm `ugetpid()` reads the page, and does not call
`getpid()`. Case 2 only checks the returned value, not the mechanism.

### Part 3 — lazy allocation (`kernel/vm.c`, `vmfault()`)

`vmfault()` returns 0 for `va >= p->sz` (fatal); otherwise walks with `alloc=0`,
and if a page is already mapped, the only fault it resolves is a COW store
(Part 4) — everything else mapped (the guard page, a store to text) is fatal;
and if nothing is mapped, it kallocs a zeroed page and maps it `R|W|U`.

**Marking.** `lazytests` gives five cases: growth is lazy (measured by free-page
count, catches an `sbrk` that allocates eagerly), a freshly faulted page is
**zeroed** (a fresh page is read before it is written and must read back all
zero — catches a handler that maps the page but skips `memset`, returning
`kalloc`'s junk), a faulted-in page reads back correctly, a lazy page passed to
a system call is faulted in, and an out-of-range access stays fatal. A handler
that allocates out-of-range typically panics `freewalk: leaf` on the next exit
rather than failing the assertion — either way it does not pass.

### Part 4 — copy-on-write (`kernel/vm.c`, `kernel/kalloc.c`)

`kalloc.c` keeps `kref.count[]`, one `int` per physical page, indexed relative to
`KERNBASE`. `kalloc` sets a fresh page to 1; `kfree` decrements under `kref.lock`
and frees only at zero (and panics on underflow, which catches a double free);
`krefinc` adds a reference. `uvmcopy` maps the parent's page into the child, marks
a writable page `PTE_COW` and read-only in both, `krefinc`s it, and `sfence.vma`s
at the end. `vmfault`'s COW branch copies the page, maps it writable, `kfree`s the
old reference and `sfence.vma`s. `copyout` triggers that same branch in software
when it meets a `PTE_COW` page.

**Marking.** `cowtest` gives four cases: a COW write is private and not lost;
three-way sharing keeps the count right; a near-full process forks successfully
(impossible with eager copy); and — the important one — the free-page count
returns to baseline across fork/exit cycles. That last is the only test in the
lab that sees a reference count that is never decremented, and it is worth the
most attention when marking a solution that "looks right".

## The free-page check earns its keep

The leak case is deliberately built so a stuck reference count becomes lost free
pages the test can see: each cycle allocates a region, forks, has both sides
write it (so both take a copy), frees the region, and repeats. With correct
reference counting the free-page count returns exactly to its start; with a COW
copy that forgets to drop the shared page's reference, hundreds of pages go
missing over eight cycles, and no other case in `cowtest` — nor `usertests` on a
short run — notices. If you change the COW code and only this case fails, believe
it: you have a leak or a double free, not a functional bug.

## `usertests`

Run `tests/run.sh --full` once before considering the lab done. `usertests`
exercises paths the part tests do not — in particular a `read()` into a
just-forked buffer, which is the `copyout`-honours-COW case, and `stacktest`,
which is the stack-guard case. A kernel can pass all of Parts 1–4 and still fail
`usertests`; that gap is the point of running it.
