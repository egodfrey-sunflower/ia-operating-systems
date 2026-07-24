# FITS.md — Part 4, the reference allocator's numbers

**Spoiler.** This is the model report. Produce your own before reading it: the
point of Part 4 is to measure *your* allocator, and yours will not have these
numbers unless it makes exactly the same design choices.

Measured with the reference `mymalloc.c` on a 1 MiB arena (`tracerun`'s
default), gcc 13.3, x86-64, two cores — **the complete allocator, Part 5's
`myfree` validation included**. That matters for the `ns_per_op` column only:
validation sits on the free path, so a table measured before Part 5 describes a
different allocator. Say which yours is. Command:

```sh
for t in small mixed grow; do
  for p in first best worst; do ./tracerun ../fixtures/trace-$t.txt $p; done
done
```

`peak_frag_pct` is external fragmentation at its worst point in the trace:
`100 × (free_bytes − largest_free_block) / free_bytes`, sampled only while at
least an eighth of the arena is in use. `end_largest_free` is the largest
single request the heap could still satisfy when the trace ended —
`tracerun`'s stand-in for "largest satisfiable request". `ns_per_op` is the
fastest of five unsampled replays — the timed pass switches off the
per-operation `mym_get_stats()` sampling, not the clock — divided by the number
of operations.

**A note on which of these numbers is solid.** Every column but the last is
deterministic: the nine runs were repeated five times and the fragmentation,
block-count and end-state figures came back identical to the digit each time.
`ns_per_op` did not. It spans roughly a factor of two between invocations of
the same command, so the figures below are the **median of five runs**, quoted
to two significant figures, and no claim in this report rests on a gap smaller
than about a factor of two. What did hold still across the five sweeps is the
*ordering* of the three policies on each trace, which is what the argument
below actually uses.

## The table

| Trace | Policy | Peak frag % | Free blocks at peak | Largest free at end | Free blocks at end | ns/op |
|---|---|---:|---:|---:|---:|---:|
| trace-small | first | 1.8 | 206 | 876 960 | 168 | 480 |
| trace-small | best | **1.1** | 89 | **882 672** | 89 | 530 |
| trace-small | worst | 58.0 | 381 | 385 648 | 407 | 3000 |
| trace-mixed | first | 18.5 | 64 | 545 920 | 66 | **160** |
| trace-mixed | best | **16.6** | 41 | **555 152** | 37 | 350 |
| trace-mixed | worst | 97.4 | 99 | 27 632 | 99 | 900 |
| trace-grow | first | 7.8 | 61 | 506 272 | 56 | **130** |
| trace-grow | best | **6.5** | 34 | **520 032** | 42 | 400 |
| trace-grow | worst | 98.4 | 161 | 11 504 | 165 | 1420 |

No policy failed an allocation on any trace: the arena is big enough that
fragmentation shows up as scattered free space rather than as refusals.

## What the numbers say

**Worst fit is a disaster on every trace, and the mechanism is visible in the
table.** It takes the largest block every time and splits it, so it converts
one big hole into two medium ones, over and over, until the heap is nothing but
medium holes. `end_largest_free` is the tell: on `trace-grow`, first fit ends
with 506 KiB available in one piece and worst fit with 11 KiB. Both have almost
exactly the same number of bytes free. Worst fit simply cannot hand any of them
out in one piece. The theory that motivates worst fit — leave a usable
remainder rather than an unusable sliver — turns out to describe a single
allocation, not a sequence of them.

**Best fit wins on fragmentation, and by less than its reputation suggests.**
16.6% against first fit's 18.5% on `trace-mixed`; 6.5 against 7.8 on
`trace-grow`. What it does clearly win is the block count: 41 free blocks at
peak against 64, 34 against 61. That is the actual mechanism — best fit leaves
the smallest possible remainder, so remainders below the split threshold are
absorbed into the allocation instead of becoming another entry on the list, and
the list stays short.

**First fit wins on latency, and the margin is the whole argument for it.**
160 ns/op against best fit's 350 on `trace-mixed`. First fit stops at the first
block that fits; on an address-ordered list, most requests are served near the
front. Best fit and worst fit have no early exit — both must walk the entire
free list on every single allocation, because the block they want could be the
last one. The gap widens exactly as the free list lengthens, which is why worst
fit is slowest of all: it both causes long lists and pays for them.

`trace-small` is the counter-example that stops any of this becoming a rule.
All sizes there are between 16 and 192 bytes, so almost every hole fits almost
every request, and first fit and best fit are within a whisker of each other on
fragmentation: 1.8% against 1.1%, on a heap where nothing is ever more than 2%
scattered either way.

Latency there needs stating carefully. First fit is faster on `trace-small`
too — 480 against 530 ns/op — but that is about 10%, and 10% is inside what
this measurement can support: five invocations of the *same* command spanned
458–494 for first fit and 508–607 for best fit. The honest reading is not "the
latency edge disappears on `trace-small`"; it is that the edge **shrinks from a
factor of two-plus to something this instrument cannot separate cleanly**, and
that the shrinking is itself the finding, because it tracks the free list.
On `trace-mixed` and `trace-grow` best fit costs 2.2× and 3× first fit; on
`trace-small` the two are close. The mechanism is the same one in both
directions: first fit's advantage is the walk it does *not* do, and with 206
free blocks of nearly interchangeable sizes it ends up walking a good part of
the list anyway, so there is less advantage left to have.

Worst fit is the one number here that is not close: 3000 ns/op, six times first
fit, and that gap is far outside the noise.

On this workload the choice between first and best barely matters, and any
effort spent choosing between those two is wasted.

## What I would take away

The ordering is not a property of the policies. It is a property of the
policies **and** the workload, and the second half is the one you do not get to
choose. Across these three traces:

- when requests are all one size, the choice between first and best fit does
  not matter — though worst fit still manages to be six times slower and
  thirty times more fragmented;
- when requests have several size classes with different lifetimes, best fit
  buys you a modestly better-packed heap for roughly twice the allocation cost;
- worst fit is bad everywhere here — but "everywhere here" is three synthetic
  traces, and the honest claim is about these traces, not about all workloads.

The measurement that surprised me was `end_largest_free`. Peak fragmentation
percentages of 97% and 18% sound like a difference of degree. "The biggest
thing this heap can still give you is 27 KiB" versus "half a megabyte", with
the same number of bytes free in both, is the same fact stated in a way that
would actually break a program.
