/* disksim.c -- Lab 6 Parts 2 and 3: a disk cost model, a workload generator,
 * and four request schedulers.
 *
 * Two subcommands:
 *
 *   disksim gen  <kind> n=<N> seed=<S> maxlba=<M> [loc=<0..100>]
 *       Emit a request stream on stdout, one "lba nsec" per line.
 *       kind is one of: seq, rand, mixed.
 *
 *   disksim run  <policy> <stream-file> [key=value ...]
 *       Serve the stream under one scheduling policy and print, per request,
 *       the seek/rotation/transfer breakdown, then a machine-readable summary.
 *       policy is one of: fifo, sstf, scan, cscan.
 *
 * The cost model (chapter 37): T_io = T_seek + T_rot + T_transfer.
 *
 *   - The disk has C cylinders and SPT sectors per track.  Logical block
 *     address lba maps to cylinder lba/SPT and sector lba%SPT.
 *   - Seek time is |cyl_to - cyl_from| * seek, a linear model.
 *   - The platter never stops turning.  One sector passes under the head every
 *     `rot` ticks, so a full rotation is SPT*rot ticks.  After the seek, the
 *     head is at whatever rotational position the elapsed time left it, and the
 *     request waits for its target sector to come round.
 *   - Transfer is nsec * xfer ticks.
 *
 * Defaults: C=500 spt=100 seek=1 rot=1 xfer=1 start=0 dir=up.
 *
 * Everything here is deterministic: the same stream, geometry and start
 * produce the same numbers every time, so the harness can assert on them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_REQ 200000      /* hard cap: a stream longer than this is refused */

typedef struct {
	long lba;
	long nsec;
	long cyl;
	long sec;
	/* filled in while serving */
	long served_idx;    /* position in the service order, -1 until served */
	long seek;
	long rot;
	long xfer;
	long done;          /* completion time */
} req_t;

typedef struct {
	long C;      /* cylinders */
	long spt;    /* sectors per track */
	long seek;   /* ticks per cylinder of seek */
	long rot;    /* ticks per sector of rotation */
	long xfer;   /* ticks per sector transferred */
} geom_t;

static req_t reqs[MAX_REQ];
static int nreq;

static long labs_l(long x) { return x < 0 ? -x : x; }

/* ---- parsing -------------------------------------------------------------- */

/* Parse a key=value argument like "start=50".  Returns 1 and sets *out if the
 * argument's key matches; returns 0 otherwise. */
static int kv(const char *arg, const char *key, long *out)
{
	size_t klen = strlen(key);
	if (strncmp(arg, key, klen) == 0 && arg[klen] == '=') {
		*out = strtol(arg + klen + 1, NULL, 10);
		return 1;
	}
	return 0;
}

static int load_stream(const char *path)
{
	FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
	if (!f) {
		fprintf(stderr, "disksim: cannot open stream '%s'\n", path);
		return -1;
	}
	char line[256];
	nreq = 0;
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\n' || *p == '\0') continue;
		long lba, nsec = 1;
		int n = sscanf(p, "%ld %ld", &lba, &nsec);
		if (n < 1) continue;
		if (nreq >= MAX_REQ) {
			fprintf(stderr, "disksim: stream longer than %d requests\n", MAX_REQ);
			if (f != stdin) fclose(f);
			return -1;
		}
		if (nsec < 1) nsec = 1;
		reqs[nreq].lba = lba;
		reqs[nreq].nsec = nsec;
		reqs[nreq].served_idx = -1;
		nreq++;
	}
	if (f != stdin) fclose(f);
	return 0;
}

/* ---- scheduling ----------------------------------------------------------- */

/* Each policy fills order[] with request indices in the sequence they will be
 * served.  order[] has nreq entries.  head_cyl is the starting cylinder. */

static void sched_fifo(int *order)
{
	for (int i = 0; i < nreq; i++) order[i] = i;
}

static void sched_sstf(int *order, long head_cyl)
{
	char *done = calloc(nreq, 1);
	long cur = head_cyl;
	for (int k = 0; k < nreq; k++) {
		int best = -1;
		long bestd = 0;
		long bestcyl = 0;
		for (int i = 0; i < nreq; i++) {
			if (done[i]) continue;
			long d = labs_l(reqs[i].cyl - cur);
			/* nearest wins; ties go to the lower cylinder, then to the
			 * earlier arrival, so the order is fully determined. */
			if (best < 0 || d < bestd ||
			    (d == bestd && reqs[i].cyl < bestcyl)) {
				best = i; bestd = d; bestcyl = reqs[i].cyl;
			}
		}
		order[k] = best;
		done[best] = 1;
		cur = reqs[best].cyl;
	}
	free(done);
}

