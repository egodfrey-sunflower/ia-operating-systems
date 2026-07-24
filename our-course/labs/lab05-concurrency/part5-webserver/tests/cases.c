/*
 * cases.c -- the Part 5 test driver: an HTTP client that knows what the
 * answers should be.
 *
 *      cases <case-name> <port> <docroot>
 *
 * One case, one process, exit 0 if it passed, 1 if it failed, 2 if the name
 * is not a case. Complaints go to stderr, four spaces in. run.sh starts the
 * server, reads the port it announces, and runs these against it -- once for
 * each of the two servers, except for the two cases that exist to tell the
 * two architectures apart.
 *
 * Everything here is a real client on a real socket. Nothing reads the
 * server's source, and nothing assumes anything about how it is built beyond
 * the contract in http.h: bytes in, bytes out, and how long they take.
 *
 * The interesting cases are the ones about time rather than content:
 *
 *   p5_stalled          -- a client that sends half a request and then says
 *                          nothing must not stop another client being
 *                          served. Both servers must pass this.
 *
 *   p5_headofline_*     -- the same thing with MORE stalled clients than the
 *                          thread pool has workers. The thread-pool server
 *                          must block (there is nobody left to serve with)
 *                          and the event server must not (there was never
 *                          anybody in the first place). One case each, and
 *                          they are the pair that makes "twice" mean
 *                          something.
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Kept in step with http.h by hand: the driver must not include the
 * student's header, because the point of some of these cases is what
 * happens when the student's idea of the contract has drifted. */
#define POOL_THREADS 4

static int failures;
static int port;
static const char *docroot;

static void fail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "    ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	failures++;
}

#define CHECK(cond, ...) do { if (!(cond)) fail(__VA_ARGS__); } while (0)
#define REQUIRE(cond, ...) do { \
	if (!(cond)) { fail(__VA_ARGS__); return; } \
} while (0)

static double now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void nap_ms(long ms)
{
	struct timespec ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) == -1)
		;
}

/* ==================================================================
 * a small HTTP client
 * ================================================================== */

#define RESP_MAX (1024 * 1024)

/*
 * rcvbuf: if positive, the client asks for a tiny receive buffer before
 * connecting. That is how the partial-write case is built -- see c_drip.
 *
 * RETRIES ARE THE HARNESS PROTECTING ITSELF, NOT THE SERVER. Running this
 * suite over and over leaves thousands of sockets in TIME_WAIT, and
 * connect() then fails intermittently with EADDRNOTAVAIL -- measured: after
 * twenty back-to-back runs, seven of them failed a case because the
 * HARNESS could not open a socket. That is a defect in a test, not evidence
 * about a server, so a failed connect is retried a few times and, if it
 * still fails, the case says so in those words rather than blaming the
 * server.
 */
static int last_dial_errno;

static int dial_buf(int rcvbuf)
{
	struct sockaddr_in addr;
	struct timeval tv;
	int fd, attempt;

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons((unsigned short)port);

	for (attempt = 0; attempt < 8; attempt++) {
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			last_dial_errno = errno;
			nap_ms(25);
			continue;
		}
		if (rcvbuf > 0)
			setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf,
				   sizeof rcvbuf);
		tv.tv_sec = 3;          /* never wait for ever: a hung server
					 * must fail a case, not the suite.
					 * Three seconds is comfortably more
					 * than the ~1 s the thread pool takes
					 * while the stalled-client cases run,
					 * and comfortably less than run.sh's
					 * per-case timeout. */
		tv.tv_usec = 0;
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
		if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0)
			return fd;
		last_dial_errno = errno;
		close(fd);
		nap_ms(25);
	}
	return -1;
}

static int dial(void)
{
	return dial_buf(0);
}

static int send_all(int fd, const char *buf, size_t len)
{
	while (len > 0) {
		ssize_t n = write(fd, buf, len);

		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			return -1;
		}
		buf += n;
		len -= (size_t)n;
	}
	return 0;
}

