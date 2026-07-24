/* fileclient.c -- Lab 10 Parts 4-5: the file client, with caching.
 * STARTER.
 *
 * Interface (fixed; the autograder reads these exact lines):
 *
 *   fileclient <port> workload name=<f> n=<K> [id=..] [timeout=..]
 *                      [retries=..] [delay=<ms>]
 *       The graded workload, cache off.  For i = 0..K-1 write the 32-byte
 *       record  "rec%05d" + 24 dots  at offset 32*i, printing
 *       "progress phase=write i=%d" per write; then read each record back,
 *       verify it byte-for-byte ("progress phase=read i=%d"), check
 *       getattr size == 32*K, and print
 *       "workload done n=%d wrote=%d verified=%d size=%ld rpcs=%ld retrans=%ld"
 *       Exactly 2K+2 RPCs: one lookup, K writes, K reads, one getattr.
 *       Exit 0 iff wrote == verified == n and the size matched.
 *
 *   fileclient <port> cmd [id=..] [ac=<ms>] [timeout=..] [retries=..]
 *       Command loop on stdin, one command per line, output flushed per
 *       command (the harness watches it live):
 *         open <name>          -> "open name=%s fh=%llu"   (creates)
 *         read <off> <len>     -> "read off=%ld len=%ld data=\"%s\""
 *         write <off> <text>   -> "write off=%ld len=%zu ok"
 *         getattr              -> "getattr size=%llu"
 *         sleep <ms>           -> (nothing)
 *         stats                -> "stats rpcs=%ld reads=%ld hits=%ld revals=%ld"
 *         quit                 -> exit 0
 *
 * The cache (Part 5), governed by ac=<ms> (0 = off):
 *   - the whole file's data plus its attributes are cached; attributes are
 *     trusted for ac ms after they were fetched;
 *   - a read inside that window is served from the cache: zero messages, a
 *     hit;
 *   - a read after the window revalidates with one getattr: if (size,
 *     mtime) are unchanged the cached data is still good (a reval), else
 *     it is discarded and refetched;
 *   - a write goes through to the server before the command completes, and
 *     invalidates the cache (the next read revalidates).
 * The guarantee this buys: a read is never staler than ac ms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fsproto.h"
#include "msg.h"
#include "rpc.h"

#define RECSIZE 32
#define MAXFILE 65536

static int kv(const char *arg, const char *key, long *out)
{
	size_t klen = strlen(key);
	if (strncmp(arg, key, klen) == 0 && arg[klen] == '=') {
		*out = strtol(arg + klen + 1, NULL, 10);
		return 1;
	}
	return 0;
}

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ------------------------------------------------------------------ */
/* Typed wrappers around rpc_call                                      */
/* ------------------------------------------------------------------ */

struct fc {
	rpc_client *rc;
	/* current file */
	int have;
	uint64_t fh;
	char name[FS_MAXNAME + 1];
	/* attribute cache */
	long ac_ms;
	int avalid;
	uint64_t asize, amt_s;
	uint32_t amt_ns;
	long afetched;          /* now_ms() when attrs were fetched */
	/* data cache */
	int dvalid;
	uint8_t data[MAXFILE];
	size_t dsize;
	/* counters */
	long reads, hits, revals;
};

static int fs_lookup(struct fc *f, const char *name, int create,
                     uint64_t *fh)
{
	uint8_t arg[FS_MAXNAME + 16], reply[RPC_MAXPAYLOAD];
	mbuf m;
	mb_winit(&m, arg, sizeof arg);
	mb_put_u32(&m, (uint32_t)create);
	mb_put_blob(&m, name, strlen(name));
	size_t rlen;
	int st = rpc_call(f->rc, FS_LOOKUP, arg, m.len,
	                  reply, sizeof reply, &rlen);
	if (st != RPC_OK)
		return st;
	mbuf rm;
	mb_rinit(&rm, reply, rlen);
	*fh = mb_get_u64(&rm);
	return mb_ok(&rm) ? RPC_OK : RPC_EARG;
}

static int fs_getattr(struct fc *f, uint64_t fh, uint64_t *size,
                      uint64_t *mt_s, uint32_t *mt_ns)
{
	(void)f; (void)fh; (void)size; (void)mt_s; (void)mt_ns;
	/* TODO (Part 4): marshal per fsproto.h, call FS_GETATTR, unmarshal
	 * size and mtime.  fs_lookup above is the worked example. */
	return RPC_EIO; /* TODO */
}

