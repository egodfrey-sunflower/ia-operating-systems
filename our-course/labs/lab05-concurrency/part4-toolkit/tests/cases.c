/*
 * cases.c -- the Part 4 test driver.
 *
 *      cases <case-name>
 *
 * One case, one process, exit 0 if it passed, 1 if it failed, 2 if the name
 * is not a case. Complaints go to stderr, four spaces in.
 *
 * Nothing here reads a field of any of the five structures. Every case goes
 * through the functions in toolkit.h, and every check is on behaviour: how
 * many threads were inside at once, whether a thread that should have
 * blocked came back early, whether a fork was in two hands, how long a
 * writer waited.
 *
 * The test-side bookkeeping deliberately does NOT use the student's
 * semaphore. Counters are atomics and the handful of places that need a
 * thread to wait for another use pthreads directly. A harness that
 * synchronised itself with the primitive under test would hang instead of
 * failing, and would report the hang as the student's deadlock.
 *
 * Three failure shapes, and run.sh keeps them apart:
 *
 *   WRONG ANSWER -- a count is off, two threads were inside a lock that
 *       admits one, a barrier let somebody out early. Usually a `while`
 *       that became an `if`, or a missing turnstile.
 *
 *   DEADLOCK -- the case had to be killed. Everybody is waiting: philosophers
 *       in a circle, a rendezvous that waits before it posts, a barrier
 *       whose gate nobody opened.
 *
 *   A NUMBER THE CASE PRINTS -- writer starvation is neither of the above.
 *       The writer never runs but the readers run for ever, so the process
 *       is not deadlocked and never will be. That case measures and reports.
 *
 * Scaling knobs: TK_ITERS and TK_THREADS, so run.sh can shrink the work
 * under helgrind.
 */

#define _POSIX_C_SOURCE 200809L

#include "toolkit.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures;

static void fail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "    ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	failures++;
}

#define CHECK(cond, ...) do { if (!(cond)) fail(__VA_ARGS__); } while (0)
#define REQUIRE(cond, ...) do { \
	if (!(cond)) { fail(__VA_ARGS__); return; } \
} while (0)

static int envint(const char *name, int dflt)
{
	const char *s = getenv(name);
	long v;

	if (!s || !*s)
		return dflt;
	v = strtol(s, NULL, 10);
	if (v < 1 || v > 100000000)
		return dflt;
	return (int)v;
}

static void nap_ms(long ms)
{
	struct timespec ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) == -1)
		;
}

static double now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static double cpu_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* Highest value ever seen in *max, updated race-free. */
static void note_max(int *max, int v)
{
	int cur = __atomic_load_n(max, __ATOMIC_RELAXED);

	while (v > cur) {
		if (__atomic_compare_exchange_n(max, &cur, v, 1,
						__ATOMIC_RELAXED,
						__ATOMIC_RELAXED))
			return;
	}
}

/* ==================================================================
 * the semaphore
 * ================================================================== */

static msem sem;
static long counter;            /* deliberately not atomic: the semaphore
                                 * is what makes the increments safe */
static int iters, nthreads;
static pthread_barrier_t startline;

/*
 * Contention has to be arranged, not hoped for. Without a start line the
 * first thread can be most of the way through its loop before the last one
 * is created, and threads that do not overlap cannot lose an update -- Part
 * 2 found the same thing and its negative control exists to keep checking
 * it. This is a pthreads barrier, not the student's: the harness must not
 * synchronise itself with the primitive it is testing.
 */
static void startline_init(int n)
{
	pthread_barrier_init(&startline, NULL, (unsigned)n);
}

/* A short window of work, so that two threads inside a room that should
 * hold one are actually inside it at the same moment. */
static void dawdle(int spins)
{
	volatile int x = 0;
	int i;

	for (i = 0; i < spins; i++)
		x = x + 1;
}

/*
 * Read, pause, write back. The pause is what makes a lost update likely
 * rather than merely possible: a bare counter++ compiles to three
 * instructions and two threads on two cores can run it a hundred thousand
 * times each without ever interleaving inside it. Part 2 found the same
 * thing and its negative control exists to keep proving that this workload
 * really can lose an update on the machine it is running on. So does the
 * control below.
 */
static void critical_bump(void)
{
	long v = counter;
	int k;

	for (k = 0; k < 32; k++)
		__asm__ __volatile__("" ::: "memory");
	counter = v + 1;
}

static void *sem_bumper(void *a)
{
	int i;

	(void)a;
	pthread_barrier_wait(&startline);
	for (i = 0; i < iters; i++) {
		msem_wait(&sem);
		critical_bump();
		msem_post(&sem);
	}
	return NULL;
}

static void *unlocked_bumper(void *a)
{
	int i;

	(void)a;
	pthread_barrier_wait(&startline);
	for (i = 0; i < iters; i++)
		critical_bump();
	return NULL;
}

/*
 * The negative control, run first. The same workload with no semaphore at
 * all MUST get the wrong answer. If it does not -- if this machine is so
 * short of cores, or so lightly loaded, that two unsynchronised threads
 * cannot lose an update -- then the cases that follow are not evidence that
 * anything is being excluded, and run.sh says so.
 */
