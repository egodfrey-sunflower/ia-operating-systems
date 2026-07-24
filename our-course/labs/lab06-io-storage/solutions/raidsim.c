/* raidsim.c -- Lab 6 Parts 4 and 5: a RAID engine for levels 0, 1, 4 and 5,
 * with XOR parity, additive/subtractive parity updates, degraded-mode reads,
 * and a failed-disk rebuild.
 *
 * raidsim reads a command script from a file argument (or stdin if "-") and
 * executes it against an in-memory array, printing one or more lines per
 * command.  The commands:
 *
 *   init <level> <ndisks> <chunk> <blocks_per_disk> <blocksize>
 *          level is 0, 1, 4 or 5.  chunk is the stripe unit in BLOCKS (how many
 *          consecutive logical blocks land on one disk before striping moves to
 *          the next).  blocksize is the block's size in bytes.
 *   parity <additive|subtractive|auto>
 *          how a small write updates parity (levels 4 and 5).  Default auto.
 *   capacity
 *          print the usable capacity in logical blocks.
 *   place <lba>
 *          print where logical block lba lives: data disk and row, and (for 4/5)
 *          the parity disk for its stripe.
 *   fill <seed>
 *          write every logical block with a deterministic pattern from lba+seed.
 *   write <lba> <byte>
 *          write logical block lba, every byte set to <byte> (0..255).
 *   read <lba>
 *          print the logical block's first bytes, served through redundancy if a
 *          disk has failed.  Prints "LOST" if the data cannot be recovered.
 *   readraw <disk> <row>
 *          print the raw bytes physically stored at (disk, row) -- lets a test
 *          check the parity block directly.  Prints "FAILED" for a failed disk.
 *   iostat / reset
 *          print / zero the physical read and write counters.
 *   fail <disk>
 *          mark a disk failed: its contents become inaccessible.
 *   rebuild <disk>
 *          reconstruct a failed disk onto a fresh replacement and print how many
 *          physical blocks the rebuild had to read.
 *   checksum
 *          print a 64-bit hash over every logical block, read through
 *          redundancy.  Two arrays with byte-identical logical contents print
 *          the same checksum; this is the byte-identity oracle for rebuild.
 *
 * Parity maths is plain XOR: the parity block of a stripe is the XOR of that
 * stripe's data blocks.  Any one missing block in a stripe is the XOR of all
 * the others, which is what makes single-disk failure survivable on 4 and 5.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_DISKS 32
#define MAX_ROWS  4096
#define MAX_BS    4096

static int    level;
static int    ndisks;
static long   chunk;          /* stripe unit, in blocks */
static long   rows;           /* blocks per disk */
static long   bs;             /* block size, in bytes */
static int    parity_mode;    /* 0 auto, 1 additive, 2 subtractive */

static unsigned char *disk[MAX_DISKS];   /* disk[d] is rows*bs bytes */
static int    failed[MAX_DISKS];

static long   phys_reads, phys_writes;

enum { PM_AUTO = 0, PM_ADD = 1, PM_SUB = 2 };

/* ---- physical block access (with I/O accounting) -------------------------- */

static unsigned char *cell(int d, long row)
{
	return disk[d] + row * bs;
}

static void phys_read(int d, long row, unsigned char *out)
{
	phys_reads++;
	memcpy(out, cell(d, row), bs);
}

static void phys_write(int d, long row, const unsigned char *in)
{
	phys_writes++;
	memcpy(cell(d, row), in, bs);
}

static void xor_into(unsigned char *acc, const unsigned char *src)
{
	for (long i = 0; i < bs; i++) acc[i] ^= src[i];
}

/* ---- geometry: logical block -> physical placement ------------------------ */

/* Number of data disks per stripe. */
static int data_disks(void)
{
	if (level == 0) return ndisks;
	if (level == 1) return 1;           /* mirror: one column of live data */
	return ndisks - 1;                  /* 4 and 5: one parity disk per stripe */
}

/* Which physical disk holds the parity for stripe s (levels 4 and 5). */
static int parity_disk_of(long stripe)
{
	if (level == 4) return ndisks - 1;
	/* level 5: parity rotates with the stripe number.  Off-by-one here passes
	 * casual testing and fails rebuild, so it is pinned down exactly:
	 * stripe 0 -> disk ndisks-1, stripe 1 -> ndisks-2, ... wrapping. */
	return (ndisks - 1 - (int)(stripe % ndisks) + ndisks) % ndisks;
}