static int fs_read(struct fc *f, uint64_t fh, uint64_t off, uint32_t len,
                   uint8_t *out, size_t *outlen)
{
	(void)f; (void)fh; (void)off; (void)len; (void)out;
	*outlen = 0;
	/* TODO (Part 4): FS_READ per fsproto.h. */
	return RPC_EIO; /* TODO */
}

static int fs_write(struct fc *f, uint64_t fh, uint64_t off,
                    const void *buf, size_t len)
{
	(void)f; (void)fh; (void)off; (void)buf; (void)len;
	/* TODO (Part 4): FS_WRITE per fsproto.h; treat a short write as
	 * RPC_EIO. */
	return RPC_EIO; /* TODO */
}

/* ------------------------------------------------------------------ */
/* The cache                                                           */
/* ------------------------------------------------------------------ */

static void cache_drop(struct fc *f)
{
	f->avalid = 0;
	f->dvalid = 0;
}

/* A read through the cache.  *hit must be set iff the read cost zero
 * messages to the server.
 *
 * TODO (Part 5): implement the cache policy specified in the header
 * comment above, so a read inside the window costs no messages and the
 * staleness bound still holds.  Count a revalidating getattr in
 * f->revals.  Until you do, this goes straight to the server -- the
 * correct (cache-off) behaviour for Part 4. */
static int cached_read(struct fc *f, uint64_t off, uint32_t len,
                       uint8_t *out, size_t *outlen, int *hit)
{
	*hit = 0;
	(void)now_ms;   /* silence -Werror until your cache uses it */
	return fs_read(f, f->fh, off, len, out, outlen);
}

/* ------------------------------------------------------------------ */
/* The graded workload (Part 4)                                        */
/* ------------------------------------------------------------------ */

static void record_bytes(long i, char *out)
{
	snprintf(out, RECSIZE + 1, "rec%05ld........................", i);
}

static int workload(struct fc *f, const char *name, long n, long delay_ms)
{
	uint64_t fh;
	if (fs_lookup(f, name, 1, &fh) != RPC_OK) {
		fprintf(stderr, "workload: lookup failed\n");
		return 1;
	}
	long wrote = 0, verified = 0;
	for (long i = 0; i < n; i++) {
		char rec[RECSIZE + 1];
		record_bytes(i, rec);
		if (fs_write(f, fh, (uint64_t)(i * RECSIZE), rec,
		             RECSIZE) == RPC_OK)
			wrote++;
		else
			fprintf(stderr, "workload: write %ld failed\n", i);
		printf("progress phase=write i=%ld\n", i);
		fflush(stdout);
		if (delay_ms > 0)
			usleep((useconds_t)delay_ms * 1000u);
	}
	for (long i = 0; i < n; i++) {
		char rec[RECSIZE + 1];
		record_bytes(i, rec);
		uint8_t got[FS_MAXDATA];
		size_t glen = 0;
		if (fs_read(f, fh, (uint64_t)(i * RECSIZE), RECSIZE, got,
		            &glen) == RPC_OK &&
		    glen == RECSIZE && memcmp(got, rec, RECSIZE) == 0)
			verified++;
		else
			fprintf(stderr, "workload: read %ld bad\n", i);
		printf("progress phase=read i=%ld\n", i);
		fflush(stdout);
		if (delay_ms > 0)
			usleep((useconds_t)delay_ms * 1000u);
	}
	uint64_t size = 0, mt_s;
	uint32_t mt_ns;
	fs_getattr(f, fh, &size, &mt_s, &mt_ns);
	printf("workload done n=%ld wrote=%ld verified=%ld size=%ld "
	       "rpcs=%ld retrans=%ld\n",
	       n, wrote, verified, (long)size,
	       rpc_client_calls(f->rc), rpc_client_retrans(f->rc));
	return (wrote == n && verified == n &&
	        (long)size == n * RECSIZE) ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* The command loop (Parts 4-5)                                        */
/* ------------------------------------------------------------------ */

static void print_escaped(const uint8_t *b, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		int c = b[i];
		if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
			putchar(c);
		else
			printf("\\x%02x", c);
	}
}

