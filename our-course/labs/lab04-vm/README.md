# Lab 4 — xv6 virtual memory & copy-on-write fork

**Weeks 8–11 · 12 hours · OSTEP ch. 18–23 · xv6 rev5 ch. 3 (Page tables) and ch. 5 (Page faults)**

This lab instruments and then extends xv6's virtual memory. You will print the
real three-level Sv39 page table as a tree, map a shared read-only page into
every process, make `sbrk` hand out address space without physical memory until
a page is touched, and finally make `fork` copy a page table and not a byte of
user data. Everything runs on xv6-riscv under QEMU with the cross-compiler from
Lab 0. No root.

The destination is copy-on-write, because it is what makes the `fork`/`exec`
pair you built a shell on in Lab 1 affordable: every page `fork` avoids copying
is a page ch. 23 says VAX/VMS taught everyone to avoid copying. The three
earlier parts each build a piece you need to get there — a way to see a page
table, the mapping-as-sharing idea, and the page-fault handler.

## Layout

```
lab04-vm/
  README.md          this handout
  setup.sh           makes you a fresh xv6 tree to work in   <- run this first
  starter/overlay/   the files setup.sh lays over the pinned xv6
  tests/run.sh       the autograder: builds your tree, boots it, scrapes the console
  solutions/         SPOILERS. Reference kernel + the answer key. Afterwards.
```

The pinned xv6 checkout at `../xv6-riscv/` is never modified. `setup.sh` copies
it and lays `starter/overlay/` on top; the copy is what you edit, so starting
again from a clean kernel is one command:

```sh
./setup.sh ~/lab4          # makes ~/lab4, refuses if it exists (-f to replace)
cd ~/lab4
make qemu                  # boots; Ctrl-A x quits. Never make -j.
```

The tree builds and boots as it stands. Out of the box `vmprint()` prints
nothing, `ugetpid()` fails, a lazily-grown page faults fatally, and `fork` still
copies every page. Those are Parts 1–4.

A note the whole lab depends on: when you change a page-table entry that the
hardware may already have cached in the TLB — turning a page writable, say —
you must execute `sfence.vma` (the kernel wrapper is `sfence_vma()`), or the old
translation lingers and your change takes effect *sometimes*. A stale-TLB bug
reproduces once in twenty runs and is the worst afternoon in this lab. When
something works under GDB but not at speed, suspect a missing `sfence.vma`
first.

## Part 1 — Print a page table (~2.5 h, 21%)

Write `vmprint(pagetable_t)` in `kernel/vm.c`. It walks all three levels of an
Sv39 page table and prints every valid entry, indented by level, and then it is
called once for the first user process so you can read `/init`'s page table off
the console at boot.

A page table is itself made of pages: the root is one page of 512 entries, each
valid entry with none of R/W/X set points at another page of 512 entries, and
so on for three levels; only at the third level does an entry point at a page of
actual data. Sheets 8–9 walked this by hand — this is deliberately the same
walk, in code.

### The exact output format

Print, in this order and spacing:

```
page table 0xADDR
 ..IDX: pte 0xPTE pa 0xPA FLAGS
 .. ..IDX: pte 0xPTE pa 0xPA FLAGS
 .. .. ..IDX: pte 0xPTE pa 0xPA FLAGS
```

- A header line `page table ` followed by the page table's own address, printed
  with `%p` (which gives `0x` and 16 hex digits).
- One line per **valid** entry (skip entries without `PTE_V`). The line begins
  with one ` ..` group for a top-level (root) entry, two for a middle-level
  entry, three for a leaf, then `IDX: pte 0xPTE pa 0xPA FLAGS`, where `IDX` is
  the entry's index within its page (0–511), `PTE` is the raw entry (`%p`), `PA`
  is the physical address it points at (`PTE2PA`, `%p`), and `FLAGS` is five
  characters: `r`/`w`/`x`/`u`/`c` for `PTE_R`, `PTE_W`, `PTE_X`, `PTE_U`,
  `PTE_COW`, each shown as its letter if set and `-` if not.
- After printing a middle-level or root entry, recurse into the page it points
  at. Stop recursing at the leaves — an entry with any of R/W/X set is a leaf.

For `/init` the tree is deterministic. Its shape — the indices and flags below —
is the same on every correct build; only the physical addresses move, because
they depend on how big your kernel is.

```
page table 0x....
 ..0:   ...  -----          root -> a middle table
 .. ..0:   ...  -----        middle -> a leaf table
 .. .. ..0:   ...  r-xu-      /init's text  (read, execute, user)
 .. .. ..1:   ...  rw-u-      /init's data
 .. .. ..2:   ...  rw---      the stack guard page (mapped, but NOT user: no u)
 .. .. ..3:   ...  rw-u-      the user stack
 ..255:   ...  -----          root -> a middle table
 .. ..511:   ...  -----        middle -> a leaf table
 .. .. ..507: ...  r--u-      USYSCALL   (Part 2's page: readable, not writable)
 .. .. ..510: ...  rw---      the trapframe (not user)
 .. .. ..511: ...  r-x--      the trampoline (not user)
```

