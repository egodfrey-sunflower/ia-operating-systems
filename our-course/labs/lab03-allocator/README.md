# Lab 3 — Memory allocator

**Weeks 6–8 · 10 hours · OSTEP ch. 14 and ch. 17 · = App. G, "Memory-allocation Library"**

Userspace C on Linux, from scratch. No kernel work, no xv6, no QEMU, no root.
You will need `bash`, `gcc`, `make`, `nm`, and `valgrind` for one of the tests.

You are writing a `malloc` and a `free`. Not a wrapper around the C library's —
a replacement, which asks the kernel for a large region once and then does all
the bookkeeping itself, inside that region, using no storage anywhere else.
That last constraint is the lab: the metadata has to live in the memory being
managed, because there is nowhere else for it to go.

**Read OSTEP ch. 17 before your week-7 session.** Splitting, coalescing,
headers and fit policies are chapter 17, and Parts 2–4 build all four. Week 6's
half hour is an on-ramp: read ch. 14 and App. G, get the arena standing up, and
leave the free list until the reading has landed.

## Layout

```
lab03-allocator/
  README.md          this handout
  starter/           Makefile, mymalloc.h (the contract), mymalloc.c  <- work here
  fixtures/          the three Part 4 workload traces
  tests/run.sh       the autograder for Parts 1-5
  tests/tracerun.c   the Part 4 measurement driver; `make trace` builds it
  solutions/         SPOILERS. Reference allocator, model FITS.md, answer key. Later.
```

Copy the three working directories somewhere of your own and work there. Copy
all three, not just `starter/` — the autograder and the traces are referred to
as `../tests/…` and `../fixtures/…`, and those paths only resolve if the layout
comes with you:

```sh
cp -r starter tests fixtures ~/lab3
cd ~/lab3/starter
```

`solutions/` is deliberately left behind.

## What you hand in

| File | Part | Weight |
|---|---|---|
| `mymalloc.c` — the arena and the header | 1 | 25% |
| `mymalloc.c` — free list, first fit, splitting | 2 | 25% |
| `mymalloc.c` — coalescing | 3 | 15% |
| `FITS.md` — the Part 4 measurements and what they mean | 4 | 20% |
| `mymalloc.c` — the debug mode and the heap walker | 5 | 15% |

**What the harness marks, and what it does not.** Parts 1, 2, 3 and 5 are
machine-checked in full: `tests/run.sh` has a case for every behaviour those
parts require, and a green run means the code does what the handout says. Part 4
is machine-checked only in part — the harness replays all nine runs and requires
them to allocate without failures and to leave `mym_check_heap()` happy, checks
that the three policies genuinely behave differently, and checks that `FITS.md`
mentions all nine runs. It says nothing about whether the report is any good.

Four pieces of prose are marked by hand and by no test at all: **Part 1's two
questions**, **Part 2's sentence about your split threshold**, **Part 3's note
on how you find a block's physical neighbours**, and **the whole argument in
`FITS.md`**. Together they are a substantial share of the marks, and a full
`34 passed, 0 failed` is reachable having written none of them. Do not read a
green run as "the lab is finished". **The numbers and the argument in `FITS.md`
are marked by the rubric in `solutions/README.md`, which you read afterwards,
not before.** There is no single right answer to Part 4, and that is the point
of it.

---

# ⚠ The contract

`starter/mymalloc.h` is the specification. The harness includes *that header*
and links against *your* library, so the declarations in it cannot change. Read
it before you write anything. What follows is the part of it worth saying
twice.

**Alignment: every pointer `mymalloc` returns is a multiple of 16.** Not
"usually", not "when the size is a multiple of 16". x86-64 will let you get
away with 8 for a long time and then a `movaps` instruction inside `memcpy`
will fault, three weeks later, on a workload you did not write. The first test
in the harness checks alignment for fourteen awkward sizes.

**The header is 16 bytes and sits immediately before the payload.**

