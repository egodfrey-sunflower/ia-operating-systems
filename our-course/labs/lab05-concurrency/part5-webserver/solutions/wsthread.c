/*
 * wsthread.c -- reference thread-pool web server for Lab 5 Part 5.
 *
 * One accept loop, a bounded queue of accepted descriptors, and a fixed pool
 * of WS_POOL_THREADS workers. The queue is Part 3's bounded buffer, used
 * unchanged: it holds ints, and a file descriptor is an int, which is the
 * whole of the adaptation.
 *
 * THE THREE THINGS THIS FILE IS ABOUT.
 *
 * 1. The pool is FIXED and the queue is BOUNDED, and both of those are
 *    decisions rather than details. A thread per connection is one line
 *    shorter to write and falls over at a few thousand connections, because
 *    each one is a stack; a pool of four with a queue of sixteen has a
 *    hard, known memory cost and degrades by making people wait. The
 *    bounded queue also gives back-pressure for free: when it is full,
 *    accept() simply stops being called, and the kernel's listen backlog
 *    holds the line.
 *
 * 2. A worker blocked on a slow client is a worker that is not serving
 *    anybody. With four workers, four slow clients are the whole pool. This
 *    is head-of-line blocking, it is the honest behaviour of this
 *    architecture, and the harness has a case that requires it to happen --
 *    the same case requires the event server NOT to do it. That pair is the
 *    reason Part 5 asks for two servers rather than one.
 *
 * 3. Every accepted descriptor is closed exactly once, on every path. A
 *    server that leaks one per connection works perfectly for the first
 *    thousand requests of your testing and dies in the middle of somebody
 *    else's load test. The harness runs this server under a descriptor
 *    limit of 64 for exactly that reason.
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
#include <time.h>
#include <unistd.h>

static const char *docroot;
static pcbuffer queue;

/* write() until it is all gone. A short write is not an error: it is the
 * socket buffer being full, and the only correct answer is to write the
 * rest. The event server has to do this without blocking, which is where it
 * gets interesting. */
static int write_all(int fd, const char *buf, size_t len)
{
	while (len > 0) {
		ssize_t n = write(fd, buf, len);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return -1;
		buf += n;
		len -= (size_t)n;
	}
	return 0;
}

static void send_error(int fd, int status)
{
	char head[512];
	const char *body = http_error_body(status);
	int hn = http_header(head, sizeof head, status, "text/plain",
			     (long)strlen(body));

	if (hn < 0)
		return;
	if (write_all(fd, head, (size_t)hn) == 0)
		write_all(fd, body, strlen(body));
}

static void send_file(int fd, const char *path, const char *fullpath)
{
	char head[512], buf[16384];
	long len;
	int filefd, hn;
	ssize_t n;

	filefd = http_open_file(fullpath, &len);
	if (filefd < 0) {
		send_error(fd, 404);
		return;
	}
	hn = http_header(head, sizeof head, 200, http_mime(path), len);
	if (hn < 0 || write_all(fd, head, (size_t)hn) < 0) {
		close(filefd);
		return;
	}
	while ((n = read(filefd, buf, sizeof buf)) > 0) {
		if (write_all(fd, buf, (size_t)n) < 0)
			break;
	}
	close(filefd);
}

/*
 * One connection, start to finish, on this worker's thread. Blocking calls
 * throughout, which is the point: the code reads like a description of the
 * protocol because the thread's stack is holding the state. Compare
 * wsevent.c, which does the same job with the state in a struct.
 */
static void serve(int fd)
{
	char req[WS_REQ_MAX], path[WS_MAXPATH], full[WS_MAXPATH * 2];
	size_t used = 0;
	int rc;

	for (;;) {
		ssize_t n;

		rc = http_parse_request(req, used, path, sizeof path);
		if (rc != HTTP_INCOMPLETE)
			break;
		if (used == sizeof req) {
			send_error(fd, 400);
			return;
		}
		n = read(fd, req + used, sizeof req - used);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return;                 /* client vanished */
		}
		if (n == 0)
			return;                 /* closed before finishing */
		used += (size_t)n;
	}

	if (rc == HTTP_BAD) {
		send_error(fd, 400);
		return;
	}
	if (rc == HTTP_NOTGET) {
		send_error(fd, 501);
		return;
	}
	if (http_resolve(docroot, path, full, sizeof full) < 0) {
		send_error(fd, 403);
		return;
	}
	send_file(fd, path, full);
}

static void *worker(void *arg)
{
	(void)arg;
	for (;;) {
		int fd = pcb_get(&queue);

		if (fd < 0)
			return NULL;            /* not used: the pool is
						 * shut down by exit() */
		serve(fd);
		close(fd);                      /* once, on every path */
	}
}

int main(int argc, char **argv)
{
	pthread_t pool[WS_POOL_THREADS];
	int listenfd, port, bound, i;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <docroot> <port>\n", argv[0]);
		return 2;
	}
	docroot = argv[1];
	port = atoi(argv[2]);

	/* A dead client mid-response must not kill the server. */
	signal(SIGPIPE, SIG_IGN);

	listenfd = http_listen(port, &bound);
	if (listenfd < 0)
		return 1;

	pcb_init(&queue, WS_QUEUE_CAP);
	for (i = 0; i < WS_POOL_THREADS; i++) {
		if (pthread_create(&pool[i], NULL, worker, NULL) != 0) {
			perror("pthread_create");
			return 1;
		}
	}

	http_announce(bound);

	for (;;) {
		int fd = accept(listenfd, NULL, NULL);

		if (fd < 0) {
			if (errno == EINTR)
				continue;
			/* Out of descriptors, most likely (EMFILE under the
			 * harness's 64-descriptor limit). Do not spin on it: a
			 * bare `continue` would call accept() again immediately,
			 * fail again, and burn a core printing errors until a
			 * worker frees a descriptor. Pause a millisecond and let
			 * one drain. */
			perror("accept");
			nanosleep(&(struct timespec){ 0, 1000000L }, NULL);
			continue;
		}
		/* Blocks when the queue is full, which stops this thread
		 * calling accept() and leaves the arrivals in the kernel's
		 * backlog. That is the back-pressure. */
		pcb_put(&queue, fd);
	}
}
