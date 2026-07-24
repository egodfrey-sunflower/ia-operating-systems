# The scheduler fixtures

Two fixed request streams, in the format `disksim run` reads: one request per
line, `<lba> [nsec]`, `#` for comments. They are fixed files, not generated, so
the numbers below are reproducible and comparable between students.

## `disk-sep.stream` — the separation fixture

Six requests at cylinders `{10, 90, 20, 80, 55, 45}` (lba = cylinder × 100,
sector 0), served with the head starting at cylinder 50 (`start=50`), default
geometry. The requests are chosen so that the four policies produce four
different service orders **and** four different total seek distances:

| Policy | `seek_total` (cylinders) | `order_cyl` |
|---|---:|---|
| `fifo`  | 285 | 10, 90, 20, 80, 55, 45 |
| `sstf`  | 130 | 45, 55, 80, 90, 20, 10 |
| `scan`  | 120 | 55, 80, 90, 45, 20, 10 |
| `cscan` | 155 | 55, 80, 90, 10, 20, 45 |

If any policy collides with FCFS's 285 on this stream, it is not scheduling.
`tests/run.sh` asserts each order exactly and asserts the four seek totals are
all different — that is the check that the policies are four policies and not one
under four names.

(All six requests are at sector 0, so `service_total` is the same for every
policy here — rotation dominates uniformly. That is deliberate: this fixture
isolates the *seek*-ordering behaviour, which is what the policies decide. To
see service time separate as well, run the policies over a random workload from
`disksim gen` — `RESULTS.md` does exactly that.)

## `disk-starve.stream` — the SSTF starvation fixture

A tight cluster of ten requests either side of cylinder 15 (the head start,
`start=15`), plus one lone request far away at cylinder 490 (lba 49000). Under
SSTF the head is repeatedly pulled back into the cluster, so the outlier is
served **last** (service position 10, counting from 0); under SCAN the up-sweep
reaches cylinder 490 in a single pass, so it is served at position 5. The
autograder checks that SCAN serves the outlier strictly earlier than SSTF.

This is the elevator fixing the pathology by *sweeping*. Sheet 16's BSATF fixes
the same pathology by *bounding the window*; the two are complementary, and
`RESULTS.md` reports them side by side.