/* Read to end of file. Returns the byte count, or -1. */
static ssize_t slurp(int fd, char *buf, size_t max)
{
	size_t got = 0;

	for (;;) {
		ssize_t n = read(fd, buf + got, max - got);

		if (n == 0)
			return (ssize_t)got;
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;      /* timeout counts as a failure */
		}
		got += (size_t)n;
		if (got == max)
			return (ssize_t)got;
	}
}

struct response {
	int status;
	long content_length;    /* -1 if the header did not carry one */
	const char *body;
	size_t bodylen;
	char buf[RESP_MAX];
	size_t len;
};

static int parse_response(struct response *r)
{
	const char *hdr_end, *cl;
	char ver[16];
	int status;

	if (r->len < 12)
		return -1;
	if (sscanf(r->buf, "%15s %d", ver, &status) != 2)
		return -1;
	if (strncmp(ver, "HTTP/1.", 7) != 0)
		return -1;
	r->status = status;
	hdr_end = memmem(r->buf, r->len, "\r\n\r\n", 4);
	if (!hdr_end)
		return -1;
	r->body = hdr_end + 4;
	r->bodylen = r->len - (size_t)(r->body - r->buf);
	r->content_length = -1;
	cl = memmem(r->buf, (size_t)(hdr_end - r->buf), "Content-Length:", 15);
	if (cl)
		r->content_length = strtol(cl + 15, NULL, 10);
	return 0;
}

/*
 * One request, one connection, the whole answer. The stalled-client cases
 * below do it the other way: send a request WITHOUT its final blank line and
 * leave the connection open.
 */
static int request(struct response *r, const char *path)
{
	char req[512];
	ssize_t n;
	int fd = dial();

	if (fd < 0)
		return -1;
	snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: localhost\r\n\r\n",
		 path);
	if (send_all(fd, req, strlen(req)) < 0) {
		close(fd);
		return -1;
	}
	n = slurp(fd, r->buf, sizeof r->buf);
	close(fd);
	if (n <= 0)
		return -1;
	r->len = (size_t)n;
	return parse_response(r);
}

static int raw_request(struct response *r, const char *raw)
{
	ssize_t n;
	int fd = dial();

	if (fd < 0)
		return -1;
	if (send_all(fd, raw, strlen(raw)) < 0) {
		close(fd);
		return -1;
	}
	n = slurp(fd, r->buf, sizeof r->buf);
	close(fd);
	if (n <= 0)
		return -1;
	r->len = (size_t)n;
	return parse_response(r);
}

/* The fixture, read straight off the disk for comparison. */
static char *read_fixture(const char *name, size_t *len)
{
	char full[1024];
	struct stat st;
	char *buf;
	FILE *f;

	snprintf(full, sizeof full, "%s/%s", docroot, name);
	if (stat(full, &st) < 0)
		return NULL;
	buf = malloc((size_t)st.st_size + 1);
	if (!buf)
		return NULL;
	f = fopen(full, "rb");
	if (!f) {
		free(buf);
		return NULL;
	}
	if (fread(buf, 1, (size_t)st.st_size, f) != (size_t)st.st_size) {
		fclose(f);
		free(buf);
		return NULL;
	}
	fclose(f);
	*len = (size_t)st.st_size;
	return buf;
}

/* Where the two byte strings first differ, or -1. */
static long first_difference(const char *a, size_t alen, const char *b,
			     size_t blen)
{
	size_t i, n = alen < blen ? alen : blen;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return (long)i;
	if (alen != blen)
		return (long)n;
	return -1;
}

/* ==================================================================
 * the fixtures, fetched and compared byte for byte
 * ================================================================== */

struct fixture {
	const char *path;       /* what to ask for */
	const char *file;       /* what should come back */
};

static const struct fixture fixtures[] = {
	{ "/",           "index.html" },
	{ "/index.html", "index.html" },
	{ "/hello.txt",  "hello.txt" },
	{ "/page.html",  "page.html" },
	{ "/bytes.bin",  "bytes.bin" },
	{ "/big.bin",    "big.bin" },
};

