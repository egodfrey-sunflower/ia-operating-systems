/*
 * cases.c -- the Part 3 test driver.
 *
 *      cases <case-name>
 *
 * One case, one process, exit 0 if it passed, 1 if it failed, 2 if the name
 * is not a case. Complaints go to stderr, four spaces in.
 *
 * Two things a bounded buffer can do wrong, and they need different cases:
 *
 *   - it can produce the WRONG ANSWER: an item delivered twice, an item never
 *     delivered, an item read out of a slot that was never written. That is
 *     what a wait guarded by `if` instead of `while` looks like, because the
 *     thread proceeds on a predicate that was true when it was signalled and
 *     false by the time it ran. These cases detect it by accounting for every
 *     item individually rather than by adding them up: a checksum can come
 *     out right with two errors that cancel.
 *
 *   - it can produce NO ANSWER: everybody asleep, nobody left to wake them.
 *     That is what one condition variable instead of two looks like. No
 *     assertion in this file can catch that, because the process never gets
 *     to one. It is caught by run.sh's timeout, which reports it as a
 *     deadlock rather than as a failure.
 *
 * MYLOCK-style scaling knobs: PCB_ITEMS and PCB_THREADS, so run.sh can shrink
 * the work under helgrind.
 *
 * Nothing here looks inside the buffer. Every case goes through pcbuffer.h.
 */

#define _POSIX_C_SOURCE 200809L

#include "pcbuffer.h"

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

/* ==================================================================
 * item accounting
 *
 * Every item put in carries a distinct value, and the harness counts how
 * many times each value comes out. Exactly once, for every value, is the
 * only acceptable answer -- and it is a strictly stronger check than a sum,
 * which a duplicate and a loss can satisfy together.
 * ================================================================== */

#define SENTINEL (-1)

static unsigned char *seen;             /* seen[v] = times value v came out */
static int seen_n;
static pthread_mutex_t seen_lock = PTHREAD_MUTEX_INITIALIZER;
static int bad_value;                   /* a value outside the range issued */
static int bad_value_example;

static void seen_init(int n)
{
	seen = calloc((size_t)n, 1);
	seen_n = n;
	bad_value = 0;
}

static void seen_record(int v)
{
	pthread_mutex_lock(&seen_lock);
	if (v < 0 || v >= seen_n) {
		if (!bad_value)
			bad_value_example = v;
		bad_value = 1;
	} else if (seen[v] < 255) {
		seen[v]++;
	}
	pthread_mutex_unlock(&seen_lock);
}

/* Returns 0 if every value came out exactly once. */
static int seen_verify(void)
{
	int i, missing = 0, dup = 0, first_missing = -1, first_dup = -1;

	if (bad_value) {
		fail("a value came out of the buffer that was never put in: "
		     "%d. That is a slot being read before it was written -- "
		     "the classic result of waiting with `if` instead of "
		     "`while`, where a consumer wakes, another consumer has "
		     "already taken the item, and it proceeds anyway.",
		     bad_value_example);
		return 1;
	}
	for (i = 0; i < seen_n; i++) {
		if (seen[i] == 0) {
			missing++;
			if (first_missing < 0)
				first_missing = i;
		} else if (seen[i] > 1) {
			dup++;
			if (first_dup < 0)
				first_dup = i;
		}
	}
	if (missing || dup) {
		fail("%d of %d items never came out (first: %d) and %d came out "
		     "more than once (first: %d). Every item put in must come "
		     "out exactly once.",
		     missing, seen_n, first_missing, dup, first_dup);
		return 1;
	}
	return 0;
}

/* ==================================================================
 * producers and consumers
 * ================================================================== */

static pcbuffer buf;
static int per_producer;

static void *producer(void *a)
{
	long id = (long)a;
	int i;

	for (i = 0; i < per_producer; i++)
		pcb_put(&buf, (int)id * per_producer + i);
	return NULL;
}

static void *consumer(void *a)
{
	(void)a;
	for (;;) {
		int v = pcb_get(&buf);

		if (v == SENTINEL)
			return NULL;
		seen_record(v);
	}
}

