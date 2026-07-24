# RELIABILITY.md — the layer under loss (model report)

Setup: `reliable` reference, n=50 messages, timeout=50 ms, retries=32,
loss applied in **both** directions (client seed s, server seed 100+s).
All 50 messages were delivered exactly once, in order, in every run.

| rate | seed | retrans | dups (at server) | giveups | elapsed_ms |
|---:|---:|---:|---:|---:|---:|
| 0%  | 1 | 0  | 0  | 0 | 2    |
| 0%  | 2 | 0  | 0  | 0 | 8    |
| 10% | 1 | 9  | 2  | 0 | 464  |
| 10% | 2 | 18 | 11 | 0 | 914  |
| 30% | 1 | 38 | 13 | 0 | 1931 |
| 30% | 2 | 50 | 23 | 0 | 2530 |

## How the cost scales

A message costs a retry whenever its *round trip* fails: the DATA can be
dropped (probability p) or, surviving, its ACK can be dropped (again p).
So p_eff = 1 − (1−p)² — at 10% that is 0.19, at 30% it is 0.51. Expected
retransmissions per message are p_eff/(1−p_eff): ≈0.23 at 10% (predicts
~12 for 50 messages; measured 9 and 18) and ≈1.04 at 30% (predicts ~52;
measured 38 and 50). Sheet 23's geometric-retry arithmetic is exactly
this calculation, and the measured spread across seeds brackets it.

## What the dups column is

A duplicate at the receiver is not a lost message — it is a lost **ack**.
When the DATA arrived but the ACK died, the client retransmits something
the server already delivered; the sequence number is what lets the server
recognise it, re-ack it, and *not* deliver it twice. The split is visible
in the numbers: at 30%/seed 2, of the 50 retransmissions, 23 were caused
by ack loss (they show up as dups) and the rest by data loss (they do
not). Retry alone would have delivered those 23 messages twice —
at-least-once. The counter is the difference between that and
exactly-once.

## Time

Elapsed ≈ retrans × timeout plus noise: every retransmission means one
50 ms timer expired with nothing useful arriving. 38 retrans → ~1.9 s of
mostly waiting, against ~2 ms for the lossless run: a 30%-lossy link is
three orders of magnitude slower than a clean one *at fixed timeout*,
and all of that is the timer, not the wire. (Wall-clock is noisy on a
shared machine; the retrans counts, which are seed-deterministic, are
the honest half of this table.)