#define NFIXTURES (sizeof fixtures / sizeof fixtures[0])

static struct response resp;    /* one megabyte; not on the stack */

static void c_fetch(void)
{
	size_t i;

	for (i = 0; i < NFIXTURES; i++) {
		size_t flen;
		char *want = read_fixture(fixtures[i].file, &flen);
		long diff;

		REQUIRE(want != NULL, "the harness could not read its own "
			"fixture %s -- this is a bug in the harness, not in "
			"your server", fixtures[i].file);
		if (request(&resp, fixtures[i].path) < 0) {
			fail("GET %s produced no usable HTTP response at all. "
			     "Either the connection was closed without one, or "
			     "what came back does not start with a status line "
			     "and end its header with a blank line.",
			     fixtures[i].path);
			free(want);
			continue;
		}
		CHECK(resp.status == 200, "GET %s answered %d, not 200",
		      fixtures[i].path, resp.status);
		CHECK(resp.content_length == (long)flen,
		      "GET %s said Content-Length: %ld; %s is %zu bytes. The "
		      "length has to be the file's length, and it has to be "
		      "known before the header goes out.",
		      fixtures[i].path, resp.content_length,
		      fixtures[i].file, flen);
		diff = first_difference(resp.body, resp.bodylen, want, flen);
		if (diff >= 0)
			fail("GET %s returned %zu bytes and %s is %zu bytes; "
			     "they first differ at offset %ld. %s",
			     fixtures[i].path, resp.bodylen,
			     fixtures[i].file, flen, diff,
			     resp.bodylen < flen ?
			     "The body is short, which is what a write() whose "
			     "return value was ignored looks like: it took "
			     "what fitted in the socket buffer and the rest "
			     "was never sent." :
			     "The bytes are wrong: a body written with a "
			     "string function stops at the first NUL, and one "
			     "of these fixtures is full of them.");
		free(want);
	}
}

/* ==================================================================
 * status codes
 * ================================================================== */

static void c_status(void)
{
	if (request(&resp, "/nothing-here.txt") == 0)
		CHECK(resp.status == 404,
		      "a request for a file that does not exist answered %d, "
		      "not 404", resp.status);
	else
		fail("a request for a file that does not exist got no usable "
		     "response; it should be a 404 with a body");

	if (raw_request(&resp, "NONSENSE\r\n\r\n") == 0)
		CHECK(resp.status == 400,
		      "an unparseable request line answered %d, not 400",
		      resp.status);
	else
		fail("an unparseable request got no usable response; it "
		     "should be a 400. Do not close the connection without "
		     "answering: a client cannot tell that apart from a "
		     "crash.");

	if (raw_request(&resp, "POST /hello.txt HTTP/1.0\r\n\r\n") == 0)
		CHECK(resp.status == 501,
		      "a POST answered %d, not 501", resp.status);
	else
		fail("a POST got no usable response; it should be a 501");
}

/*
 * Path traversal. http_resolve rejects it and returns -1, so this is really
 * a check that the resolver's answer is being acted on -- but it is worth a
 * case of its own because the failure is not a wrong status code, it is
 * handing out a file from outside the document root.
 */
static void c_traversal(void)
{
	static const char *tries[] = {
		"/../secret.txt",
		"/subdir/../../secret.txt",
		"/..%2fsecret.txt",
	};
	size_t i;

	for (i = 0; i < sizeof tries / sizeof tries[0]; i++) {
		if (request(&resp, tries[i]) < 0) {
			fail("GET %s got no usable response; it should be a "
			     "403 or a 404, not a closed connection",
			     tries[i]);
			continue;
		}
		CHECK(resp.status == 403 || resp.status == 404,
		      "GET %s answered %d. A path containing '..' is a 403 "
		      "(http_resolve returns -1 for it) and anything else "
		      "that does not resolve is a 404.", tries[i],
		      resp.status);
		if (resp.bodylen &&
		    memmem(resp.body, resp.bodylen, "SECRET-CONTENT", 14)) {
			fail("GET %s SERVED A FILE FROM OUTSIDE THE DOCUMENT "
			     "ROOT. http_resolve told you not to; its return "
			     "value has to be checked before the file is "
			     "opened.", tries[i]);
		}
	}
}