/* comparison for a stable ascending-by-cylinder sort (ties keep arrival order,
 * which we encode by comparing the original index). */
static int cmp_up(const void *a, const void *b)
{
	int ia = *(const int *)a, ib = *(const int *)b;
	if (reqs[ia].cyl != reqs[ib].cyl)
		return reqs[ia].cyl < reqs[ib].cyl ? -1 : 1;
	return ia < ib ? -1 : 1;
}

/* SCAN (the elevator): sweep in the current direction serving requests in
 * cylinder order; when none remain ahead, reverse and serve the rest.  This is
 * the LOOK form -- it reverses at the last requested cylinder, not at the
 * physical edge of the disk -- and that choice is fixed here so every student's
 * numbers agree.  Default direction is up (toward higher cylinders). */
static void sched_scan(int *order, long head_cyl, int dir_up)
{
	int *idx = malloc(nreq * sizeof(int));
	for (int i = 0; i < nreq; i++) idx[i] = i;
	qsort(idx, nreq, sizeof(int), cmp_up);   /* ascending by cylinder */

	int k = 0;
	if (dir_up) {
		/* ahead = cylinder >= head, ascending; then behind, descending */
		for (int i = 0; i < nreq; i++)
			if (reqs[idx[i]].cyl >= head_cyl) order[k++] = idx[i];
		for (int i = nreq - 1; i >= 0; i--)
			if (reqs[idx[i]].cyl < head_cyl) order[k++] = idx[i];
	} else {
		/* ahead = cylinder <= head, descending; then behind, ascending */
		for (int i = nreq - 1; i >= 0; i--)
			if (reqs[idx[i]].cyl <= head_cyl) order[k++] = idx[i];
		for (int i = 0; i < nreq; i++)
			if (reqs[idx[i]].cyl > head_cyl) order[k++] = idx[i];
	}
	free(idx);
}

/* C-SCAN: sweep up serving requests in cylinder order, then instead of
 * reversing, jump back to the lowest pending request and sweep up again.  The
 * jump is real head movement and is counted in the seek total.  This is the
 * C-LOOK form.  Default direction is up. */
static void sched_cscan(int *order, long head_cyl, int dir_up)
{
	int *idx = malloc(nreq * sizeof(int));
	for (int i = 0; i < nreq; i++) idx[i] = i;
	qsort(idx, nreq, sizeof(int), cmp_up);

	int k = 0;
	if (dir_up) {
		for (int i = 0; i < nreq; i++)
			if (reqs[idx[i]].cyl >= head_cyl) order[k++] = idx[i];
		for (int i = 0; i < nreq; i++)
			if (reqs[idx[i]].cyl < head_cyl) order[k++] = idx[i];
	} else {
		for (int i = nreq - 1; i >= 0; i--)
			if (reqs[idx[i]].cyl <= head_cyl) order[k++] = idx[i];
		for (int i = nreq - 1; i >= 0; i--)
			if (reqs[idx[i]].cyl > head_cyl) order[k++] = idx[i];
	}
	free(idx);
}

/* ---- the cost model ------------------------------------------------------- */

/* Serve the requests in the given order, threading time through the model, and
 * print the per-request breakdown and a summary line. */
static void serve(int *order, const geom_t *g, long start_cyl, const char *policy)
{
	long t = 0;
	long cur_cyl = start_cyl;
	long seek_total = 0;
	long period = g->spt * g->rot;   /* ticks per full rotation */

	for (int k = 0; k < nreq; k++) {
		req_t *r = &reqs[order[k]];

		long seek = labs_l(r->cyl - cur_cyl) * g->seek;
		t += seek;

		/* rotational latency: where is the target sector's leading edge
		 * relative to the head's current angular position? */
		long rot;
		if (period > 0) {
			long pos = t % period;                 /* head position, in ticks */
			long target = r->sec * g->rot;          /* target sector's edge */
			rot = (target - pos + period) % period;
		} else {
			rot = 0;
		}
		t += rot;

		long xfer = r->nsec * g->xfer;
		t += xfer;

		r->served_idx = k;
		r->seek = seek;
		r->rot = rot;
		r->xfer = xfer;
		r->done = t;
		seek_total += labs_l(r->cyl - cur_cyl);
		cur_cyl = r->cyl;

		printf("served idx=%d lba=%ld cyl=%ld sec=%ld seek=%ld rot=%ld xfer=%ld done=%ld\n",
		       k, r->lba, r->cyl, r->sec, seek, rot, xfer, r->done);
	}

	/* summary: seek_total is in CYLINDERS (head movement), the headline
	 * metric that separates the policies; service_total is in TICKS. */
	printf("summary policy=%s served=%d seek_total=%ld service_total=%ld order_cyl=",
	       policy, nreq, seek_total, t);
	for (int k = 0; k < nreq; k++)
		printf("%s%ld", k ? "," : "", reqs[order[k]].cyl);
	printf(" order_lba=");
	for (int k = 0; k < nreq; k++)
		printf("%s%ld", k ? "," : "", reqs[order[k]].lba);
	printf("\n");
}