static int cmdloop(struct fc *f)
{
	char line[FS_MAXDATA + 64];
	while (fgets(line, sizeof line, stdin) != NULL) {
		char a[FS_MAXDATA + 32], b[FS_MAXDATA + 32];
		long x, y;
		if (sscanf(line, "open %200s", a) == 1) {
			uint64_t fh;
			int st = fs_lookup(f, a, 1, &fh);
			if (st != RPC_OK) {
				printf("open name=%s ERR status=%d\n", a, st);
			} else {
				f->have = 1;
				f->fh = fh;
				snprintf(f->name, sizeof f->name, "%.200s",
				         a);
				cache_drop(f);
				printf("open name=%s fh=%llu\n", a,
				       (unsigned long long)fh);
			}
		} else if (sscanf(line, "read %ld %ld", &x, &y) == 2) {
			if (!f->have) {
				printf("read ERR no-file\n");
			} else {
				uint8_t out[FS_MAXDATA];
				size_t n = 0;
				int hit = 0;
				f->reads++;
				int st = cached_read(f, (uint64_t)x,
				                     (uint32_t)y, out, &n,
				                     &hit);
				if (hit)
					f->hits++;
				if (st != RPC_OK) {
					printf("read ERR status=%d\n", st);
				} else {
					printf("read off=%ld len=%ld data=\"",
					       x, y);
					print_escaped(out, n);
					printf("\"\n");
				}
			}
		} else if (sscanf(line, "write %ld %1055s", &x, b) == 2) {
			if (!f->have) {
				printf("write ERR no-file\n");
			} else {
				size_t len = strlen(b);
				int st = fs_write(f, f->fh, (uint64_t)x, b,
				                  len);
				if (st != RPC_OK) {
					printf("write ERR status=%d\n", st);
				} else {
					cache_drop(f);  /* write-through;
					                   revalidate next */
					printf("write off=%ld len=%zu ok\n",
					       x, len);
				}
			}
		} else if (strncmp(line, "getattr", 7) == 0) {
			uint64_t size, mt_s;
			uint32_t mt_ns;
			if (f->have &&
			    fs_getattr(f, f->fh, &size, &mt_s,
			               &mt_ns) == RPC_OK)
				printf("getattr size=%llu\n",
				       (unsigned long long)size);
			else
				printf("getattr ERR\n");
		} else if (sscanf(line, "sleep %ld", &x) == 1) {
			usleep((useconds_t)x * 1000u);
		} else if (strncmp(line, "stats", 5) == 0) {
			printf("stats rpcs=%ld reads=%ld hits=%ld revals=%ld\n",
			       rpc_client_calls(f->rc), f->reads, f->hits,
			       f->revals);
		} else if (strncmp(line, "quit", 4) == 0) {
			return 0;
		} else if (line[0] != '\n' && line[0] != '#') {
			printf("ERR bad command\n");
		}
		fflush(stdout);
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr,
		        "usage: fileclient <port> workload name=<f> n=<K> "
		        "[id=..] [timeout=..] [retries=..] [delay=<ms>]\n"
		        "       fileclient <port> cmd [id=..] [ac=<ms>] "
		        "[timeout=..] [retries=..]\n");
		return 2;
	}
	int port = atoi(argv[1]);
	const char *mode = argv[2];
	long id = (long)getpid(), timeout = 100, retries = 32;
	long n = 100, ac = 0, delay = 0;
	char name[FS_MAXNAME + 1] = "f";
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "id", &v)) id = v;
		else if (kv(argv[i], "timeout", &v)) timeout = v;
		else if (kv(argv[i], "retries", &v)) retries = v;
		else if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "ac", &v)) ac = v;
		else if (kv(argv[i], "delay", &v)) delay = v;
		else if (strncmp(argv[i], "name=", 5) == 0)
			snprintf(name, sizeof name, "%s", argv[i] + 5);
	}

	struct fc f;
	memset(&f, 0, sizeof f);
	f.ac_ms = ac;
	f.rc = rpc_client_open(port, (uint32_t)id, (int)timeout,
	                       (int)retries);
	if (f.rc == NULL)
		return 1;

	int rc;
	if (strcmp(mode, "workload") == 0)
		rc = workload(&f, name, n, delay);
	else if (strcmp(mode, "cmd") == 0)
		rc = cmdloop(&f);
	else {
		fprintf(stderr, "fileclient: unknown mode '%s'\n", mode);
		rc = 2;
	}
	rpc_client_close(f.rc);
	return rc;
}
