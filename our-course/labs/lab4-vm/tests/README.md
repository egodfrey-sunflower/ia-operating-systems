# Lab 4 autograder (`tests/`)

`run.sh <workdir>` builds the xv6 tree at `<workdir>`, boots it under QEMU
via the shared harness (`../../common/qemuharness.py`), and runs the Lab 4
checks (vmprint, ugetpid, cowtest, forkbench, `usertests -q`). It prints a
PASS/FAIL table and exits non-zero on any failure.

```
tests/run.sh ~/lab4            # everything (lab tier, then regression tier)
QUICK=1 tests/run.sh ~/lab4    # lab tier only; usertests -q marked SKIPPED
```

## What the vmprint check does (and does not) grade

The rubric promises credit for the **correct indented-tree format** mandated
in the lab README ("Task 1 — `vmprint`"). The grader does not diff your
output against a golden printout — physical addresses and PTE contents vary
run to run — so **full formatting is human-checked** against the README's
required format. The automated check enforces only these structural markers,
all of which a fresh process's page table must exhibit:

* the `page table 0x<16 hex digits>` header line;
* a top-level ` ..255: pte ... pa ...` line (the entry covering the
  trampoline/trapframe region — index 255, not 511, because `MAXVA` is
  `1 << 38`);
* at least one level-1 ` .. ..N: pte ... pa ...` line;
* 3-level-deep leaf lines ` .. .. ..N: pte ... pa ...`, including both
  index **511 (trampoline)** and **510 (trapframe)**.

Passing the automated check therefore does not by itself certify the format;
conversely, output that fails it cannot match the mandated format.

## What the `ugetpid` check does (and does not) grade

The `ugetpid` test forks and confirms `ugetpid()` returns the calling
process's pid — it certifies that the `USYSCALL` page is mapped, populated
with the pid, and readable from user space. It does **not** store to the
page, so the **"never writable"** half of the rubric is not machine-caught: a
mapping made `R+W+U` reads back correctly and passes the whole grader.

The read-only property is therefore human-verified by inspection: in your
`vmprint` output the `USYSCALL` leaf must carry no `W` bit (`r-x` / `r--`
permissions, never `rw-`).
