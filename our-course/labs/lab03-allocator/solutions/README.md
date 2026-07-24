# Lab 3 — Reference allocator and answer key

```
╔═══════════════════════════════════════════════════════════════════╗
║                           ⚠  SPOILERS  ⚠                          ║
║                                                                   ║
║  The reference mymalloc.c, the answers to the Part 1 questions,   ║
║  the Part 3 design discussion, and the rubric for Part 4 — which  ║
║  is 20% of the lab, is self-marked, and has no single right       ║
║  answer. Reading the rubric before you have your own numbers      ║
║  turns the one open-ended part of the lab into a fill-in form.    ║
║                                                                   ║
║  FITS.md in this directory is the model report. Same warning,     ║
║  twice as strongly: it contains the conclusions.                  ║
╚═══════════════════════════════════════════════════════════════════╝
```

Every model answer below is inside a collapsed `<details>` block, so you can
check them one at a time.

```sh
make            # -Wall -Wextra -Werror -std=gnu11 -g, zero warnings
make test       # ../tests/run.sh on this directory: 34 passed, 0 failed
```

The reference is 409 lines as shipped, of which about 270 are code and the rest
comment. **The starter skeleton scores `1 passed, 31 failed, 2 skipped` of 34**
— the one case it passes is `the allocator uses no libc heap`, which a file
that does nothing trivially satisfies. The two skips are the `MYM_COALESCE=0`
control (skipped while the merge case it controls is failing) and the valgrind
run (skipped while the stress case fails on its own assertions). Every other
case in the harness is falsifiable by an allocator that does nothing, and every
one of them fails.

**A correct Part 1 and nothing else scores `9 passed, 23 failed, 2 skipped`**,
not seven: the seven `Part 1:` cases, plus the libc-heap case, plus
`with MYM_COALESCE=0 the walker allows them`, which asks the walker to report
*nothing* and so is satisfied by the stub that returns 0. The handout says the
same number in the same words, so a student who counts is not left wondering.

## The reference design in one paragraph

One arena, mapped once with `mmap`. Every block is a 16-byte header
`{size, flags}` followed by its payload, and the blocks tile the arena exactly,
so a block's physical successor is at `header + 16 + size`. Free blocks are
additionally threaded onto a **singly linked list in ascending address order**,
whose links live in the first 8 bytes of each free block's own payload. Address
order is the whole trick: the list neighbours of a freed block are the only
candidates for being its physical neighbours, so the single walk that finds the
insertion point also finds both merge candidates. `find_block` scans that list
once and implements all three fit policies from it — first fit returns early,
best and worst run to the end.

Boundary tags are the natural alternative and are fully accepted: a copy of the
size at the end of each block lets you step backwards, which buys you O(1)
coalescing at the cost of more bytes per block and a second copy of the size to
keep consistent. The harness cannot tell the two designs apart, and it was
written that way on purpose — which costs something to maintain. No case
assumes a request for *n* bytes consumes exactly *n* + 16: `alloc_largest()`
and `fill_arena()` in `tests/cases.c` exist so that every case that needs the
arena full gets there by asking `mym_get_stats` rather than by arithmetic on
`MYM_HEADER_BYTES`, and `p3_collapse` asks for the arena minus a kilobyte of
slack rather than for the arena minus one header.

**How that claim is checked.** By building an independent boundary-tag
allocator — 16-byte header, 16-byte footer on every block, an unordered LIFO
free list, backwards coalescing off the footer — and running the whole suite
against it: **34/34**. That is the only sound way to check it. Simulating a
footer's cost by padding every request inside the reference does *not* test the
claim, however identical the arithmetic looks: padding lands inside the
payload, so a fully coalesced arena still reports
`largest_free_block == arena − 16` — which is exactly the number a real footer
does not produce. Anything changed in this area has to be re-checked against an
implementation that really carries the extra metadata.

One thing a footer design does have to change in code the handout otherwise
hands over: `arena_init()`'s `h->size = arena_len - MYM_HEADER_BYTES`, which
assumes one header of metadata in the opening block. The handout says so in
Part 1 and again in Part 3, and the starter carries the same warning at the
line itself.