/* Map logical block lba to its data disk and row.  For 4/5 also report the
 * stripe (so the caller can find the parity disk). */
static void map_data(long lba, int *out_disk, long *out_row, long *out_stripe)
{
	long dd = data_disks();
	long chunk_index = lba / chunk;          /* which stripe unit */
	long chunk_off   = lba % chunk;          /* offset within the unit */
	long stripe      = chunk_index / dd;     /* which stripe */
	long col         = chunk_index % dd;     /* which data column in the stripe */
	long row         = stripe * chunk + chunk_off;

	int pdisk;
	int phys;
	if (level == 0 || level == 1) {
		phys = (int)col;                 /* level 1: col is always 0 (mirror) */
		pdisk = -1;
	} else if (level == 4) {
		phys = (int)col;                 /* data disks are 0..ndisks-2 */
		pdisk = ndisks - 1;
	} else { /* level 5: data columns skip the parity disk for this stripe */
		pdisk = parity_disk_of(stripe);
		/* the col-th data disk, skipping pdisk */
		phys = (int)col;
		if (phys >= pdisk) phys++;
	}
	(void)pdisk;
	*out_disk = phys;
	*out_row = row;
	if (out_stripe) *out_stripe = stripe;
}

static long capacity_blocks(void)
{
	return (long)data_disks() * rows;
}

/* ---- parity reconstruction ------------------------------------------------ */

/* Reconstruct the block at (want_disk, row) for a given stripe by XORing every
 * other disk's block in that row.  Used for degraded reads and rebuild.  All
 * other disks in the stripe must be alive. */
static int reconstruct(int want_disk, long row, long stripe, unsigned char *out)
{
	memset(out, 0, bs);
	unsigned char *tmp = malloc(bs);
	int pdisk = (level == 4 || level == 5) ? parity_disk_of(stripe) : -1;
	int dd = data_disks();

	/* Walk every physical disk that participates in this stripe (all data
	 * columns plus the parity disk) except want_disk, and XOR it in. */
	for (int col = 0; col < dd; col++) {
		int phys = col;
		if (level == 5 && phys >= pdisk) phys++;
		if (phys == want_disk) continue;
		if (failed[phys]) { free(tmp); return -1; }
		phys_read(phys, row, tmp);
		xor_into(out, tmp);
	}
	if (pdisk >= 0 && pdisk != want_disk) {
		if (failed[pdisk]) { free(tmp); return -1; }
		phys_read(pdisk, row, tmp);
		xor_into(out, tmp);
	}
	free(tmp);
	return 0;
}

/* Recompute and store the parity block for a stripe from scratch (additive
 * form): read every data disk's block in the row and XOR. */
static void parity_recompute(long stripe, long row)
{
	int pdisk = parity_disk_of(stripe);
	int dd = data_disks();
	unsigned char *acc = calloc(1, bs);
	unsigned char *tmp = malloc(bs);
	for (int col = 0; col < dd; col++) {
		int phys = col;
		if (level == 5 && phys >= pdisk) phys++;
		phys_read(phys, row, tmp);
		xor_into(acc, tmp);
	}
	phys_write(pdisk, row, acc);
	free(acc);
	free(tmp);
}

/* ---- logical read / write ------------------------------------------------- */

/* Read logical block lba into out, serving from redundancy if its disk failed.
 * Returns 0 on success, -1 if the data is unrecoverable (too many failures). */
static int logical_read(long lba, unsigned char *out)
{
	int d; long row, stripe;
	map_data(lba, &d, &row, &stripe);

	if (level == 1) {
		/* mirror: read any surviving copy */
		for (int i = 0; i < ndisks; i++)
			if (!failed[i]) { phys_read(i, row, out); return 0; }
		return -1;
	}
	if (!failed[d]) {
		phys_read(d, row, out);
		return 0;
	}
	/* the data disk failed */
	if (level == 0) return -1;                 /* no redundancy */
	return reconstruct(d, row, stripe, out);   /* 4/5: rebuild from the stripe */
}

