/* iobuf.c -- Lab 6 Part 1: a buffered device interface.  STARTER.
 *
 * Model a slow output device fed by a CPU producer, and measure how three
 * buffering strategies overlap the two.  Then model the same transfer under
 * polling, interrupts and DMA, and count the CPU work each wastes.
 *
 * Two subcommands (the interface and output format are fixed -- the autograder
 * reads them, so keep the printf lines exactly as they are):
 *
 *   iobuf buf <strategy> n=<N> tdev=<D> tcpu=<C> [depth=<K>] [burst=<B>]
 *   iobuf io  <mode>     n=<N> tdev=<D> [hoverhead=<H>] [setup=<S>]
 *
 * What you implement is marked TODO below.  See README.md, Part 1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_N 1000000

static int kv(const char *arg, const char *key, long *out)
{
	size_t klen = strlen(key);
	if (strncmp(arg, key, klen) == 0 && arg[klen] == '=') {
		*out = strtol(arg + klen + 1, NULL, 10);
		return 1;
	}
	return 0;
}

/* Bounded-buffer producer/consumer with K slots.
 *
 * prod[i] is the CPU time to compute unit i.  The device drains a full slot in
 * D ticks.  A slot frees when its unit is drained.  K=1 is unbuffered (fully
 * serial), K=2 is double buffering, K large is a deep circular buffer.
 *
 * TODO (Part 1): return the completion time of the whole burst -- the time the
 * last unit finishes draining.  The recurrence in README.md, Part 1 is:
 *
 *   acquire_i = max(ready_{i-1}, drained_{i-K})
 *   ready_i   = acquire_i + prod[i]
 *   drained_i = max(ready_i, drained_{i-1}) + D
 *
 * with drained_j and ready_j taken as 0 for j < 0.  Return drained_{n-1}.
 */
static long simulate(long n, long D, const long *prod, long K)
{
	(void)n; (void)D; (void)prod; (void)K;
	return 0;   /* TODO: replace with the bounded-buffer recurrence */
}

static long lmax(long a, long b) { return a > b ? a : b; }

static int do_buf(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: iobuf buf <unbuffered|double|circular> n=.. tdev=.. tcpu=.. [depth=..] [burst=..]\n");
		return 2;
	}
	const char *strat = argv[2];
	long n = 16, D = 10, C = 3, depth = 8, burst = 0;
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "tdev", &v)) D = v;
		else if (kv(argv[i], "tcpu", &v)) C = v;
		else if (kv(argv[i], "depth", &v)) depth = v;
		else if (kv(argv[i], "burst", &v)) burst = v;
	}
	if (n < 0) n = 0;
	if (n > MAX_N) { fprintf(stderr, "iobuf: n capped at %d\n", MAX_N); n = MAX_N; }

	/* TODO (Part 1): map the strategy name to a number of buffers K.
	 * unbuffered -> 1, double -> 2, circular -> depth. */
	long K = 1;
	if (strcmp(strat, "unbuffered") == 0) K = 1;
	else if (strcmp(strat, "double") == 0) K = 1;     /* TODO */
	else if (strcmp(strat, "circular") == 0) K = 1;   /* TODO: use depth */
	else { fprintf(stderr, "iobuf: unknown strategy '%s'\n", strat); return 2; }
	(void)depth;   /* TODO: circular buffering should use this; remove when done */
	if (K < 1) K = 1;

	long *prod = malloc((n ? n : 1) * sizeof(long));
	for (long i = 0; i < n; i++) {
		if (burst > 0)
			prod[i] = ((i % (burst + 1)) == burst) ? burst * C : 1;
		else
			prod[i] = C;
	}

	long t = simulate(n, D, prod, K);
	printf("buf strategy=%s n=%ld tdev=%ld tcpu=%ld depth=%ld burst=%ld time=%ld\n",
	       strat, n, D, C, K, burst, t);
	free(prod);
	return 0;
}

static int do_io(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: iobuf io <polling|interrupt|dma> n=.. tdev=.. [hoverhead=..] [setup=..]\n");
		return 2;
	}
	const char *mode = argv[2];
	long n = 16, D = 10, H = 50, S = 100;
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "tdev", &v)) D = v;
		else if (kv(argv[i], "hoverhead", &v)) H = v;
		else if (kv(argv[i], "setup", &v)) S = v;
	}
	if (n < 0) n = 0;

	/* TODO (Part 1): count the CPU cycles each mode wastes.
	 *   polling   : busy-waits the whole device time per unit  -> n*D
	 *   interrupt : one handler cost per unit                  -> n*H
	 *   dma       : one setup plus one completion interrupt     -> S+H (n>0) */
	long wasted = 0;
	if (strcmp(mode, "polling") == 0)        wasted = 0;   /* TODO */
	else if (strcmp(mode, "interrupt") == 0) wasted = 0;   /* TODO */
	else if (strcmp(mode, "dma") == 0)       wasted = 0;   /* TODO */
	else { fprintf(stderr, "iobuf: unknown mode '%s'\n", mode); return 2; }

	printf("io mode=%s n=%ld tdev=%ld hoverhead=%ld setup=%ld wasted=%ld\n",
	       mode, n, D, H, S, wasted);
	return 0;
}

int main(int argc, char **argv)
{
	(void)lmax;   /* you will want this once simulate() is written */
	if (argc < 2) {
		fprintf(stderr, "usage: iobuf <buf|io> ...\n");
		return 2;
	}
	if (strcmp(argv[1], "buf") == 0) return do_buf(argc, argv);
	if (strcmp(argv[1], "io") == 0) return do_io(argc, argv);
	fprintf(stderr, "iobuf: unknown subcommand '%s'\n", argv[1]);
	return 2;
}