```
     +--------+--------+-----------------------------+
     |  size  | flags  |  payload: size bytes        |
     +--------+--------+-----------------------------+
     ^                 ^
     the header        what mymalloc() returns
```

`size` is the payload size in bytes, not counting the header, and is always a
multiple of 16. `flags` holds a magic number in its top 32 bits and the in-use
bit at the bottom. Both are given to you in the starter, along with the two
conversions between a header and its payload.

Sixteen is not arbitrary. `mmap` returns a page-aligned address; if every
header and every payload is a multiple of 16 bytes, then every payload lands on
a 16-byte boundary for free, and you never have to round a returned pointer.

**Blocks tile the arena exactly.** Every byte between the start and the end of
the arena belongs to exactly one block. No gaps, no padding between blocks, no
unclaimed tail. This is what makes it possible to find a block's physical
neighbour, and there is a test (`the stats account for every byte of the
arena`) that holds you to it.

**`mymalloc(0)` returns a real pointer.** Unique, aliasing nothing else live,
minimum-sized, and safe to pass to `myfree`. Returning `NULL` would be legal
for the C library but is a bad choice here, because then the caller cannot tell
it apart from out-of-memory.

**`myfree(NULL)` does nothing.** Not an error, not a diagnostic, not a
segfault, and it must work before the arena even exists.

**The fit policies are defined by behaviour, not by your data structure:**

| Policy | Chooses |
|---|---|
| `MYM_FIT_FIRST` | the **lowest-address** free block that fits |
| `MYM_FIT_BEST` | the **smallest** free block that fits |
| `MYM_FIT_WORST` | the **largest** free block, if it fits |

Best and worst break ties by taking the lowest-address candidate. And **when a
block is split, the allocation is taken from the low end** and the remainder
stays free at the high end. Those two rules are what let the tests check your
policies by looking only at the addresses you return, without knowing anything
about how you store your free list.

**No storage outside the arena.** No `malloc`, `calloc`, `realloc`, `free`,
`strdup` or `posix_memalign` anywhere in `mymalloc.c` — no static array of
block descriptors either. The free list lives in the free blocks. There is a
test that reads your object file's undefined symbols and fails if any of the
libc heap functions appear. That test sees the libc calls and nothing else:
**the static-array rule is on your honour**, because no test can see a static
array. It is still the rule, and it is the whole point of the lab.

**Two environment variables**, both already handled for you in the starter:
`MYM_ARENA_BYTES` sets the arena size (clamped to 64 KiB … 64 MiB, default
1 MiB) and the arena never grows — when it is full, `mymalloc` returns `NULL`.
`MYM_COALESCE=0` must switch coalescing off; Part 3 explains why.

**Thread safety is out of scope.** Explicitly, deliberately, and it is not an
oversight you are being invited to fix. Nothing in this lab is safe to call
from two threads at once and nothing needs to be. If you have read ahead to
ch. 26 and are reaching for a lock, don't: you would be adding a mechanism
whose cost you have no way to measure yet, and Lab 5 is where that gets done
properly.

**Cast to `char *` before doing pointer arithmetic.** `void *` arithmetic is a
GNU extension; it works, it is a byte at a time only by accident, and it hides
exactly the kind of unit confusion — bytes? blocks? headers? — that this lab is
made of.

---

# Part 1 — The arena and the header (~2.5 h, weeks 6–7, 25%)

Get a large region from the kernel and hand pieces of it out. That is all.

The `mmap` call is written for you in `arena_init()`, and so is `mym_reset()`,
which throws the arena away again — it is platform-specific, fiddly, and not
the lesson. Read it anyway; you should be able to say what each flag does.

One line of `arena_init()` is not scenery, and it is flagged in the starter:
`h->size = arena_len - MYM_HEADER_BYTES`, which lays the whole arena out as one
free block carrying one header. That is a statement about how much metadata a
block costs, so if you later choose a design that costs more — Part 3's
boundary tag is the obvious one — this line has to change with it.

What you write:

