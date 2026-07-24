# The Part 1 transcripts

Six files, each the exact output of one case in `tests/cases.c` run against a
package that obeys `uthread.h`. `tests/run.sh` diffs your program's output
against them.

They can be exact because Part 1's threads are cooperative. A fixed pattern of
`uthread_yield()` calls against a FIFO ready queue has exactly one legal
interleaving, so the output is a property of the specification rather than of
the machine. Nothing else in Lab 5 can be tested this way.

| File | What it runs | What it is for |
|---|---|---|
| `p1_smoke.expected` | one thread, no yields | the smallest thing that can work: create, switch in, run, exit, and control back in `uthread_run` |
| `p1_roundrobin.expected` | three threads, three rounds each | the turn-taking order itself. A queue that hands the processor back to the thread that just yielded, or that runs threads in creation order once and then stops, diverges on line 2 |
| `p1_uneven.expected` | threads that live for 1, 2 and 5 rounds | what happens as the queue drains. Threads leave at different times and one is left alone for three rounds; a `dequeue` that empties the queue without clearing the tail pointer diverges here and in `p1_nested`, and fails the slot-reuse and main-context cases too |
| `p1_exitpaths.expected` | one thread returns, one calls `uthread_exit` | that the two are the same thing. The second thread has a print after its `uthread_exit`, so a package where `uthread_exit` can return says so in the transcript |
| `p1_nested.expected` | a thread that creates two more, mid-run | that `uthread_create` works from inside a thread and appends behind the threads already waiting |
| `p1_order.expected` | twelve threads, five rounds | the same property as `p1_roundrobin` at a size where an off-by-one in a wrapping queue has room to show up |

The other nine have no transcript. Seven check register contents, stack
contents, ids and return values, which a transcript cannot show, and decide
their own verdict. The last two are the valgrind runs, whose verdict is
valgrind's exit code and which re-run a case that has already passed on its
own.

**Do not edit these files.** If one disagrees with your package, the disagreement
is the finding.
