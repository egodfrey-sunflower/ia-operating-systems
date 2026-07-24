/*
 * cases.c -- the Part 2 test driver.
 *
 *      cases <case-name>
 *
 * One case, one process, exit 0 if it passed, 1 if it failed, 2 if the name
 * is not a case. Complaints go to stderr, four spaces in.
 *
 * READ THIS BEFORE YOU TRUST A GREEN RUN. Every case below is probabilistic.
 * A lock that is wrong can pass all of them on a quiet machine, and the same
 * lock can fail them on a busy one. What the cases are built to do is make
 * the wrong answer LIKELY: many threads, many iterations, several repeats,
 * and a critical section short enough that two threads racing into it
 * actually collide. The negative control ('an unsynchronised counter really
 * does lose updates') exists to check that the machine is still capable of
 * producing the wrong answer at all -- if it fails, the other cases are not
 * evidence of anything.
 *
 * Three environment variables scale the work, so run.sh can shrink it under
 * valgrind: MYLOCK_THREADS, MYLOCK_ITERS, MYLOCK_REPS.
 *
 * Nothing here looks inside a lock. Every case goes through mylock.h.
 */

#define _POSIX_C_SOURCE 200809L

#include "mylock.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
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
	if (v < 1 || v > 1000000)
		return dflt;
	return (int)v;
}

/*
 * How many threads a SPINNING lock may be given.
 *
 * A lock that hands over in a fixed order -- a ticket lock, or any other fair
 * spin lock -- convoys catastrophically when there are more runnable threads
 * than cores: the thread whose turn it is may be off the processor, and every
 * other waiter burns a full scheduler timeslice discovering that. Measured on
 * a two-core machine, four threads taking a ticket lock 20000 times each
 * finish in 5 ms when nobody is descheduled and do not finish in 25 seconds
 * when somebody is. That is a true fact about fair spin locks -- OSTEP ch. 28
 * makes it the argument for a lock that sleeps -- and not a defect in an
 * implementation, so the harness does not create the situation: the spinning
 * locks get one thread per core.
 *
 * The sleeping lock has no such problem, and gets as many threads as asked
 * for.
 */
static int spin_threads(int requested)
{
	long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	int cap = (ncpu < 2) ? 2 : (int)ncpu;

	return requested < cap ? requested : cap;
}

static void nap_ms(long ms)
{
	struct timespec ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) == -1)
		;
}

static double now_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static double cpu_s(void)
{
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1e6 +
	       (double)ru.ru_stime.tv_sec + (double)ru.ru_stime.tv_usec / 1e6;
}

/* ==================================================================
 * one interface over the three locks
 *
 * The harness holds a lock as a void *, so the same case body can be run
 * against all three. It still calls nothing but the nine published functions.
 * ================================================================== */

struct lockapi {
	const char *name;
	void *lock;
	void (*init)(void *);
	void (*acquire)(void *);
	void (*release)(void *);
};

static spinlock_t   spin_a, spin_b;
static ticketlock_t tick_a, tick_b;
static sleeplock_t  sleep_a, sleep_b;

static void spin_i(void *l) { spinlock_init(l); }
static void spin_a_(void *l) { spinlock_acquire(l); }
static void spin_r(void *l) { spinlock_release(l); }
static void tick_i(void *l) { ticketlock_init(l); }
static void tick_a_(void *l) { ticketlock_acquire(l); }
static void tick_r(void *l) { ticketlock_release(l); }
static void slp_i(void *l) { sleeplock_init(l); }
static void slp_a_(void *l) { sleeplock_acquire(l); }
static void slp_r(void *l) { sleeplock_release(l); }

