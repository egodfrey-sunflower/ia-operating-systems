/* tracerun.c -- Lab 3 Part 4: replay a workload trace under one fit policy.
 *
 *     tracerun <trace-file> <first|best|worst> [arena-bytes]
 *
 * A trace is a text file of one operation per line:
 *
 *     a <id> <size>     allocate <size> bytes and remember them as <id>
 *     f <id>            free whatever <id> refers to
 *     # ...             comment, ignored, as are blank lines
 *
 * ids run from 0 to 4095. The driver replays the trace in two passes, against
 * a fresh arena each time:
 *
 *   pass 1  samples mym_get_stats() after every operation, to find the peak.
 *           Walking the heap that often is far too expensive to time.
 *   pass 2  replays the same trace five times with no sampling at all, and
 *           reports the fastest as ns_per_op.
 *
 * It prints one key=value line. Collect the nine lines (three policies x three
 * traces) into FITS.md and write the paragraph that Part 4 asks for.
 *
 * Fragmentation here means EXTERNAL fragmentation: free memory that no single
 * request can use, as a percentage of all free memory --
 *
 *     frag% = 100 x (free_bytes - largest_free_block) / free_bytes
 *
 * 0% is one contiguous free block; 90% means nine tenths of the free space is
 * scattered in pieces smaller than the largest one. peak_frag_pct is the worst
 * value seen at any point in the trace, ignoring the moments when less than an
 * eighth of the arena is in use (a nearly empty heap can post a spectacular
 * percentage of almost nothing).
 *
 * That floor is why an arena much larger than the trace's live set reports
 * nothing at all: if the heap never gets an eighth full, no sample ever
 * qualifies and the column reads `n/a` rather than a misleading 0.0.
 *
 * The driver also calls mym_check_heap() periodically while it replays, and
 * once at the end, and reports `heap=ok` or `heap=BROKEN`. A nine-row table of
 * fragmentation numbers measured on a heap the allocator's own walker calls
 * inconsistent is nine rows of nothing.
 */

#include "mymalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXID 4096

struct op {
	char   kind;      /* 'a' or 'f' */
	int    id;
	size_t size;
};

static struct op *ops;
static long nops;

static void die(const char *what, const char *arg)
{
	fprintf(stderr, "tracerun: %s: %s\n", what, arg);
	exit(2);
}

/* The trace is read into an array with the C library's own malloc before the
 * first mymalloc() call, so that reading the file is not part of what we
 * measure and libc's heap is nowhere near ours. */
static void load(const char *path)
{
	char line[256];
	long cap = 1024;
	FILE *f = fopen(path, "r");

	if (!f)
		die("cannot open trace", path);
	ops = malloc((size_t)cap * sizeof(*ops));
	if (!ops)
		die("out of memory reading", path);

	while (fgets(line, sizeof(line), f)) {
		char kind;
		int id;
		unsigned long size;
		int n;

		if (line[0] == '#' || line[0] == '\n')
			continue;
		n = sscanf(line, " %c %d %lu", &kind, &id, &size);
		if (n < 2 || (kind != 'a' && kind != 'f'))
			die("malformed line in", path);
		if (kind == 'a' && n != 3)
			die("allocation without a size in", path);
		if (id < 0 || id >= MAXID)
			die("id out of range in", path);
		if (nops == cap) {
			cap *= 2;
			ops = realloc(ops, (size_t)cap * sizeof(*ops));
			if (!ops)
				die("out of memory reading", path);
		}
		ops[nops].kind = kind;
		ops[nops].id = id;
		ops[nops].size = (kind == 'a') ? (size_t)size : 0;
		nops++;
	}
	fclose(f);
}