Leaf 507 (USYSCALL) is mapped by Part 2, not Part 1, so it is not there until you
do Part 2. Part 1's autograder grades only the walk it is responsible for and
**ignores whatever appears — or does not — at leaf 507**, so doing Part 1 before
Part 2 does not fail Part 1 for a leaf that is not yet mapped. Every other entry
above is checked exactly.

Capture your output into **`PGTBL-NOTES.md`** and annotate it: which line is the
text page and how you can tell from its flags, why the guard page and trapframe
have no `u`, why the two subtrees hang off root indices 0 and 255, and where the
"a page table is itself pages" idea shows up in the addresses. That file is
marked by hand; the autograder only checks the tree's shape.

## Part 2 — A per-process read-only page (~2.0 h, 17%)

Map one page into every process at the fixed virtual address `USYSCALL` (defined
in `kernel/memlayout.h`), fill it in the kernel with that process's pid, and
read the pid from user space with no system call.

- In `kernel/proc.c`, `proc_pagetable()` builds each process's page table. Map
  a fresh page there at `USYSCALL`, and store the process's pid into it (as a
  `struct usyscall`, also in `memlayout.h`). The permissions are the point of
  the exercise: user code must be able to **read** the page but not **write** it,
  so a process cannot forge its pid. `p->pid` is already set by the time
  `proc_pagetable()` runs.
- Free that page in `proc_freepagetable()`. It sits above `p->sz`, so the normal
  teardown does not reach it.
- Implement `ugetpid()` in `user/ulib.c` to read the pid straight out of the
  page at `USYSCALL` and return it. No `getpid()` — the whole point is to avoid
  the trap into the kernel.

This is mapping as the unit of sharing, and page permissions as what makes
sharing safe. Real systems publish `gettimeofday` and the like exactly this way.

`pgtbltest` machine-checks the **page**, not your wrapper: it reads the USYSCALL
address directly and asserts it holds this process's pid (so the page exists, is
mapped, and is right), that a child reads its own pid there, and that a user
store to it faults (so it is read-only). Reading the pid through `ugetpid()`
*instead of* calling `getpid()` is the point of the exercise, but user space
cannot tell whether a trap happened — so that part is **marked by hand** against
your `ulib.c`. Writing `ugetpid()` as `return getpid();` will be caught by eye,
not by the grader; do not do it.

## Part 3 — Lazy heap allocation (~3.0 h, 25%)

Make a growing `sbrk` hand out address space with no physical memory behind it,
and allocate a page only when the process first touches one.

The plumbing is already in place: `sbrklazy()` (the lazy form of `sbrk`, which
`user/lazytests.c` uses) grows `p->sz` without allocating; a page fault on such
a page traps into `usertrap()`, which calls `vmfault()`; and `copyin()` /
`copyout()` call `vmfault()` when the *kernel* is the first to touch a
not-yet-present page. **Your job is the body of `vmfault()` in `kernel/vm.c`.**

The contract `vmfault(pagetable, va, read)` must honour — `read` is 1 for a load
fault, 0 for a store fault — is:

- **A fault inside the process's region, on a page with no physical page yet:**
  allocate a zeroed page, map it, and return its physical address. This is the
  lazy allocation itself. (`walk()` with `alloc=1` versus `alloc=0` matters
  here: think about which one a fault handler wants.) The page must be **zeroed**
  — `lazytests` reads a fresh page before writing it and fails if any byte is
  not zero; returning `kalloc`'s junk without `memset`-ing it is caught here, not
  just in the note below.
- **A fault outside any valid region** — at or above `p->sz`, or below address
  zero — is **fatal**: return 0 and the caller kills the process. A handler that
  allocates here instead will, sooner or later, panic in `freewalk` when the
  process exits, because it left a mapping the teardown does not expect.
- **A fault on the stack guard page** is fatal too. The guard page is mapped but
  has `PTE_U` cleared; a correct handler sees it is already mapped and returns 0.
- **A page passed to a system call before it was ever touched** must still work:
  when the kernel reads or writes it inside `copyin`/`copyout`, `vmfault` faults
  it in exactly as a direct access would. You get this for free once the first
  case is right, because `copyin`/`copyout` already call `vmfault`.

How the pieces interact: `sbrk` (lazy) only moves `p->sz`; `fork` copies
`p->sz` and the page table, and a page that was never touched has no PTE to copy,
so the child inherits the hole, not a page; `exec` throws the whole address
space away; and a `read()` into a fresh lazy buffer faults each page in as the
kernel writes it. `uvmunmap` and `uvmcopy` already tolerate a valid region with
holes in it — you do not need to touch them.

The fault is not an error; it is the mechanism — the software half of address
translation. That is ch. 21's point.

## Part 4 — Copy-on-write fork (~4.5 h, 37%)

Make `fork` share the parent's physical pages with the child instead of copying
them, and copy a page only when someone writes it.

