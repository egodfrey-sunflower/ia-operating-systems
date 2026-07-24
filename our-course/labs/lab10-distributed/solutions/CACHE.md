# CACHE.md — what the cache buys, and what it costs (model report)

## The reduction

Workload: `open f`, one 16-byte write, then 40 reads of the same range,
via `cmd` mode. `stats` at the end:

| ac | rpcs | reads | hits | revals |
|---:|---:|---:|---:|---:|
| 0 (off)  | 42 | 40 | 0  | 0 |
| 5000 ms  | 4  | 40 | 39 | 0 |

With the cache off, every read is a message: 1 lookup + 1 write + 40
reads = 42 RPCs. With a 5 s attribute timeout the same 40 reads cost 1
lookup + 1 write + 1 getattr + 1 fetch = 4 RPCs — the first read pays
for attributes and data, the other 39 are hits at zero messages. That is
a 10× reduction on this workload, and it scales with re-read rate: the
cache converts server load into client memory, which is ch. 49's entire
motivation for the attribute cache.

Longer windows buy more only if reads keep arriving: with ac=1000 and
the same 40 back-to-back reads the numbers are identical (they all fit
in one window); spread the reads over 10 s and ac=1000 pays ~9 extra
revalidations (one getattr each) where ac=5000 pays ~1.

## The staleness window, reproduced

Two clients, one file `g` holding `OLD-OLD-OLD-OLD.` (16 bytes),
A running with ac=2500:

| t (approx) | actor | action | A sees |
|---|---|---|---|
| 0.0 s | A | read 0 16 (miss: getattr+fetch) | `OLD-OLD-OLD-OLD.` |
| 0.2 s | B | write 0 `NEW-NEW-NEW-NEW.` (same length) | — |
| 0.4 s | A | read 0 16 | **`OLD-OLD-OLD-OLD.`** — stale, served from cache |
| 3.5 s | A | read 0 16 (window expired: revalidate) | `NEW-NEW-NEW-NEW.` |

The overwrite deliberately keeps the size at 16 bytes: revalidation must
compare mtime, not size, or the staleness never ends. The stale read at
0.4 s is not a bug — it is the purchase price of the 10× above, and it
is *bounded*: by 2.5 s after the attributes were fetched, A must ask
again.

## The guarantee, stated

**A read returns data no staler than ac milliseconds** — equivalently,
another client's completed write becomes visible to a cached reader
within at most ac ms of that reader's last attribute fetch. Writes are
write-through, so the server (and any uncached reader) sees a write the
moment the writing call returns; only *cached readers* lag, and only by
the window. This is NFS's actimeo bargain, not open-to-close semantics
and not coherence: two clients writing the same bytes inside one window
still race, and a reader can act on data it can no longer prove current.

## Against AFS (ch. 50)

AFS inverts the trade. With callbacks, the server *promises* to notify
the client when the file changes, so a cached read needs no periodic
getattr — my 1 revalidation per window becomes 0 messages on the read
path, and the staleness bound tightens from "ac ms" to "one callback
delivery" (effectively close-to-open consistency at whole-file
granularity). The price is paid elsewhere, and it is exactly what Part 4
spent the lab removing: the server now keeps per-client state — who
caches what — and a server restart is no longer invisible; AFS must
rebuild or conservatively break its callback promises when it comes
back. One design buys statelessness and pays in staleness; the other
buys consistency and pays in state. The two labs' halves are the two
sides of that ledger.
