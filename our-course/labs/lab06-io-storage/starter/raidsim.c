/* raidsim.c -- Lab 6 Parts 4 and 5: a RAID engine for levels 0, 1, 4 and 5.
 * STARTER.
 *
 * raidsim reads a command script from a file argument (or stdin if "-") and
 * executes it against an in-memory array.  The command loop, argument parsing,
 * the physical block accessors with their I/O counters, and the checksum are
 * written for you.  What you build is the geometry (block -> disk mapping), the
 * XOR parity, the read/write paths, degraded-mode reconstruction and the
 * rebuild -- every function marked TODO.  See README.md, Parts 4 and 5.
 *
 * The command language and every output line are fixed (the autograder reads
 * them).  The commands: init, parity, capacity, place, fill, write, read,
 * readraw, iostat, reset, fail, rebuild, checksum -- documented in README.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_DISKS 32
#define MAX_ROWS  4096
#define MAX_BS    4096

static int    level, ndisks;
static long   chunk, rows, bs;
static int    parity_mode;

static unsigned char *disk[MAX_DISKS];
static int    failed[MAX_DISKS];
static long   phys_reads, phys_writes;

enum { PM_AUTO = 0, PM_ADD = 1, PM_SUB = 2 };

/* ---- physical block access (given; keep the I/O counters honest) ---------- */

static unsigned char *cell(int d, long row) { return disk[d] + row * bs; }
static void phys_read(int d, long row, unsigned char *out)  { phys_reads++;  memcpy(out, cell(d, row), bs); }
static void phys_write(int d, long row, const unsigned char *in) { phys_writes++; memcpy(cell(d, row), in, bs); }
static void xor_into(unsigned char *acc, const unsigned char *src) { for (long i = 0; i < bs; i++) acc[i] ^= src[i]; }

/* ---- geometry (Part 4): logical block -> physical placement --------------- */

/* TODO (Part 4): data disks per stripe.  RAID0 uses all N; RAID1 stores one
 * disk's worth of data (a mirror); RAID4/5 give one disk per stripe to parity. */
static int data_disks(void)
{
	return ndisks;   /* TODO: correct for levels 1, 4 and 5 */
}

/* TODO (Part 4): which physical disk holds parity for this stripe.  RAID4 uses
 * a fixed disk; RAID5 rotates it with the stripe number.  Get the rotation
 * exactly right -- an off-by-one here passes casual testing and fails rebuild. */
static int parity_disk_of(long stripe)
{
	(void)stripe;
	return ndisks - 1;   /* TODO: rotate for level 5 */
}

/* TODO (Part 4): map logical block lba to its data disk and row (and stripe,
 * for 4/5).  Mind the difference between the chunk (stripe unit, in blocks) and
 * the block: `chunk` consecutive blocks sit on one disk before striping moves
 * on.  For RAID5 the data columns must skip the parity disk for their stripe. */
static void map_data(long lba, int *out_disk, long *out_row, long *out_stripe)
{
	(void)lba;
	*out_disk = 0;                       /* TODO */
	*out_row = 0;                        /* TODO */
	if (out_stripe) *out_stripe = 0;     /* TODO */
}

static long capacity_blocks(void) { return (long)data_disks() * rows; }

/* ---- parity reconstruction (Parts 4 and 5) -------------------------------- */

/* TODO (Part 5): reconstruct the block at (want_disk, row) in this stripe by
 * XORing every other participating disk's block in that row.  Return -1 if any
 * of those is itself failed (unrecoverable).  Used by degraded reads and
 * rebuild -- so getting this right is most of Part 5. */
static int reconstruct(int want_disk, long row, long stripe, unsigned char *out)
{
	(void)want_disk; (void)row; (void)stripe;
	memset(out, 0, bs);
	return -1;   /* TODO */
}

/* TODO (Part 4): recompute a stripe's parity block from scratch by XORing every
 * data disk's block in the row.  (Used on the degraded-write path, which the
 * tests do not exercise — so this may stay unused in a tests-only solution; the
 * `unused` attribute keeps `-Werror` from tripping on that, whatever you do to
 * `logical_write` below.) */
__attribute__((unused))
static void parity_recompute(long stripe, long row) { (void)stripe; (void)row; }

/* ---- logical read / write (Part 4) ---------------------------------------- */

/* TODO (Part 4/5): read logical block lba into out, serving from redundancy if
 * its disk has failed.  Return -1 if the data cannot be recovered. */