static struct lockapi api_spin   = { "spin lock",     &spin_a,  spin_i, spin_a_, spin_r };
static struct lockapi api_ticket = { "ticket lock",   &tick_a,  tick_i, tick_a_, tick_r };
static struct lockapi api_sleep  = { "sleeping lock", &sleep_a, slp_i,  slp_a_,  slp_r };
static struct lockapi api_spin2   = { "spin lock",     &spin_b,  spin_i, spin_a_, spin_r };
static struct lockapi api_ticket2 = { "ticket lock",   &tick_b,  tick_i, tick_a_, tick_r };
static struct lockapi api_sleep2  = { "sleeping lock", &sleep_b, slp_i,  slp_a_,  slp_r };

/*
 * A start barrier. Without it, thread 0 can finish a short case before
 * thread 7 has been created, and a case in which the threads never overlap
 * cannot detect a lock that does not exclude. Measured: with 20000
 * iterations and no barrier, two threads with NO LOCK AT ALL lost no updates
 * at all, because they were never running at the same time.
 */
static pthread_barrier_t startline;

static void barrier_init(int nthreads)
{
	pthread_barrier_init(&startline, NULL, (unsigned)nthreads);
}

static void barrier_wait(void)
{
	pthread_barrier_wait(&startline);
}

static void barrier_done(void)
{
	pthread_barrier_destroy(&startline);
}

/* ==================================================================
 * the shared counter
 * ================================================================== */

static struct lockapi *ctr_api;
/* volatile: an ordinary long at -O2 becomes a single read-modify-write
 * instruction, which is not atomic but is very hard to interleave. The
 * counter a student's lock protects in real code is an ordinary variable
 * whose load, add and store are three separate steps, and that is what has
 * to be modelled here. */
static volatile long counter;
static int  ctr_iters;

/*
 * The body of the critical section, used both under a lock and, by the
 * control, without one.
 *
 * The load and the store are separated by a handful of compiler barriers,
 * which is what a real critical section looks like: something is read,
 * something is decided, something is written. A bare `counter++` compiles at
 * -O2 into one read-modify-write instruction whose window is a few cycles
 * wide, and two threads with no lock at all then lose an update only
 * sometimes -- measured here, about half of the runs. Widening the window to
 * a few tens of cycles makes an unprotected counter lose updates every time,
 * which is the difference between a case that detects a missing lock and a
 * case that occasionally notices one.
 *
 * It costs a correct lock nothing: the window is inside the critical
 * section.
 */
static void critical_bump(void)
{
	long v = counter;
	int k;

	for (k = 0; k < 16; k++)
		__asm__ __volatile__("" ::: "memory");
	counter = v + 1;
}

static void *counter_thread(void *a)
{
	int i;

	(void)a;
	barrier_wait();
	for (i = 0; i < ctr_iters; i++) {
		ctr_api->acquire(ctr_api->lock);
		critical_bump();
		ctr_api->release(ctr_api->lock);
	}
	return NULL;
}

static void counter_case(struct lockapi *api, int spinning)
{
	int nthreads = envint("MYLOCK_THREADS", 8);
	int reps     = envint("MYLOCK_REPS", 5);
	int rep, i;
	pthread_t t[64];

	ctr_iters = envint("MYLOCK_ITERS", 1000000);
	if (nthreads > 64)
		nthreads = 64;
	if (spinning)
		nthreads = spin_threads(nthreads);
	ctr_api = api;

	for (rep = 0; rep < reps; rep++) {
		long want = (long)nthreads * ctr_iters;

		api->init(api->lock);
		counter = 0;
		barrier_init(nthreads);
		for (i = 0; i < nthreads; i++) {
			REQUIRE(pthread_create(&t[i], NULL, counter_thread,
					       NULL) == 0,
				"pthread_create failed");
		}
		for (i = 0; i < nthreads; i++)
			pthread_join(t[i], NULL);
		barrier_done();
		if (counter != want) {
			fail("%s, repeat %d of %d: %d threads x %d increments "
			     "left the counter at %ld, not %ld -- %ld updates "
			     "were lost.",
			     api->name, rep + 1, reps, nthreads, ctr_iters,
			     counter, want, want - counter);
			return;
		}
	}
}