static void c_race_control(void)
{
	pthread_t t[TK_MAX_THREADS];
	long i, want;
	int attempt;

	nthreads = envint("TK_THREADS", 8);
	if (nthreads > TK_MAX_THREADS)
		nthreads = TK_MAX_THREADS;
	/* Eight threads and 200 000 increments each is not a round number
	 * chosen for looking serious: it is where the unsynchronised counter
	 * loses an update on every one of twenty runs on an idle two-core
	 * machine. At 4 x 50 000 it comes out exactly right about half the
	 * time, and a control that only sometimes controls is worse than
	 * none. */
	iters = envint("TK_ITERS", 200000);
	want = (long)nthreads * iters;

	/*
	 * Up to three attempts, and one loss is enough. The claim being
	 * tested is existential -- "this machine CAN lose an update under
	 * this workload" -- so a single attempt that happens not to
	 * interleave is not evidence against it. On a saturated machine
	 * (four unrelated busy loops on the same two cores) one attempt in
	 * four came out exactly right and three attempts in a row never did.
	 */
	for (attempt = 0; attempt < 3; attempt++) {
		counter = 0;
		startline_init(nthreads);
		for (i = 0; i < nthreads; i++)
			REQUIRE(pthread_create(&t[i], NULL, unlocked_bumper,
					       NULL) == 0,
				"pthread_create failed");
		for (i = 0; i < nthreads; i++)
			pthread_join(t[i], NULL);
		pthread_barrier_destroy(&startline);
		if (counter != want)
			return;                 /* the control controls */
	}
	fail("%d threads incremented a shared counter %d times each with no "
	     "synchronisation whatsoever, three times over, and the answer "
	     "came out exactly right (%ld) every time. Nothing is wrong with "
	     "your semaphore -- this is a statement about this machine. Until "
	     "this case fails, a pass on the cases below is not evidence that "
	     "anything is being excluded.",
	     nthreads, iters, counter);
}

/*
 * A semaphore initialised to 1 is a mutex. N threads, M increments each, and
 * the answer must be exactly N x M -- the plan's outcome test, applied to
 * the primitive everything else in this part is built on.
 */
static void c_sem_mutex(void)
{
	pthread_t t[TK_MAX_THREADS];
	long i, want;

	nthreads = envint("TK_THREADS", 8);
	if (nthreads > TK_MAX_THREADS)
		nthreads = TK_MAX_THREADS;
	/* Eight threads and 200 000 increments each is not a round number
	 * chosen for looking serious: it is where the control below fails on
	 * every one of twenty runs on a two-core machine. At 4 x 50 000 the
	 * unsynchronised counter comes out exactly right about half the time,
	 * and a control that only sometimes controls is worse than none. */
	iters = envint("TK_ITERS", 200000);
	counter = 0;
	msem_init(&sem, 1);
	startline_init(nthreads);
	for (i = 0; i < nthreads; i++)
		REQUIRE(pthread_create(&t[i], NULL, sem_bumper, NULL) == 0,
			"pthread_create failed");
	for (i = 0; i < nthreads; i++)
		pthread_join(t[i], NULL);
	pthread_barrier_destroy(&startline);
	want = (long)nthreads * iters;
	CHECK(counter == want,
	      "%d threads incremented a counter %d times each under a "
	      "semaphore initialised to 1, and it ended at %ld instead of "
	      "%ld -- %ld updates were lost. Two threads were inside at once. "
	      "The usual cause is msem_wait deciding to proceed on a value it "
	      "read before it was signalled: wait in a `while`, not an `if`.",
	      nthreads, iters, counter, want, want - counter);
	msem_destroy(&sem);
}

/* ------------------------------------------------------------------
 * a counting semaphore counts
 * ------------------------------------------------------------------ */

#define ROOM 3

static int occupancy;
static int occupancy_max;
static int over_capacity;
static int over_capacity_seen;

static void *room_visitor(void *a)
{
	int i;

	(void)a;
	pthread_barrier_wait(&startline);
	for (i = 0; i < iters; i++) {
		int in;

		msem_wait(&sem);
		in = __atomic_add_fetch(&occupancy, 1, __ATOMIC_ACQ_REL);
		note_max(&occupancy_max, in);
		if (in > ROOM) {
			__atomic_store_n(&over_capacity, 1, __ATOMIC_RELAXED);
			note_max(&over_capacity_seen, in);
		}
		/* Stay in the room for a while -- tens of microseconds, not
		 * one. This is the number that decides whether the case can
		 * see an over-admitting semaphore at all: with a window of a
		 * microsecond, eight threads on TWO cores are never inside
		 * together even when nothing is stopping them, because only
		 * two of them are running. Measured: at dawdle(200) a
		 * semaphore that admits everybody passed this case; at
		 * dawdle(20000) it fails it every time. */
		dawdle(20000);
		__atomic_sub_fetch(&occupancy, 1, __ATOMIC_ACQ_REL);
		msem_post(&sem);
	}
	return NULL;
}

/*
 * A semaphore initialised to 3 admits three. Both directions are checked and
 * both matter: never more than three inside (a semaphore whose wait proceeds
 * on a stale value), and at some point more than one inside (a "semaphore"
 * that ignores its initial value and behaves as a mutex would pass the first
 * check trivially and is not a counting semaphore).
 */
static void c_sem_room(void)
{
	pthread_t t[TK_MAX_THREADS];
	long i;

	nthreads = envint("TK_THREADS", 8);
	if (nthreads > TK_MAX_THREADS)
		nthreads = TK_MAX_THREADS;
	iters = envint("TK_ITERS", 300);
	occupancy = 0;
	occupancy_max = 0;
	over_capacity = 0;
	over_capacity_seen = 0;
	msem_init(&sem, ROOM);
	startline_init(nthreads);
	for (i = 0; i < nthreads; i++)
		REQUIRE(pthread_create(&t[i], NULL, room_visitor, NULL) == 0,
			"pthread_create failed");
	for (i = 0; i < nthreads; i++)
		pthread_join(t[i], NULL);
	pthread_barrier_destroy(&startline);
	CHECK(!over_capacity,
	      "a semaphore initialised to %d let %d threads inside at once. "
	      "The value went below zero, which means a msem_wait returned "
	      "without there having been a value to take.",
	      ROOM, over_capacity_seen);
	CHECK(occupancy_max >= 2,
	      "%d threads went through a semaphore initialised to %d and no "
	      "two of them were ever inside at the same time (the most seen "
	      "was %d). That is a mutex, not a counting semaphore: msem_wait "
	      "is blocking while the value is still positive, or msem_init is "
	      "ignoring its argument.",
	      nthreads, ROOM, occupancy_max);
	msem_destroy(&sem);
}

/* ------------------------------------------------------------------
 * a wait on zero blocks
 * ------------------------------------------------------------------ */

static volatile int posted;
static volatile int returned_early;

static void *zero_waiter(void *a)
{
	(void)a;
	msem_wait(&sem);
	if (!__atomic_load_n(&posted, __ATOMIC_ACQUIRE))
		returned_early = 1;
	return NULL;
}