/* ==================================================================
 * the partial write
 *
 * The case a 256 KB fetch does NOT cover, and why it does not is worth
 * stating because it was measured rather than assumed: on loopback, a 256 KB
 * write() into a socket whose send buffer has autotuned up to a few megabytes
 * is accepted whole, even when the client's receive buffer is 4 KB and the
 * client is not reading. A server that ignores write()'s return value passed
 * every other case in this suite.
 *
 * So this case is built to make the write go short. The fixture is 8 MB --
 * larger than this kernel's maximum send buffer (net.ipv4.tcp_wmem's third
 * number) -- the client asks for a 4 KB receive buffer before connecting, and
 * it waits 300 ms before reading anything. Measured against the reference:
 * the first write() takes about 1.8 MB of the 8 MB and the rest goes out in
 * further writes of ~700 KB. An EVENT server that assumes one write() is
 * enough delivers about a fifth of the file and hangs up.
 *
 * What this case does NOT cover, and no case here can: the same mistake in
 * the THREADED server. Its socket is blocking, and a blocking write on Linux
 * does not return short -- it waits until the last byte is in the kernel.
 * Measured: a single blocking write() of all 8 388 608 bytes to this very
 * client returned 8 388 608. Both servers are run against this case anyway,
 * because a threaded submission that somehow does go short must still be
 * caught; but a green result here says nothing about the threaded server's
 * write loop, and solutions/README.md lists that among the honour items.
 *
 * The body is compared as it arrives rather than being buffered, because 8 MB
 * of response does not want to live in a static buffer.
 * ================================================================== */

#define DRIP_CHUNK 65536

static void c_drip(void)
{
	char req[256], buf[DRIP_CHUNK];
	size_t flen, got = 0, bodylen = 0;
	char *want, *hdr_end;
	size_t hdrlen = 0;
	char hdr[1024];
	int fd, status = 0, in_body = 0, mismatch = -1;

	want = read_fixture("huge.bin", &flen);
	REQUIRE(want != NULL, "the harness could not read its own huge.bin");
	fd = dial_buf(4096);
	if (fd < 0) {
		fail("could not connect");
		free(want);
		return;
	}
	snprintf(req, sizeof req,
		 "GET /huge.bin HTTP/1.0\r\nHost: localhost\r\n\r\n");
	if (send_all(fd, req, strlen(req)) < 0) {
		fail("could not send the request");
		close(fd);
		free(want);
		return;
	}
	nap_ms(300);            /* fill the buffers, so the write goes short */
	for (;;) {
		ssize_t n = read(fd, buf, sizeof buf);
		size_t off = 0;

		if (n == 0)
			break;
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		got += (size_t)n;
		if (!in_body) {
			size_t take = (size_t)n;

			if (take > sizeof hdr - 1 - hdrlen)
				take = sizeof hdr - 1 - hdrlen;
			memcpy(hdr + hdrlen, buf, take);
			hdrlen += take;
			hdr[hdrlen] = '\0';
			hdr_end = memmem(hdr, hdrlen, "\r\n\r\n", 4);
			if (!hdr_end)
				continue;
			sscanf(hdr, "HTTP/1.%*d %d", &status);
			off = (size_t)(hdr_end + 4 - hdr);
			in_body = 1;
		}
		for (; off < (size_t)n; off++) {
			if (bodylen < flen && buf[off] != want[bodylen] &&
			    mismatch < 0)
				mismatch = (int)bodylen;
			bodylen++;
		}
	}
	close(fd);
	free(want);
	CHECK(status == 200, "GET /huge.bin answered %d, not 200", status);
	CHECK(mismatch < 0,
	      "a slow-reading client got the wrong byte at offset %d of "
	      "huge.bin", mismatch);
	CHECK(bodylen == flen,
	      "a client that read slowly got %zu bytes of huge.bin's %zu. Its "
	      "receive buffer was 4 KB and it waited 300 ms before reading, so "
	      "the server's write() could not take the whole response and "
	      "returned a smaller number than it was given. That number is not "
	      "advice: the rest of the body still has to be sent, on the next "
	      "writable event. (This case can only fire against the event "
	      "server. The threaded server writes to a blocking socket, which "
	      "on Linux does not return short.)",
	      bodylen, flen);
}

