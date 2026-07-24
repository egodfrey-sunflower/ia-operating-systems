/*
 * loadgen.c -- supplied load generator for Lab 5 Part 5's measurements.
 *
 *      loadgen <port> <path> <concurrency> <seconds>
 *
 * Opens `concurrency` connections' worth of client threads, each of which
 * makes requests back to back -- connect, GET, read the whole response,
 * close -- for `seconds` seconds, and reports:
 *
 *      requests, throughput in requests/second, and the latency
 *      distribution: mean, median, 95th percentile and worst.
 *
 * This is a CLOSED-LOOP generator: each client waits for its answer before
 * asking again, so `concurrency` is the number of requests in flight, not a
 * request rate. That matters when you write PERF.md, because it is why
 * throughput and latency are not independent here: at a fixed concurrency,
 * doubling the latency halves the throughput by construction. An open-loop
 * generator, which fires at a fixed rate whatever the server does, measures
 * something different and shows queueing collapse rather than gentle
 * degradation. Say which one your numbers came from -- this one.
 *
 * It is deliberately not clever. It is not a benchmark of the load
 * generator, and on two cores it and the server are competing for the same
 * ones; that is a real effect and belongs in your write-up rather than in a
 * footnote.
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS  64
#define MAX_SAMPLES  100000
#define RESP_MAX     (1024 * 1024)

static int port;
static const char *path;
static volatile int stop;

struct client {
	pthread_t th;
	double *lat;
	long n;
	long errors;
};

static double now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static int dial(void)
{
	struct sockaddr_in addr;
	struct timeval tv;
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0)
		return -1;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons((unsigned short)port);
	if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static int one_request(char *buf)
{
	char req[512];
	size_t reqlen, got = 0;
	int fd = dial();

	if (fd < 0)
		return -1;
	reqlen = (size_t)snprintf(req, sizeof req,
				  "GET %s HTTP/1.0\r\nHost: localhost\r\n\r\n",
				  path);
	if (write(fd, req, reqlen) != (ssize_t)reqlen) {
		close(fd);
		return -1;
	}
	for (;;) {
		ssize_t n = read(fd, buf + got, RESP_MAX - got);

		if (n == 0)
			break;
		if (n < 0) {
			if (errno == EINTR)
				continue;
			close(fd);
			return -1;
		}
		got += (size_t)n;
		if (got == RESP_MAX)
			break;
	}
	close(fd);
	if (got < 12 || memcmp(buf, "HTTP/1.", 7) != 0)
		return -1;
	return 0;
}

static void *client_main(void *a)
{
	struct client *c = a;
	char *buf = malloc(RESP_MAX);

	if (!buf)
		return NULL;
	while (!__atomic_load_n(&stop, __ATOMIC_ACQUIRE)) {
		double t0 = now_ms(), dt;

		if (one_request(buf) < 0) {
			c->errors++;
			continue;
		}
		dt = now_ms() - t0;
		if (c->n < MAX_SAMPLES)
			c->lat[c->n] = dt;
		c->n++;
	}
	free(buf);
	return NULL;
}

static int cmp_double(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;

	return x < y ? -1 : x > y ? 1 : 0;
}

int main(int argc, char **argv)
{
	struct client cl[MAX_CLIENTS];
	double *all, t0, elapsed, sum = 0;
	long total = 0, errors = 0, kept = 0;
	int conc, secs, i;

	if (argc != 5) {
		fprintf(stderr,
			"usage: loadgen <port> <path> <concurrency> <seconds>\n");
		return 2;
	}
	port = atoi(argv[1]);
	path = argv[2];
	conc = atoi(argv[3]);
	secs = atoi(argv[4]);
	if (conc < 1 || conc > MAX_CLIENTS || secs < 1 || secs > 60) {
		fprintf(stderr, "loadgen: concurrency 1..%d, seconds 1..60\n",
			MAX_CLIENTS);
		return 2;
	}

	memset(cl, 0, sizeof cl);
	for (i = 0; i < conc; i++) {
		cl[i].lat = malloc(MAX_SAMPLES * sizeof(double));
		if (!cl[i].lat) {
			fprintf(stderr, "loadgen: out of memory\n");
			return 1;
		}
	}
	t0 = now_ms();
	for (i = 0; i < conc; i++) {
		if (pthread_create(&cl[i].th, NULL, client_main, &cl[i]) != 0) {
			fprintf(stderr, "loadgen: pthread_create failed\n");
			return 1;
		}
	}
	sleep((unsigned)secs);
	__atomic_store_n(&stop, 1, __ATOMIC_RELEASE);
	for (i = 0; i < conc; i++)
		pthread_join(cl[i].th, NULL);
	elapsed = (now_ms() - t0) / 1000.0;

	for (i = 0; i < conc; i++) {
		total += cl[i].n;
		errors += cl[i].errors;
	}
	all = malloc((size_t)(total > 0 ? total : 1) * sizeof(double));
	if (!all)
		return 1;
	for (i = 0; i < conc; i++) {
		long j, n = cl[i].n < MAX_SAMPLES ? cl[i].n : MAX_SAMPLES;

		for (j = 0; j < n; j++) {
			all[kept++] = cl[i].lat[j];
			sum += cl[i].lat[j];
		}
		free(cl[i].lat);
	}
	qsort(all, (size_t)kept, sizeof(double), cmp_double);

	printf("concurrency %d  seconds %.1f  requests %ld  errors %ld\n",
	       conc, elapsed, total, errors);
	printf("throughput %.0f req/s\n", elapsed > 0 ? total / elapsed : 0.0);
	if (kept > 0)
		printf("latency ms: mean %.2f  p50 %.2f  p95 %.2f  max %.2f\n",
		       sum / kept, all[kept / 2], all[(long)(kept * 0.95)],
		       all[kept - 1]);
	free(all);
	return 0;
}