static void c_sem_blocks(void)
{
	pthread_t t;

	msem_init(&sem, 0);
	posted = 0;
	returned_early = 0;
	REQUIRE(pthread_create(&t, NULL, zero_waiter, NULL) == 0,
		"pthread_create failed");
	nap_ms(250);
	__atomic_store_n(&posted, 1, __ATOMIC_RELEASE);
	msem_post(&sem);
	pthread_join(t, NULL);
	CHECK(!returned_early,
	      "msem_wait returned from a semaphore whose value was 0, before "
	      "anything had posted to it. A wait on zero blocks; that is the "
	      "whole of the primitive.");
	msem_destroy(&sem);
}

/* ------------------------------------------------------------------
 * ...and blocks by sleeping, not by spinning
 * ------------------------------------------------------------------ */

#define PARK_WAITERS 4
#define PARK_MS 300

static int still_waiting;

static void *park_waiter(void *a)
{
	(void)a;
	msem_wait(&sem);
	__atomic_sub_fetch(&still_waiting, 1, __ATOMIC_ACQ_REL);
	return NULL;
}

/*
 * The Part 2 processor-time measurement, reused. Four threads wait 300 ms
 * for a semaphore nobody posts to. A msem_wait built on a condition variable
 * costs no processor time at all while it waits; one that loops on the value
 * costs four cores' worth. The threshold sits far from both.
 */
static void c_sem_parks(void)
{
	pthread_t t[PARK_WAITERS];
	double cpu0, wall0, used, elapsed;
	int i, left;

	msem_init(&sem, 0);
	still_waiting = PARK_WAITERS;
	cpu0 = cpu_ms();
	wall0 = now_ms();
	for (i = 0; i < PARK_WAITERS; i++)
		REQUIRE(pthread_create(&t[i], NULL, park_waiter, NULL) == 0,
			"pthread_create failed");
	nap_ms(PARK_MS);
	used = cpu_ms() - cpu0;
	elapsed = now_ms() - wall0;
	left = __atomic_load_n(&still_waiting, __ATOMIC_ACQUIRE);
	for (i = 0; i < PARK_WAITERS; i++)
		msem_post(&sem);
	for (i = 0; i < PARK_WAITERS; i++)
		pthread_join(t[i], NULL);
	CHECK(left == PARK_WAITERS,
	      "%d of %d threads that called msem_wait on a semaphore at zero "
	      "had returned from it %.0f ms later, without anything having "
	      "posted. This case measures how a thread waits; those threads "
	      "did not wait at all.",
	      PARK_WAITERS - left, PARK_WAITERS, elapsed);
	CHECK(used < elapsed * 0.25,
	      "%d threads waited %.0f ms for a semaphore at zero and the "
	      "process burned %.0f ms of processor time doing it -- more than "
	      "a quarter of one core. They are spinning on the value. "
	      "msem_wait must wait on the condition variable, which costs "
	      "nothing until somebody posts.",
	      PARK_WAITERS, elapsed, used);
	msem_destroy(&sem);
}

/* ==================================================================
 * the barrier
 * ================================================================== */

#define BAR_THREADS 8

static mbarrier bar;
static int at_barrier;
static int saw_fewer;
static int saw_fewer_n;

static void *gate_thread(void *a)
{
	long id = (long)a;
	int seen;

	nap_ms(id * 15);                /* arrive spread out over ~105 ms */
	__atomic_add_fetch(&at_barrier, 1, __ATOMIC_ACQ_REL);
	mbarrier_wait(&bar);
	seen = __atomic_load_n(&at_barrier, __ATOMIC_ACQUIRE);
	if (seen < BAR_THREADS) {
		__atomic_store_n(&saw_fewer, 1, __ATOMIC_RELAXED);
		__atomic_store_n(&saw_fewer_n, seen, __ATOMIC_RELAXED);
	}
	return NULL;
}

/*
 * Nobody leaves until everybody has arrived. The threads arrive 15 ms apart,
 * so a barrier that does not actually block lets the early ones out while
 * the count is still 1 or 2 and this fails on the first round -- no
 * repetition needed and no luck involved.
 */
static void c_barrier_gate(void)
{
	pthread_t t[BAR_THREADS];
	long i;

	at_barrier = 0;
	saw_fewer = 0;
	saw_fewer_n = 0;
	mbarrier_init(&bar, BAR_THREADS);
	for (i = 0; i < BAR_THREADS; i++)
		REQUIRE(pthread_create(&t[i], NULL, gate_thread, (void *)i) == 0,
			"pthread_create failed");
	for (i = 0; i < BAR_THREADS; i++)
		pthread_join(t[i], NULL);
	CHECK(!saw_fewer,
	      "a thread came out of mbarrier_wait when only %d of %d threads "
	      "had arrived. The barrier is not blocking: the nth arrival is "
	      "what opens the gate, and until then everybody waits.",
	      saw_fewer_n, BAR_THREADS);
	mbarrier_destroy(&bar);
}

/* ------------------------------------------------------------------
 * ...and again, and again
 * ------------------------------------------------------------------ */

static mbarrier bar2;
static int phase[BAR_THREADS];
static int rounds;
static int wrong_phase;
static int wrong_phase_saw, wrong_phase_want;

static void *round_thread(void *a)
{
	long id = (long)a;
	int r, j;

	for (r = 0; r < rounds; r++) {
		__atomic_store_n(&phase[id], r, __ATOMIC_RELAXED);
		mbarrier_wait(&bar);
		for (j = 0; j < BAR_THREADS; j++) {
			int p = __atomic_load_n(&phase[j], __ATOMIC_RELAXED);

			if (p != r) {
				__atomic_store_n(&wrong_phase, 1,
						 __ATOMIC_RELAXED);
				__atomic_store_n(&wrong_phase_saw, p,
						 __ATOMIC_RELAXED);
				__atomic_store_n(&wrong_phase_want, r,
						 __ATOMIC_RELAXED);
			}
		}
		/* The second barrier is the harness's, not the student's
		 * problem: without it a fast thread would legitimately start
		 * writing round r + 1 into its slot while a slow one is
		 * still reading round r. */
		mbarrier_wait(&bar2);
	}
	return NULL;
}

