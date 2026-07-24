/*
 * pcbuffer.c -- a bounded buffer with condition variables. Lab 5, Part 3.
 *
 * This is the shortest piece of code in the lab and the easiest to get
 * subtly wrong. Four functions, about sixty lines, and every line of the two
 * interesting ones is one of three rules:
 *
 *   1. Hold the mutex whenever you look at or change the state. There must be
 *      no path through pcb_put or pcb_get that touches count, head, tail or
 *      slots without it, and no path out that forgets to unlock.
 *
 *   2. Wait in a `while`, never an `if`. pthread_cond_wait returns when the
 *      thread has been signalled, when it has been broadcast to, when a
 *      signal arrives, and sometimes for no reason at all. None of those
 *      means the predicate is true. This is Mesa semantics -- POSIX's, and
 *      OSTEP ch. 30's -- and it says a signal is a hint that the state MIGHT
 *      have changed, not a handoff of the lock together with a promise.
 *      Between the signaller releasing the mutex and you reacquiring it, any
 *      number of other threads can run and undo the thing you were told
 *      about.
 *
 *   3. Two condition variables. Consumers wait on `fill` for something to
 *      consume; producers wait on `empty` for room. A put signals `fill`; a
 *      get signals `empty`. Crossing those over, or using one variable for
 *      both, means a wakeup meant for a producer can land on a consumer,
 *      which finds nothing and goes back to sleep -- and the producer is
 *      still asleep too.
 *
 * Rules 2 and 3 are the two things you break on purpose afterwards, for
 * BREAKAGE.md. Get the correct version passing first: you cannot show a bug
 * convincingly in code that was not working to begin with.
 *
 * One more rule, which pcbuffer.h states and the harness enforces: SIGNAL
 * WHILE YOU STILL HOLD THE MUTEX, before the unlock.
 */

#include "pcbuffer.h"

#include <assert.h>

void pcb_init(pcbuffer *b, int capacity)
{
	assert(capacity >= 1 && capacity <= PCB_MAX_CAPACITY);
	/* TODO: capacity, count, head, tail, and then pthread_mutex_init and
	 * pthread_cond_init for all three objects. Given a NULL attribute
	 * pointer, the defaults are what you want. */
	(void)b;
	(void)capacity;
}

void pcb_put(pcbuffer *b, int item)
{
	/*
	 * TODO, in this order:
	 *
	 *   lock the mutex;
	 *   while there is no room, wait on the condition variable producers
	 *     wait on, passing the mutex -- pthread_cond_wait releases it
	 *     while it sleeps and has reacquired it by the time it returns,
	 *     which is the only reason the `while` test is safe to repeat;
	 *   store the item, advance tail (wrapping at capacity), bump count;
	 *   signal the condition variable consumers wait on;
	 *   unlock.
	 */
	(void)b;
	(void)item;
}

int pcb_get(pcbuffer *b)
{
	/* TODO: the mirror image of pcb_put. Note that the item has to be
	 * copied out of the slot BEFORE the mutex is released -- returning
	 * b->slots[b->head] after the unlock reads a slot another producer may
	 * already have overwritten. */
	(void)b;
	return -1;
}

void pcb_destroy(pcbuffer *b)
{
	/* TODO: destroy the two condition variables and the mutex. */
	(void)b;
}
