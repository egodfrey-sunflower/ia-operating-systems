# The Part 4 workload traces

Three traces, replayed by `tests/tracerun.c`. Each is a text file of one
operation per line:

```
a <id> <size>     allocate <size> bytes and remember them as <id>
f <id>            free whatever <id> refers to
# ...             a comment; blank lines are ignored too
```

`id` runs from 0 to 4095, and a trace never allocates an id that is already
live, so the driver's table of live pointers is a plain array. Every trace
frees a block long after it allocates it, with other operations in between —
which is the only reason any of this is interesting. An allocate/free pair with
nothing between them tells you nothing about fragmentation.

| Trace | Ops | Size classes | What it is for |
|---|---:|---|---|
| `trace-small.txt` | 13 089 | one: 16–192 bytes | The control — but only for **first fit against best fit**. Every request is about the size of every hole, so those two have almost nothing to decide between and land within a per cent or two of each other; if they diverge sharply here, look for a bug before you write it up. Worst fit is a different story on this trace and is *supposed* to be: see below. |
| `trace-mixed.txt` | 11 833 | three: 16–128, 256–1024, 4096–16384, with lifetimes to match | The workload the policies were invented for. Small blocks churn fast, big ones sit still, so the heap fills with holes of one size while requests arrive in another. |
| `trace-grow.txt` | 9 396 | three, with a live set that keeps growing | The classic way to strand free space: long-lived blocks are allocated throughout, so every hole ends up wedged between two blocks that never leave. |

**What "the control" does not control for.** A correct allocator on
`trace-small` posts roughly 1.8%, 1.1% and **58%** peak fragmentation for first,
best and worst — a thirty-fold gap, and not a bug. `trace-small` never holds
more than about 140 KiB live in a 1 MiB arena, so most of the arena is one
untouched tail; worst fit takes the largest block every time, which means it
chews that tail into small pieces from the first allocation onwards, while first
and best fit leave it alone. Worst fit being catastrophic on the trace with the
least to decide is one of the more interesting things in this data, so treat it
as a result to explain rather than a bug to hunt.

Peak live bytes are roughly 140 KiB, 400 KiB and 500 KiB respectively, against
`tracerun`'s default 1 MiB arena — loaded enough to fragment, not so loaded
that allocations start failing and the numbers turn into a story about
refusals. If you want to see refusals, pass a smaller arena as `tracerun`'s
third argument; 256 KiB on `trace-grow` is a good place to start, and it is
worth doing once.

Those peak-live figures are also the ceiling on how big an arena is worth
using. `tracerun` samples fragmentation only while at least an eighth of the
arena is in use — a nearly empty heap can post a spectacular percentage of
almost nothing — so with an arena of 4 MiB or more none of these traces ever
reaches the floor, and `peak_frag_pct` comes out as `n/a` for every run, with
a note on stderr explaining why.

The traces are fixed files, not generated per run, so your numbers are
reproducible and comparable with someone else's.
