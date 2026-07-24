/* disksim.c -- Lab 6 Parts 2 and 3: a disk cost model, a workload generator,
 * and four request schedulers.  STARTER.
 *
 *   disksim gen  <kind> n=<N> seed=<S> maxlba=<M> [loc=<0..100>]   (given)
 *   disksim run  <policy> <stream-file> [key=value ...]            (you build)
 *
 * The workload generator (gen) is written for you.  What you build is the cost
 * model in serve() (Part 2) and the four schedulers (Part 3).  The output
 * format is fixed -- the autograder reads the "served ..." and "summary ..."
 * lines, so keep the printf lines exactly as they are.  See README.md.
 *
 * Defaults: C=500 spt=100 seek=1 rot=1 xfer=1 start=0 dir=up.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_REQ 200000

typedef struct {
	long lba, nsec, cyl, sec;
	long served_idx, seek, rot, xfer, done;
} req_t;

typedef struct { long C, spt, seek, rot, xfer; } geom_t;

static req_t reqs[MAX_REQ];
static int nreq;

static long labs_l(long x) { return x < 0 ? -x : x; }

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
	if (!f) { fprintf(stderr, "disksim: cannot open stream '%s'\n", path); return -1; }
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

/* ---- scheduling (Part 3) -------------------------------------------------- */
/* Each policy fills order[] (nreq entries) with request indices in service
 * order.  Only fifo is done for you.  See README.md, Part 3, for the exact
 * definition of each policy -- including SCAN's LOOK-style reversal and
 * C-SCAN's wrap, which are specified there so everyone's numbers agree. */

static void sched_fifo(int *order)
{
	for (int i = 0; i < nreq; i++) order[i] = i;
}

/* TODO (Part 3): nearest-seek-time-first.  Ties go to the lower cylinder, then
 * to the earlier arrival.  Starts from head_cyl. */
static void sched_sstf(int *order, long head_cyl)
{
	(void)head_cyl;
	sched_fifo(order);   /* TODO: replace */
}

/* TODO (Part 3): the elevator.  Sweep in the current direction serving requests
 * in cylinder order; when none remain ahead, reverse (reverse at the last
 * requested cylinder, not the disk edge).  Default direction up. */
static void sched_scan(int *order, long head_cyl, int dir_up)
{
	(void)head_cyl; (void)dir_up;
	sched_fifo(order);   /* TODO: replace */
}

/* TODO (Part 3): like SCAN but instead of reversing, jump back to the lowest
 * pending request and sweep up again.  The jump counts as head movement. */
static void sched_cscan(int *order, long head_cyl, int dir_up)
{
	(void)head_cyl; (void)dir_up;
	sched_fifo(order);   /* TODO: replace */
}

/* ---- the cost model (Part 2) ---------------------------------------------- */

static void serve(int *order, const geom_t *g, long start_cyl, const char *policy)
{
	long t = 0;
	long cur_cyl = start_cyl;
	long seek_total = 0;
	(void)g; (void)cur_cyl;   /* TODO: the cost model uses these; remove when done */

	for (int k = 0; k < nreq; k++) {
		req_t *r = &reqs[order[k]];

		/* TODO (Part 2): compute this request's seek, rotational latency and
		 * transfer time, advance t by each, and set seek_total (in cylinders).
		 *   seek = |r->cyl - cur_cyl| * g->seek
		 *   rot  = wait for sector r->sec to reach the head, given t and the
		 *          rotation period g->spt * g->rot   (see README.md, Part 2)
		 *   xfer = r->nsec * g->xfer
		 * The platter never stops: the head's angular position is t modulo the
		 * rotation period. */
		long seek = 0;   /* TODO */
		long rot  = 0;   /* TODO */
		long xfer = 0;   /* TODO */
		t += seek + rot + xfer;

		r->served_idx = k;
		r->seek = seek; r->rot = rot; r->xfer = xfer; r->done = t;
		seek_total += 0;   /* TODO: += |r->cyl - cur_cyl| */
		cur_cyl = r->cyl;

		printf("served idx=%d lba=%ld cyl=%ld sec=%ld seek=%ld rot=%ld xfer=%ld done=%ld\n",
		       k, r->lba, r->cyl, r->sec, seek, rot, xfer, r->done);
	}

	printf("summary policy=%s served=%d seek_total=%ld service_total=%ld order_cyl=",
	       policy, nreq, seek_total, t);
	for (int k = 0; k < nreq; k++)
		printf("%s%ld", k ? "," : "", reqs[order[k]].cyl);
	printf(" order_lba=");
	for (int k = 0; k < nreq; k++)
		printf("%s%ld", k ? "," : "", reqs[order[k]].lba);
	printf("\n");
}

/* ---- workload generator (given) ------------------------------------------- */

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
		if (strcmp(kind, "seq") == 0) lba = i % maxlba;
		else if (strcmp(kind, "rand") == 0) lba = rng_next() % maxlba;
		else {
			if ((long)(rng_next() % 100) < loc) {
				long delta = (long)(rng_next() % 17) - 8;
				lba = prev + delta;
				if (lba < 0) lba = 0;
				if (lba >= maxlba) lba = maxlba - 1;
			} else lba = rng_next() % maxlba;
		}
		printf("%ld 1\n", lba);
		prev = lba;
	}
	return 0;
}

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

	for (int i = 0; i < nreq; i++) {
		reqs[i].cyl = reqs[i].lba / g.spt;
		reqs[i].sec = reqs[i].lba % g.spt;
	}

	int *order = malloc((nreq ? nreq : 1) * sizeof(int));
	if (strcmp(policy, "fifo") == 0)       sched_fifo(order);
	else if (strcmp(policy, "sstf") == 0)  sched_sstf(order, start);
	else if (strcmp(policy, "scan") == 0)  sched_scan(order, start, dir_up);
	else if (strcmp(policy, "cscan") == 0) sched_cscan(order, start, dir_up);
	else { fprintf(stderr, "disksim: unknown policy '%s'\n", policy); free(order); return 2; }

	serve(order, &g, start, policy);
	free(order);
	return 0;
}

int main(int argc, char **argv)
{
	(void)labs_l;   /* you will want this for seek distance */
	if (argc < 2) { fprintf(stderr, "usage: disksim <gen|run> ...\n"); return 2; }
	if (strcmp(argv[1], "gen") == 0) return do_gen(argc, argv);
	if (strcmp(argv[1], "run") == 0) return do_run(argc, argv);
	fprintf(stderr, "disksim: unknown subcommand '%s'\n", argv[1]);
	return 2;
}
