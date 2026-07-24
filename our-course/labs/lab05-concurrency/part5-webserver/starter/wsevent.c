/*
 * wsevent.c -- Lab 5 Part 5, the event-driven web server.
 *
 * Build: webserver-event, run as
 *
 *      ./webserver-event <docroot> <port>          (port 0 = any free one)
 *
 * ONE THREAD. One select() loop. No pthread_create anywhere in this file --
 * the harness reads Threads: from /proc/<pid>/status while your server is
 * serving, and this server must say 1.
 *
 * Everything the threaded server keeps on a worker's stack has to be written
 * down here instead, because this loop may never stop in the middle of a
 * connection: stopping would stop all the others too. Per connection you
 * need at least
 *
 *      the descriptor;
 *      which half of the conversation it is in -- reading or writing;
 *      how much of the request has arrived;
 *      the response, and how much of it has gone out.
 *
 * That is the struct below, and filling it in is the exercise. Ch. 33 calls
 * it stack ripping: the straight-line code of wsthread.c, torn at every
 * point where it would have blocked, with the state that used to live
 * between two statements now living in a table.
 *
 * TWO THINGS THAT WILL BITE.
 *
 *   - PARTIAL READS. A request may arrive one byte at a time. When
 *     http_parse_request says HTTP_INCOMPLETE, return to the loop. Do not
 *     loop on read() until the request is complete: that is exactly the
 *     blocking you came here to avoid, and the harness has a case with a
 *     client that sends half a request and then says nothing for a second.
 *
 *   - PARTIAL WRITES. write() on a non-blocking socket takes what fits and
 *     tells you how much that was. The 8 MB fixture does not fit -- and the
 *     256 KB one usually does, on this kind of machine, which is why the
 *     harness has a case that fetches the big one through a client with a
 *     4 KB receive buffer that does not read for 300 ms. Track how much has
 *     gone; wait for select() to say the socket is writable again; send the
 *     rest.
 */

#define _GNU_SOURCE

#include "http.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

enum state {
	ST_FREE = 0,
	ST_READING,     /* waiting for the rest of the request */
	ST_WRITING      /* waiting for room to send the rest of the response */
};

struct conn {
	enum state state;
	int fd;
	char req[WS_REQ_MAX];
	size_t reqlen;
	char *out;              /* header + body */
	size_t outlen;
	size_t sent;
};

static struct conn conns[WS_MAXCONN];
static const char *docroot;

/* TODO: close the descriptor exactly once, free whatever was allocated for
 * the response, and put the slot back. */
static void conn_close(struct conn *c)
{
	(void)c;
}

/* TODO: find a free slot for a newly accepted descriptor, or return NULL if
 * there is none -- in which case the caller should close it rather than
 * accept a connection it will never answer. */
static struct conn *conn_new(int fd)
{
	(void)fd;
	return NULL;
}

/*
 * TODO: the connection is readable.
 *
 * Read once into c->req; handle 0 (the client hung up) and EAGAIN. Then
 * http_parse_request: HTTP_INCOMPLETE means come back later, and anything
 * else means build the response into c->out and move to ST_WRITING.
 */
static void on_readable(struct conn *c)
{
	(void)c;
}

/*
 * TODO: the connection is writable. Send what fits, add the number of bytes
 * write() actually took to c->sent, and close only when c->sent reaches
 * c->outlen.
 */
static void on_writable(struct conn *c)
{
	(void)c;
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
	signal(SIGPIPE, SIG_IGN);

	/* Only so that the skeleton compiles under -Werror while these are
	 * still stubs. Delete this line once they are called for real. */
	(void)conn_close; (void)conn_new; (void)on_readable; (void)on_writable;
	(void)conns;

	listenfd = http_listen(port, &bound);
	if (listenfd < 0)
		return 1;
	http_set_nonblocking(listenfd);
	http_announce(bound);

	for (;;) {
		fd_set rd, wr;
		int maxfd = listenfd, n;

		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_SET(listenfd, &rd);

		/* TODO: add each live connection to the read set or the write
		 * set according to its state, and keep maxfd up to date. */

		n = select(maxfd + 1, &rd, &wr, NULL, NULL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("select");
			return 1;
		}

		if (FD_ISSET(listenfd, &rd)) {
			/* TODO: accept everything that is waiting -- the
			 * listening socket is non-blocking, so accept() until
			 * it fails -- set each new descriptor non-blocking,
			 * and give it a slot. The close() below is a
			 * placeholder so that the skeleton does not spin on a
			 * connection it never takes; it goes when the slots
			 * arrive. */
			for (;;) {
				int fd = accept(listenfd, NULL, NULL);

				if (fd < 0)
					break;
				close(fd);
			}
		}

		/* TODO: for each live connection, if select() says it is
		 * ready, call on_readable or on_writable. */
	}
}