---

## Part 1 — The arena and the header (25%)

**Marking note.** Seven cases. Give yourself the marks if all seven
`Part 1:` cases pass *and* you wrote both comments. They cover: alignment
across fourteen awkward sizes plus every size from 1 to 64; non-overlap of 64
painted blocks; contents surviving 40 later allocations; `mymalloc(0)` being
unique, freeable **and minimum-sized** (the case measures how much payload it
charged the arena, so an allocator that hands out a kilobyte for a zero-byte
request fails); `myfree(NULL)` before the arena exists and after; the stats
invariant at four different points; and an impossible request returning `NULL`
without breaking anything — together with the other end of the same rule,
`MYM_ARENA_BYTES=1024` still having to produce an arena of at least 64 KiB.

If `the stats account for every byte of the arena` is the one that failed, the
bug is nearly always in Part 2's split rather than in Part 1 — see the marking
note there.

<details>
<summary><b>Why <code>free(ptr)</code> needs no size argument</b></summary>

Because the size never left. `malloc` was told it, and it wrote it down —
in the header immediately before the pointer it returned. `free` recovers it
by subtracting the header size from the pointer it is given and reading the
size field back out. The information the caller supplied at allocation time is
still there; the library kept it, in the one place it is certain to be able to
find from the pointer alone.