Read ch. 23 first and plan the reference-count scheme on paper (that is week
10's ~1.0 hour); the code lands in week 11. Four pieces, in `kernel/vm.c`,
`kernel/kalloc.c` and — for the fault — the `vmfault()` you wrote in Part 3:

- **`uvmcopy()`** (called by `fork`): instead of copying each page, map the same
  physical page into the child. A page that was writable is made **read-only and
  marked `PTE_COW`** (bit defined in `kernel/vm.h`) in **both** page tables, so
  the first write from either side faults. A read-only page (the text) is shared
  as-is. Every shared page gains a reference (see below).
- **Reference counts** in `kalloc.c`: the array and its size are given. Maintain
  a count per physical page. `kalloc` sets a fresh page's count to 1; `kfree`
  **decrements** and only actually frees at zero; `krefinc()` adds a reference.
  Guard every access with the given lock.
- **`vmfault()` on a store to a `PTE_COW` page**: allocate a private page, copy
  the data in, map it writable in place of the shared one, and **drop this page
  table's reference** to the shared page.
- **`copyout()`**: when the kernel writes into a user page that is read-only
  because it is `PTE_COW`, it must trigger the copy in software — the hardware
  fault a user store would take does not happen for a store the kernel makes.
  `usertests` exercises exactly this.

### The invariants that must hold

- A physical page is freed **exactly once**, when its last reference goes. A
  decrement you skip leaks the page; one you do twice frees a page still in use.
  The leak is invisible to every functional test — only the free-page count
  around fork/exit cycles sees it, which is why the tests check it.
- After `fork`, a page shared read-write between parent and child has `PTE_W`
  clear and `PTE_COW` set in both, and a reference count of (at least) 2.
- After a COW fault, the faulting side has its own page, writable, `PTE_COW`
  clear, count 1; the other side still maps the original, with one fewer
  reference.
- Reads never copy. A page shared by three processes (fork, then fork again
  before any write) has a count of 3 and stays shared until someone writes.

`cowtest` cannot pass without this: it fills most of memory in a parent, forks,
and runs both — impossible if `fork` copied eagerly, because two copies would
not fit. And the free-page count returns to where it started once the children
exit.

## Running the tests

```sh
cd lab04-vm
tests/run.sh --fast ~/lab4     # Parts 1-4, skips usertests   (iterate with this)
tests/run.sh ~/lab4            # Parts 1-4, then usertests -q  (a few minutes)
tests/run.sh --full ~/lab4     # Parts 1-4, then all usertests (before you hand in)
```

`run.sh` builds your tree from clean, checks the page table `vmprint` printed at
boot, then runs `pgtbltest`, `lazytests` and `cowtest`, and finally xv6's own
`usertests` — the regression gate that catches a VM bug leaving the common path
working while corrupting an edge case. Iterate with `--fast`; run `--full` once
before you hand in. Each phase boots its own emulator and tears it down; the
harness keeps exactly one QEMU alive at a time.

The three test programs (`user/pgtbltest.c`, `user/lazytests.c`,
`user/cowtest.c`) are given and the grader reads their output line by line;
`run.sh` checks they are unmodified. If you break one while poking at it, restore
it with `setup.sh` or by copying it back.

## Stretch goals

- Confirm the reference-count arithmetic holds for a page shared by more than
  two processes: fork a child, fork it again before any write, and check all
  three get private copies on their first write with no leak. (`cowtest`'s
  three-way case already exercises this; try four.)
- Add demand-zero pages for the BSS and measure the drop in physical pages a
  process holds at start.

## When you are stuck

- **`make qemu` prints `panic: freewalk: leaf`.** Your fault handler left a
  mapping the teardown did not expect: it survives until the process exits, then
  trips `freewalk`. Look at which faults your handler decides to back with a
  page and which it leaves fatal.
- **`panic: kfree` or `kfree: refcount underflow`.** A physical page's reference
  count is not tracking how many page tables map it — it reached the free list
  with a live mapping, or was pushed below zero. Revisit where counts are raised
  and dropped, against the invariants above.
- **COW works under GDB but corrupts data at speed.** A page-table change is
  taking effect only sometimes — the classic sign of a translation the hardware
  cached going stale. Memory ordering and the TLB are what to think about here
  (see the note at the top of this handout, and xv6 ch. 3); it is a nasty
  ~1-in-20 non-deterministic bug, so suspect it early rather than distrusting
  your COW logic.
- **`usertests` fails but the part tests pass.** A COW page corrupted by a
  *kernel* write is the usual culprit: a `read()` into a just-forked buffer has
  the kernel store into the page, and a kernel store takes no hardware fault.
  Look again at what `copyout` does with a page it finds read-only.
- **`vmprint` indentation looks right but the grader rejects it.** Count the
  ` ..` groups: a root entry has one, a leaf has three. An off-by-one there is
  an off-by-one in your level bookkeeping.
- **A lazy page read back as zeros.** Expected — a freshly faulted page must be
  zeroed. If it reads back as junk (the `5`s `kalloc` fills with), the page
  reached the process still holding what `kalloc` left in it — look at what your
  handler does to a page between allocating it and mapping it.
