/*
 * bench.c -- the measurement driver behind LOCKS.md.
 *
 *      ./bench [milliseconds-per-point]
 *
 * For each of the three locks and each thread count, runs for a fixed WALL
 * TIME and counts how many acquire/release pairs got through. Fixed time
 * rather than fixed iterations, deliberately: a fair spin lock with more
 * threads than cores can convoy so badly that a fixed iteration count does
 * not finish this afternoon, and a benchmark you have to kill produces no
 * number at all. This one always produces a number, and a catastrophic
 * convoy shows up as the number being terrible, which is the finding.
 *
 * Each acquire/release pair does a tiny amount of work inside the critical
 * section, so that a lock which lets two threads in at once would corrupt
 * the checksum and be visible rather than merely fast.
 *
 * WHAT THE NUMBERS ARE WORTH. On a two-core machine this measures three
 * things at once -- the lock, the scheduler, and whatever else is running --
 * and only one of them is the subject. Run each configuration several times
 * before believing it. Measured here, run-to-run spread on the contended
 * points is around a factor of two, and on the ticket lock above two threads
 * it is far larger than that, because the run either convoys or does not.
 * Treat anything under a factor of two as no result. What is stable, and
 * what LOCKS.md should be about, is the SHAPE: which lock degrades as
 * threads are added, which does not, and why.
 *
 * Part of the harness, not of the lab. You do not have to modify it.
 */

#define _POSIX_C_SOURCE 200809L

#include "mylock.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile int stop;
static long shared;

struct api {
	const char *name;
	void *lock;
	void (*acquire)(void *);
	void (*release)(void *);
};

static spinlock_t   sl;
static ticketlock_t tl;
static sleeplock_t  kl;

static void sl_a(void *l) { spinlock_acquire(l); }
static void sl_r(void *l) { spinlock_release(l); }
static void tl_a(void *l) { ticketlock_acquire(l); }
static void tl_r(void *l) { ticketlock_release(l); }
static void kl_a(void *l) { sleeplock_acquire(l); }
static void kl_r(void *l) { sleeplock_release(l); }

static struct api apis[3] = {
	{ "spin",   &sl, sl_a, sl_r },
	{ "ticket", &tl, tl_a, tl_r },
	{ "sleep",  &kl, kl_a, kl_r },
};

static struct api *cur;
static long counts[64];

static void *worker(void *a)
{
	long idx = (long)a;
	long n = 0;

	while (!stop) {
		int k;

		cur->acquire(cur->lock);
		for (k = 0; k < 8; k++)
			shared += 1;
		cur->release(cur->lock);
		n++;
	}
	counts[idx] = n;
	return NULL;
}

static double now_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void nap_ms(long ms)
{
	struct timespec ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) == -1)
		;
}

static void point(struct api *a, int nthreads, long ms)
{
	pthread_t t[64];
	double t0, t1;
	long total = 0;
	int i;

	spinlock_init(&sl);
	ticketlock_init(&tl);
	sleeplock_init(&kl);
	cur = a;
	shared = 0;
	stop = 0;
	for (i = 0; i < nthreads; i++)
		counts[i] = 0;

	t0 = now_s();
	for (i = 0; i < nthreads; i++) {
		if (pthread_create(&t[i], NULL, worker, (void *)(long)i) != 0) {
			fprintf(stderr, "bench: pthread_create failed\n");
			exit(1);
		}
	}
	nap_ms(ms);
	stop = 1;
	for (i = 0; i < nthreads; i++)
		pthread_join(t[i], NULL);
	t1 = now_s();

	for (i = 0; i < nthreads; i++)
		total += counts[i];

	printf("%-7s %2d %12ld %12.1f %12.1f   %s\n", a->name, nthreads, total,
	       total / (t1 - t0),
	       total ? (t1 - t0) * 1e9 / (double)total : 0.0,
	       shared == total * 8 ? "ok" : "CHECKSUM WRONG");
}

int main(int argc, char **argv)
{
	static const int threads[] = { 1, 2, 4, 8 };
	long ms = 300;
	size_t a, k;

	if (argc > 1)
		ms = strtol(argv[1], NULL, 10);
	if (ms < 20)
		ms = 20;

	printf("%-7s %2s %12s %12s %12s   %s\n", "lock", "T", "acquires",
	       "acq/s", "ns/acq", "checksum");
	for (a = 0; a < 3; a++)
		for (k = 0; k < sizeof threads / sizeof threads[0]; k++)
			point(&apis[a], threads[k], ms);
	return 0;
}