What that costs, and what the caller does not see: **every allocation carries
its metadata with it**. A 1-byte request occupies 32 bytes here — 16 of header
and a 16-byte minimum payload — and there is no way to avoid it while `free`
takes one argument. The header is also *adjacent to caller-writable memory*,
which is why a buffer overflow in application code turns into heap corruption
rather than merely bad data: the sixteen bytes past the end of a block are not
padding, they are the next block's bookkeeping. That is the trade the C
interface makes, and it is a deliberate one — the alternative interface, where
the caller passes the size back (as `kfree(ptr, size)` does in some kernels,
and as Rust's `dealloc` does), removes the per-block header but makes every
caller responsible for a fact it must not get wrong.

**Marking.** Full marks for (a) naming the header as where the size lives and
(b) naming a cost of the header — the per-block overhead, or its adjacency to
caller memory. Half marks for (a) alone.
</details>

<details>
<summary><b>Why the library asks the kernel once</b></summary>

Because asking the kernel is a **system call**, and a system call is not a
function call: it is a trap into the kernel, a privilege-level switch, a
register save, a walk through the kernel's own VM data structures to create or
extend a mapping, and a return — hundreds of nanoseconds at best, against a few
tens for a walk of a free list. Chapter 6's point about limited direct
execution is exactly this: the calls that cross the boundary are the expensive
ones, and a library that crosses it on every allocation would be paying that
cost thousands of times a second for no benefit.

There is a second cost that matters as much. The kernel manages memory in
**pages**. A `mmap` per allocation would round every request up to 4 KiB and
would need a separate VMA for each one, so a program making a million 24-byte
allocations would ask the kernel for 4 GiB and give it a million mappings to
track. The kernel's granularity is simply the wrong granularity for a program's
allocation pattern.

What the library gets in exchange for all this bookkeeping is the ability to
work at *its* granularity: sixteen bytes, in user space, with no privilege
transition, reusing memory the kernel still believes is in use. Two levels of
memory management, each at the scale that suits it — the kernel retails pages
to processes, the allocator retails bytes to the program. That is chapter 14's
whole argument, and it is why `malloc` is a library and not a system call.

**Marking.** Full marks for naming the system-call boundary (not just "slow")
*and* either the page-granularity mismatch or the per-mapping kernel
bookkeeping. Half marks for the boundary alone.
</details>

---

## Part 2 — Free list, first fit, splitting (25%)

**Marking note.** Five cases: reuse, 2000 rounds of churn (500 KiB of traffic
through a 64 KiB arena — sizes cycle 64…448, mean 256), the eight-way split,
the too-small remainder, and the libc-heap symbol check. Full marks needs all
five *and* a named split threshold with a sentence justifying it.

<details>
<summary><b>Where the link goes, and why it can be there</b></summary>

In the first 8 bytes of the free block's own payload:

```c
static hdr_t *fl_next(hdr_t *h)               { return *(hdr_t **)payload_of(h); }
static void   fl_set_next(hdr_t *h, hdr_t *n) { *(hdr_t **)payload_of(h) = n; }
```

It is safe because a free block's payload is, by definition, not in use by
anybody. It is *correctly aligned* storage for a pointer because the payload is
16-byte aligned. And it costs nothing, because those bytes were sitting there
anyway — which is the difference between this and a separate array of block
descriptors, which would need memory outside the arena and would therefore need
an allocator of its own.

The moment a block is allocated the link is overwritten by the caller's data.
That is fine, and it is also the source of the single nastiest bug in this lab:
if a block is left on the free list *after* being handed out, the caller's
first 8 bytes are a free-list pointer, and the list now leads into whatever the
caller happened to write. It usually crashes hundreds of operations later, in
an unrelated allocation. The stress case exists to shorten that distance.

`MYM_MIN_PAYLOAD` is 16 rather than 8 for this reason and one other: 8 would be
enough for the link, but a 16-byte minimum keeps every size in the system a
multiple of the alignment, which is what makes alignment self-maintaining.
</details>

<details>
<summary><b>The split threshold</b></summary>

```c
if (b->size >= need + MYM_HEADER_BYTES + MYM_MIN_PAYLOAD)
```

Split only when the remainder can be a block in its own right: a header **plus**
a minimum payload. The common bug is `need + MYM_MIN_PAYLOAD`, which creates a
remainder with nowhere to put its header — in practice a block whose size field
is 0 or whose header overlaps the payload in front of it, and the heap is
corrupt from that moment on. `p2_tiny_remainder` builds exactly that situation
(a 256-byte hole, a 240-byte request) and requires that no free block comes out
of it. Without that case the bug is caught only by the Part 5 stress run, three
parts later and with no clue as to where it came from.

The threshold is a real parameter. Raise it to, say, `need + 256` and you get
fewer, larger free blocks, a shorter list, faster allocation, and more bytes
wasted inside allocations that did not need them — internal fragmentation
traded for external. Part 4's `free_blocks_at_peak` column is where that trade
would show up if you measured it, and doing so is a reasonable stretch goal.

**Marking.** The threshold has to account for the remainder's own header. Any
justified value above that floor is correct.
</details>

---

## Part 3 — Coalescing (15%)

**Marking note.** Five cases: three merge orders, the collapse-to-one-block
case, and the `MYM_COALESCE=0` control. All five are needed. If exactly one of
the three merge orders fails you are merging in one direction only, and the key
below says which is which.

The control is **skipped**, not passed, while `p3_merge_up` is failing with
coalescing *on*. It has to be: `p3_merge_up` fails with "did not merge" for any
allocator that cannot serve the 4096-byte request, a bump allocator included,
so running it with `MYM_COALESCE=0` against a broken allocator would award a
mark for a mechanism that is not there.

<details>
<summary><b>Finding the physical neighbour</b></summary>

The **forward** neighbour is easy in any design, because the blocks tile the
arena: it is at `(char *)h + MYM_HEADER_BYTES + h->size`, and it exists as long
as that address is below the end of the arena. Nothing else is needed.

The **backward** neighbour is the real question, because a header is at the
*front* of its block and nothing in it points backwards. Two good answers:

1. **An address-ordered free list.** Walk the list to find where the freed
   block belongs; the entry before the insertion point is the nearest free
   block below it. If that block's forward neighbour is the block being freed,
   they are physically adjacent and merge. This is what the reference does. It
   costs an O(n) walk on every free — which is a real cost, and it is one of
   the reasons the reference is slower under best fit, where the list is
   already being walked twice.

2. **Boundary tags** (Knuth's). Write a copy of the size at the *end* of every
   block as well as at the front. The backward neighbour's footer is then the
   16 bytes immediately before this block's header, so its size, and therefore
   its header, can be read directly: O(1), no list walk at all. It costs 16
   more bytes per block — 8 would do for the size itself, but the payload must
   stay 16-byte aligned, so the footer has to be a multiple of `MYM_ALIGN` like
   everything else; an 8-byte tag would put the next block's payload on an
   8-byte boundary and `p1_align` fails on the spot. It also costs a second
   copy of the size to keep in step — and if the two copies ever disagree, that
   is itself a corruption check worth making.

The reference chose the address-ordered list because it needs the address order
anyway to make first fit mean "lowest address that fits", so coalescing came
free with a decision already taken. In an allocator where allocation latency
mattered more than this one's does, boundary tags are the better trade.

**The ordering trap.** Merge forwards *then* backwards. Do it the other way and
a block freed between two free neighbours merges into its predecessor, and then
the forward merge is being applied to a block that no longer exists. The
`merges both ways` case is the one that catches it.

**Marking.** Full marks for a working implementation plus a written
justification that names the alternative and one thing it would have cost.
Either design gets full marks; neither gets full marks without the comparison.
</details>

<details>
<summary><b>Which merge direction each case catches</b></summary>

| Case | Free order | Fails if you merge... |
|---|---|---|
| `p3_merge_up` | 0, 1, 2, 3 (low to high) | **forwards only** — each block's successor is still in use when it is freed, so nothing ever merges |
| `p3_merge_down` | 3, 2, 1, 0 (high to low) | **backwards only** — each block's predecessor is still in use, same reason |
| `p3_merge_both` | 1, 3, 0, 2 | either — the last free has a free block on *both* sides and must absorb both |

A one-directional implementation passes exactly one of the first two and fails
the third, which is why all three are in the harness. One merge case would have
shipped a half-working allocator.

`p3_collapse` allocates 100 blocks, fills the rest of the arena so the hundred
holes are the only free space there is, and frees them in a shuffled order. It
catches a class of bug the four-block pattern cannot: a merge that works for
adjacent pairs but loses a block from the list when three or more runs join.

Its verdict is **not** the statistics. `free_blocks == 1` and
`largest_free_block >= arena - 1024` are asserted, but only as a sanity rail: an
allocator that never merges and reports whatever it likes can satisfy them. The
load-bearing assertions are the two allocations, and neither consults the stats
at all. After the hundred frees it requires `mymalloc(23040)` to succeed **and
to come back at the address of the lowest of the hundred blocks**; after
everything is freed it requires `mymalloc(arena - 1024)` to succeed **and to
come back at that same address**, which is where the first allocation out of a
fresh arena landed. The address half is what makes them evidence. Without it,
an allocator that quietly `mmap`s a side region when its free list cannot serve
a request passes the case while never merging anything: the two requests are
answered, the stats describe the arena, and nothing ever asked where the
pointer came from. With first fit and the "split from the low end" rule, a
genuinely coalesced arena can only answer from the bottom, and no borrowed
memory can produce that address.
</details>

---

## Part 4 — Fit policies, compared (20%)

**This part is marked by rubric, not by the numbers.** Your allocator's numbers
will differ from the reference's, and should — a different split threshold or a
different list order moves them all. `FITS.md` in this directory is the model
report; the rubric is what marks yours.

<details>
<summary><b>The rubric</b></summary>

The three fit cases lay out five candidate holes of `{512, 128, 1024, 128,
1024}`, not three. The duplicated sizes are there so that best fit and worst
fit each face a genuine tie, and the rule in `mymalloc.h` — ties go to the
lowest-address candidate — is one the cases can fail you on. With three
distinct sizes no tie ever arises and the rule is decoration.

| | Marks | What it takes |
|---|---|---|
| **Completeness** | 6 | All nine runs — three policies × three traces — in one table, with peak fragmentation, largest satisfiable request at the end, and ns/op. The machine and arena size stated. Missing runs cost 1 each; a table with no arena size stated cannot be compared with anyone else's and loses 1. |
| **Mechanism** | 6 | Each result explained in terms of *what the policy does to the free list on each request*, not restated. "Worst fit fragmented most" is the observation; "worst fit splits the largest block every time, so it converts one big hole into two medium ones repeatedly, and `end_largest_free` collapses while `free_bytes` barely moves" is the mechanism. Full marks needs the mechanism for at least two of the three policies. |
| **The trade** | 4 | Fragmentation and latency do not move together. The report must name the trade explicitly and quote both numbers: first fit's early exit against best fit's full-list scan, with the measured ns/op gap. A report that only discusses fragmentation loses all four. |
| **Honesty** | 4 | One surprise, or a prediction stated and then checked against the data. Also: does the report's conclusion actually follow from its own table? A confident general claim ("best fit is the right default") drawn from three synthetic traces is worth fewer marks than the same claim scoped to the evidence. Claiming a result the table does not show is the only way to score 0 here. |

**What a full-marks report notices.** Any of these, and there are others:

- `trace-small` is the control, and on it the policies barely differ. A report
  that draws its general conclusion without noticing that one of its three
  workloads disagrees has over-generalised from two data points.
- `end_largest_free` and `free_bytes` are nearly independent. Two policies can
  end with the same amount of free memory and a twentyfold difference in the
  largest piece of it — and it is the largest piece that decides whether the
  next big request succeeds.
- Best fit's win is in the *block count* more than in the byte count: leaving
  the smallest possible remainder means remainders below the split threshold
  get absorbed instead of becoming list entries.
- Worst fit is slowest as well as most fragmented, and the two are the same
  fact: it lengthens the free list, and it has to walk all of it.
- Latency measurement on a shared machine is noisy. `tracerun` reports the
  fastest of five replays for that reason, and it is *still* noisy: five sweeps
  of the reference span roughly a **factor of two** on the same command
  (best/`trace-small` 523–1028 ns/op, worst/`trace-mixed` 866–1766). A report
  that treats a 5% ns/op difference as a finding has over-read its data, and so
  has one that explains a 10% difference with a mechanism — the bar is a factor
  of two. What is stable is the *ordering* of the policies on each trace, and an
  argument built on ordering is safe. The fragmentation columns, by contrast,
  are deterministic and reproduce to the digit — say which of your numbers is
  which.
- **Which allocator the table describes.** Part 5's validation sits on the
  `myfree` path and moves `ns_per_op`, so a table measured at the end of Part 4
  is not a table of the submitted allocator. Full credit either way, provided
  the report says which: re-measured after Part 5, or explicitly labelled as
  Part-4 numbers. A report that is silent about it has left the reader unable to
  reproduce anything.

**The textbook ordering is not a property of the policies.** OSTEP's discussion
of best/worst/first is about mechanisms, and the ordering it might leave you
expecting depends entirely on the workload. Discovering that on your own data
is the point of the part; a report that ends up less certain than it started is
doing well.
</details>

---

## Part 5 — Correctness under abuse (15%)

**Marking note.** Eleven cases: double free (which also settles the order of
`myfree`'s checks); the interior-pointer case (three interior frees, the third
with a plausible forged header in front of it); the foreign pointer; the
overflow the walker catches; four heaps built to exercise one walker check
each; the `MYM_COALESCE=0` heap where adjacent free blocks are legal; the
200 000-operation stress run; and the valgrind clean run.
All eleven. 7 + 5 + 5 + 6 + 11 = 34 cases in the suite.

**The four corners the contract settles**, all of them inside existing cases
rather than in cases of their own: a non-NULL free before the arena exists is
`MYM_ERR_NOT_OURS` and must not create an arena; a misaligned pointer *inside*
the arena is `MYM_ERR_NOT_A_BLOCK`, not `NOT_OURS`; `mym_check_heap()` on a
clean heap returns 0 and leaves `last_error` exactly as it found it; and a
pointer that is *both* invalid and already free reports `NOT_A_BLOCK`, because
the validity tests come first and it never reaches the in-use bit. The first
two are in the foreign-pointer and interior-pointer cases, the third in the
foreign-pointer case, and the fourth in `p5_double_free`, which frees `a + 8`
after `a` has already been freed: misaligned, and inside a block that now reads
as free. The reference gets all four from the ordering of the tests in `myfree`
— bounds, then alignment and header validity, then the in-use bit — and from
the walker only ever *setting* `last_error`. Move the in-use test up in front of
the validity tests and `p5_double_free` is the case that fails.

<details>
<summary><b>What each check is really doing</b></summary>

**The magic number is the whole mechanism.** Without something recognisable in
the header there is no way to distinguish "a header" from "16 bytes of the
caller's data that happen to be in front of this pointer". With it, freeing
`p + 32` is caught immediately, because the 16 bytes at `p + 16` are painted
caller data and the magic does not match. The check costs one mask and one
comparison per `free`, which is why real allocators can afford to do it.

Painted caller data is the *easy* version, and on its own it does not prove
anything: `0x6c6b6a6968676665` is not a multiple of 16 and not smaller than the
arena, so a plausible-size check alone rejects it and the magic never has to
work. `p5_interior` therefore ends with the version that matters. It writes
`{64, 1}` — a caller's own length and flag word — over the front of the block
and frees the pointer sixteen bytes in. That is a legal size, in range, with
the in-use bit set; every check except the magic passes it. Delete `hdr_ok()`
from `myfree`'s validation and this is the only case in the suite that fails,
and the allocator it lets through silently frees a block that does not exist.

**Check before you touch anything.** Every one of the abuse cases asserts that
the heap is *unchanged* afterwards — `mym_check_heap() == 0` and, in the
interior-pointer case, that the block's contents were not disturbed. An
implementation that clears the in-use bit and *then* validates has already
corrupted the heap by the time it notices.

**Order the checks from cheapest and safest to most dereferencing.** Outside
the arena → not our pointer, and we have not touched memory we do not own.
Misaligned → cannot be a header. Bad magic → not a block. Not in use → double
free. Reversing that order means dereferencing a pointer you have not yet
established points at anything.

**The walker runs on damaged heaps.** That is its only job. It must validate
each header before following it and bound the total number of steps — the
reference uses `arena_len / (MYM_HEADER_BYTES + MYM_MIN_PAYLOAD) + 1`, which no
correct heap can reach. It also stops the physical walk at the first bad header
rather than carrying on: past a smashed header there is no reason to believe
any address it computes.

**The free-list checks are the ones that catch the subtle bugs.** "No two
physically adjacent free blocks" is a missed merge, detected without knowing
anything about how merging was implemented. The reconciliation with the free
list is really two checks and needs both: *every entry the list holds is a
block the physical walk agrees is free* catches a block left on the list after
being allocated (the crash described in Part 2), and *the list holds exactly as
many entries as the walk found free blocks* catches a free block dropped from
the list — a leak, and the bug a real allocator is likeliest to commit, since
losing a link costs nothing at the time and nothing ever notices. Each has its
own case, because each is invisible to the other: delete the count comparison
and `p5_walker_leak` is the only case in the suite that fails.

**How the five walker cases isolate the five checks.** Each builds a heap that
is clean by every measure except one, using nothing but the header layout the
contract fixes:

| Case | The damage | The only check that sees it |
|---|---|---|
| `p5_walker_list` | the in-use bit is set on a block that is on the free list | the per-entry half of the free-list check: an entry the physical walk says is in use. Every header is valid, the arena is still tiled, and no two free blocks are adjacent |
| `p5_walker_leak` | the in-use bit is *cleared* on a live block whose two physical neighbours are both live, while a different block is genuinely freed | the count half of the free-list check, and nothing else. Every header is valid, the arena is tiled, no two free blocks are adjacent, and the one list entry is a real free block — the walk finds two free blocks and the list holds one |
| `p5_walker_adjacent` | a live block's in-use bit is cleared *and* its predecessor's size field is grown to swallow a free block, so the free-block counts still agree | the adjacency check. Every header is valid, the arena is still tiled, the list is still ascending and genuinely free, and the counts match — the one thing wrong is two free blocks side by side |
| `p5_walker_coalesce_off` | nothing; two adjacent free blocks, made legitimately with `MYM_COALESCE=0` | nobody — the walker must return **0**. This is the case that holds you to the "when coalescing is on" qualifier |
| `p5_walker_bound` | one size field is set so the physical successor is an *earlier* block | the bound, or the refusal of a size larger than the arena. Both headers carry the magic, so a magic-only walker loops for ever and the harness reports a timeout |

That second row is the fiddly one, and it is fiddly for a reason: every simpler
way of forging two adjacent free blocks also desynchronises the free list, and
then the count check catches it and the adjacency check is never needed.
Swallowing the free block into its predecessor's size field is what puts the
counts back in step.

**Why valgrind is a separate case.** An allocator handing out memory from its
own `mmap` region is largely invisible to valgrind's heap checking — valgrind
does not know your blocks exist. What it does catch is the allocator's own
misbehaviour: a read of an uninitialised link field, a walk that runs past the
end of the mapping, a jump on an uninitialised value. Those are exactly the
bugs that pass every functional test and fail on someone else's machine.
</details>

---

## Reading the harness

`tests/cases.c` has one function per case and a dispatch table at the bottom;
each case's comment says what it is trying to catch. Nothing in it reaches into
the allocator's free list — every assertion goes through `mymalloc.h`. That
constraint is why, for example, the fit-policy cases work by comparing the
address returned against the addresses of five known free blocks, rather than
by inspecting a free list they have no right to know the shape of. Two helpers,
`alloc_largest()` and `fill_arena()`, exist so that no case has to assume a
block costs exactly `MYM_HEADER_BYTES` more than its payload.

The four `p5_walker_*` cases are the exception, and a deliberate one: a heap
walker cannot be tested without a damaged heap, and there is no way to damage a
heap through `mymalloc.h`. They write to block *headers* — the sixteen bytes
the contract fixes, size then flags — and to nothing else.

Four cases are worth understanding before you write your own:

- **`Part 3: with MYM_COALESCE=0 the same pattern fails`** is a test that
  requires a *failure*. Its purpose is to prove that the merge cases are
  measuring coalescing and not something else about the allocator. Any test
  that passes both with and without the mechanism it claims to check is not
  evidence of anything.
- **`Part 4: the policies really do behave differently`** compares the numbers
  from all three policies on `trace-mixed` and fails if they are identical.
  Three policies producing one set of numbers are one policy with three names,
  and Part 4's entire exercise would be vacuous.
- **`Part 5: with MYM_COALESCE=0 the walker allows them`** is the mirror of
  `p5_walker_adjacent`: the same heap shape, and the walker must report
  *nothing*. A rule stated with a condition on it needs a case on each side of
  the condition, or the condition is not being tested.
- **`Part 5: the walker returns on a heap whose walk is a cycle`** is the only
  case whose real assertion is that it finishes. It prints a line to stderr
  before the call for that reason, so a `TIMED OUT` verdict explains itself
  instead of looking like a harness fault.

### What the harness marks and what you mark

Parts 1, 2, 3 and 5 are machine-checked in full: every behaviour those parts
require has a case, verified by mutation testing — injecting each bug and
confirming a test fires. Part 4's code is machine-checked (the three policies must
behave differently); Part 4's report is marked entirely by the rubric above.

Four pieces of prose have no test at all behind them and are marked by hand:
Part 1's two questions, Part 2's split-threshold sentence, Part 3's note on how
the design finds a block's physical neighbours, and `FITS.md`'s argument. A
submission of `34 passed, 0 failed` with a one-line `FITS.md` and no prose
anywhere is a real possibility, and it is not a pass — say so when marking your
own work.

### What is still on your honour

The graded build is `run.sh`'s own `gcc` line on `mymalloc.c`, so a `Makefile`
cannot change it; the libc-heap check reads the object file `run.sh` compiled,
so a `Makefile` that builds some other source cannot hide a `malloc` from it
either. What no test can see is the **static-array** prohibition — a file that
keeps its block descriptors in a `static` array outside the arena passes
everything. The handout says so in as many words, and it is the one rule in the
contract with nothing behind it but the point of the exercise.