/*
 * The reusable-barrier case. A barrier that is correct for one round and
 * jams on the second is the classic bug in ch. 31, and it is invisible to
 * every test that uses the barrier once. Two hundred rounds, eight threads,
 * and every thread checks that every other thread is in the same round it
 * is. A one-turnstile barrier either lets a thread lap the others -- caught
 * here as a wrong phase -- or wedges, and run.sh calls that a deadlock.
 */
static void c_barrier_rounds(void)
{
	pthread_t t[BAR_THREADS];
	long i;

	rounds = envint("TK_ITERS", 200);
	if (rounds > 5000)
		rounds = 5000;
	wrong_phase = 0;
	for (i = 0; i < BAR_THREADS; i++)
		phase[i] = -1;
	mbarrier_init(&bar, BAR_THREADS);
	mbarrier_init(&bar2, BAR_THREADS);
	for (i = 0; i < BAR_THREADS; i++)
		REQUIRE(pthread_create(&t[i], NULL, round_thread, (void *)i) == 0,
			"pthread_create failed");
	for (i = 0; i < BAR_THREADS; i++)
		pthread_join(t[i], NULL);
	CHECK(!wrong_phase,
	      "a thread came out of round %d of the barrier and found another "
	      "thread already in round %d. One of them went round the loop "
	      "twice while the other went round once: the gate that let the "
	      "first round out was still open when the fastest thread came "
	      "back to it. That is what the second turnstile is for.",
	      wrong_phase_want, wrong_phase_saw);
	mbarrier_destroy(&bar2);
	mbarrier_destroy(&bar);
}

/* ------------------------------------------------------------------
 * ...at the sizes the header promises, not just at eight
 * ------------------------------------------------------------------
 *
 * Every other barrier case in this file builds a barrier of exactly
 * BAR_THREADS. toolkit.h promises 1 <= n <= TK_MAX_THREADS, and a barrier
 * that ignores its argument entirely -- `b->n = 8` -- passes all of them.
 * So does one that is correct everywhere except at exactly two threads,
 * which is the shape a hand-written barrier is most likely to have.
 *
 * Four sizes: both ends of the contract and the two smallest interesting
 * values. Threads arrive staggered, so the check is not probabilistic: by
 * the time the first thread is allowed out, every one of the n must already
 * have arrived, and a barrier that opened early sees a smaller count.
 *
 * Three rounds each, with a per-round counter so nothing has to be reset
 * between rounds. A barrier that never opens at some n does not fail an
 * assertion here -- it hangs, and run.sh reports that distinctly as a
 * DEADLOCK, which is the right verdict for a barrier that waits for
 * arrivals that are never coming.
 */

#define SIZE_ROUNDS 3

static mbarrier bsz;
static int sz_n;
static int sz_arrived[SIZE_ROUNDS];
static int sz_short;            /* saw fewer than n arrivals */
static int sz_short_saw;

static void *size_thread(void *a)
{
	long id = (long)a;
	int r;

	for (r = 0; r < SIZE_ROUNDS; r++) {
		nap_ms((id % 4) * 5);   /* arrive spread out, every round */
		__atomic_add_fetch(&sz_arrived[r], 1, __ATOMIC_ACQ_REL);
		mbarrier_wait(&bsz);
		if (__atomic_load_n(&sz_arrived[r], __ATOMIC_ACQUIRE) < sz_n) {
			__atomic_store_n(&sz_short, sz_n, __ATOMIC_RELAXED);
			__atomic_store_n(&sz_short_saw,
					 __atomic_load_n(&sz_arrived[r],
							 __ATOMIC_ACQUIRE),
					 __ATOMIC_RELAXED);
		}
	}
	return NULL;
}

static void barrier_at(int n)
{
	pthread_t t[TK_MAX_THREADS];
	long i;

	sz_n = n;
	for (i = 0; i < SIZE_ROUNDS; i++)
		sz_arrived[i] = 0;
	mbarrier_init(&bsz, n);
	for (i = 0; i < n; i++)
		REQUIRE(pthread_create(&t[i], NULL, size_thread, (void *)i) == 0,
			"pthread_create failed");
	for (i = 0; i < n; i++)
		pthread_join(t[i], NULL);
	mbarrier_destroy(&bsz);
}

static void c_barrier_sizes(void)
{
	static const int sizes[] = { 1, 2, 3, TK_MAX_THREADS };
	size_t k;

	sz_short = 0;
	sz_short_saw = 0;
	for (k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
		barrier_at(sizes[k]);
		if (failures)
			return;
		CHECK(!sz_short,
		      "mbarrier_init was given n = %d and the barrier opened "
		      "with only %d thread(s) through the door. toolkit.h "
		      "promises 1 <= n <= %d, so n is an argument and not a "
		      "constant: a barrier that works at eight and not at %d "
		      "is not a barrier. (If instead this case never finished "
		      "at all, the barrier is waiting for arrivals that are "
		      "not coming, which is the same bug from the other side.)",
		      __atomic_load_n(&sz_short, __ATOMIC_RELAXED),
		      __atomic_load_n(&sz_short_saw, __ATOMIC_RELAXED),
		      TK_MAX_THREADS, sizes[k]);
		if (failures)
			return;
	}
}

/* ==================================================================
 * the rendezvous
 * ================================================================== */

#define RV_ROUNDS 40

static rendezvous rv;
static int rv_before[2];
static int rv_missed;
static pthread_barrier_t rv_sync;
static int rv_skew;