- **`mymalloc`**, in its crudest possible form. Round the request up to a
  multiple of 16, never below `MYM_MIN_PAYLOAD`, and check that the rounding
  did not wrap around. Then keep a pointer to the first unused byte of the
  arena, write a header there, and return the payload. **Nothing is reused
  yet.** A bump pointer is the right answer for Part 1 and you will delete it
  in Part 2.
- **`myfree`**, which finds the header in front of the pointer and clears the
  in-use bit. It does nothing else. The space is not reusable, and it should
  not be — that is Part 2's job, and doing it now means debugging two things at
  once.
- **`mym_get_stats`**, which walks the heap from the start of the arena and
  counts. The invariant it must satisfy is in `mymalloc.h`:
  `bytes_in_use + free_bytes + header_bytes == arena_bytes`, exactly, on every
  call. Get this right now: it is how you will debug everything after it, and
  the test that checks it is the one that catches a split which forgot to
  charge for the header it created.

**Answer these two before you move on** — in a comment at the top of your
`mymalloc.c`, three or four sentences each. They are marked.

1. **Why can `free(ptr)` take no size argument, when `malloc(size)` needs
   one?** The answer is a sentence about where the size went, and a sentence
   about what the library therefore has to do that the caller does not see.
2. **`mymalloc` asks the kernel for memory once, and then never again.** Why is
   that the right shape? What would it cost to ask the kernel for each
   allocation instead — name the specific mechanism, not "it would be slow" —
   and what does the library get in exchange for taking on all this
   bookkeeping?

At the end of Part 1 the expected score is **`9 passed, 23 failed, 2 skipped`**.
The nine are the seven `Part 1:` cases, plus two a bump allocator passes for
reasons that have nothing to do with Part 1: `the allocator uses no libc heap`,
which a file that never calls `malloc` satisfies trivially, and
`with MYM_COALESCE=0 the walker allows them`, which asks `mym_check_heap()` to
report *nothing* and so is satisfied by the stub that returns 0. The two skips
are cases the harness declines to judge until the case each of them controls
is passing. Do not go looking for what is broken.

---

# Part 2 — Free list, first fit, and splitting (~2.5 h, week 7, 25%)

Now make freed memory come back.

**The free list lives in the free blocks.** This is chapter 17's central trick
and the reason the lab exists. You have no memory outside the arena to build a
list in — so the link goes *inside the free block's own payload*. A free block
is not in use by definition, so its payload is yours to scribble in; the moment
it is allocated, the link is overwritten by the caller's data, which is fine,
because a block that is in use is not on the list. That is why
`MYM_MIN_PAYLOAD` is 16 and not 1: the payload has to be big enough to hold a
pointer.

Build:

- a singly linked free list threaded through the free blocks;
- **first fit**: search the list for a block big enough and take it. Remember
  that the harness defines first fit as *the lowest-address block that fits*.
  Keeping the list in ascending address order makes that fall out for free, and
  it will make Part 3 dramatically easier. You are allowed to do it another way
  and prove the same behaviour;
- **splitting**: when the block you found is much bigger than the request, cut
  the front off it for the allocation and leave the rest on the list. Split
  only when the remainder can be a block in its own right — a 16-byte header
  *plus* at least `MYM_MIN_PAYLOAD` of payload. A remainder smaller than that
  has nowhere to put its header and stays with the allocation as internal
  waste.

That threshold is a real design parameter, not a magic number. Name it. Raising
it means fewer, larger free blocks and more wasted bytes inside allocations;
lowering it means the opposite. Write one sentence in your source saying which
way you leaned and why — you will have data about it after Part 4.

Three of this part's tests are worth reading before you start.
`2000 rounds of churn through a 64 KiB arena` cycles sizes from 64 to 448 bytes
and back, a mean of 256, so it pushes 500 KiB of traffic through the arena —
which only fits if freed blocks come back. `one big free block
serves eight small requests` fills the arena, frees it all in one block, and
then asks for eight pieces — which only succeeds if that block gets cut up.
And `a remainder too small to be a block is not made one` sets up a 256-byte
hole and asks for 240: the 16 bytes over are exactly one header with no room
for a payload, and a "block" made out of them has a zero-length payload, so
writing a free-list link into it writes into the next block's header instead.