static void c_spin_counter(void)   { counter_case(&api_spin, 1); }
static void c_ticket_counter(void) { counter_case(&api_ticket, 1); }
static void c_sleep_counter(void)  { counter_case(&api_sleep, 0); }

/*
 * The negative control. The same shape of workload with no lock at all must
 * produce the wrong answer -- if it does not, this machine cannot lose an
 * update in this loop, and the three cases above prove nothing today.
 *
 * It runs exactly the same critical section as the three cases above, with
 * nothing around it. Same variable, same loop, same thread count -- the only
 * difference is the lock, which is the only way a control is a control.
 */
static int raw_iters;

static void *raw_thread(void *a)
{
	int i;

	(void)a;
	barrier_wait();
	for (i = 0; i < raw_iters; i++)
		critical_bump();
	return NULL;
}

static void c_race_control(void)
{
	int nthreads = spin_threads(envint("MYLOCK_THREADS", 8));
	int attempts = 20;
	int a, i;
	pthread_t t[64];

	raw_iters = envint("MYLOCK_ITERS", 1000000);
	if (nthreads > 64)
		nthreads = 64;

	for (a = 0; a < attempts; a++) {
		long want = (long)nthreads * raw_iters;

		counter = 0;
		barrier_init(nthreads);
		for (i = 0; i < nthreads; i++) {
			REQUIRE(pthread_create(&t[i], NULL, raw_thread,
					       NULL) == 0,
				"pthread_create failed");
		}
		for (i = 0; i < nthreads; i++)
			pthread_join(t[i], NULL);
		barrier_done();
		if (counter != want)
			return;         /* an update was lost: the point */
	}
	fail("%d attempts at %d threads x %d unsynchronised increments and not "
	     "one update was lost. The counter cases cannot distinguish a "
	     "working lock from a missing one on this machine right now -- "
	     "usually because only one core is available to this process. "
	     "Treat every other Part 2 result as unproven until this passes.",
	     attempts, nthreads, raw_iters);
}

/* ==================================================================
 * mutual exclusion, observed directly
 *
 * The counter case detects a lost update. This one detects the overlap that
 * causes it: inside the critical section a thread writes its own id into a
 * shared slot, dawdles, and checks the slot still says what it wrote.
 * ================================================================== */

static struct lockapi *ex_api;
static volatile int occupant;
static volatile int ex_overlap;
static int ex_rounds;

static void *excl_thread(void *a)
{
	long me = (long)a;
	int r, k;

	barrier_wait();
	for (r = 0; r < ex_rounds; r++) {
		ex_api->acquire(ex_api->lock);
		occupant = (int)me;
		/* Long enough that a second thread which got in would be seen,
		 * short enough that the whole case takes well under a second.
		 * Not a sleep: a sleep would let the scheduler run the other
		 * thread elsewhere and hide the overlap. */
		for (k = 0; k < 40; k++)
			__asm__ __volatile__("" ::: "memory");
		if (occupant != (int)me)
			ex_overlap = 1;
		ex_api->release(ex_api->lock);
	}
	return NULL;
}

static void excl_case(struct lockapi *api, int spinning)
{
	int nthreads = envint("MYLOCK_THREADS", 8);
	int i;
	pthread_t t[64];

	ex_rounds = envint("MYLOCK_ITERS", 1000000);
	if (ex_rounds < 100)
		ex_rounds = 100;
	if (nthreads > 64)
		nthreads = 64;
	if (spinning)
		nthreads = spin_threads(nthreads);

	ex_api = api;
	api->init(api->lock);
	occupant = -1;
	ex_overlap = 0;
	barrier_init(nthreads);
	for (i = 0; i < nthreads; i++) {
		REQUIRE(pthread_create(&t[i], NULL, excl_thread,
				       (void *)(long)i) == 0,
			"pthread_create failed");
	}
	for (i = 0; i < nthreads; i++)
		pthread_join(t[i], NULL);
	barrier_done();
	CHECK(!ex_overlap,
	      "%s: a thread found another thread's id in the critical section, "
	      "so two threads held the lock at once.", api->name);
}