static void *rv_side(void *a)
{
	long side = (long)a;
	int r;

	for (r = 0; r < RV_ROUNDS; r++) {
		pthread_barrier_wait(&rv_sync);         /* harness-side */
		if (side == rv_skew)
			nap_ms(3);
		__atomic_store_n(&rv_before[side], r + 1, __ATOMIC_RELEASE);
		rv_arrive(&rv, (int)side);
		if (__atomic_load_n(&rv_before[1 - side], __ATOMIC_ACQUIRE)
		    != r + 1)
			__atomic_store_n(&rv_missed, 1, __ATOMIC_RELAXED);
		pthread_barrier_wait(&rv_sync);
		if (side == 0) {
			rv_destroy(&rv);
			rv_init(&rv);
			rv_skew = 1 - rv_skew;
		}
		pthread_barrier_wait(&rv_sync);
	}
	return NULL;
}

/*
 * Whatever one side did before arriving has happened before whatever the
 * other side does after arriving, in both directions. The 3 ms skew
 * alternates sides between rounds, so a rv_arrive that does not block is
 * caught in the first round or two rather than eventually.
 */
static void c_rendezvous(void)
{
	pthread_t t[2];
	long i;

	rv_missed = 0;
	rv_before[0] = rv_before[1] = 0;
	rv_skew = 0;
	pthread_barrier_init(&rv_sync, NULL, 2);
	rv_init(&rv);
	for (i = 0; i < 2; i++)
		REQUIRE(pthread_create(&t[i], NULL, rv_side, (void *)i) == 0,
			"pthread_create failed");
	for (i = 0; i < 2; i++)
		pthread_join(t[i], NULL);
	CHECK(!rv_missed,
	      "a thread returned from rv_arrive before the other side had "
	      "arrived: it could not see work the other side does immediately "
	      "before its own call. rv_arrive must not return until both "
	      "sides have called it.");
	rv_destroy(&rv);
	pthread_barrier_destroy(&rv_sync);
}

/* ==================================================================
 * the reader-writer lock
 * ================================================================== */

static rwlock rwl;

/* ------------------------------------------------------------------
 * readers share
 * ------------------------------------------------------------------ */

#define RW_READERS 4

static int readers_in;
static int readers_in_max;

static void *sharing_reader(void *a)
{
	int in;

	(void)a;
	rw_acquire_read(&rwl);
	in = __atomic_add_fetch(&readers_in, 1, __ATOMIC_ACQ_REL);
	note_max(&readers_in_max, in);
	nap_ms(150);
	__atomic_sub_fetch(&readers_in, 1, __ATOMIC_ACQ_REL);
	rw_release_read(&rwl);
	return NULL;
}

/*
 * Four readers each hold the lock for 150 ms with nothing else running. If
 * the lock is really a mutex in a wig they go through one at a time and the
 * most ever inside is 1.
 */
static void c_rw_shared(void)
{
	pthread_t t[RW_READERS];
	long i;

	readers_in = 0;
	readers_in_max = 0;
	rw_init(&rwl);
	for (i = 0; i < RW_READERS; i++)
		REQUIRE(pthread_create(&t[i], NULL, sharing_reader, NULL) == 0,
			"pthread_create failed");
	for (i = 0; i < RW_READERS; i++)
		pthread_join(t[i], NULL);
	CHECK(readers_in_max >= 2,
	      "%d readers each held the read lock for 150 ms and the most "
	      "that were ever inside together was %d. Readers do not exclude "
	      "readers: only the FIRST one in takes the room, and only the "
	      "LAST one out releases it.",
	      RW_READERS, readers_in_max);
	rw_destroy(&rwl);
}

/* ------------------------------------------------------------------
 * writers exclude everybody
 * ------------------------------------------------------------------ */

#define RW_WRITERS 4

static long guarded_a, guarded_b;       /* kept equal outside a write */
static int torn_read;
static int writers_running;
static int reader_saw_writer;
static int writers_overlapped;

static void *rw_writer(void *a)
{
	int i;

	(void)a;
	for (i = 0; i < iters; i++) {
		int in;

		rw_acquire_write(&rwl);
		in = __atomic_add_fetch(&writers_running, 1, __ATOMIC_ACQ_REL);
		if (in > 1)
			__atomic_store_n(&writers_overlapped, 1,
					 __ATOMIC_RELAXED);
		/* Deliberately not atomic and deliberately not simultaneous:
		 * outside the write lock the two must always agree, and
		 * inside it they briefly do not. */
		guarded_a++;
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
		guarded_b++;
		__atomic_sub_fetch(&writers_running, 1, __ATOMIC_ACQ_REL);
		rw_release_write(&rwl);
	}
	return NULL;
}

/*
 * The readers' stop flag, guarded by a plain pthreads mutex rather than by
 * an atomic. This case is one of the two run under helgrind, and helgrind
 * does not model the __atomic builtins: it would report the harness's own
 * stop flag as a race and there would be nothing to fix. Everything else
 * shared by these threads is inside the lock under test, where helgrind gets
 * its happens-before from the pthreads primitives the semaphore is built on.
 */
static pthread_mutex_t stop_lock = PTHREAD_MUTEX_INITIALIZER;
static int rw_stop;

static int stopped(void)
{
	int s;

	pthread_mutex_lock(&stop_lock);
	s = rw_stop;
	pthread_mutex_unlock(&stop_lock);
	return s;
}

static void set_stopped(void)
{
	pthread_mutex_lock(&stop_lock);
	rw_stop = 1;
	pthread_mutex_unlock(&stop_lock);
}

static void *rw_reader(void *a)
{
	(void)a;
	while (!stopped()) {
		long x, y;

		rw_acquire_read(&rwl);
		if (__atomic_load_n(&writers_running, __ATOMIC_ACQUIRE) > 0)
			__atomic_store_n(&reader_saw_writer, 1,
					 __ATOMIC_RELAXED);
		x = guarded_a;
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
		y = guarded_b;
		if (x != y)
			__atomic_store_n(&torn_read, 1, __ATOMIC_RELAXED);
		rw_release_read(&rwl);
	}
	return NULL;
}

/*
 * The exclusion half. Writers update two counters that must agree outside
 * the write lock; readers check they agree. A reader that gets in during a
 * write sees them disagree, and a writer that gets in during another write
 * loses updates.
 */