---

# Part 3 — Coalescing (~1.5 h, weeks 7–8, 15%)

Free the four blocks of a 4 KiB buffer one at a time and you have four 1 KiB
holes and no way to satisfy a 4 KiB request, even though the memory is sitting
there in one piece. That is external fragmentation, and merging on release is
the direct answer to it.

**The question to answer before you write any code: given a block, how do you
find the block that physically precedes and follows it in the arena?**

Its *free-list* neighbours are a different thing entirely, and merging two
list neighbours that are not physically adjacent will corrupt your heap in a
way that surfaces thousands of operations later, in a completely unrelated
allocation. There is more than one good answer:

- keep the free list in **ascending address order**, and the candidates for
  physical neighbour are then exactly the list entries either side of the
  insertion point — one walk finds both;
- or add a **boundary tag**: a copy of the size at the *end* of each block, so
  that the block before this one can be found by reading backwards.

Both are accepted, and the tests cannot tell which you chose. The boundary tag
costs bytes the address-ordered list does not, and nothing in the harness
begrudges you them: no case assumes that a request for *n* bytes consumes
exactly *n* + 16. Where a case needs the arena full it asks `mym_get_stats` for
`largest_free_block` and works down from there, and where one needs a fully
merged heap it asks for the arena minus a kilobyte of slack rather than for the
arena minus one header. What the harness does hold you to is the 16-byte header
immediately before the payload, and the stats invariant — whatever your
per-block overhead is, it has to appear in `bytes_in_use`, `free_bytes` or
`header_bytes`, because those three still have to add up to the arena.

**If you take the boundary tag, budget for the edit it forces in Part 1's
code.** `arena_init()` is otherwise written for you, but its
`h->size = arena_len - MYM_HEADER_BYTES` assumes one header of metadata per
block; with a tag on every block the opening block is
`arena_len - MYM_HEADER_BYTES - 16`, and the tag has to be written there too.
Miss it and the arena is over-committed by sixteen bytes from the first
allocation, which surfaces as the tiling test failing rather than as anything
that mentions Part 3. Every other place that creates a block — the split, the
merge — has to pay the tag as well.

Pick one, and write down in your source why — including what the other one
would have cost. That note is marked by hand; no test can see it.

Merge in **both directions**. A block freed between two free neighbours must
come out as one block, not two. The harness runs the same four-block pattern
three times, freeing low-to-high, high-to-low, and in an order that forces a
merge in both directions on the same call; a one-directional merge passes
exactly one of the three.

**And honour `MYM_COALESCE=0`.** With it set, `myfree` must not merge anything.
`coalesce_on` is already read for you in the starter. This is not busywork:

```sh
../tests/run.sh .                       # the merge cases pass
gcc -Wall -Wextra -Werror -std=gnu11 -g -I. -o cases ../tests/cases.c libmymalloc.a
MYM_COALESCE=0 ./cases p3_merge_up      # and now the same pattern fails
```

A test that passes whether or not the blocks merged is not evidence of
anything, so the harness runs the pattern both ways and requires it to fail
with coalescing off. Watch it fail once yourself. It is the clearest single
demonstration in this lab of a mechanism doing what it claims.

---

# Part 4 — Fit policies, compared (~2.0 h, week 8, 20%)

Add best fit and worst fit behind `mym_set_fit()`, then measure all three.

The definitions are in the contract above and in `mymalloc.h`, and they are
about *which block gets chosen*, not about how you look for it. First fit can
stop at the first match; best and worst cannot — they have to see the whole
list before they can know. That difference is most of what you are about to
measure.

## The measurement

`tests/tracerun.c` replays a workload trace against your allocator under one
policy and prints one line of numbers. Build it and run it nine times:

```sh
make trace                      # builds ./tracerun against your library
for t in small mixed grow; do
  for p in first best worst; do ./tracerun ../fixtures/trace-$t.txt $p; done
done
```