/* ==================================================================
 * load
 * ================================================================== */

#define SOAK_CLIENTS 8
#define SOAK_EACH    25

static int soak_bad;
static int soak_bad_status;
static int soak_nores;

static void *soak_client(void *a)
{
	struct response *r = malloc(sizeof *r);
	long id = (long)a;
	int i;

	if (!r)
		return NULL;
	for (i = 0; i < SOAK_EACH; i++) {
		const struct fixture *f = &fixtures[(size_t)(id + i) % NFIXTURES];
		size_t flen;
		char *want;

		if (request(r, f->path) < 0) {
			/* Stop after five: a server that has stopped
			 * answering has stopped answering, and the case
			 * should say so rather than time out. */
			if (__atomic_add_fetch(&soak_nores, 1,
					       __ATOMIC_ACQ_REL) >= 5)
				break;
			continue;
		}
		if (r->status != 200) {
			__atomic_store_n(&soak_bad_status, r->status,
					 __ATOMIC_RELAXED);
			__atomic_add_fetch(&soak_bad, 1, __ATOMIC_ACQ_REL);
			continue;
		}
		want = read_fixture(f->file, &flen);
		if (!want)
			continue;
		if (first_difference(r->body, r->bodylen, want, flen) >= 0)
			__atomic_add_fetch(&soak_bad, 1, __ATOMIC_ACQ_REL);
		free(want);
	}
	free(r);
	return NULL;
}

/*
 * Eight clients at once, twenty-five requests each, over the whole fixture set.
 * This is the case a thread pool that loses a task fails: the request goes
 * into the queue and no worker ever takes it out, so a client waits for a
 * response that is not coming and its read times out.
 */
static void c_soak(void)
{
	pthread_t t[SOAK_CLIENTS];
	long i;

	soak_bad = 0;
	soak_nores = 0;
	for (i = 0; i < SOAK_CLIENTS; i++)
		REQUIRE(pthread_create(&t[i], NULL, soak_client, (void *)i) == 0,
			"pthread_create failed");
	for (i = 0; i < SOAK_CLIENTS; i++)
		pthread_join(t[i], NULL);
	CHECK(soak_nores == 0,
	      "%d of %d concurrent requests got no response at all. A request "
	      "that is accepted and then never answered is a task lost "
	      "between the accept loop and the workers -- a descriptor put "
	      "into the queue on a path where nothing takes it out, or taken "
	      "out and dropped.",
	      soak_nores, SOAK_CLIENTS * SOAK_EACH);
	CHECK(soak_bad == 0,
	      "%d of %d concurrent requests came back wrong (one status was "
	      "%d). Under load the bytes still have to be the file's bytes: "
	      "shared state between workers, or a buffer that is not "
	      "per-connection, will do this.",
	      soak_bad, SOAK_CLIENTS * SOAK_EACH, soak_bad_status);
}

/*
 * A hundred and fifty connections, one after another. run.sh runs the server under
 * a descriptor limit of 64 for this, so a server that leaks one descriptor
 * per connection runs out about a fifth of the way in and everything after
 * that fails. A server that closes what it accepts does not notice.
 */
#define FDSOAK_CONNS 150