static void c_rw_excl(void)
{
	pthread_t wt[RW_WRITERS], rt[RW_READERS];
	long i, want;

	iters = envint("TK_ITERS", 20000);
	guarded_a = guarded_b = 0;
	torn_read = 0;
	writers_running = 0;
	writers_overlapped = 0;
	reader_saw_writer = 0;
	rw_stop = 0;
	rw_init(&rwl);
	for (i = 0; i < RW_READERS; i++)
		REQUIRE(pthread_create(&rt[i], NULL, rw_reader, NULL) == 0,
			"pthread_create failed");
	for (i = 0; i < RW_WRITERS; i++)
		REQUIRE(pthread_create(&wt[i], NULL, rw_writer, NULL) == 0,
			"pthread_create failed");
	for (i = 0; i < RW_WRITERS; i++)
		pthread_join(wt[i], NULL);
	set_stopped();
	for (i = 0; i < RW_READERS; i++)
		pthread_join(rt[i], NULL);
	want = (long)RW_WRITERS * iters;
	CHECK(!torn_read,
	      "a reader holding the read lock saw the two guarded values "
	      "disagree. A writer was inside at the same time: rw_acquire_read "
	      "must not return while a writer holds the lock.");
	CHECK(!reader_saw_writer,
	      "a reader held the read lock while a writer held the write "
	      "lock. Many readers OR one writer, never both.");
	CHECK(!writers_overlapped,
	      "two writers held the write lock at the same time.");
	CHECK(guarded_a == want && guarded_b == want,
	      "%d writers incremented the guarded pair %d times each; it "
	      "should have ended at %ld and ended at %ld/%ld. Updates were "
	      "lost, so two writers were inside at once.",
	      RW_WRITERS, iters, want, guarded_a, guarded_b);
	rw_destroy(&rwl);
}

/* ------------------------------------------------------------------
 * a waiting writer is not overtaken for ever
 *
 * The interesting case in this part, and the one that decides which
 * reader-writer lock has been built.
 *
 * A background reader holds the read lock for 400 ms. Behind it a RELAY of
 * readers keeps the room permanently occupied: each relay reader hands the
 * baton to the next one BEFORE releasing its own read lock, so under a
 * reader-preferring lock the reader count provably never reaches zero and a
 * writer waiting for an empty room waits for ever. It is not a race; it is a
 * construction.
 *
 * A writer arrives 100 ms in. Under this lab's contract it must get the lock
 * once the readers already inside have drained -- 400 ms, near enough. Under
 * the textbook reader-preferring lock it never gets it at all, and the case
 * reports how long it waited rather than hanging, because a starved writer
 * is not a deadlock: everything else is making progress.
 * ------------------------------------------------------------------ */

#define RELAY 4
#define STARVE_HOLD_MS 400
#define STARVE_LIMIT_MS 3000

static pthread_mutex_t relay_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t relay_cv = PTHREAD_COND_INITIALIZER;
static int relay_turn;
static int relay_stop;
static int relay_acqs;                  /* read acquisitions by the relay */
static int writer_in;                   /* set when the writer has the lock */
static int acqs_when_writer_got_in;
static double writer_waited_ms;

static int entered[RELAY];      /* read acquisitions by relay reader i */

/*
 * The relay is what makes this case a construction rather than a race. A
 * reader hands the baton to the next reader and then WAITS UNTIL THAT READER
 * IS ACTUALLY INSIDE before letting go of its own read lock -- so under a
 * reader-preferring lock the count provably never reaches zero, and a writer
 * waiting for an empty room never gets one.
 *
 * The wait has a 50 ms limit, and that limit is the whole trick. Under a
 * lock that makes readers queue behind a waiting writer, the successor
 * cannot get in, the limit expires, this reader lets go, the room drains and
 * the writer runs. Under a reader-preferring lock the successor is inside
 * within microseconds and the limit never expires. Fifty milliseconds is
 * three orders of magnitude away from a thread wakeup on an idle machine and
 * two on a busy one, so neither direction is a coin flip.
 */
static void *relay_reader(void *a)
{
	long id = (long)a;
	int next = (int)((id + 1) % RELAY);

	for (;;) {
		struct timespec deadline;
		int target;

		pthread_mutex_lock(&relay_lock);
		while (relay_turn != id && !relay_stop)
			pthread_cond_wait(&relay_cv, &relay_lock);
		if (relay_stop) {
			pthread_mutex_unlock(&relay_lock);
			return NULL;
		}
		pthread_mutex_unlock(&relay_lock);

		rw_acquire_read(&rwl);

		pthread_mutex_lock(&relay_lock);
		entered[id]++;
		relay_acqs++;
		target = entered[next] + 1;
		relay_turn = next;
		pthread_cond_broadcast(&relay_cv);
		clock_gettime(CLOCK_REALTIME, &deadline);
		deadline.tv_nsec += 50 * 1000000L;
		if (deadline.tv_nsec >= 1000000000L) {
			deadline.tv_nsec -= 1000000000L;
			deadline.tv_sec++;
		}
		while (entered[next] < target && !relay_stop) {
			if (pthread_cond_timedwait(&relay_cv, &relay_lock,
						   &deadline) != 0)
				break;
		}
		pthread_mutex_unlock(&relay_lock);

		rw_release_read(&rwl);
	}
}

static int relay_count(void)
{
	int n;

	pthread_mutex_lock(&relay_lock);
	n = relay_acqs;
	pthread_mutex_unlock(&relay_lock);
	return n;
}

static void *background_reader(void *a)
{
	(void)a;
	rw_acquire_read(&rwl);
	nap_ms(STARVE_HOLD_MS);
	rw_release_read(&rwl);
	return NULL;
}

static void *starve_writer(void *a)
{
	double t0;

	(void)a;
	t0 = now_ms();
	rw_acquire_write(&rwl);
	writer_waited_ms = now_ms() - t0;
	__atomic_store_n(&acqs_when_writer_got_in, relay_count(),
			 __ATOMIC_RELAXED);
	__atomic_store_n(&writer_in, 1, __ATOMIC_RELEASE);
	nap_ms(20);
	rw_release_write(&rwl);
	return NULL;
}

