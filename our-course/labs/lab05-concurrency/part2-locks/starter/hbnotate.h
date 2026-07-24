/*
 * hbnotate.h -- telling helgrind what your locks promise.
 *
 * helgrind knows how pthread_mutex_lock() orders memory because it
 * intercepts the call. It cannot know how YOUR lock orders memory, because
 * your lock is a loop around an atomic instruction and there is no call to
 * intercept. Run helgrind over a correct hand-built spin lock with no
 * annotations and it reports a data race on every byte the lock protects,
 * plus one on the lock word itself. Thousands of them, all false.
 *
 * So you tell it. These four macros are the standard valgrind client
 * requests, renamed for this lab:
 *
 *   MYLOCK_HG_INIT(l)       "forget everything you knew about this address,
 *                            and stop reporting races on the lock's own
 *                            bytes -- the atomics make those safe."
 *   MYLOCK_HG_ACQUIRED(l)   "this thread now sees everything any thread had
 *                            written before it released l."
 *   MYLOCK_HG_RELEASING(l)  "everything this thread has written is now
 *                            published to whoever acquires l next."
 *
 * Put HG_INIT at the end of your init function, HG_ACQUIRED at the point
 * where acquire is about to return, and HG_RELEASING immediately before the
 * store or the atomic that lets the next thread in. That last one matters:
 * announce the release after you have released and you have described a lock
 * you did not build.
 *
 * ---------------------------------------------------------------------------
 * BE CLEAR ABOUT WHAT THIS BUYS AND WHAT IT COSTS.
 *
 * These macros are assertions, not measurements. They tell helgrind that your
 * lock establishes an ordering; helgrind believes you. A lock built out of
 * plain loads and stores, with the annotations in the right places, gets a
 * clean helgrind run -- a lock built that way was run under helgrind and came
 * back clean. helgrind cannot audit your atomics, and
 * this lab does not claim it can. That is what the counter cases are for.
 *
 * What helgrind still finds, and what nothing else in Part 2 finds, is shared
 * state you forgot to put inside a critical section at all: the variable read
 * just before the acquire, the flag updated just after the release, the
 * counter one thread touches without taking the lock. Those are the bugs that
 * survive a thousand green outcome runs, and they are the reason the run is
 * required.
 * ---------------------------------------------------------------------------
 *
 * Outside valgrind all four compile to nothing, so there is no reason to
 * remove them. If <valgrind/helgrind.h> is missing, build with
 * -DMYLOCK_NO_VALGRIND and they compile to nothing too -- and the helgrind
 * cases in the harness skip.
 *
 * Given to you complete. Nothing to do in this file.
 */
#ifndef HBNOTATE_H
#define HBNOTATE_H

#ifdef MYLOCK_NO_VALGRIND

#define MYLOCK_HG_INIT(l)       ((void)(l))
#define MYLOCK_HG_ACQUIRED(l)   ((void)(l))
#define MYLOCK_HG_RELEASING(l)  ((void)(l))

#else

#include <valgrind/helgrind.h>

#define MYLOCK_HG_INIT(l) do {                                          \
	ANNOTATE_HAPPENS_BEFORE_FORGET_ALL((l));                        \
	ANNOTATE_BENIGN_RACE_SIZED((l), sizeof *(l), "lock word");      \
} while (0)

#define MYLOCK_HG_ACQUIRED(l)   ANNOTATE_HAPPENS_AFTER((l))
#define MYLOCK_HG_RELEASING(l)  ANNOTATE_HAPPENS_BEFORE((l))

#endif

#endif /* HBNOTATE_H */
