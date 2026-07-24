/*
 * toolkit.c -- reference synchronisation toolkit for Lab 5 Part 4.
 *
 * One primitive and four structures built out of it. The primitive is
 * fifteen lines; everything after it uses semaphores and nothing else -- no
 * mutex, no condition variable, no atomic appears below the semaphore layer.
 * That constraint is the exercise. It is also what makes the four structures
 * read as *patterns* rather than as four separate pieces of cleverness:
 * guard a counter with a binary semaphore, hold a turnstile shut to stop
 * traffic, initialise a semaphore to zero to make it a signal rather than a
 * lock.
 *
 * Where the value of a semaphore is used as a lock it is initialised to 1
 * and the acquire/release pair is a wait/post pair on it. Where it is used
 * as a signal it is initialised to 0 and the post happens on one thread and
 * the wait on another. Those are the only two idioms in this file, and
 * knowing which one you are looking at is most of reading it.
 */

#include "toolkit.h"

#include <assert.h>

/* ==================================================================
 * the semaphore
 *
 * The one place pthreads appears. It is the Part 3 bounded buffer with the
 * buffer taken out: a mutex over a single integer, a condition variable for
 * the one predicate that can block, a `while` around the wait because a
 * signal is a hint, and the signal sent from inside the critical section.
 * ================================================================== */

void msem_init(msem *s, int value)
{
	assert(value >= 0);
	s->value = value;
	pthread_mutex_init(&s->lock, NULL);
	pthread_cond_init(&s->cv, NULL);
}

void msem_wait(msem *s)
{
	pthread_mutex_lock(&s->lock);
	/* `while`, not `if`, and for the reason it always is: between the
	 * post that woke this thread and this thread reacquiring the mutex,
	 * another waiter can run and take the value. With `if` the woken
	 * thread decrements a zero value and two threads are inside a lock
	 * that admits one. */
	while (s->value == 0)
		pthread_cond_wait(&s->cv, &s->lock);
	s->value--;
	pthread_mutex_unlock(&s->lock);
}

void msem_post(msem *s)
{
	pthread_mutex_lock(&s->lock);
	s->value++;
	/* Signal, not broadcast: one post makes exactly one wait runnable.
	 * Inside the critical section, before the unlock -- the house rule
	 * from Part 3, and helgrind reports the other form. */
	pthread_cond_signal(&s->cv);
	pthread_mutex_unlock(&s->lock);
}

void msem_destroy(msem *s)
{
	pthread_cond_destroy(&s->cv);
	pthread_mutex_destroy(&s->lock);
}

/* ==================================================================
 * the barrier
 *
 * Two turnstiles, which is the answer to "why doesn't one work". With one
 * turnstile the last thread to arrive opens the gate and every thread walks
 * through -- including, if it is quick and the round is short, the thread
 * that has already gone round the loop and come back. It walks through a
 * gate that was opened for the previous round, and the barrier is now one
 * round out of step for ever.
 *
 * The second turnstile closes the first one again before anybody can get
 * back to it. Round n's gate is shut by the last thread out of round n
 * before round n + 1's gate is opened, so a thread that laps the others
 * finds the first gate closed and waits, which is exactly what a barrier is
 * for.
 *
 * `gate_a` starts closed (0) -- nobody may leave a barrier nobody has
 * arrived at. `gate_b` starts open (1), because the second turnstile only
 * has work to do from the second round onwards.
 *
 * Each `wait` on a turnstile is immediately followed by a `post`: that is
 * the idiom for "let everybody through", one thread waking the next as it
 * leaves. n threads, n waits, n posts, and the gate ends up as it started.
 * ================================================================== */

void mbarrier_init(mbarrier *b, int n)
{
	assert(n >= 1 && n <= TK_MAX_THREADS);
	b->n = n;
	b->arrived = 0;
	msem_init(&b->mutex, 1);
	msem_init(&b->gate_a, 0);
	msem_init(&b->gate_b, 1);
}

void mbarrier_wait(mbarrier *b)
{
	msem_wait(&b->mutex);
	if (++b->arrived == b->n) {
		msem_wait(&b->gate_b);          /* shut the second gate */
		msem_post(&b->gate_a);          /* open the first */
	}
	msem_post(&b->mutex);

	msem_wait(&b->gate_a);              /* turnstile one */
	msem_post(&b->gate_a);

	msem_wait(&b->mutex);
	if (--b->arrived == 0) {
		msem_wait(&b->gate_a);          /* shut the first gate */
		msem_post(&b->gate_b);          /* open the second */
	}
	msem_post(&b->mutex);

	msem_wait(&b->gate_b);              /* turnstile two */
	msem_post(&b->gate_b);
}

void mbarrier_destroy(mbarrier *b)
{
	msem_destroy(&b->gate_b);
	msem_destroy(&b->gate_a);
	msem_destroy(&b->mutex);
}

