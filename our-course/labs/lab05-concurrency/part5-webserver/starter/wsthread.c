/*
 * wsthread.c -- Lab 5 Part 5, the thread-pool web server.
 *
 * Build: webserver-threaded, run as
 *
 *      ./webserver-threaded <docroot> <port>       (port 0 = any free one)
 *
 * The shape, which is not negotiable because the harness tests a consequence
 * of it:
 *
 *   - one thread accepts, and puts each accepted descriptor into a BOUNDED
 *     queue -- pcbuffer.c, supplied, the one you wrote in Part 3;
 *   - WS_POOL_THREADS worker threads take descriptors out of the queue and
 *     serve one connection each, start to finish;
 *   - a descriptor is closed exactly once, on every path out of serving it.
 *
 * The workers are a FIXED pool, not one thread per connection. When all four
 * are busy with slow clients, nobody else is served until one comes free.
 * That is not a bug to be fixed here -- it is the property Part 5 is asking
 * you to observe, and the event server is the other half of the comparison.
 *
 * What is already done for you: parsing, path resolution, MIME types,
 * headers, the listening socket (http.c) and the queue (pcbuffer.c). What is
 * left is the concurrency.
 */

#define _GNU_SOURCE

#include "http.h"
#include "pcbuffer.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char *docroot;
static pcbuffer queue;

/*
 * TODO: write() until it is all gone. A short write is not an error, it is
 * the socket buffer being full, and the response is to write the rest.
 *
 * Be honest with yourself about this one: no case in the suite can catch you
 * skipping the loop here. This socket is BLOCKING, and a blocking write on
 * Linux waits until the last byte is in the kernel rather than coming back
 * short -- measured, with the 8 MB fixture and a client reading 4 KB at a
 * time. Write the loop regardless. POSIX permits a short blocking write, a
 * signal arriving mid-write produces one, and the identical function in
 * wsevent.c, on a non-blocking socket, returns short constantly. Handle
 * EINTR while you are here.
 */
static int write_all(int fd, const char *buf, size_t len)
{
	(void)fd;
	(void)buf;
	(void)len;
	return -1;
}

/*
 * TODO: one connection, start to finish.
 *
 *   1. read() until http_parse_request stops returning HTTP_INCOMPLETE.
 *      Requests arrive in pieces; one read() is not a request.
 *   2. HTTP_BAD -> 400, HTTP_NOTGET -> 501.
 *   3. http_resolve; a negative return is a 403 (that is the ".." case).
 *   4. http_open_file; failure is a 404.
 *   5. http_header, then the file's bytes. Bytes, not strings: one of the
 *      fixtures has NUL bytes in the middle of it.
 *
 * Do not close(fd) in here -- the caller does it, once, so that there is one
 * place to get it right.
 */
static void serve(int fd)
{
	(void)fd;
}

static void *worker(void *arg)
{
	(void)arg;
	/* TODO: take a descriptor off the queue, serve it, close it, repeat.
	 * For ever: a worker never exits. */
	return NULL;
}

int main(int argc, char **argv)
{
	int listenfd, port, bound;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <docroot> <port>\n", argv[0]);
		return 2;
	}
	docroot = argv[1];
	port = atoi(argv[2]);
	signal(SIGPIPE, SIG_IGN);        /* a client that hangs up mid-write
					  * must not kill the server */

	/* Only so that the skeleton compiles under -Werror while these are
	 * still stubs. Delete this line once they are called for real. */
	(void)write_all; (void)serve; (void)worker; (void)queue;

	listenfd = http_listen(port, &bound);
	if (listenfd < 0)
		return 1;

	/* TODO: pcb_init the queue at WS_QUEUE_CAP, and start
	 * WS_POOL_THREADS workers, BEFORE announcing the port. */

	http_announce(bound);

	for (;;) {
		int fd = accept(listenfd, NULL, NULL);

		if (fd < 0) {
			if (errno == EINTR)
				continue;
			perror("accept");
			continue;
		}
		/* TODO: hand it to the pool. Note what happens when the queue
		 * is full, and why that is the right thing rather than a
		 * problem to solve. */
		close(fd);
	}
}