static void c_rw_nostarve(void)
{
	pthread_t bt, wt, rt[RELAY];
	long i;
	double waited = 0;
	int got, acqs_before, acqs_after;

	relay_turn = 0;
	relay_stop = 0;
	relay_acqs = 0;
	for (i = 0; i < RELAY; i++)
		entered[i] = 0;
	writer_in = 0;
	acqs_when_writer_got_in = 0;
	rw_init(&rwl);

	REQUIRE(pthread_create(&bt, NULL, background_reader, NULL) == 0,
		"pthread_create failed");
	nap_ms(20);                     /* let it get the lock first */
	for (i = 0; i < RELAY; i++)
		REQUIRE(pthread_create(&rt[i], NULL, relay_reader, (void *)i) == 0,
			"pthread_create failed");
	nap_ms(80);
	acqs_before = relay_count();
	REQUIRE(pthread_create(&wt, NULL, starve_writer, NULL) == 0,
		"pthread_create failed");

	/* Wait for the writer, but not for ever: a starved writer never
	 * arrives and the relay would run until run.sh killed us. */
	for (waited = 0; waited < STARVE_LIMIT_MS; waited += 10) {
		if (__atomic_load_n(&writer_in, __ATOMIC_ACQUIRE))
			break;
		nap_ms(10);
	}
	got = __atomic_load_n(&writer_in, __ATOMIC_ACQUIRE);
	nap_ms(150);                    /* let the relay run on afterwards */
	acqs_after = relay_count();

	pthread_mutex_lock(&relay_lock);
	relay_stop = 1;
	pthread_cond_broadcast(&relay_cv);
	pthread_mutex_unlock(&relay_lock);
	for (i = 0; i < RELAY; i++)
		pthread_join(rt[i], NULL);
	pthread_join(wt, NULL);
	pthread_join(bt, NULL);

	CHECK(acqs_before >= 1,
	      "the harness's reader relay did not get going before the writer "
	      "arrived (%d acquisitions in 100 ms), so this case proves "
	      "nothing about starvation. If rw_acquire_read is blocking when "
	      "no writer is present, fix that first.", acqs_before);
	if (!got) {
		fail("a writer waited %d ms for the write lock while readers "
		     "kept arriving, and never got it. The readers made %d "
		     "acquisitions in that time, and the room was never empty "
		     "for a moment because each of them takes the read lock "
		     "before the previous one lets go.\n"
		     "    This is the textbook reader-preferring lock from "
		     "ch. 31: correct exclusion, and a writer can wait for "
		     "ever. This lab's contract asks for the other one -- a "
		     "writer that is waiting must shut the door behind the "
		     "readers already inside, so that readers arriving after "
		     "it queue up behind it. See toolkit.h and the handout.",
		     STARVE_LIMIT_MS, relay_count());
	} else {
		CHECK(writer_waited_ms < STARVE_LIMIT_MS,
		      "the writer waited %.0f ms", writer_waited_ms);
		CHECK(acqs_after > acqs_when_writer_got_in,
		      "the writer got the lock, but only after the reader "
		      "relay had stopped arriving (%d acquisitions before, %d "
		      "after). This case is only evidence when the readers "
		      "keep coming, so it is reporting itself as inconclusive "
		      "rather than passing.",
		      acqs_when_writer_got_in, acqs_after);
	}
	rw_destroy(&rwl);
}

/* ==================================================================
 * dining philosophers
 * ================================================================== */

#define PHILS 5

static int nphils = PHILS;      /* how many are at the table this run */
static phils table;
static int fork_held[PHIL_MAX];         /* the harness's shadow of the forks */
static int fork_clash;
static int fork_clash_i = -1;
static int meals;
static int meals_eaten[PHIL_MAX];
static int eating_now;
static int eating_max;
static int phil_nap;

static void take_shadow(int f)
{
	if (__atomic_exchange_n(&fork_held[f], 1, __ATOMIC_ACQ_REL)) {
		__atomic_store_n(&fork_clash, 1, __ATOMIC_RELAXED);
		__atomic_store_n(&fork_clash_i, f, __ATOMIC_RELAXED);
	}
}

static void drop_shadow(int f)
{
	__atomic_store_n(&fork_held[f], 0, __ATOMIC_RELEASE);
}

static void *philosopher(void *a)
{
	long w = (long)a;
	int i;

	for (i = 0; i < meals; i++) {
		int in;

		phil_pickup(&table, (int)w);
		take_shadow((int)w);
		take_shadow((int)((w + 1) % nphils));
		in = __atomic_add_fetch(&eating_now, 1, __ATOMIC_ACQ_REL);
		note_max(&eating_max, in);
		if (phil_nap)
			nap_ms(1);
		__atomic_sub_fetch(&eating_now, 1, __ATOMIC_ACQ_REL);
		drop_shadow((int)((w + 1) % nphils));
		drop_shadow((int)w);
		phil_putdown(&table, (int)w);
		__atomic_add_fetch(&meals_eaten[w], 1, __ATOMIC_ACQ_REL);
	}
	return NULL;
}

static void run_table(int n, int nmeals, int nap)
{
	pthread_t t[PHIL_MAX];
	long i;

	nphils = n;
	meals = nmeals;
	phil_nap = nap;
	fork_clash = 0;
	eating_now = 0;
	eating_max = 0;
	for (i = 0; i < PHIL_MAX; i++) {
		fork_held[i] = 0;
		meals_eaten[i] = 0;
	}
	phil_init(&table, n);
	for (i = 0; i < n; i++)
		REQUIRE(pthread_create(&t[i], NULL, philosopher, (void *)i) == 0,
			"pthread_create failed");
	for (i = 0; i < n; i++)
		pthread_join(t[i], NULL);
	phil_destroy(&table);
}

/*
 * Five philosophers, a few thousand meals each, and two things checked. No
 * fork is ever in two hands -- philosopher w eats with forks w and w + 1, so
 * the harness can shadow every fork and catch a pickup that takes one fork
 * and calls it two. And everybody finishes: a solution that lets one
 * philosopher eat while another never does is not a solution.
 *
 * The interesting result here is the negative one: four thousand back-to-back
 * meals with no pause do NOT deadlock the reference, and a student who tests
 * only this fast loop can wrongly conclude that "take the left fork, then the
 * right" works. It usually does, on this machine, in this loop -- the window
 * where all five hold one fork at once is a few nanoseconds wide. The case
 * that actually catches left-then-right is c_phil_parallel below, where a
 * 1 ms meal widens that window until the circle closes; run.sh reports that
 * as a DEADLOCK, and the four conditions in DEADLOCK.md are about why.
 */
