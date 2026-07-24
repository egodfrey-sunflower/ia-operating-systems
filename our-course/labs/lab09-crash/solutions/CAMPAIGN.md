# CAMPAIGN.md — model answer (Part 4)

## Model assumption

The commit record is one block, and the model treats a single-block write as
atomic: install it or don't, never half of it. Without that anchor the scheme
is circular — you would need a commit record for the commit record. Real
journals buy the same property with a checksum over the commit block
(ext4's `journal_checksum`); the atomic-sector assumption is the classical
form and the one xv6 makes.

## The three clean sweeps

```
sweep mode=data dev=fifo    misorder=none points=46 ok=46 bad=0
sweep mode=data dev=reorder misorder=none points=46 ok=46 bad=0
recovery sweep: workload crash point i*=16, recovery installs=6
sweep mode=recovery points=6 ok=6 bad=0
```

46 points because the workload is four transactions of n = 4, 5, 4, 6 blocks
at 2n+2 installs each. Every point recovers to an image that is xfsck-clean,
holds an exact prefix f1..fm, and has m ≥ every transaction committed before
the crash. The reorder sweep is the same 46 points against a device that
installs each barrier group last-in-first-out — it passes only because all
three ordering rules are enforced by explicit barriers. The recovery sweep
crashes *recovery* at each of its 6 installs (5 replays + the clear for the
n=5 transaction pending at i*=16) and re-runs it; idempotence means the
second pass finishes what the first started, whatever the interleaving.

## The deliberate violation

`misorder=commit-first` issues the commit record before the log blocks, with
no barrier between. The sweep finds:

```
point i=1 ... BAD structure: FAIL root: inode 1 must be an allocated directory (type is 0)
point i=2 ... BAD structure: (same)
point i=3 ... BAD structure: FAIL block-free-but-used: block 13 ...
point i=4 ... BAD atomicity: f1 block 0 has wrong contents
point i=5 onwards ... ok
```

**The first corrupting point is i=1**, and it is derivable: in this mode
install 1 is the commit record itself. The header now says "transaction 1,
n=4, destinations 10, 12, 13, 14" — while slots 3..6 still hold the zeroes
mkfs left. Recovery believes the header (it must; that is what a commit
record *is*), replays four blocks of zeroes, and thereby destroys the inode
block, the bitmap and the root directory it was supposed to protect. It is
recovery, not the crash, that does the damage — armed by a commit record
that lied about the log being durable.

By i=5 all four slots happen to be durable too, so replay is harmless — the
scheme is back to "usually fine". Rule 1 (log durable before commit) is
therefore not a detail of the protocol; it is the entire difference between
a recovery that restores the transaction and one that shreds the file
system, and only the barrier between the slot writes and the header write
enforces it.