/*
 * The shared body of the throughput cases. Producers issue distinct values;
 * when they are all done, one sentinel per consumer is put in to stop them.
 */
static void run_mpmc(int capacity, int producers, int consumers, int per)
{
	pthread_t pt[32], ct[32];
	long i;

	per_producer = per;
	seen_init(producers * per);
	pcb_init(&buf, capacity);

	for (i = 0; i < consumers; i++) {
		REQUIRE(pthread_create(&ct[i], NULL, consumer, NULL) == 0,
			"pthread_create failed");
	}
	for (i = 0; i < producers; i++) {
		REQUIRE(pthread_create(&pt[i], NULL, producer, (void *)i) == 0,
			"pthread_create failed");
	}
	for (i = 0; i < producers; i++)
		pthread_join(pt[i], NULL);
	for (i = 0; i < consumers; i++)
		pcb_put(&buf, SENTINEL);
	for (i = 0; i < consumers; i++)
		pthread_join(ct[i], NULL);

	seen_verify();
	pcb_destroy(&buf);
}

static void c_spsc(void)
{
	run_mpmc(4, 1, 1, envint("PCB_ITEMS", 20000));
}

static void c_mpmc(void)
{
	int n = envint("PCB_THREADS", 4);

	if (n > 16)
		n = 16;
	run_mpmc(8, n, n, envint("PCB_ITEMS", 20000));
}

/*
 * Capacity 1 is where a wait guarded by `if` breaks fastest: every put fills
 * the buffer and every get empties it, so every consumer that wakes is
 * racing every other consumer for the single item, and the loser proceeds
 * into an empty buffer.
 */
static void c_cap1(void)
{
	int n = envint("PCB_THREADS", 4);

	if (n > 16)
		n = 16;
	run_mpmc(1, n, n, envint("PCB_ITEMS", 20000));
}

/* The long run. Same shape, more of it: the plan's "does not stall". */
static void c_soak(void)
{
	run_mpmc(16, 4, 4, envint("PCB_ITEMS", 250000));
}

/* ==================================================================
 * order
 * ================================================================== */

static int order_ok = 1;
static int order_last = -1;
static int order_count;
static int order_bad_prev, order_bad_now;

static void *order_consumer(void *a)
{
	(void)a;
	for (;;) {
		int v = pcb_get(&buf);

		if (v == SENTINEL)
			return NULL;
		if (v != order_last + 1) {
			if (order_ok) {
				order_bad_prev = order_last;
				order_bad_now = v;
			}
			order_ok = 0;
		}
		order_last = v;
		order_count++;
	}
}

static void c_order(void)
{
	pthread_t pt, ct;
	int per = envint("PCB_ITEMS", 20000);

	per_producer = per;
	pcb_init(&buf, 4);
	order_ok = 1;
	order_last = -1;
	order_count = 0;
	REQUIRE(pthread_create(&ct, NULL, order_consumer, NULL) == 0,
		"pthread_create failed");
	REQUIRE(pthread_create(&pt, NULL, producer, (void *)0L) == 0,
		"pthread_create failed");
	pthread_join(pt, NULL);
	pcb_put(&buf, SENTINEL);
	pthread_join(ct, NULL);
	CHECK(order_count == per,
	      "%d of %d items came out of the buffer before the sentinel did. "
	      "A pcb_get that returns without waiting -- or that returns a "
	      "value nothing put in -- ends the consumer early.",
	      order_count, per);
	CHECK(order_ok,
	      "with one producer and one consumer the items must come out in "
	      "the order they went in; %d was followed by %d. The buffer is a "
	      "queue, not a stack: head advances on get, tail on put, and both "
	      "wrap at capacity.", order_bad_prev, order_bad_now);
	pcb_destroy(&buf);
}

/* ==================================================================
 * the buffer is bounded
 *
 * With capacity 4 and nobody consuming, a producer gets four items in and
 * then blocks. A buffer that is not bounded -- one that grows, or one that
 * overwrites the oldest slot -- gets all seven in and passes everything
 * above, because everything above eventually consumes.
 * ================================================================== */