static void c_phil(void)
{
	int i;

	run_table(PHILS, envint("TK_ITERS", 4000), 0);
	CHECK(!fork_clash,
	      "fork %d was in two philosophers' hands at once. Philosopher w "
	      "eats with forks w and (w + 1) %% n, and each fork is one "
	      "binary semaphore: taking one and eating with it does not do.",
	      __atomic_load_n(&fork_clash_i, __ATOMIC_RELAXED));
	for (i = 0; i < PHILS; i++)
		CHECK(meals_eaten[i] == meals,
		      "philosopher %d ate %d of its %d meals", i,
		      meals_eaten[i], meals);
}

/*
 * ...and at least two of them eat at once. Five philosophers with a 1 ms
 * meal each: if pickup is one big lock over the whole table then nothing
 * deadlocks, no fork is ever shared, everybody eats -- and the table seats
 * one. That is a correct answer to a different question, and this is the
 * case that tells them apart.
 *
 * The 1 ms meal does a second job: it is what makes "take the left fork, then
 * the right" actually deadlock rather than merely being able to. With the
 * window between the two forks widened to a millisecond, all five philosophers
 * reach for their second fork while holding their first and the circle closes;
 * run.sh's timeout reports it. That is the failure to expect from the naive
 * solution, and c_phil above is the fast loop that hides it.
 */
static void c_phil_parallel(void)
{
	run_table(PHILS, 60, 1);
	CHECK(!fork_clash, "fork %d was in two hands at once",
	      __atomic_load_n(&fork_clash_i, __ATOMIC_RELAXED));
	CHECK(eating_max >= 2,
	      "%d philosophers each ate %d one-millisecond meals and no two "
	      "of them were ever eating at the same time. A single lock "
	      "around the whole table deadlocks nothing and feeds everybody, "
	      "one at a time; the point of the exercise is that "
	      "non-neighbours eat together.", PHILS, meals);
}

/*
 * ...at the sizes the header promises, not just at five.
 *
 * toolkit.h says 2 <= n <= PHIL_MAX, and both other philosopher cases build
 * a table of exactly five. A phil_init that ignores its argument -- a
 * footman fixed at four, say -- passes them both.
 *
 * TWO is the size that matters, and it is worth knowing why. With two
 * philosophers there are two forks and both of them sit between the same
 * pair of hands, so the table is inherently serial: at most one eats at a
 * time and that is correct, not a bug. What a footman fixed at four does
 * here is let BOTH philosophers past it; each then takes its left fork and
 * waits for a right fork the other is holding, and the table stops. That is
 * the deadlock the footman exists to prevent, arriving because n was
 * ignored, and run.sh reports it as a DEADLOCK rather than as an assertion.
 *
 * SEVEN is a plain odd size on the other side of five, checked for fork
 * clashes and for everybody being fed. Neither size gets the "two eat at
 * once" check: at two it is false by construction, and at seven the parallel
 * case at five already makes the point.
 */
static void c_phil_sizes(void)
{
	static const int sizes[] = { 2, 7 };
	size_t k;
	int i;

	for (k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
		int n = sizes[k];

		/* Fifty thousand meals, and the number was measured rather
		 * than guessed. At two philosophers the deadlock that a
		 * footman fixed at four produces needs both of them inside
		 * phil_pickup between the first fork and the second, and
		 * that window is a few instructions wide. Whole-suite runs
		 * against that mutant: 200 meals caught it 0 times in 5,
		 * 4000 caught it 3 times in 5, 50 000 caught it 6 times in
		 * 6 and 12 times in 12 standalone. The reference runs both
		 * sizes in 0.12 s, so the margin is cheap. */
		run_table(n, 50000, 0);
		if (failures)
			return;
		CHECK(!fork_clash,
		      "at a table of %d, fork %d was in two philosophers' "
		      "hands at once. Philosopher w eats with forks w and "
		      "(w + 1) %% n, and n is what phil_init was given -- not "
		      "five.", n,
		      __atomic_load_n(&fork_clash_i, __ATOMIC_RELAXED));
		for (i = 0; i < n; i++)
			CHECK(meals_eaten[i] == meals,
			      "at a table of %d, philosopher %d ate %d of its "
			      "%d meals", n, i, meals_eaten[i], meals);
		if (failures)
			return;
	}
}

/* ================================================================== */

struct testcase {
	const char *name;
	void (*fn)(void);
};

static const struct testcase cases[] = {
	{ "p4_race_control",   c_race_control },
	{ "p4_sem_mutex",      c_sem_mutex },
	{ "p4_sem_room",       c_sem_room },
	{ "p4_sem_blocks",     c_sem_blocks },
	{ "p4_sem_parks",      c_sem_parks },
	{ "p4_barrier_gate",   c_barrier_gate },
	{ "p4_barrier_rounds", c_barrier_rounds },
	{ "p4_barrier_sizes",  c_barrier_sizes },
	{ "p4_rendezvous",     c_rendezvous },
	{ "p4_rw_shared",      c_rw_shared },
	{ "p4_rw_excl",        c_rw_excl },
	{ "p4_rw_nostarve",    c_rw_nostarve },
	{ "p4_phil",           c_phil },
	{ "p4_phil_parallel",  c_phil_parallel },
	{ "p4_phil_sizes",     c_phil_sizes },
};

int main(int argc, char **argv)
{
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "usage: cases <case-name>\n");
		return 2;
	}
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		if (strcmp(argv[1], cases[i].name) == 0) {
			cases[i].fn();
			fflush(stdout);
			return failures ? 1 : 0;
		}
	}
	fprintf(stderr, "cases: no such case '%s'\n", argv[1]);
	return 2;
}