static int logical_read(long lba, unsigned char *out)
{
	int d; long row, stripe;
	map_data(lba, &d, &row, &stripe);
	if (!failed[d]) { phys_read(d, row, out); return 0; }
	/* TODO: reconstruct for 4/5 (and read a surviving mirror for level 1). */
	(void)reconstruct; (void)stripe;
	return -1;
}

/* TODO (Part 4): write logical block lba (all bytes = val), updating parity for
 * levels 4 and 5.  Implement BOTH additive and subtractive parity updates and
 * choose between them by cost when parity_mode is auto.  Subtractive must read
 * the OLD data and OLD parity; additive reads every other data block. */
static int logical_write(long lba, unsigned char val)
{
	int d; long row, stripe;
	map_data(lba, &d, &row, &stripe);
	unsigned char *nd = malloc(bs);
	memset(nd, val, bs);
	if (!failed[d]) phys_write(d, row, nd);
	/* TODO: parity update for 4/5, mirroring for 1, degraded handling. */
	(void)stripe; (void)parity_mode; (void)data_disks;
	free(nd);
	return 0;
}

/* ---- rebuild (Part 5) ----------------------------------------------------- */

/* TODO (Part 5): reconstruct every block of failed disk d onto a fresh (zeroed)
 * replacement, and return the number of physical blocks read while doing so. */
static long rebuild(int d)
{
	(void)d;
	return 0;   /* TODO */
}

/* ---- checksum (given: the byte-identity oracle) --------------------------- */

static uint64_t checksum(void)
{
	uint64_t h = 1469598103934665603ULL;
	unsigned char *tmp = malloc(bs);
	long cap = capacity_blocks();
	for (long lba = 0; lba < cap; lba++) {
		if (logical_read(lba, tmp) < 0) { h ^= 0xDEADBEEFCAFEBABEULL; h *= 1099511628211ULL; continue; }
		for (long i = 0; i < bs; i++) { h ^= tmp[i]; h *= 1099511628211ULL; }
	}
	free(tmp);
	return h;
}

/* ---- setup (given) -------------------------------------------------------- */

static int do_init(int level_, int ndisks_, long chunk_, long rows_, long bs_)
{
	if (ndisks_ < 1 || ndisks_ > MAX_DISKS) { fprintf(stderr, "raidsim: ndisks out of range\n"); return -1; }
	if (rows_ < 1 || rows_ > MAX_ROWS) { fprintf(stderr, "raidsim: blocks_per_disk out of range\n"); return -1; }
	if (bs_ < 1 || bs_ > MAX_BS) { fprintf(stderr, "raidsim: blocksize out of range\n"); return -1; }
	if (chunk_ < 1 || rows_ % chunk_ != 0) { fprintf(stderr, "raidsim: chunk must divide blocks_per_disk\n"); return -1; }
	if ((level_ == 4 || level_ == 5) && ndisks_ < 2) { fprintf(stderr, "raidsim: levels 4/5 need at least 2 disks\n"); return -1; }
	if (level_ == 1 && ndisks_ < 2) { fprintf(stderr, "raidsim: level 1 needs at least 2 disks\n"); return -1; }
	for (int i = 0; i < ndisks; i++) free(disk[i]);
	level = level_; ndisks = ndisks_; chunk = chunk_; rows = rows_; bs = bs_;
	parity_mode = PM_AUTO;
	phys_reads = phys_writes = 0;
	for (int i = 0; i < ndisks; i++) { disk[i] = calloc(rows, bs); failed[i] = 0; }
	return 0;
}

/* ---- command loop (given) ------------------------------------------------- */

static void print_block(const unsigned char *b)
{
	long show = bs < 8 ? bs : 8;
	for (long i = 0; i < show; i++) printf("%02x", b[i]);
}

