/*
 * mylock.c -- reference locks for Lab 5 Part 2.
 *
 * Three locks, in the order OSTEP ch. 28 builds them: test-and-set, then
 * fetch-and-add, then a lock that stops burning the processor while it waits.
 *
 * On memory ordering. Every atomic here carries an explicit ordering, and
 * none of them is __ATOMIC_RELAXED by accident:
 *
 *   - the atomic that WINS the lock is ACQUIRE. Nothing the compiler or the
 *     processor does may move a read of the protected data up above it, or
 *     the critical section would start before the lock was held.
 *   - the atomic or store that GIVES UP the lock is RELEASE. Nothing may sink
 *     below it, or the next holder could see a half-finished write.
 *
 * That is the whole rule, and it is why an unbarriered lock is not slightly
 * wrong but wrong in a way that appears months later on a different machine.
 * On x86-64 the hardware supplies acquire/release ordering on ordinary loads
 * and stores for free, so the acquire and release here mostly constrain the
 * COMPILER -- which is quite enough: at -O2 gcc will happily hoist a load of
 * a shared variable out of a spin loop and leave you looping for ever on a
 * value it fetched once.
 */

#include "mylock.h"

#include <limits.h>
#include <linux/futex.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>

/* A hint to the processor that this is a spin-wait: it reduces the pipeline
 * penalty when the loop finally exits, and on hyperthreaded cores it hands
 * some issue bandwidth to the sibling. Correctness does not depend on it. */
static inline void cpu_relax(void)
{
	__asm__ __volatile__("pause" ::: "memory");
}

/* ------------------------------------------------------------ spin lock */

void spinlock_init(spinlock_t *l)
{
	__atomic_store_n(&l->flag, 0, __ATOMIC_RELEASE);
	MYLOCK_HG_INIT(l);
}

void spinlock_acquire(spinlock_t *l)
{
	for (;;) {
		if (__atomic_exchange_n(&l->flag, 1, __ATOMIC_ACQUIRE) == 0)
			break;
		/* Spin on a plain load until the lock looks free, and only then
		 * try the exchange again. The exchange writes, and a write
		 * takes the cache line exclusively away from every other core
		 * on every attempt; reads share it. This is test-and-test-and-
		 * set, and on a contended lock it is the difference between a
		 * lock that scales badly and one that does not scale at all. */
		while (__atomic_load_n(&l->flag, __ATOMIC_RELAXED) != 0)
			cpu_relax();
	}
	MYLOCK_HG_ACQUIRED(l);
}

void spinlock_release(spinlock_t *l)
{
	MYLOCK_HG_RELEASING(l);
	__atomic_store_n(&l->flag, 0, __ATOMIC_RELEASE);
}

/* ---------------------------------------------------------- ticket lock */

void ticketlock_init(ticketlock_t *l)
{
	__atomic_store_n(&l->ticket, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&l->serving, 0, __ATOMIC_RELEASE);
	MYLOCK_HG_INIT(l);
}

void ticketlock_acquire(ticketlock_t *l)
{
	unsigned int mine = __atomic_fetch_add(&l->ticket, 1, __ATOMIC_RELAXED);

	while (__atomic_load_n(&l->serving, __ATOMIC_ACQUIRE) != mine)
		cpu_relax();
	MYLOCK_HG_ACQUIRED(l);
}

void ticketlock_release(ticketlock_t *l)
{
	MYLOCK_HG_RELEASING(l);
	/* Only the holder ever writes `serving`, so a plain store would do;
	 * the add is here because it says what is happening. */
	__atomic_fetch_add(&l->serving, 1, __ATOMIC_RELEASE);
}

/* --------------------------------------------------------- sleeping lock */

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

/* Compare-and-swap that returns the value that was there. */
static int cas(int *p, int expected, int desired, int success_order)
{
	int e = expected;

	__atomic_compare_exchange_n(p, &e, desired, 0, success_order,
				    __ATOMIC_ACQUIRE);
	return e;
}

void sleeplock_init(sleeplock_t *l)
{
	__atomic_store_n(&l->state, 0, __ATOMIC_RELEASE);
	MYLOCK_HG_INIT(l);
}

void sleeplock_acquire(sleeplock_t *l)
{
	int c = cas(&l->state, 0, 1, __ATOMIC_ACQUIRE);

	if (c != 0) {
		do {
			/* Before parking, the word must say 2, or a releasing
			 * holder will see 1, conclude nobody is waiting, and
			 * skip the wake. */
			if (c == 2 || cas(&l->state, 1, 2, __ATOMIC_ACQUIRE) != 0)
				futex_wait(&l->state, 2);
			/* The kernel returns here for a wake, a signal, or
			 * because the value changed before it slept. In all
			 * three cases the answer is the same: look again. This
			 * loop is the same `while`, not `if`, rule that Part 3
			 * makes a rule -- a wakeup is a hint, and the state has
			 * to be rechecked. */
		} while ((c = cas(&l->state, 0, 2, __ATOMIC_ACQUIRE)) != 0);
		/* We took the lock via the 0 -> 2 path, so the word says 2
		 * even if no one is waiting any more. That costs at most one
		 * needless FUTEX_WAKE on release, and never a lost wakeup. */
	}
	MYLOCK_HG_ACQUIRED(l);
}

void sleeplock_release(sleeplock_t *l)
{
	MYLOCK_HG_RELEASING(l);
	if (__atomic_fetch_sub(&l->state, 1, __ATOMIC_RELEASE) != 1) {
		/* The word was 2: someone may be parked. Drop to 0 and wake
		 * one of them. */
		__atomic_store_n(&l->state, 0, __ATOMIC_RELEASE);
		futex_wake_one(&l->state);
	}
	/* The word was 1: uncontended. The fetch_sub already made it 0, and
	 * there was no system call on this path at all. That is the whole
	 * reason for the third value. */
}
