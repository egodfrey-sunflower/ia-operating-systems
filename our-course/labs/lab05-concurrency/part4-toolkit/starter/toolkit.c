/*
 * toolkit.c -- Lab 5 Part 4. The synchronisation toolkit.
 *
 * Fifteen functions in five groups. The first group is the only one allowed
 * to mention pthreads: build the semaphore on a mutex and a condition
 * variable, and then build the other four out of semaphores and nothing
 * else. toolkit.h says why.
 *
 * A suggested order, which is also the order the cases come in:
 *
 *   1. msem_*             -- Part 3 with the buffer taken out. Twenty lines.
 *   2. rv_*               -- two semaphores at zero. Four lines, and it is
 *                            the one that shows you what a semaphore at zero
 *                            is FOR.
 *   3. mbarrier_*         -- the one with the classic bug in it. Get it
 *                            working for one round, then run it for a
 *                            hundred and find out why one turnstile is not
 *                            enough.
 *   4. rw_*               -- read the handout's section on writer
 *                            starvation BEFORE you write this, or you will
 *                            build the textbook version, which the harness
 *                            fails on purpose.
 *   5. phil_*             -- three lines, once you have decided which of the
 *                            four conditions you are going to break.
 *
 * The `(void)` casts below are only there so the skeleton compiles under
 * -Wextra -Werror. Delete each one as you use its parameter.
 */

#include "toolkit.h"

/* ==================================================================
 * the semaphore -- the only place pthreads may appear
 * ================================================================== */

void msem_init(msem *s, int value)
{
	s->value = value;
	pthread_mutex_init(&s->lock, NULL);
	pthread_cond_init(&s->cv, NULL);
}

void msem_wait(msem *s)
{
	/* TODO: take the mutex; wait while the value is zero -- in a `while`,
	 * for the reason Part 3 spent a chapter on; decrement; unlock. */
	(void)s;
}

void msem_post(msem *s)
{
	/* TODO: take the mutex; increment; signal one waiter from INSIDE the
	 * critical section, before the unlock; unlock. */
	(void)s;
}

void msem_destroy(msem *s)
{
	pthread_cond_destroy(&s->cv);
	pthread_mutex_destroy(&s->lock);
}

/* ==================================================================
 * the barrier -- n threads in, none out until all n are in, then again
 * ================================================================== */

void mbarrier_init(mbarrier *b, int n)
{
	/* TODO: record n, zero the arrival count, and initialise the
	 * semaphores. Think about which of the gates starts open. */
	(void)b;
	(void)n;
}

void mbarrier_wait(mbarrier *b)
{
	/* TODO: count yourself in under the mutex; the nth arrival opens the
	 * way; everyone passes through a turnstile.
	 *
	 * Then read that again and ask what happens when the fastest thread
	 * comes back round while the slowest is still leaving. */
	(void)b;
}

void mbarrier_destroy(mbarrier *b)
{
	(void)b;
}

/* ==================================================================
 * the rendezvous -- a barrier for two, and much easier
 * ================================================================== */

void rv_init(rendezvous *r)
{
	(void)r;
}

void rv_arrive(rendezvous *r, int side)
{
	/* TODO: this side must not return until both sides have arrived. Two
	 * semaphores at zero; work out the order of post and wait yourself. */
	(void)r;
	(void)side;
}

void rv_destroy(rendezvous *r)
{
	(void)r;
}

/* ==================================================================
 * the reader-writer lock -- many readers or one writer, and no writer
 * left waiting for ever
 * ================================================================== */

void rw_init(rwlock *l)
{
	(void)l;
}

void rw_acquire_read(rwlock *l)
{
	/* TODO: the first reader in locks the room; the count needs its own
	 * guard. And something has to stop new readers walking past a writer
	 * that is already waiting -- see the handout. */
	(void)l;
}

void rw_release_read(rwlock *l)
{
	(void)l;
}

void rw_acquire_write(rwlock *l)
{
	(void)l;
}

void rw_release_write(rwlock *l)
{
	(void)l;
}

void rw_destroy(rwlock *l)
{
	(void)l;
}

/* ==================================================================
 * dining philosophers -- philosopher w eats with forks w and (w + 1) % n
 * ================================================================== */

void phil_init(phils *p, int n)
{
	/* TODO: one semaphore per fork, and whatever your chosen solution
	 * needs beyond that. */
	(void)p;
	(void)n;
}

void phil_pickup(phils *p, int w)
{
	/* TODO: return holding both forks, having made a circular wait
	 * impossible rather than unlikely. */
	(void)p;
	(void)w;
}

void phil_putdown(phils *p, int w)
{
	(void)p;
	(void)w;
}

void phil_destroy(phils *p)
{
	(void)p;
}
