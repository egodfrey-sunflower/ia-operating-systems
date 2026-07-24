/*
 * mylock.c -- three locks. Lab 5, Part 2.
 *
 * Build them in the order OSTEP ch. 28 builds them: test-and-set, then
 * fetch-and-add, then a lock that stops burning the processor while it waits.
 * Each one is short. The difficulty is not the number of lines, it is that
 * the wrong version of any of them works nearly all the time.
 *
 * THE MEMORY-ORDERING RULE, once, because every atomic below needs it:
 *
 *   - the atomic that WINS the lock is __ATOMIC_ACQUIRE. Nothing may move
 *     from after it to before it, or the critical section would start before
 *     the lock was held.
 *   - the atomic or store that GIVES UP the lock is __ATOMIC_RELEASE.
 *     Nothing may move from before it to after it, or the next holder could
 *     see a half-finished write.
 *
 * On x86-64 the hardware already gives ordinary loads and stores
 * acquire/release ordering, so what these mostly constrain is the COMPILER.
 * That is not a lesser problem. At -O2, gcc looking at a loop that reads a
 * variable nothing in the loop writes is entitled to read it once and spin on
 * the copy, and your lock then waits for ever for a value that already
 * changed. The graded build is -O2 for exactly this reason.
 *
 * Every atomic you need is a gcc builtin -- no <stdatomic.h>, no inline
 * assembly:
 *
 *   __atomic_exchange_n(&x, v, order)                  old value, sets x = v
 *   __atomic_fetch_add(&x, n, order)                   old value, adds n
 *   __atomic_load_n(&x, order)                         a plain read, ordered
 *   __atomic_store_n(&x, v, order)                     a plain write, ordered
 *   __atomic_compare_exchange_n(&x, &exp, new, 0, s, f)
 *
 * Do not put the annotation macros off until the end. Write each lock, then
 * put its three MYLOCK_HG_* macros in as you go; hbnotate.h says where.
 */

#include "mylock.h"

#include <limits.h>
#include <linux/futex.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>

/* A hint to the processor that this is a spin-wait: it reduces the pipeline
 * penalty when the loop exits and hands issue bandwidth to a sibling
 * hyperthread. Correctness does not depend on it. Given to you. */
static inline void cpu_relax(void)
{
	__asm__ __volatile__("pause" ::: "memory");
}

/* ------------------------------------------------------------ spin lock */

void spinlock_init(spinlock_t *l)
{
	/* TODO: put the lock in the free state, and then MYLOCK_HG_INIT(l). */
	(void)l;
}

void spinlock_acquire(spinlock_t *l)
{
	/*
	 * TODO: loop until an atomic exchange of 1 into l->flag returns 0 --
	 * that is, until you were the one who changed it from free to held.
	 * Acquire ordering.
	 *
	 * Then improve it. The version above hammers the cache line with a
	 * WRITE on every attempt, and a write takes the line exclusively away
	 * from every other core each time. Spin on a plain __atomic_load_n
	 * until the lock looks free and only then try the exchange again --
	 * test-and-test-and-set. On a contended lock it is the difference
	 * between a lock that scales badly and one that does not scale at all,
	 * and you will be able to see it in `make bench`.
	 *
	 * Finish with MYLOCK_HG_ACQUIRED(l).
	 */
	(void)l;
	(void)cpu_relax;
}

void spinlock_release(spinlock_t *l)
{
	/* TODO: MYLOCK_HG_RELEASING(l) first, then store 0 with release
	 * ordering. In that order: announcing a release you have not performed
	 * describes a lock you did not build. */
	(void)l;
}

/* ---------------------------------------------------------- ticket lock */

void ticketlock_init(ticketlock_t *l)
{
	/* TODO. */
	(void)l;
}

void ticketlock_acquire(ticketlock_t *l)
{
	/* TODO: take a number with fetch-and-add on l->ticket, then spin until
	 * l->serving equals it. Which of the two needs acquire ordering, and
	 * which does not? Answer that before you write it; the answer is in
	 * the rule at the top of this file. */
	(void)l;
}

void ticketlock_release(ticketlock_t *l)
{
	/* TODO: hand the lock to the next number. */
	(void)l;
}

/* --------------------------------------------------------- sleeping lock */
/*
 * The two futex calls, given to you -- the system call interface is not the
 * lesson and glibc does not wrap it.
 *
 *   futex_wait(addr, expected)
 *       Sleep until someone wakes this address. Checks *addr == expected
 *       first, ATOMICALLY WITH GOING TO SLEEP, and returns immediately if it
 *       is not. That check is the whole reason a futex exists: without it,
 *       every "look at the state, decide to sleep, sleep" sequence has a
 *       window in which the state changes and the wakeup is lost.
 *
 *       It may also return for no reason at all -- a signal, or a wake meant
 *       for someone else. Never assume that returning means you got the lock.
 *
 *   futex_wake_one(addr)
 *       Wake at most one thread sleeping on this address. Waking nobody is
 *       not an error.
 */

static void futex_wait(int *addr, int expected)
{
	syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected,
		(const struct timespec *)NULL, (int *)NULL, 0);
}

static void futex_wake_one(int *addr)
{
	syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, 1,
		(const struct timespec *)NULL, (int *)NULL, 0);
}

void sleeplock_init(sleeplock_t *l)
{
	/* TODO. */
	(void)l;
}

void sleeplock_acquire(sleeplock_t *l)
{
	/*
	 * TODO. The three-state protocol from OSTEP ch. 28:
	 *
	 *   - try to move state 0 -> 1 with a compare-and-swap. If it worked,
	 *     the lock was free and is now yours. Return. No system call.
	 *   - otherwise loop: make sure the word says 2 (held, someone is
	 *     waiting) before parking, park with futex_wait(&l->state, 2), and
	 *     when you come back TRY AGAIN rather than assuming the lock is
	 *     yours -- move 0 -> 2 with a compare-and-swap, and keep looping
	 *     until that succeeds.
	 *
	 * Why 2 must be written before parking: a releasing holder that sees 1
	 * concludes nobody is waiting and skips the wake. Park while the word
	 * still says 1 and the wake never comes.
	 *
	 * Why you take the lock as 2 rather than 1 on the slow path: you
	 * cannot know whether anyone else is still parked. Claiming 2 costs at
	 * most one unnecessary wake on release, and claiming 1 costs a thread
	 * that sleeps for ever.
	 *
	 * Finish with MYLOCK_HG_ACQUIRED(l).
	 */
	(void)l;
	(void)futex_wait;
}

void sleeplock_release(sleeplock_t *l)
{
	/*
	 * TODO. Decrement the state with an atomic fetch-and-subtract, release
	 * ordering. If the value you got back was 1 the lock was uncontended,
	 * it is now 0, and there is nothing more to do -- no system call at
	 * all, which is the entire point of the third state. Otherwise it was
	 * 2: store 0 and futex_wake_one(&l->state).
	 *
	 * MYLOCK_HG_RELEASING(l) goes first, before any of it.
	 */
	(void)l;
	(void)futex_wake_one;
}