static void c_fdsoak(void)
{
	int i, bad = 0, nores = 0, first_bad = -1;

	for (i = 0; i < FDSOAK_CONNS; i++) {
		if (request(&resp, "/hello.txt") < 0) {
			if (!nores)
				first_bad = i;
			nores++;
			/* Once it has started failing it does not recover,
			 * and 150 timeouts is a long time to spend proving
			 * it. Ten is the answer. */
			if (nores >= 10)
				break;
			continue;
		}
		if (resp.status != 200) {
			if (!bad)
				first_bad = i;
			bad++;
		}
	}
	CHECK(nores == 0 && bad == 0,
	      "of %d connections made one after another, %d got no response "
	      "and %d got a non-200; the first was number %d. The server is "
	      "running under a descriptor limit of 64 for this case, so this "
	      "is what a descriptor leaked per connection looks like: it "
	      "works perfectly until it suddenly does not. Every accepted "
	      "descriptor is closed exactly once, on every path -- including "
	      "the error paths, and including the path where the client hung "
	      "up first.",
	      FDSOAK_CONNS, nores, bad, first_bad);
}

/* ==================================================================
 * stalled clients
 * ================================================================== */

/*
 * How long a stalled client stalls for. It has to release itself on a timer
 * rather than being closed by the main thread after the measurement: on the
 * thread-pool server the fresh request is queued BEHIND the stalled ones, so
 * a harness that waited for the fetch before letting go would be waiting for
 * something that is waiting for it.
 */
#define STALL_MS 1000

static int stall_fds[32];
static int stall_n;
static pthread_t stall_th;

/*
 * A client that connects, sends a request with its final blank line missing,
 * and then says nothing. To the server this is a connection that is not
 * finished arriving -- which is what a client on a bad network looks like,
 * and what an attacker looks like.
 */