#define BOUND_CAP 4
#define BOUND_PUTS 7

static volatile int puts_done;

static void *bound_producer(void *a)
{
	int i;

	(void)a;
	for (i = 0; i < BOUND_PUTS; i++) {
		pcb_put(&buf, i);
		__atomic_fetch_add(&puts_done, 1, __ATOMIC_RELEASE);
	}
	return NULL;
}

static void c_bounded(void)
{
	pthread_t pt;
	int done, i;

	pcb_init(&buf, BOUND_CAP);
	puts_done = 0;
	REQUIRE(pthread_create(&pt, NULL, bound_producer, NULL) == 0,
		"pthread_create failed");
	nap_ms(250);
	done = __atomic_load_n(&puts_done, __ATOMIC_ACQUIRE);
	if (done != BOUND_CAP)
		fail("a producer with no consumer completed %d puts into a "
		     "buffer of capacity %d within 250 ms. It should have "
		     "completed exactly %d and then blocked on the %dth. The "
		     "buffer is not bounded: check that pcb_put waits while "
		     "count == capacity.",
		     done, BOUND_CAP, BOUND_CAP, BOUND_CAP + 1);
	/* Drain, so the producer finishes and the case does not hang here. */
	for (i = 0; i < BOUND_PUTS; i++)
		pcb_get(&buf);
	pthread_join(pt, NULL);
	pcb_destroy(&buf);
}

/* ==================================================================
 * an empty buffer blocks the consumer
 *
 * pcb_get on an empty buffer must wait, not return a stale slot. Detected by
 * making it wait 250 ms for an item that is not there yet, and checking it
 * did not come back before the item did.
 * ================================================================== */

static volatile int got_early;
static volatile int got_value;
static volatile int item_is_in;

static void *waiting_consumer(void *a)
{
	int v;

	(void)a;
	v = pcb_get(&buf);
	if (!__atomic_load_n(&item_is_in, __ATOMIC_ACQUIRE))
		got_early = 1;
	got_value = v;
	return NULL;
}

static void c_blocking_get(void)
{
	pthread_t ct;

	pcb_init(&buf, 4);
	got_early = 0;
	got_value = -12345;
	item_is_in = 0;
	REQUIRE(pthread_create(&ct, NULL, waiting_consumer, NULL) == 0,
		"pthread_create failed");
	nap_ms(250);
	__atomic_store_n(&item_is_in, 1, __ATOMIC_RELEASE);
	pcb_put(&buf, 77);
	pthread_join(ct, NULL);
	CHECK(!got_early,
	      "pcb_get returned from an empty buffer before anything was put "
	      "in. It must wait while count == 0, not read slots[head] and "
	      "hope.");
	CHECK(got_value == 77, "pcb_get returned %d; 77 was put in",
	      got_value);
	pcb_destroy(&buf);
}

/* ==================================================================
 * a full buffer wakes producers, an empty one wakes consumers
 *
 * The two-condition-variable rule, tested from the outside. Sixteen threads
 * take turns through a buffer of capacity 1 in a pattern that forces every
 * wakeup path to be used: producers blocking on full, consumers blocking on
 * empty, alternately, thousands of times. If a signal ever goes to the wrong
 * queue, this stops dead and run.sh reports the timeout.
 * ================================================================== */

static void c_wakeups(void)
{
	run_mpmc(1, 8, 8, envint("PCB_ITEMS", 4000));
}

/* ================================================================== */

struct testcase {
	const char *name;
	void (*fn)(void);
};

static const struct testcase cases[] = {
	{ "p3_spsc",         c_spsc },
	{ "p3_order",        c_order },
	{ "p3_bounded",      c_bounded },
	{ "p3_blocking_get", c_blocking_get },
	{ "p3_mpmc",         c_mpmc },
	{ "p3_cap1",         c_cap1 },
	{ "p3_wakeups",      c_wakeups },
	{ "p3_soak",         c_soak },
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