static void c_spin_excl(void)   { excl_case(&api_spin, 1); }
static void c_ticket_excl(void) { excl_case(&api_ticket, 1); }
static void c_sleep_excl(void)  { excl_case(&api_sleep, 0); }

/* ==================================================================
 * two locks are two locks
 *
 * A lock whose state lives in a file-scope variable rather than in *l passes
 * every case above -- it is simply one very slow global lock. It fails this
 * one: while lock A is held, lock B must still be free.
 * ================================================================== */

static struct lockapi *tl_a, *tl_b;
static volatile int tl_done;

static void *other_lock_thread(void *a)
{
	(void)a;
	tl_b->acquire(tl_b->lock);
	tl_done = 1;
	tl_b->release(tl_b->lock);
	return NULL;
}

static void twolocks_case(struct lockapi *a, struct lockapi *b)
{
	pthread_t t;
	double deadline;

	tl_a = a;
	tl_b = b;
	a->init(a->lock);
	b->init(b->lock);
	tl_done = 0;

	a->acquire(a->lock);
	REQUIRE(pthread_create(&t, NULL, other_lock_thread, NULL) == 0,
		"pthread_create failed");
	deadline = now_s() + 2.0;
	while (!tl_done && now_s() < deadline)
		nap_ms(5);
	if (!tl_done)
		fail("%s: with one lock held, a second, separate lock of the "
		     "same kind could not be acquired within 2 s. The two locks "
		     "are sharing state -- the flag, the ticket counter or the "
		     "futex word is probably a file-scope variable rather than "
		     "a field of *l.", a->name);
	a->release(a->lock);
	pthread_join(t, NULL);
}

static void c_spin_two(void)   { twolocks_case(&api_spin, &api_spin2); }
static void c_ticket_two(void) { twolocks_case(&api_ticket, &api_ticket2); }
static void c_sleep_two(void)  { twolocks_case(&api_sleep, &api_sleep2); }

/* ==================================================================
 * the ticket lock hands over in arrival order
 *
 * Six threads arrive 40 ms apart while the lock is held, so their arrival
 * order is not in doubt. A ticket lock serves them 0,1,2,3,4,5. A
 * test-and-set lock serves whoever wins the exchange, which is usually not
 * that. This is the case that tells the two locks apart, and it is the only
 * one in Part 2 that depends on timing rather than on volume.
 *
 * Six threads on a ticket lock on a two-core machine is the configuration
 * spin_threads() exists to refuse -- and it is safe here because each thread
 * acquires exactly once, so the convoy is bounded by six handovers rather than
 * by the tens of thousands the counter cases would do. A loop around the
 * acquire in fifo_thread would reintroduce exactly the bimodal hang the cap
 * avoids.
 * ================================================================== */

#define FIFO_N       6
#define FIFO_GAP_MS  40

static ticketlock_t fifo_lock;
static int fifo_order[FIFO_N];
static int fifo_next;
static spinlock_t fifo_guard;

static void *fifo_thread(void *a)
{
	long me = (long)a;

	nap_ms(me * FIFO_GAP_MS);
	ticketlock_acquire(&fifo_lock);
	/* Recording the order is itself shared state; guard it with the other
	 * lock rather than assuming an int store is safe. */
	spinlock_acquire(&fifo_guard);
	if (fifo_next < FIFO_N)
		fifo_order[fifo_next++] = (int)me;
	spinlock_release(&fifo_guard);
	ticketlock_release(&fifo_lock);
	return NULL;
}