Each line reports, among other things:

| Field | Meaning |
|---|---|
| `peak_frag_pct` | **external fragmentation at its worst**: `100 × (free_bytes − largest_free_block) / free_bytes`. 0% is one contiguous hole; 90% means nine tenths of the free space is in pieces smaller than the biggest one. **Sampled only while at least an eighth of the arena is in use** — a nearly empty heap posts a spectacular percentage of almost nothing. If no sample ever qualified the column reads `n/a`, with a note on stderr saying so |
| `end_largest_free` | **largest satisfiable request** when the trace ends |
| `free_blocks_at_peak` | how many holes the heap was carrying at that moment |
| `ns_per_op` | **allocation latency**: the fastest of five unsampled replays (the timed pass switches the per-operation `mym_get_stats()` sampling off, not the clock), divided by the number of operations. The one noisy column — see the warning below |
| `heap=ok` | `mym_check_heap()` was called every 1024 operations and at the end of the replay, and reported nothing. `heap=BROKEN` means the rest of the line describes a heap your own walker calls inconsistent |
| `failed` | requests the arena could not satisfy |

**`ns_per_op` is noisier than it looks.** Every other column here reproduces to
the digit across repeated runs; this one varies by **roughly a factor of two**
across five sweeps on the machine this lab was built on — best fit on
`trace-small` ranged 523–1028 ns/op, worst fit on `trace-mixed` 866–1766. Report
it, but do not build an argument on a gap of tens of per cent, and if you want a
number you can defend, run each configuration several times and take the median.
The one thing that *was* stable across all five sweeps is the **ordering** of
the three policies on each trace, which is why Part 4's question is about
ordering and mechanism rather than about magnitudes.

**Measure after Part 5, or say that you did not.** Part 5 adds pointer
validation to `myfree`, which is on the hot path, so it moves `ns_per_op` —
which means a table measured now describes an allocator different from the one
you hand in. Either re-run the nine measurements once Part 5 is finished, or
state plainly in `FITS.md` that the numbers are for the Part-4 allocator without
the Part-5 validation. Both are acceptable; silently doing the first thing while
claiming the second is not. Nothing else in the table is affected — the
fragmentation and end-state columns are unchanged by Part 5.

`fixtures/README.md` says what each of the three traces is made of and which
one is the control.

## `FITS.md` — the deliverable

Write it yourself. It must contain:

1. **A table of all nine runs** — three policies × three traces — with peak
   fragmentation, largest satisfiable request at the end, and ns/op. Say what
   machine and what arena size.
2. **Which policy won on which trace, and by what mechanism.** Not "best fit
   was best". *Why* — in terms of what each policy does to the free list on
   each request. The `free_blocks_at_peak` and `end_largest_free` columns are
   where the mechanism shows itself; two policies can have nearly the same
   number of free bytes and a twentyfold difference in the largest one.
3. **The trade you actually measured.** Fragmentation and latency do not
   improve together here, and the size of the gap between them is the useful
   finding.
4. **One thing that surprised you, or contradicted what ch. 17 led you to
   expect.** If nothing did, say which result you had predicted and what the
   number was.

Then, and not before, read the Part 4 rubric in `solutions/README.md`.

> Sheet 7 §B2 traces free lists by hand and does the fragmentation arithmetic
> on paper. This is a different activity and the numbers will not match: there
> you were checking your understanding of the mechanism, here you are measuring
> a real implementation — **yours**, with your split threshold and your list
> order — on workloads big enough that no one could do them by hand. Do not
> re-derive sheet 7's numbers, and do not expect to reproduce them.

If you want to see the policies under real pressure, pass a smaller arena as
`tracerun`'s third argument. 256 KiB on `trace-grow` starts refusing
allocations, and it is worth doing once, because "how much fragmentation" and
"the program now fails" are different sentences about the same number.

Going the other way is a trap. These traces never hold more than about 500 KiB
live, so an arena of 4 MiB or more never reaches the eighth-full floor,
`peak_frag_pct` reads `n/a` for all nine runs, and there is nothing to explain.
If you state an arena size larger than the default, check that your table still
has numbers in it.