/* Write logical block lba (all bytes = val), updating parity for 4/5. */
static int logical_write(long lba, unsigned char val)
{
	int d; long row, stripe;
	map_data(lba, &d, &row, &stripe);
	unsigned char *nd = malloc(bs);
	memset(nd, val, bs);

	if (level == 0) {
		if (failed[d]) { free(nd); return -1; }
		phys_write(d, row, nd);
		free(nd);
		return 0;
	}
	if (level == 1) {
		for (int i = 0; i < ndisks; i++)
			if (!failed[i]) phys_write(i, row, nd);   /* write every mirror */
		free(nd);
		return 0;
	}

	/* levels 4 and 5: data + parity */
	int pdisk = parity_disk_of(stripe);
	if (failed[d] || failed[pdisk]) {
		/* Degraded write: keep it simple -- write whatever disks survive and
		 * recompute parity from the live data.  (A production array logs and
		 * reconstructs; that is beyond this lab.) */
		if (!failed[d]) phys_write(d, row, nd);
		if (!failed[pdisk]) parity_recompute(stripe, row);
		free(nd);
		return 0;
	}

	int use_sub;
	int dd = data_disks();
	if (parity_mode == PM_SUB) use_sub = 1;
	else if (parity_mode == PM_ADD) use_sub = 0;
	else {
		/* auto: subtractive costs 2 reads + 2 writes regardless of width;
		 * additive costs (dd-1) reads + 2 writes.  Pick the cheaper. */
		long sub_ios = 2 + 2;
		long add_ios = (dd - 1) + 2;
		use_sub = sub_ios <= add_ios;
	}

	unsigned char *old_d = malloc(bs);
	unsigned char *par   = malloc(bs);
	if (use_sub) {
		/* subtractive: new_parity = old_parity XOR old_data XOR new_data.
		 * Must read the OLD data and the OLD parity -- two reads. */
		phys_read(d, row, old_d);
		phys_read(pdisk, row, par);
		xor_into(par, old_d);   /* remove old data */
		xor_into(par, nd);      /* add new data */
		phys_write(d, row, nd);
		phys_write(pdisk, row, par);
	} else {
		/* additive: read every OTHER data block, XOR with new data. */
		memset(par, 0, bs);
		for (int col = 0; col < dd; col++) {
			int phys = col;
			if (level == 5 && phys >= pdisk) phys++;
			if (phys == d) continue;
			phys_read(phys, row, old_d);
			xor_into(par, old_d);
		}
		xor_into(par, nd);
		phys_write(d, row, nd);
		phys_write(pdisk, row, par);
	}
	free(old_d); free(par); free(nd);
	return 0;
}

/* ---- rebuild -------------------------------------------------------------- */

/* Reconstruct every block of a failed disk onto a fresh replacement.  Returns
 * the number of physical blocks read during reconstruction. */
static long rebuild(int d)
{
	if (!failed[d]) return 0;
	long before = phys_reads;

	/* fresh replacement: start from a zeroed disk */
	memset(disk[d], 0, rows * bs);

	unsigned char *tmp = malloc(bs);
	for (long row = 0; row < rows; row++) {
		if (level == 0) {
			/* no redundancy: nothing to reconstruct from */
			continue;
		}
		if (level == 1) {
			/* copy from any surviving mirror */
			int src = -1;
			for (int i = 0; i < ndisks; i++)
				if (i != d && !failed[i]) { src = i; break; }
			if (src >= 0) {
				phys_read(src, row, tmp);
				/* mark rebuilt live before writing so the write lands */
				failed[d] = 0;
				phys_write(d, row, tmp);
				failed[d] = 1;
			}
			continue;
		}
		/* levels 4 and 5: the stripe for this row is row/chunk */
		long stripe = row / chunk;
		if (reconstruct(d, row, stripe, tmp) == 0) {
			failed[d] = 0;
			phys_write(d, row, tmp);
			failed[d] = 1;
		}
	}
	free(tmp);
	failed[d] = 0;   /* replacement is now live */
	return phys_reads - before;
}

/* ---- checksum (byte-identity oracle) -------------------------------------- */