static double now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main(int argc, char **argv)
{
	void *live[MAXID];
	mym_stats_t s;
	mym_fit_t policy;
	const char *pname, *tname, *slash;
	double peak_frag = 0.0, t0, alloc_ns = 0.0;
	size_t peak_in_use = 0, free_blocks_at_peak = 0, floor_in_use;
	long i, allocs = 0, failed = 0, frag_samples = 0, heap_bad = 0;
	int rep;

	if (argc < 3 || argc > 4) {
		fprintf(stderr,
		        "usage: tracerun <trace-file> <first|best|worst> "
		        "[arena-bytes]\n");
		return 2;
	}
	pname = argv[2];
	if (!strcmp(pname, "first"))      policy = MYM_FIT_FIRST;
	else if (!strcmp(pname, "best"))  policy = MYM_FIT_BEST;
	else if (!strcmp(pname, "worst")) policy = MYM_FIT_WORST;
	else { die("unknown policy", pname); return 2; }

	setenv("MYM_ARENA_BYTES", argc == 4 ? argv[3] : "1048576", 1);

	load(argv[1]);
	slash = strrchr(argv[1], '/');
	tname = slash ? slash + 1 : argv[1];

	/* ------------------------------------------------ pass 1: fragmentation */
	memset(live, 0, sizeof(live));
	mym_set_fit(policy);
	mym_get_stats(&s);            /* zeroes; the arena appears on first use */
	floor_in_use = 0;

	for (i = 0; i < nops; i++) {
		if (ops[i].kind == 'a') {
			if (live[ops[i].id])
				continue;         /* a trace never reuses a live id */
			live[ops[i].id] = mymalloc(ops[i].size);
			allocs++;
			if (!live[ops[i].id]) {
				failed++;
				continue;
			}
			memset(live[ops[i].id], 0xa5, ops[i].size);
		} else {
			myfree(live[ops[i].id]);
			live[ops[i].id] = NULL;
		}

		/* Replaying thirteen thousand operations and reporting numbers
		 * from a heap that is quietly broken is worth nothing, so the
		 * driver checks the heap as it goes -- once every 1024
		 * operations, which is often enough to catch damage near where
		 * it happened and rare enough not to dominate the run. Only
		 * this pass checks; the timing pass below must measure the
		 * allocator and nothing else. */
		if ((i % 1024) == 0 && mym_check_heap() != 0)
			heap_bad++;

		mym_get_stats(&s);
		if (!floor_in_use)
			floor_in_use = s.arena_bytes / 8;
		if (s.bytes_in_use > peak_in_use)
			peak_in_use = s.bytes_in_use;
		if (s.free_bytes > 0 && s.bytes_in_use >= floor_in_use) {
			double frag = 100.0 *
				(double)(s.free_bytes - s.largest_free_block) /
				(double)s.free_bytes;
			frag_samples++;
			if (frag > peak_frag) {
				peak_frag = frag;
				free_blocks_at_peak = s.free_blocks;
			}
		}
	}
	mym_get_stats(&s);

	printf("policy=%-5s trace=%-16s ops=%ld allocs=%ld failed=%ld "
	       "peak_in_use=%zu ",
	       pname, tname, nops, allocs, failed, peak_in_use);

	/* No sample ever qualified: the heap never got an eighth full, so there
	 * is no fragmentation reading to report. Printing 0.0 here would say
	 * "perfectly packed" about a measurement that was never taken, and a
	 * table of nine zeros is what an arena much larger than the trace's
	 * live set produces. */
	if (frag_samples == 0) {
		printf("peak_frag_pct=n/a free_blocks_at_peak=n/a ");
		fprintf(stderr,
		        "tracerun: %s/%s: fragmentation was never sampled. It is "
		        "measured only while at least an eighth of the arena "
		        "(%zu of %zu bytes) is in use, and this trace never got "
		        "that far. Use a smaller arena -- try %zu -- if you want "
		        "this column to mean anything.\n",
		        tname, pname, floor_in_use, s.arena_bytes,
		        peak_in_use ? peak_in_use * 4 : (size_t)65536);
	} else {
		printf("peak_frag_pct=%.1f free_blocks_at_peak=%zu ",
		       peak_frag, free_blocks_at_peak);
	}

	if (mym_check_heap() != 0)
		heap_bad++;

	printf("end_largest_free=%zu end_free_blocks=%zu",
	       s.largest_free_block, s.free_blocks);

	if (heap_bad == 0) {
		printf(" heap=ok");
	} else {
		printf(" heap=BROKEN");
		fprintf(stderr,
		        "tracerun: %s/%s: mym_check_heap() reported damage at "
		        "%ld of the points it was called. The numbers on this "
		        "line describe a heap your own walker says is "
		        "inconsistent, so fix that before you write any of them "
		        "down. (If you have not written the walker yet, it "
		        "cannot be reporting damage -- something returned "
		        "non-zero.)\n",
		        tname, pname, heap_bad);
	}

	/* ------------------------------------------------------ pass 2: latency
	 *
	 * Five repetitions, and the FASTEST is reported. A single timed run on a
	 * machine that is also doing other things measures the other things as
	 * much as the allocator; the minimum is the closest thing to a reading
	 * with the noise taken out.
	 *
	 * It is still noisy. Measured on the reference across five sweeps of all
	 * nine runs, the same command spans roughly a FACTOR OF TWO --
	 * best/trace-small ran 523 to 1028 ns/op, worst/trace-mixed 866 to
	 * 1766 -- so treat anything under a factor of two as no result at all,
	 * and take a median if you need a number you can defend. The ordering
	 * of the three policies on a given trace was stable across all five
	 * sweeps; only the magnitudes moved. The fragmentation columns above
	 * are deterministic and reproduce to the digit; this one does not. */
	alloc_ns = 0.0;
	for (rep = 0; rep < 5; rep++) {
		double t1;
		mym_reset();
		memset(live, 0, sizeof(live));
		mym_set_fit(policy);

		t0 = now_ns();
		for (i = 0; i < nops; i++) {
			if (ops[i].kind == 'a') {
				if (live[ops[i].id])
					continue;
				live[ops[i].id] = mymalloc(ops[i].size);
			} else {
				myfree(live[ops[i].id]);
				live[ops[i].id] = NULL;
			}
		}
		t1 = now_ns() - t0;
		if (rep == 0 || t1 < alloc_ns)
			alloc_ns = t1;
	}

	printf(" ns_per_op=%.1f\n", nops ? alloc_ns / (double)nops : 0.0);

	mym_reset();
	free(ops);
	return 0;
}