static int run_script(FILE *f)
{
	char line[256];
	unsigned char *tmp = malloc(MAX_BS);
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\n' || *p == '\0') continue;
		char cmd[32];
		if (sscanf(p, "%31s", cmd) != 1) continue;

		if (strcmp(cmd, "init") == 0) {
			int lv, nd; long ch, rw, b;
			if (sscanf(p, "%*s %d %d %ld %ld %ld", &lv, &nd, &ch, &rw, &b) != 5) { printf("error init-args\n"); continue; }
			if (do_init(lv, nd, ch, rw, b) < 0) printf("error init-failed\n");
			else printf("init level=%d ndisks=%d chunk=%ld rows=%ld bs=%ld\n", level, ndisks, chunk, rows, bs);
		} else if (strcmp(cmd, "parity") == 0) {
			char m[32];
			if (sscanf(p, "%*s %31s", m) != 1) { printf("error parity-args\n"); continue; }
			if (strcmp(m, "additive") == 0) parity_mode = PM_ADD;
			else if (strcmp(m, "subtractive") == 0) parity_mode = PM_SUB;
			else parity_mode = PM_AUTO;
			printf("parity mode=%s\n", m);
		} else if (strcmp(cmd, "capacity") == 0) {
			printf("capacity blocks=%ld disks=%d datadisks=%d\n", capacity_blocks(), ndisks, data_disks());
		} else if (strcmp(cmd, "place") == 0) {
			long lba;
			if (sscanf(p, "%*s %ld", &lba) != 1) { printf("error place-args\n"); continue; }
			int d; long row, stripe;
			map_data(lba, &d, &row, &stripe);
			if (level == 4 || level == 5)
				printf("place lba=%ld data_disk=%d row=%ld stripe=%ld parity_disk=%d\n", lba, d, row, stripe, parity_disk_of(stripe));
			else if (level == 1)
				printf("place lba=%ld mirror_row=%ld ndisks=%d\n", lba, row, ndisks);
			else
				printf("place lba=%ld data_disk=%d row=%ld\n", lba, d, row);
		} else if (strcmp(cmd, "fill") == 0) {
			long seed;
			if (sscanf(p, "%*s %ld", &seed) != 1) { printf("error fill-args\n"); continue; }
			long cap = capacity_blocks();
			for (long lba = 0; lba < cap; lba++)
				logical_write(lba, (unsigned char)((lba * 2654435761UL + seed) & 0xff));
			printf("fill blocks=%ld seed=%ld\n", cap, seed);
		} else if (strcmp(cmd, "write") == 0) {
			long lba, val;
			if (sscanf(p, "%*s %ld %ld", &lba, &val) != 2) { printf("error write-args\n"); continue; }
			if (logical_write(lba, (unsigned char)(val & 0xff)) < 0) printf("write lba=%ld LOST\n", lba);
			else printf("write lba=%ld val=%ld\n", lba, val & 0xff);
		} else if (strcmp(cmd, "read") == 0) {
			long lba;
			if (sscanf(p, "%*s %ld", &lba) != 1) { printf("error read-args\n"); continue; }
			if (logical_read(lba, tmp) < 0) printf("read lba=%ld LOST\n", lba);
			else { printf("read lba=%ld bytes=", lba); print_block(tmp); printf("\n"); }
		} else if (strcmp(cmd, "readraw") == 0) {
			int d; long row;
			if (sscanf(p, "%*s %d %ld", &d, &row) != 2) { printf("error readraw-args\n"); continue; }
			if (d < 0 || d >= ndisks || row < 0 || row >= rows) { printf("error readraw-range\n"); continue; }
			if (failed[d]) printf("readraw disk=%d row=%ld FAILED\n", d, row);
			else { printf("readraw disk=%d row=%ld bytes=", d, row); print_block(cell(d, row)); printf("\n"); }
		} else if (strcmp(cmd, "iostat") == 0) {
			printf("iostat reads=%ld writes=%ld\n", phys_reads, phys_writes);
		} else if (strcmp(cmd, "reset") == 0) {
			phys_reads = phys_writes = 0;
			printf("reset\n");
		} else if (strcmp(cmd, "fail") == 0) {
			int d;
			if (sscanf(p, "%*s %d", &d) != 1) { printf("error fail-args\n"); continue; }
			if (d < 0 || d >= ndisks) { printf("error fail-range\n"); continue; }
			failed[d] = 1;
			printf("fail disk=%d\n", d);
		} else if (strcmp(cmd, "rebuild") == 0) {
			int d;
			if (sscanf(p, "%*s %d", &d) != 1) { printf("error rebuild-args\n"); continue; }
			if (d < 0 || d >= ndisks) { printf("error rebuild-range\n"); continue; }
			long readblocks = rebuild(d);
			printf("rebuild disk=%d blocks_read=%ld\n", d, readblocks);
		} else if (strcmp(cmd, "checksum") == 0) {
			printf("checksum %016llx\n", (unsigned long long)checksum());
		} else {
			printf("error unknown-cmd %s\n", cmd);
		}
	}
	free(tmp);
	return 0;
}

int main(int argc, char **argv)
{
	(void)xor_into;   /* you will want this for parity */
	FILE *f = stdin;
	if (argc >= 2 && strcmp(argv[1], "-") != 0) {
		f = fopen(argv[1], "r");
		if (!f) { fprintf(stderr, "raidsim: cannot open '%s'\n", argv[1]); return 1; }
	}
	int rc = run_script(f);
	if (f != stdin) fclose(f);
	for (int i = 0; i < ndisks; i++) free(disk[i]);
	return rc;
}