static int stall_one(void)
{
	static const char *half = "GET /hello.txt HTTP/1.0\r\nHost: x\r\n";
	int fd = dial();

	if (fd < 0)
		return -1;
	if (send_all(fd, half, strlen(half)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void *stall_release(void *a)
{
	int i;

	(void)a;
	nap_ms(STALL_MS);
	for (i = 0; i < stall_n; i++)
		close(stall_fds[i]);
	return NULL;
}

static int start_stalls(int n)
{
	int i;

	stall_n = 0;
	for (i = 0; i < n; i++) {
		int fd = stall_one();

		if (fd < 0)
			return -1;
		stall_fds[stall_n++] = fd;
	}
	nap_ms(200);            /* let the server take them all */
	if (pthread_create(&stall_th, NULL, stall_release, NULL) != 0)
		return -1;
	return 0;
}

static void end_stalls(void)
{
	pthread_join(stall_th, NULL);
	stall_n = 0;
}

/* Fetch /hello.txt and return how long it took, or -1. */
static double timed_fetch(int *ok)
{
	double t0 = now_ms(), t1;
	size_t flen;
	char *want;

	*ok = 0;
	if (request(&resp, "/hello.txt") < 0)
		return now_ms() - t0;
	t1 = now_ms();
	want = read_fixture("hello.txt", &flen);
	if (want && resp.status == 200 &&
	    first_difference(resp.body, resp.bodylen, want, flen) < 0)
		*ok = 1;
	free(want);
	return t1 - t0;
}

/*
 * Two stalled clients, which is fewer than the pool has workers. BOTH
 * servers must serve somebody else while they stall. A server that reads one
 * connection to completion before accepting the next fails this, and it is
 * the minimum meaning of the word "concurrent".
 */
static void c_stalled(void)
{
	double ms;
	int ok;

	REQUIRE(start_stalls(2) == 0,
		"the harness could not open its stalled connections (errno %d "
		"on connect after 8 tries). That is the machine running out of "
		"sockets -- usually thousands in TIME_WAIT from running this "
		"suite repeatedly -- and not a verdict about your server. Wait "
		"a minute and run it again.", last_dial_errno);
	ms = timed_fetch(&ok);
	end_stalls();
	CHECK(ok, "a normal request made while two clients were stalled "
	      "mid-request did not come back correctly");
	CHECK(ms < 500,
	      "a normal request took %.0f ms while two clients were stalled "
	      "mid-request. Two connections that are not finished arriving "
	      "are holding up a third: the server is serving one connection "
	      "at a time. A thread pool has %d workers and only two of them "
	      "are stuck; an event loop is not stuck at all.",
	      ms, POOL_THREADS);
}

/*
 * The pair that separates the two architectures. POOL_THREADS + 4 stalled
 * clients: more than the thread pool has workers, and a nothing to an event
 * loop.
 */
#define HOL_STALLS (POOL_THREADS + 4)
#define HOL_THRESHOLD_MS 400

static void c_hol_blocks(void)
{
	double ms;
	int ok;

	REQUIRE(start_stalls(HOL_STALLS) == 0,
		"the harness could not open its stalled connections (errno %d "
		"on connect after 8 tries) -- the machine is out of sockets, "
		"which is not a verdict about your server.", last_dial_errno);
	ms = timed_fetch(&ok);
	end_stalls();
	fprintf(stderr, "    (measured: %.0f ms with %d clients stalled)\n",
		ms, HOL_STALLS);
	CHECK(ms >= HOL_THRESHOLD_MS,
	      "with %d clients stalled mid-request -- more than the %d "
	      "workers this server is contracted to have -- a fresh request "
	      "was answered in %.0f ms. It should have had to WAIT: every "
	      "worker is blocked reading from a client that has stopped "
	      "talking, and there is nobody left to serve anyone with.\n"
	      "    This case is not asking you to make the thread pool "
	      "slower. It is asking whether this binary really is a bounded "
	      "pool of %d threads. If webserver-threaded is a copy of the "
	      "event loop, or a thread per connection, this is the case that "
	      "says so -- and Part 5's whole point is the comparison between "
	      "two architectures that behave differently.",
	      HOL_STALLS, POOL_THREADS, ms, POOL_THREADS);
	CHECK(ok, "the fresh request eventually came back, but not "
	      "correctly");
}

static void c_hol_free(void)
{
	double ms;
	int ok;

	REQUIRE(start_stalls(HOL_STALLS) == 0,
		"the harness could not open its stalled connections (errno %d "
		"on connect after 8 tries) -- the machine is out of sockets, "
		"which is not a verdict about your server.", last_dial_errno);
	ms = timed_fetch(&ok);
	end_stalls();
	fprintf(stderr, "    (measured: %.0f ms with %d clients stalled)\n",
		ms, HOL_STALLS);
	CHECK(ms < HOL_THRESHOLD_MS,
	      "with %d clients stalled mid-request, a fresh request took "
	      "%.0f ms. An event loop has no per-connection thread to be "
	      "blocked in: a connection that is not finished arriving is a "
	      "slot in a table and a bit set in the read mask, and it costs "
	      "the others nothing.\n"
	      "    The usual cause is a read loop that keeps reading until "
	      "the request is complete. That is the threaded server's code, "
	      "and in a single-threaded loop it stops everybody. When "
	      "http_parse_request says HTTP_INCOMPLETE, return to select().",
	      HOL_STALLS, ms);
	CHECK(ok, "the request came back, but not correctly");
}

/* ================================================================== */

struct testcase {
	const char *name;
	void (*fn)(void);
};

static const struct testcase cases[] = {
	{ "p5_fetch",            c_fetch },
	{ "p5_drip",             c_drip },
	{ "p5_status",           c_status },
	{ "p5_traversal",        c_traversal },
	{ "p5_soak",             c_soak },
	{ "p5_fdsoak",           c_fdsoak },
	{ "p5_stalled",          c_stalled },
	{ "p5_hol_blocks",       c_hol_blocks },
	{ "p5_hol_free",         c_hol_free },
};

int main(int argc, char **argv)
{
	size_t i;

	if (argc != 4) {
		fprintf(stderr, "usage: cases <case-name> <port> <docroot>\n");
		return 2;
	}
	port = atoi(argv[2]);
	docroot = argv[3];
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		if (strcmp(argv[1], cases[i].name) == 0) {
			cases[i].fn();
			fflush(stdout);
			return failures ? 1 : 0;
		}
	}
	fprintf(stderr, "cases: no such case '%s'\n", argv[1]);
	return 2;
}