---

# Part 5 — Correctness under abuse (~1.5 h, week 8, 15%)

An allocator that trusts its caller is a security bug with a convenient
interface. Chapter 14's seven memory errors are all things the *caller* does;
this part is the same list from the allocator's side.

This is the largest single piece of code in the lab — two bounded walks, a plausibility check on every size read out of the
heap, the tiling arithmetic, a reconciliation of the free list against the
physical walk, and a second validation path inside `myfree`. Eleven cases are
built to defeat it, one per check. Budget accordingly; it is not a tidying-up
exercise at the end.

- **Validate every pointer `myfree` is given.** Is it inside the arena at all?
  Is there a real header 16 bytes in front of it — right alignment, right magic
  number, plausible size? Is that block actually in use? Each failure sets the
  matching `mym_err_t` in `last_error` and returns, **leaving the heap exactly
  as it was**. No aborting, no `exit`, and no partial modification before the
  check. Printing a diagnostic to stderr is allowed; the tests never read it.

  **Four corners the contract settles for you**, because they are exactly the
  ones worth an hour of guessing:

  | Situation | Answer |
  |---|---|
  | a non-NULL pointer passed before the arena exists | `MYM_ERR_NOT_OURS` — and `myfree` must not create the arena to find out |
  | a pointer inside the arena that is not 16-byte aligned | `MYM_ERR_NOT_A_BLOCK`. `NOT_OURS` means *outside the arena* and nothing else |
  | `mym_check_heap()` on a clean heap | returns 0 and leaves `last_error` **untouched** — it neither sets nor clears it. Only `mym_clear_error()` clears it — and `mym_reset()`, which throws the whole heap away and discards everything with it |
  | a pointer that is **both** invalid and already free | `MYM_ERR_NOT_A_BLOCK`. The checks go bounds, then alignment and header validity, then the in-use bit, so a pointer that is not a block never reaches the question of whether it is free |

  The third matters because an error the caller has not read yet has to survive
  a heap check: otherwise a rejected `myfree` followed by a routine
  `mym_check_heap()` loses the diagnosis. The fourth is the check *order* made
  visible: the double-free case frees `a`, then frees `a + 8`, which is
  misaligned and sits inside a block that now reads as free. Both answers are
  defensible in the abstract and the contract picks one, so there is a case
  that fails you for picking the other. All four are tested.

- **The magic number is what makes an interior pointer detectable.** Free
  `p + 32` and the 16 bytes in front of it are the caller's own data, not a
  header. Without something recognisable to look for, you have no way to tell.
- **Write `mym_check_heap()`**, a walk of the whole heap that checks it against
  itself: every header carries the magic, sizes are non-zero multiples of 16,
  the blocks tile the arena exactly, no two free blocks are physically adjacent
  (when coalescing is on — that would be a missed merge), and the free list has
  exactly as many entries as the physical walk found free blocks, all inside
  the arena, all actually free.

  **It runs on heaps that are already damaged.** That is its whole purpose. So
  validate before you dereference, and put a hard bound on the walk: a checker
  that loops for ever on a corrupt heap is worse than no checker, because the
  harness reports it as a timeout and you have no idea why.

  Note that the last of those is two checks, not one, and both are tested. A
  block left on the free list after being handed out breaks "all actually
  free"; a free block *dropped* from the list breaks only the count, and
  dropping a link is the cheaper mistake to make and the harder one to notice.

  Every one of those checks is tested, and each is tested by a heap that only
  that check can see through. Four cases build one deliberately damaged heap
  each — a free list the physical walk contradicts, a free block that has
  fallen off the list, two physically adjacent free blocks, and a size field
  that makes the physical walk a cycle — and a fifth builds the one heap where
  adjacent free blocks are *legal*, because it runs with `MYM_COALESCE=0`.
  Those four, with the overflow case, are the only places in the harness that
  write to a header directly; they use nothing about your design but the header
  layout the contract fixes.

