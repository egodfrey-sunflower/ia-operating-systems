/*
 * mylock.h -- the contract for Lab 5 Part 2.
 *
 * Three locks, nine functions, no other API. The harness includes THIS header
 * and links against YOUR mylock.c, so nothing declared here may change.
 *
 * The harness never looks inside a lock. What it can see is behaviour, and it
 * checks four kinds:
 *
 *   - a shared counter incremented under the lock reaches exactly N x M;
 *   - two threads are never inside the critical section at the same time;
 *   - the ticket lock hands the lock over in the order threads arrived;
 *   - the sleeping lock's waiters do not burn processor time;
 *   - the spin lock's waiters do burn it.
 *
 * How you get there is yours.
 */
#ifndef MYLOCK_H
#define MYLOCK_H

#include "hbnotate.h"

/* ------------------------------------------------------------------------ */
/* A spin lock on test-and-set. OSTEP ch. 28's first working lock.
 *
 * flag is 0 when the lock is free and 1 when it is held. It is a plain int
 * and it is meant to be: making it _Atomic would hide the exercise, which is
 * to reach every one of its bytes through an atomic builtin and to put the
 * memory ordering on by hand.
 *
 * A waiter here spins, and the harness checks that it does: four threads wait
 * 200 ms for one held lock and the processor time the process burns over that
 * window is measured. Burning a core while you wait is what a test-and-set
 * lock does; it is the behaviour the sleeping lock exists to fix, and getting
 * it here is not a defect. A waiter that parks instead fails this case even
 * though it excludes perfectly -- the three locks are three designs, and each
 * one is checked for the property that makes it that design. */
typedef struct {
	int flag;
} spinlock_t;

void spinlock_init(spinlock_t *l);
void spinlock_acquire(spinlock_t *l);
void spinlock_release(spinlock_t *l);

/* ------------------------------------------------------------------------ */
/* A ticket lock on fetch-and-add. Take a number; wait until it is called.
 *
 * The harness checks the resulting property directly: six threads that arrive
 * 40 ms apart get the lock back in the order they arrived. A test-and-set
 * lock does not do that, so this is the case that tells the two apart. */
typedef struct {
	unsigned int ticket;    /* the next number to hand out */
	unsigned int serving;   /* the number being served now */
} ticketlock_t;

void ticketlock_init(ticketlock_t *l);
void ticketlock_acquire(ticketlock_t *l);
void ticketlock_release(ticketlock_t *l);

/* ------------------------------------------------------------------------ */
/* A sleeping lock: a waiter parks in the kernel instead of spinning.
 *
 * state is the three-valued word from OSTEP ch. 28's futex mutex:
 *   0  free
 *   1  held, and nobody is waiting
 *   2  held, and at least one thread is parked
 *
 * The two-valued version is correct as well, but it wakes a sleeper on every
 * release whether or not one exists; the third value is what makes an
 * uncontended release a plain store and no system call.
 *
 * The harness checks that waiters really sleep, by measuring how much
 * processor time the process burns while six threads wait 400 ms for one
 * held lock. Spinning shows up as CPU seconds; sleeping does not. */
typedef struct {
	int state;
} sleeplock_t;

void sleeplock_init(sleeplock_t *l);
void sleeplock_acquire(sleeplock_t *l);
void sleeplock_release(sleeplock_t *l);

#endif /* MYLOCK_H */