static void c_ticket_fifo(void)
{
	pthread_t t[FIFO_N];
	long i;
	int k;

	ticketlock_init(&fifo_lock);
	spinlock_init(&fifo_guard);
	fifo_next = 0;
	for (k = 0; k < FIFO_N; k++)
		fifo_order[k] = -1;

	ticketlock_acquire(&fifo_lock);
	for (i = 0; i < FIFO_N; i++) {
		REQUIRE(pthread_create(&t[i], NULL, fifo_thread, (void *)i) == 0,
			"pthread_create failed");
	}
	/* Every thread has arrived and is waiting by now: the last one starts
	 * waiting at 5 x 40 ms, and this sleeps for 5 x 40 + 120. */
	nap_ms((FIFO_N - 1) * FIFO_GAP_MS + 120);
	ticketlock_release(&fifo_lock);
	for (i = 0; i < FIFO_N; i++)
		pthread_join(t[i], NULL);

	REQUIRE(fifo_next == FIFO_N, "only %d of %d threads recorded a turn",
		fifo_next, FIFO_N);
	for (k = 0; k < FIFO_N; k++) {
		if (fifo_order[k] != k) {
			char buf[128];
			int n = 0, j;

			for (j = 0; j < FIFO_N; j++)
				n += snprintf(buf + n, sizeof buf - (size_t)n,
					      "%s%d", j ? " " : "",
					      fifo_order[j]);
			fail("the threads arrived %d ms apart in the order "
			     "0 1 2 3 4 5 and were served in the order %s. A "
			     "ticket lock serves in arrival order; a "
			     "test-and-set lock does not. Check that "
			     "ticketlock_acquire really takes a number with "
			     "fetch-and-add and waits for it.",
			     FIFO_GAP_MS, buf);
			return;
		}
	}
}

/* ==================================================================
 * a sleeping lock's waiters do not burn processor time
 *
 * Six threads wait 400 ms for a lock held by the main thread. Spinning
 * waiters turn that into processor seconds; parked waiters cost nothing. The
 * measurement is processor time used by the whole process across the window,
 * so it does not depend on how many cores there are.
 * ================================================================== */

#define SLEEPY_N      6
#define SLEEPY_HOLD_MS 400

static sleeplock_t sleepy;
static volatile int sleepy_got;

static void *sleepy_thread(void *a)
{
	(void)a;
	sleeplock_acquire(&sleepy);
	sleepy_got++;
	sleeplock_release(&sleepy);
	return NULL;
}

static void c_sleep_parks(void)
{
	pthread_t t[SLEEPY_N];
	double cpu0, cpu1, wall0, wall1, used, elapsed;
	int i;

	sleeplock_init(&sleepy);
	sleepy_got = 0;
	sleeplock_acquire(&sleepy);
	for (i = 0; i < SLEEPY_N; i++) {
		REQUIRE(pthread_create(&t[i], NULL, sleepy_thread, NULL) == 0,
			"pthread_create failed");
	}
	nap_ms(50);             /* let them all reach the lock */
	cpu0 = cpu_s();
	wall0 = now_s();
	nap_ms(SLEEPY_HOLD_MS);
	cpu1 = cpu_s();
	wall1 = now_s();
	sleeplock_release(&sleepy);
	for (i = 0; i < SLEEPY_N; i++)
		pthread_join(t[i], NULL);

	CHECK(sleepy_got == SLEEPY_N, "%d of %d waiters got the lock",
	      sleepy_got, SLEEPY_N);

	used = cpu1 - cpu0;
	elapsed = wall1 - wall0;
	/* Six parked threads use no measurable processor time; six spinning
	 * ones saturate every core they can get. Half of one core-second per
	 * wall second is far above the first and far below the second. */
	CHECK(used < elapsed * 0.5,
	      "while %d threads waited %.0f ms for the lock, the process used "
	      "%.3f s of processor time (%.0f%% of one core). They are "
	      "spinning, not sleeping: a waiter has to park in the kernel with "
	      "FUTEX_WAIT, not loop.",
	      SLEEPY_N, elapsed * 1000.0, used, 100.0 * used / elapsed);
}