static uint64_t checksum(void)
{
	uint64_t h = 1469598103934665603ULL;   /* FNV-1a offset */
	unsigned char *tmp = malloc(bs);
	long cap = capacity_blocks();
	for (long lba = 0; lba < cap; lba++) {
		if (logical_read(lba, tmp) < 0) {
			/* unrecoverable: fold a distinctive marker so a lossy array
			 * checksums differently from an intact one. */
			h ^= 0xDEADBEEFCAFEBABEULL;
			h *= 1099511628211ULL;
			continue;
		}
		for (long i = 0; i < bs; i++) {
			h ^= tmp[i];
			h *= 1099511628211ULL;
		}
	}
	free(tmp);
	return h;
}

/* ---- setup ---------------------------------------------------------------- */

static int do_init(int level_, int ndisks_, long chunk_, long rows_, long bs_)
{
	if (ndisks_ < 1 || ndisks_ > MAX_DISKS) {
		fprintf(stderr, "raidsim: ndisks out of range\n"); return -1;
	}
	if (rows_ < 1 || rows_ > MAX_ROWS) {
		fprintf(stderr, "raidsim: blocks_per_disk out of range\n"); return -1;
	}
	if (bs_ < 1 || bs_ > MAX_BS) {
		fprintf(stderr, "raidsim: blocksize out of range\n"); return -1;
	}
	if (chunk_ < 1 || rows_ % chunk_ != 0) {
		fprintf(stderr, "raidsim: chunk must divide blocks_per_disk\n"); return -1;
	}
	if ((level_ == 4 || level_ == 5) && ndisks_ < 2) {
		fprintf(stderr, "raidsim: levels 4/5 need at least 2 disks\n"); return -1;
	}
	if (level_ == 1 && ndisks_ < 2) {
		fprintf(stderr, "raidsim: level 1 needs at least 2 disks\n"); return -1;
	}
	for (int i = 0; i < ndisks; i++) free(disk[i]);
	level = level_; ndisks = ndisks_; chunk = chunk_; rows = rows_; bs = bs_;
	parity_mode = PM_AUTO;
	phys_reads = phys_writes = 0;
	for (int i = 0; i < ndisks; i++) {
		disk[i] = calloc(rows, bs);
		failed[i] = 0;
	}
	return 0;
}

/* ---- command loop --------------------------------------------------------- */

static void print_block(const unsigned char *b)
{
	long show = bs < 8 ? bs : 8;      /* first 8 bytes is plenty to identify */
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
			if (sscanf(p, "%*s %d %d %ld %ld %ld", &lv, &nd, &ch, &rw, &b) != 5) {
				printf("error init-args\n"); continue;
			}
			if (do_init(lv, nd, ch, rw, b) < 0) printf("error init-failed\n");
			else printf("init level=%d ndisks=%d chunk=%ld rows=%ld bs=%ld\n",
			            level, ndisks, chunk, rows, bs);
		} else if (strcmp(cmd, "parity") == 0) {
			char m[32];
			if (sscanf(p, "%*s %31s", m) != 1) { printf("error parity-args\n"); continue; }
			if (strcmp(m, "additive") == 0) parity_mode = PM_ADD;
			else if (strcmp(m, "subtractive") == 0) parity_mode = PM_SUB;
			else parity_mode = PM_AUTO;
			printf("parity mode=%s\n", m);
		} else if (strcmp(cmd, "capacity") == 0) {
			printf("capacity blocks=%ld disks=%d datadisks=%d\n",
			       capacity_blocks(), ndisks, data_disks());
		} else if (strcmp(cmd, "place") == 0) {
			long lba;
			if (sscanf(p, "%*s %ld", &lba) != 1) { printf("error place-args\n"); continue; }
			int d; long row, stripe;
			map_data(lba, &d, &row, &stripe);
			if (level == 4 || level == 5)
				printf("place lba=%ld data_disk=%d row=%ld stripe=%ld parity_disk=%d\n",
				       lba, d, row, stripe, parity_disk_of(stripe));
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
			if (logical_write(lba, (unsigned char)(val & 0xff)) < 0)
				printf("write lba=%ld LOST\n", lba);
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
			if (failed[d]) { printf("readraw disk=%d row=%ld FAILED\n", d, row); }
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
