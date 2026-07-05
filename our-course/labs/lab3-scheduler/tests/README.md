# Lab 3 autograder (`tests/`)

`run.sh <workdir>` builds the xv6 tree at `<workdir>` once per scheduling
policy (`SCHED=PRIO/LOTTERY/MLFQ/RR`, `make clean` between), boots each under
QEMU via the shared harness (`../../common/qemuharness.py`), and runs the
self-measuring scheduler test programs (`priotest`, `lotto`, `mlfqtest`) plus
a `usertests -q` regression. It prints a PASS/FAIL table and exits non-zero on
any failure.

```
tests/run.sh ~/lab3            # everything (all policies, then regression)
QUICK=1 tests/run.sh ~/lab3    # skip the long usertests -q runs
```

## Known limitations

The `MLFQ-BOOST` check can pass on an MLFQ implementation with **no periodic
boost at all**. Its pressure children are I/O processes that burst for less
than a tick and then sleep, so the CPU is idle of high-priority work most of
the time; the bottom-queue hog runs whenever they block, and its tick count
grows regardless of any boost. The observation window is also shorter than the
handout's suggested ~100-tick boost period, so a boost may never fire during
the check. Growth in the hog's ticks therefore does **not** prove that a boost
un-starved it.

Until the workload is reworked to apply saturating top-queue pressure that
never demotes (so that, without a boost, the hog's ticks genuinely freeze),
treat the **boost / un-starving property** as human-verified from the Task 3
write-up (the starvation-then-boost measurement the student is asked to
report), not as certified by `MLFQ-BOOST`.
