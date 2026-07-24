/*
 * toolkit.h -- the contract for Lab 5 Part 4.
 *
 * Five things: a counting semaphore, a reusable barrier, a rendezvous, a
 * reader-writer lock that does not starve writers, and a deadlock-free
 * dining philosophers table. The harness includes THIS header and links
 * against YOUR toolkit.c, so nothing declared here may change.
 *
 * The structs are spelled out because the harness has to be able to declare
 * one of each. It reads no field of any of them: every case goes through the
 * functions, and what it checks is behaviour. The field names are the
 * reference's; if your design wants them to mean something else, that is your
 * business, as long as the header text is unchanged and the size is enough.
 *
 * THE ONE RULE THAT IS NOT CHECKED BY ANY TEST. The semaphore is built on a
 * pthreads mutex and condition variable. Everything else -- barrier,
 * rendezvous, reader-writer lock, philosophers -- is built on the semaphore
 * and on nothing else. No pthread_mutex_lock, no pthread_cond_wait, no
 * atomics below the semaphore layer. That is the whole point of OSTEP ch. 31:
 * the semaphore is a single primitive that can express every one of these,
 * and finding that out requires actually doing it. No test can tell; it is on
 * your honour, and it is stated in the answer key too.
 *
 * WHY PTHREADS AND NOT YOUR PART 2 LOCKS. Two reasons. Parts are independent
 * here -- a student stuck on Part 2 must still be able to do Part 4 -- and,
 * more importantly, helgrind understands the pthreads primitives completely
 * and cannot audit a lock you built yourself out of atomics. Building the
 * semaphore on pthreads is what buys this part a helgrind verdict worth
 * having. Part 2 explains the other half of that story.
 */
#ifndef TOOLKIT_H
#define TOOLKIT_H

#include <pthread.h>

/* The largest thread count the harness will use with a barrier. */
#define TK_MAX_THREADS 32

/* The largest table phil_init() will be asked for. */
#define PHIL_MAX 16

/* ==================================================================
 * the semaphore
 * ================================================================== */

struct msem {
	int value;              /* as in ch. 31: how many waits will not block */
	pthread_mutex_t lock;
	pthread_cond_t cv;
};

typedef struct msem msem;

/*
 * Prepare a semaphore with the given initial value, which is >= 0. Not
 * thread-safe: call it before any thread touches the semaphore.
 */
void msem_init(msem *s, int value);

/*
 * Decrement the value. If the value is already zero, block until a post
 * makes one available. Waiting must be *waiting*: a msem_wait that spins on
 * the value burns a core and the harness measures that it does not.
 */
void msem_wait(msem *s);

/*
 * Increment the value and wake one waiter, if there is one. Never blocks.
 *
 * Signal from inside the critical section, before the unlock, exactly as in
 * Part 3 and for the same reason: it is the form ch. 31 uses, and helgrind
 * reports the other form.
 */
void msem_post(msem *s);

/*
 * Release the mutex and the condition variable. Not thread-safe: call it
 * after every thread has finished with the semaphore.
 */
void msem_destroy(msem *s);

/* ==================================================================
 * the barrier
 *
 * n threads call mbarrier_wait; none of them returns from it until all n
 * have called it. Then the barrier is ready for the next round, with the
 * same n, for as many rounds as anyone cares to run. REUSABLE IS THE WHOLE
 * EXERCISE: a barrier that works once and jams, or lets a fast thread lap
 * the others on round two, is the classic bug and the harness has a case
 * for it.
 * ================================================================== */

struct mbarrier {
	int n;                  /* threads per round, as passed to init */
	int arrived;            /* how many have arrived in this round */
	msem mutex;             /* binary semaphore guarding `arrived` */
	msem gate_a;            /* the reference uses two turnstiles */
	msem gate_b;
};

typedef struct mbarrier mbarrier;