The adversarial cases are: a double free, followed by a free of a misaligned
pointer *into the block that was just freed* — invalid and already free at the
same time, which is what settles the order of the checks; freeing an interior
pointer; freeing
a misaligned interior pointer; **freeing an interior pointer whose preceding
sixteen bytes are a plausible header** — a caller's own length and flag word,
which is a legal size with the in-use bit set and nothing out of range about
it; freeing a stack address and a static buffer; and two 16-byte overflows off
the end of one block, straight into the header of the next. The first overflow
writes `0xff`, which any sanity check on the size field will catch. The second
writes a header that is entirely plausible — a legal size, the in-use bit set,
the arena still tiled, the free list still correct — and the only thing wrong
with it is that it is not a header. Those two plausible-looking cases are the
ones that need the magic number, on the `myfree` path and on the walker path
respectively, and they are the reason there is a magic number. Then 200 000
randomised operations with every live block's contents verified every time it
is touched, followed by a heap check.

The last of those runs a second time under `valgrind`, which must report
nothing. It runs at a tenth of the operation count there, because valgrind is
roughly 30× slower and this is a memory-safety check rather than a second
stress run. If the case fails on its own assertions under valgrind, the
harness says so and skips the memory-safety verdict rather than blaming
valgrind for it. An allocator that passes every functional test while reading
uninitialised memory or walking off the end of the arena is the normal failure
mode here, not an exotic one.

---

# Running the tests

```sh
make                              # must be warning-free: -Wall -Wextra -Werror
../tests/run.sh .                 # or: make test
```

`run.sh` compiles `mymalloc.c` **itself**, with its own `gcc` command line and
the spec's own `CFLAGS`, into its own temporary directory. Your Makefile is not
the graded build and cannot become one: `override CFLAGS`, a `-w`, an `-O2`, a
recipe that builds some other file — none of it reaches the library the cases
are linked against. Your Makefile is still required to work, and is run
separately for that reason, because `make` and `make test` are the documented
workflow. Then run.sh compiles the case driver and the trace driver against
your header and that library, and runs each case in its own process, under its
own timeout. It prints a PASS/FAIL table, an `N passed, M failed` summary, and
exits non-zero if anything failed.

A case can also come out `SKIP`. That is the harness declining to give a
verdict rather than giving a wrong one: the `MYM_COALESCE=0` control is skipped
while the merge case it controls is failing, and the valgrind case is skipped
if the stress run fails on its own assertions. Fix the case named in the skip
message and the skipped one starts meaning something.

**The harness does not inspect your free list.** Every assertion is made
through the published API: the addresses you return, the contents of blocks,
`mym_get_stats()`, `mym_check_heap()` and `mym_last_error()`. Address-ordered
list or boundary tags, one free list or ten, a boundary tag or none — the tests
cannot tell and do not care.

There are two things it does look at directly, and both are things the contract
already fixes. The libc-heap check reads your object file's undefined symbols
and nothing else. And the four Part 5 cases that build a damaged heap write to
block *headers* — sixteen bytes immediately before a payload, the size in the
first word and the flags in the second. There is no way to test a heap walker
without damaging a heap, and the header is the only part of your design the
harness is entitled to know about.

To run a single case while you are debugging it, build the driver by hand:

```sh
gcc -Wall -Wextra -Werror -std=gnu11 -g -I. -o cases ../tests/cases.c libmymalloc.a
./cases p3_merge_both            # names are in tests/run.sh
```

`tests/cases.c` is worth reading. Each case says in its comments what it is
trying to catch.

---

# Stretch goals

Unweighted. Do them if the five parts came easily.

- **Segregated free lists.** Keep separate lists for a few small size classes
  and a general list for everything else. Then re-run Part 4's traces and see
  what happened to `ns_per_op` — this is the direct answer to the latency cost
  you measured, and you now have the before-numbers to compare against.
