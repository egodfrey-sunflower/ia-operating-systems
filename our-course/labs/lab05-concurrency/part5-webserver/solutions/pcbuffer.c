/*
 * pcbuffer.c -- reference bounded buffer for Lab 5 Part 3.
 *
 * Four functions, about sixty lines, and every line of the two interesting
 * ones is one of three rules:
 *
 *   1. Hold the mutex whenever you look at or change the state. pcb_put and
 *      pcb_get take it on the way in and give it back on the way out, and
 *      there is no path between the two that does not.
 *
 *   2. Wait in a `while`, never an `if`. pthread_cond_wait returns when the
 *      thread has been signalled, when it has been broadcast to, when a
 *      signal arrived, and sometimes for no reason at all. None of those
 *      means the predicate is true. Under Mesa semantics -- which is what
 *      POSIX gives you and what OSTEP ch. 30 spends the chapter on -- a
 *      signal is a hint that the state MIGHT have changed, not a handoff of
 *      the lock together with a guarantee. Between the signaller releasing
 *      the mutex and the waiter reacquiring it, any number of other threads
 *      can run and undo the thing that was signalled.
 *
 *   3. Two condition variables. A consumer that finishes a get signals
 *      `empty`, where producers wait; a producer that finishes a put signals
 *      `fill`, where consumers wait. With one shared variable, a signal
 *      intended for a producer can be delivered to a sleeping consumer, which
 *      wakes, finds nothing to consume, and goes back to sleep -- and the
 *      producer that should have been woken is still asleep. Nobody is
 *      running and nobody is going to be.
 *
 * Rules 2 and 3 are the two failure modes Part 3 asks you to build on
 * purpose, and they fail differently: breaking rule 2 gives wrong answers and
 * crashes, breaking rule 3 gives silence.
 */

#include "pcbuffer.h"

#include <assert.h>

void pcb_init(pcbuffer *b, int capacity)
{
	assert(capacity >= 1 && capacity <= PCB_MAX_CAPACITY);
	b->capacity = capacity;
	b->count = 0;
	b->head = 0;
	b->tail = 0;
	pthread_mutex_init(&b->lock, NULL);
	pthread_cond_init(&b->fill, NULL);
	pthread_cond_init(&b->empty, NULL);
}

void pcb_put(pcbuffer *b, int item)
{
	pthread_mutex_lock(&b->lock);
	while (b->count == b->capacity)
		pthread_cond_wait(&b->empty, &b->lock);
	b->slots[b->tail] = item;
	b->tail = (b->tail + 1) % b->capacity;
	b->count++;
	/* Signal, not broadcast: exactly one consumer can use exactly one new
	 * item, and waking the rest only to have them find the buffer empty
	 * again is the thundering herd. Broadcast is not WRONG here -- with
	 * the `while` loop the extra wakeups are harmless -- it is just
	 * wasteful. That is the usual relationship between the two. */
	pthread_cond_signal(&b->fill);
	pthread_mutex_unlock(&b->lock);
}

int pcb_get(pcbuffer *b)
{
	int item;

	pthread_mutex_lock(&b->lock);
	while (b->count == 0)
		pthread_cond_wait(&b->fill, &b->lock);
	item = b->slots[b->head];
	b->head = (b->head + 1) % b->capacity;
	b->count--;
	pthread_cond_signal(&b->empty);
	pthread_mutex_unlock(&b->lock);
	return item;
}

void pcb_destroy(pcbuffer *b)
{
	pthread_cond_destroy(&b->fill);
	pthread_cond_destroy(&b->empty);
	pthread_mutex_destroy(&b->lock);
}