/* ==================================================================
 * the rendezvous
 *
 * Two semaphores, both starting at zero, each one a signal in one
 * direction. Each side announces that it has arrived and then waits for the
 * other side's announcement. Post first, then wait: the other order is the
 * two-thread deadlock ch. 32 opens with, both sides waiting for a signal
 * neither has sent.
 * ================================================================== */

void rv_init(rendezvous *r)
{
	msem_init(&r->arrived[0], 0);
	msem_init(&r->arrived[1], 0);
}

void rv_arrive(rendezvous *r, int side)
{
	assert(side == 0 || side == 1);
	msem_post(&r->arrived[side]);
	msem_wait(&r->arrived[1 - side]);
}

void rv_destroy(rendezvous *r)
{
	msem_destroy(&r->arrived[0]);
	msem_destroy(&r->arrived[1]);
}

/* ==================================================================
 * the reader-writer lock
 *
 * `room_empty` is a lock over the whole room: whoever holds it has the data
 * structure to themselves. Readers take it collectively -- the first one in
 * takes it, the last one out releases it, and `mutex` makes that counting
 * safe. A writer takes it alone. That much is the textbook
 * reader-preferring lock and it starves writers: while any reader is in the
 * room, an arriving reader increments the count and walks in, so the count
 * never reaches zero and the writer never gets `room_empty`.
 *
 * `turnstile` is the whole of the fix. Every reader must pass through it on
 * the way in, and does not hold it -- wait, post, in and out. A writer takes
 * it and *keeps* it until it is finished. So a waiting writer's first act is
 * to shut the door behind the readers already in the room; they drain,
 * `room_empty` is released, the writer works, and only then does the door
 * open again.
 *
 * That is three lines of difference and it converts "a writer may never run"
 * into "a writer runs after at most the readers already inside". It costs
 * reader throughput under a write-heavy load, which is the trade the handout
 * asks you to state.
 * ================================================================== */

void rw_init(rwlock *l)
{
	l->readers = 0;
	msem_init(&l->mutex, 1);
	msem_init(&l->room_empty, 1);
	msem_init(&l->turnstile, 1);
}

void rw_acquire_read(rwlock *l)
{
	msem_wait(&l->turnstile);           /* held shut by a waiting writer */
	msem_post(&l->turnstile);

	msem_wait(&l->mutex);
	if (++l->readers == 1)
		msem_wait(&l->room_empty);  /* the first reader locks the room */
	msem_post(&l->mutex);
}

void rw_release_read(rwlock *l)
{
	msem_wait(&l->mutex);
	if (--l->readers == 0)
		msem_post(&l->room_empty);  /* the last one unlocks it */
	msem_post(&l->mutex);
}

void rw_acquire_write(rwlock *l)
{
	msem_wait(&l->turnstile);           /* and keep it */
	msem_wait(&l->room_empty);
}

void rw_release_write(rwlock *l)
{
	msem_post(&l->turnstile);
	msem_post(&l->room_empty);
}

void rw_destroy(rwlock *l)
{
	msem_destroy(&l->turnstile);
	msem_destroy(&l->room_empty);
	msem_destroy(&l->mutex);
}

/* ==================================================================
 * dining philosophers
 *
 * One binary semaphore per fork, plus a footman who admits at most n - 1
 * philosophers to the table at a time.
 *
 * The footman is the entire solution and it is worth being precise about
 * which of the four necessary conditions it breaks. It does not touch mutual
 * exclusion (a fork is still held by one philosopher), hold-and-wait (a
 * philosopher still holds its left fork while waiting for its right), or
 * no-preemption (nothing takes a fork back). It breaks CIRCULAR WAIT, by
 * making the cycle unconstructible: a cycle needs all n philosophers each
 * holding one fork, and with only n - 1 of them at the table there is always
 * one philosopher not holding anything, so some philosopher has a free fork
 * on one side.
 *
 * The other standard answer -- have the last philosopher pick up its right
 * fork first -- breaks the same condition by a different route, and
 * DEADLOCK.md asks you to compare them.
 * ================================================================== */

void phil_init(phils *p, int n)
{
	int i;

	assert(n >= 2 && n <= PHIL_MAX);
	p->n = n;
	for (i = 0; i < n; i++)
		msem_init(&p->fork[i], 1);
	msem_init(&p->footman, n - 1);
}

void phil_pickup(phils *p, int w)
{
	msem_wait(&p->footman);
	msem_wait(&p->fork[w]);
	msem_wait(&p->fork[(w + 1) % p->n]);
}

void phil_putdown(phils *p, int w)
{
	msem_post(&p->fork[(w + 1) % p->n]);
	msem_post(&p->fork[w]);
	msem_post(&p->footman);
}

void phil_destroy(phils *p)
{
	int i;

	msem_destroy(&p->footman);
	for (i = 0; i < p->n; i++)
		msem_destroy(&p->fork[i]);
}