- **A buddy allocator as an alternative back end**, and a comparison of its
  *internal* fragmentation with your fit policies' external fragmentation.
  Sheet 7 does buddy arithmetic on paper; here you would be measuring your own,
  which is a different and more surprising experience.
- **`myrealloc`.** Deceptively small: growing in place when the physical
  successor is free is a few lines, and getting it right for shrink, for grow
  with no room, and for `myrealloc(NULL, n)` is where the corner cases live.
- Run the upstream simulator, `ostep-homework/vm-freespace/malloc.py`, on a
  policy you already have numbers for, and see whether its idealised model
  agrees with your implementation. Where it does not, the difference is your
  split threshold and your header overhead — both of which it does not model.

---

# If you get stuck

- **`make test` says it cannot find the autograder**: you copied only
  `starter/`. Copy `tests/` and `fixtures/` alongside it, or pass
  `make test TESTS=/path/to/lab03-allocator/tests/run.sh`. `make trace` has the
  same escape hatch, spelled `TESTDIR=`.
- **The build fails with `mym_header_size_check`**: you changed the header
  struct and it is no longer 16 bytes. The alignment guarantee is built on that
  size; add fields at your peril, and if you must, keep the total a multiple of
  16.
- **`the stats account for every byte of the arena` fails**: something is
  losing bytes, and the failure message prints all four totals so you can see
  which. Look at every place your code brings a new header into existence, and
  ask whether the byte it costs was accounted for.
- **Alignment fails for some sizes but not others**: think about what you
  rounded — the request, or the pointer you hand back — and whether every header
  is a whole number of alignment units. When both hold, alignment maintains
  itself.
- **`could not fill the rest of the arena`, or `could not fill the tail of the
  arena`**: several cases need the arena completely full before the interesting
  part starts, and they get there by asking `mym_get_stats` for
  `largest_free_block` and requesting a little less until one request fits. If
  no request fits, some free space is unreachable — usually a split that leaves
  a remainder the allocator can never hand out again, or a `largest_free_block`
  that reports more than the block can actually serve.
- **You added a boundary tag and now the tiling test fails from the very first
  allocation**: `arena_init()` still lays the arena out as one block costing one
  header (`h->size = arena_len - MYM_HEADER_BYTES`). With a tag on every block
  that opening block is sixteen bytes too big, so the arena is over-committed
  before you allocate anything. It is the one line of `arena_init()` a footer
  design has to edit, and the starter flags it.
- **`the walker returns on a heap whose walk is a cycle` reports `TIMED OUT`**:
  your `mym_check_heap` has no bound and no sanity check on the size it reads,
  so it walked the same two blocks for ever. Bound the number of steps *and*
  reject a size the arena could not contain.
- **A case reports `TIMED OUT`**: a list walk that never ends. Either a free
  block points to itself, or you inserted a block into the list twice, or a
  coalesce merged a block into itself. Bound every walk you write, including
  the one inside `mym_check_heap`.
- **A case dies of `signal 11`**: you followed a free-list link out of a block
  that has since been allocated and overwritten — the link and the caller's
  data occupy the same bytes. The block was still on the list when it should
  not have been.
- **`p3_merge_up` passes but `p3_merge_down` does not**, or the other way
  round: you are merging in one direction only. Both are needed, and the
  "both ways" case needs them on the same call.
- **The merge cases pass but `with MYM_COALESCE=0 the same pattern fails`
  does not**: you are ignoring the environment variable. Read `coalesce_on` in
  `myfree` — it is set for you in `arena_init`.
- **`best fit takes the smallest block that fits` fails but the other two
  policies pass**: your search returns as soon as it finds *a* fit. Best and
  worst have to see the whole list.
- **Everything passes but `valgrind finds nothing` fails**: usually reading a
  free block's link field after that block was handed out, or a stats walk that
  runs one block past the end of the arena. Valgrind prints the line.
- **The stress test corrupts a block that was live the whole time**: two
  allocations are overlapping. Print the address and size of every block as it
  is handed out, run with a 64 KiB arena so it happens sooner, and look for the
  pair whose ranges intersect.
