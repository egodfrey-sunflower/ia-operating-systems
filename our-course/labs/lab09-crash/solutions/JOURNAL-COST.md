# JOURNAL-COST.md — model answer (Part 5)

## Measured

```
done ops=4 mode=data    installs=46 barriers=16
done ops=4 mode=ordered installs=39 barriers=20
```

Both campaigns: `bad=0` (46 and 39 points).

## Reconciliation with the analytical count

Per create with d data blocks and 3 metadata blocks (inode block, bitmap
block, root directory block):

- **data journaling**: n = 3+d blocks are each written twice (journal +
  checkpoint), plus commit and clear: 2(3+d) + 2 = 8 + 2d installs.
- **ordered**: data written once in place, metadata journaled: d + (2·3+2)
  = 8 + d installs.

Sheet 21 §B2(c)'s per-operation figures are the same formulas; summing over
d = 1, 2, 1, 3 gives 46 and 39. The measured saving is exactly the 7 data
blocks written once instead of twice — data journaling's cost scales with
the *data* volume (double-write bandwidth), ordered's only with the metadata.
For this metadata-heavy toy workload the gap is 15%; for a streaming write
(d large) data journaling approaches 2× the bandwidth, which is why ordered
is ext4's default.

The barrier count moves the other way: 16 vs 20, because ordered needs a
fifth barrier per transaction (data durable *before* the journal is
written). On a real disk a barrier is a queue drain — lost concurrency and a
forced platter wait — so ordered trades bandwidth for one extra
serialisation point per transaction.

## What ordered mode gives up

Nothing that this workload can see: every data block here is freshly
allocated, written before the transaction that makes it reachable commits,
so a crash exposes either the complete file or no file.

The guarantee lost appears on **overwrite and reuse**:

- *Overwrite in place*: ordered mode writes new data over old data before
  the commit. Crash between the data write and the commit and the file is
  reachable with **half-new, half-old contents** — metadata consistent,
  data torn. Data journaling replays the whole update or none of it: data
  writes are atomic per transaction.
- *Block reuse* (delete f1, allocate its block to f2's data): the ordering
  discipline must also ensure the delete's metadata cannot be replayed
  after f2's data lands in the block (ch. 42's revoke records exist exactly
  for this).

So the honest statement: ordered journaling keeps *metadata* crash
consistency and file-create atomicity, but downgrades data atomicity to
"new data is durable before it becomes reachable" — overwrites can tear. A
campaign able to show the difference needs an overwrite workload and a
content oracle that knows both generations; ours has neither, by
construction, which is why Part 5's comparison is written analysis rather
than a sweep.
