/* iobuf.c -- Lab 6 Part 1: a buffered device interface.
 *
 * Model a slow output device fed by a CPU producer, and measure how three
 * buffering strategies overlap the two.  Then model the same transfer under
 * polling, interrupts and DMA, and count the CPU work each wastes.
 *
 * Two subcommands:
 *
 *   iobuf buf <strategy> n=<N> tdev=<D> tcpu=<C> [depth=<K>] [burst=<B>]
 *       Run a burst of N units through the device and print the completion time.
 *       strategy is: unbuffered (1 buffer), double (2 buffers), or circular
 *       (depth buffers, default 8).
 *
 *       The producer computes each unit before the device can drain it.  When
 *       burst>0 the producer is bursty: it computes B cheap units (1 tick each)
 *       and then one expensive unit (burst*tcpu ticks), repeating.  A bursty
 *       producer is what separates a deep circular buffer from a double buffer:
 *       the extra slots let the producer run ahead during the cheap run so the
 *       device never goes idle.  With a steady producer (burst=0) double and
 *       circular are identical, which is the point -- depth only helps variance.
 *
 *   iobuf io <mode> n=<N> tdev=<D> [hoverhead=<H>] [setup=<S>]
 *       Model N units under polling, interrupt or dma and print CPU cycles
 *       wasted (spent not doing useful work).
 *         polling   : the CPU busy-waits the whole D ticks per unit -> N*D.
 *         interrupt : the CPU sleeps; each unit costs H handler cycles -> N*H.
 *         dma       : one setup of S cycles and one completion interrupt of H
 *                     for the whole burst -> S + H.
 *
 * The models are deterministic so the harness can assert the numbers.
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

static long lmax(long a, long b) { return a > b ? a : b; }

/* Bounded-buffer producer/consumer with K slots.
 *
 * prod[i] is the CPU time to compute unit i.  The producer must hold a free
 * slot to compute into, then the device drains it in D ticks.  A slot frees
 * when its unit is drained.
 *
 *   acquire_i = max(ready_{i-1}, drained_{i-K})   (slot free, producer free)
 *   ready_i   = acquire_i + prod[i]
 *   drained_i = max(ready_i, drained_{i-1}) + D
 *
 * K=1 is unbuffered (fully serial); K=2 is double buffering; K large is a deep
 * circular buffer.  Returns drained_{N-1}, the completion time of the burst.
 */
static long simulate(long n, long D, const long *prod, long K)
{
	if (n <= 0) return 0;
	long *ready = malloc(n * sizeof(long));
	long *drained = malloc(n * sizeof(long));
	for (long i = 0; i < n; i++) {
		long prev_ready = (i >= 1) ? ready[i-1] : 0;
		long slot_free  = (i >= K) ? drained[i-K] : 0;
		long acquire = lmax(prev_ready, slot_free);
		ready[i] = acquire + prod[i];
		long prev_drained = (i >= 1) ? drained[i-1] : 0;
		drained[i] = lmax(ready[i], prev_drained) + D;
	}
	long done = drained[n-1];
	free(ready); free(drained);
	return done;
}

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

	long K;
	if (strcmp(strat, "unbuffered") == 0) K = 1;
	else if (strcmp(strat, "double") == 0) K = 2;
	else if (strcmp(strat, "circular") == 0) K = depth;
	else { fprintf(stderr, "iobuf: unknown strategy '%s'\n", strat); return 2; }
	if (K < 1) K = 1;

	long *prod = malloc((n ? n : 1) * sizeof(long));
	for (long i = 0; i < n; i++) {
		if (burst > 0) {
			/* cheap run of `burst` units, then one expensive unit */
			prod[i] = ((i % (burst + 1)) == burst) ? burst * C : 1;
		} else {
			prod[i] = C;   /* steady producer */
		}
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

	long wasted;
	if (strcmp(mode, "polling") == 0)        wasted = n * D;
	else if (strcmp(mode, "interrupt") == 0) wasted = n * H;
	else if (strcmp(mode, "dma") == 0)       wasted = (n > 0) ? S + H : 0;
	else { fprintf(stderr, "iobuf: unknown mode '%s'\n", mode); return 2; }

	printf("io mode=%s n=%ld tdev=%ld hoverhead=%ld setup=%ld wasted=%ld\n",
	       mode, n, D, H, S, wasted);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: iobuf <buf|io> ...\n");
		return 2;
	}
	if (strcmp(argv[1], "buf") == 0) return do_buf(argc, argv);
	if (strcmp(argv[1], "io") == 0) return do_io(argc, argv);
	fprintf(stderr, "iobuf: unknown subcommand '%s'\n", argv[1]);
	return 2;
}