/* n is between 1 and TK_MAX_THREADS. Not thread-safe. */
void mbarrier_init(mbarrier *b, int n);

/* Block until all n threads have called this. Then start the next round. */
void mbarrier_wait(mbarrier *b);

/* Not thread-safe: call it once everyone is done. */
void mbarrier_destroy(mbarrier *b);

/* ==================================================================
 * the rendezvous
 *
 * Two threads, one on each side. Neither returns from rv_arrive until both
 * have called it, so whatever side 0 did before its call has happened before
 * whatever side 1 does after its own -- and the other way round. A
 * rendezvous is a barrier for two, and it is worth building separately
 * because it is the shape almost every two-party handoff takes.
 *
 * One use per rv_init.
 * ================================================================== */

struct rendezvous {
	msem arrived[2];
};

typedef struct rendezvous rendezvous;

/* Not thread-safe. */
void rv_init(rendezvous *r);

/* side is 0 or 1, and each side is used by exactly one thread. */
void rv_arrive(rendezvous *r, int side);

/* Not thread-safe. */
void rv_destroy(rendezvous *r);

/* ==================================================================
 * the reader-writer lock
 *
 * Any number of readers, or one writer, never both.
 *
 * AND: A WRITER THAT IS WAITING MUST NOT BE OVERTAKEN BY READERS THAT ARRIVE
 * AFTER IT. The textbook reader-preferring lock -- the first one in ch. 31 --
 * does not have this property: while any reader holds the lock, new readers
 * walk straight in, and a writer waiting for the count to reach zero can
 * wait for ever. OSTEP is right that this is a *property* of that design and
 * not a bug in it; the harness's writer-starvation case exists because THIS
 * lab's contract asks for the other design, not because the textbook one is
 * broken. Read the handout's section on it before you start, or you will
 * build the wrong one and the case that catches you will look unfair.
 * ================================================================== */

struct rwlock {
	int readers;            /* readers currently inside */
	msem mutex;             /* binary semaphore guarding `readers` */
	msem room_empty;        /* held while anyone is inside */
	msem turnstile;         /* a waiting writer holds this shut */
};

typedef struct rwlock rwlock;

/* Not thread-safe. */
void rw_init(rwlock *l);

/* Block until no writer holds or is waiting for the lock. */
void rw_acquire_read(rwlock *l);
void rw_release_read(rwlock *l);

/* Block until nobody holds the lock. Readers arriving from now on wait. */
void rw_acquire_write(rwlock *l);
void rw_release_write(rwlock *l);

/* Not thread-safe. */
void rw_destroy(rwlock *l);

/* ==================================================================
 * dining philosophers
 *
 * n philosophers around a table with n forks between them. PHILOSOPHER w
 * EATS WITH FORK w AND FORK (w + 1) % n -- the harness relies on that
 * numbering to check that no fork is ever in two hands at once, so it is
 * part of the contract and not a convention you may vary.
 *
 * phil_pickup(p, w) returns when philosopher w holds both of its forks.
 * phil_putdown(p, w) returns them.
 *
 * The requirement is that a table of philosophers looping on
 * pickup-eat-putdown for ever never deadlocks, and that at least two of them
 * can eat at the same time -- which rules out the solution where pickup is
 * one big lock over the whole table. State in DEADLOCK.md which of the four
 * necessary conditions your solution breaks.
 * ================================================================== */

struct phils {
	int n;                  /* as passed to init, 2 <= n <= PHIL_MAX */
	msem fork[PHIL_MAX];
	msem footman;           /* the reference's; use it or do not */
};

typedef struct phils phils;

/* Not thread-safe. */
void phil_init(phils *p, int n);

/* Returns holding forks w and (w + 1) % n. 0 <= w < n. */
void phil_pickup(phils *p, int w);

/* Puts both of them back. */
void phil_putdown(phils *p, int w);

/* Not thread-safe. */
void phil_destroy(phils *p);

#endif /* TOOLKIT_H */