/* ==================================================================
 * a spin lock's waiters do spin
 *
 * The mirror image of the case above, and it exists for the same reason: with
 * only the counter, exclusion and two-locks cases, a submission in which
 * spinlock_* is a second copy of the sleeping lock scores full marks. Nothing
 * else in this part can tell a spin lock from any other correct lock.
 *
 * Four threads wait 200 ms for a lock the main thread holds, and the process's
 * processor time over that window is measured. Waiters that spin turn the wait
 * into processor seconds -- that is what spinning IS, and on this lock it is
 * the intended behaviour, not a defect. Waiters that park cost nothing, which
 * is the sleeping lock's job and not this one's.
 *
 * Four threads on a two-core machine, which spin_threads() would refuse for
 * the counter cases: safe here for the same reason it is safe in the
 * arrival-order case, because each thread acquires exactly once, so there are
 * four handovers in total and no convoy to run away with.
 *
 * The threshold is deliberately far from both outcomes. Measured on a two-core
 * machine: four spinning waiters use 134-150% of one core over the window, and
 * still 62% with four unrelated busy loops competing for the same two cores;
 * four parked waiters use 0.000 s, every run. A quarter of one core sits three
 * orders of magnitude above the second number and well below the first, so the
 * case can be lost only by a machine on which nothing runs at all.
 * ================================================================== */

#define SPINNY_N       4
#define SPINNY_HOLD_MS 200

static spinlock_t spinny;
static volatile int spinny_got;

static void *spinny_thread(void *a)
{
	(void)a;
	spinlock_acquire(&spinny);
	spinny_got++;
	spinlock_release(&spinny);
	return NULL;
}

static void c_spin_burns(void)
{
	pthread_t t[SPINNY_N];
	double cpu0, cpu1, wall0, wall1, used, elapsed;
	int i;

	spinlock_init(&spinny);
	spinny_got = 0;
	spinlock_acquire(&spinny);
	for (i = 0; i < SPINNY_N; i++) {
		REQUIRE(pthread_create(&t[i], NULL, spinny_thread, NULL) == 0,
			"pthread_create failed");
	}
	nap_ms(50);             /* let them all reach the lock */
	cpu0 = cpu_s();
	wall0 = now_s();
	nap_ms(SPINNY_HOLD_MS);
	cpu1 = cpu_s();
	wall1 = now_s();
	spinlock_release(&spinny);
	for (i = 0; i < SPINNY_N; i++)
		pthread_join(t[i], NULL);

	CHECK(spinny_got == SPINNY_N, "%d of %d waiters got the lock",
	      spinny_got, SPINNY_N);

	used = cpu1 - cpu0;
	elapsed = wall1 - wall0;
	CHECK(used > elapsed * 0.25,
	      "while %d threads waited %.0f ms for the spin lock, the process "
	      "used %.3f s of processor time (%.0f%% of one core). A waiter on "
	      "a test-and-set lock loops on the flag and that costs a core; "
	      "this one is going to sleep instead, so spinlock_acquire is not "
	      "the lock this part asks for. (A lock that never blocks at all "
	      "looks the same from here: check the waiters really wait.)",
	      SPINNY_N, elapsed * 1000.0, used, 100.0 * used / elapsed);
}

/* ================================================================== */

struct testcase {
	const char *name;
	void (*fn)(void);
};

static const struct testcase cases[] = {
	{ "p2_race_control",   c_race_control },
	{ "p2_spin_counter",   c_spin_counter },
	{ "p2_ticket_counter", c_ticket_counter },
	{ "p2_sleep_counter",  c_sleep_counter },
	{ "p2_spin_excl",      c_spin_excl },
	{ "p2_ticket_excl",    c_ticket_excl },
	{ "p2_sleep_excl",     c_sleep_excl },
	{ "p2_spin_two",       c_spin_two },
	{ "p2_ticket_two",     c_ticket_two },
	{ "p2_sleep_two",      c_sleep_two },
	{ "p2_ticket_fifo",    c_ticket_fifo },
	{ "p2_sleep_parks",    c_sleep_parks },
	{ "p2_spin_burns",     c_spin_burns },
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