/* ---- workload generator --------------------------------------------------- */

/* A small deterministic PRNG so a given seed reproduces a given stream.  This
 * is a stream *generator*; the harness grades the schedulers against fixed
 * fixture streams, not against generated ones, so cross-language reproduction
 * of these exact bytes is not required. */
static uint64_t rng_state;
static uint32_t rng_next(void)
{
	rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
	return (uint32_t)(rng_state >> 33);
}

static int do_gen(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: disksim gen <seq|rand|mixed> n=.. seed=.. maxlba=.. [loc=..]\n");
		return 2;
	}
	const char *kind = argv[2];
	long n = 100, seed = 1, maxlba = 10000, loc = 80;
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "seed", &v)) seed = v;
		else if (kv(argv[i], "maxlba", &v)) maxlba = v;
		else if (kv(argv[i], "loc", &v)) loc = v;
	}
	if (n < 0) n = 0;
	if (n > MAX_REQ) n = MAX_REQ;
	if (maxlba < 1) maxlba = 1;
	rng_state = (uint64_t)seed + 0x9e3779b97f4a7c15ULL;

	printf("# disksim gen %s n=%ld seed=%ld maxlba=%ld loc=%ld\n",
	       kind, n, seed, maxlba, loc);
	long prev = 0;
	for (long i = 0; i < n; i++) {
		long lba;
		if (strcmp(kind, "seq") == 0) {
			lba = i % maxlba;
		} else if (strcmp(kind, "rand") == 0) {
			lba = rng_next() % maxlba;
		} else { /* mixed: with probability loc%, stay near the last block */
			if ((long)(rng_next() % 100) < loc) {
				long delta = (long)(rng_next() % 17) - 8;
				lba = prev + delta;
				if (lba < 0) lba = 0;
				if (lba >= maxlba) lba = maxlba - 1;
			} else {
				lba = rng_next() % maxlba;
			}
		}
		printf("%ld 1\n", lba);
		prev = lba;
	}
	return 0;
}

/* ---- run ------------------------------------------------------------------ */

static int do_run(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: disksim run <fifo|sstf|scan|cscan> <stream> [key=value ...]\n");
		return 2;
	}
	const char *policy = argv[2];
	const char *stream = argv[3];

	geom_t g = { 500, 100, 1, 1, 1 };
	long start = 0;
	int dir_up = 1;
	for (int i = 4; i < argc; i++) {
		long v;
		if (kv(argv[i], "C", &v)) g.C = v;
		else if (kv(argv[i], "spt", &v)) g.spt = v;
		else if (kv(argv[i], "seek", &v)) g.seek = v;
		else if (kv(argv[i], "rot", &v)) g.rot = v;
		else if (kv(argv[i], "xfer", &v)) g.xfer = v;
		else if (kv(argv[i], "start", &v)) start = v;
		else if (strcmp(argv[i], "dir=up") == 0) dir_up = 1;
		else if (strcmp(argv[i], "dir=down") == 0) dir_up = 0;
	}
	if (g.spt < 1) g.spt = 1;

	if (load_stream(stream) < 0) return 1;

	/* map every request to (cylinder, sector) */
	for (int i = 0; i < nreq; i++) {
		reqs[i].cyl = reqs[i].lba / g.spt;
		reqs[i].sec = reqs[i].lba % g.spt;
	}

	int *order = malloc((nreq ? nreq : 1) * sizeof(int));
	if (strcmp(policy, "fifo") == 0)       sched_fifo(order);
	else if (strcmp(policy, "sstf") == 0)  sched_sstf(order, start);
	else if (strcmp(policy, "scan") == 0)  sched_scan(order, start, dir_up);
	else if (strcmp(policy, "cscan") == 0) sched_cscan(order, start, dir_up);
	else {
		fprintf(stderr, "disksim: unknown policy '%s'\n", policy);
		free(order);
		return 2;
	}

	serve(order, &g, start, policy);
	free(order);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: disksim <gen|run> ...\n");
		return 2;
	}
	if (strcmp(argv[1], "gen") == 0) return do_gen(argc, argv);
	if (strcmp(argv[1], "run") == 0) return do_run(argc, argv);
	fprintf(stderr, "disksim: unknown subcommand '%s'\n", argv[1]);
	return 2;
}
