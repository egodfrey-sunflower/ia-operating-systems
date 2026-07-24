/*
 * wsevent.c -- reference event-driven web server for Lab 5 Part 5.
 *
 * One thread. One select() loop. A struct per connection holding the state
 * that wsthread.c keeps on a worker's stack.
 *
 * THE TRADE, STATED ONCE.
 *
 * The threaded server keeps a connection's state in the natural place: the
 * program counter and the stack of the thread serving it. `read the request;
 * open the file; write the response` is a paragraph of straight-line code
 * because a thread is allowed to stop in the middle of it.
 *
 * This server may never stop in the middle of anything, because stopping
 * would stop every other connection too. So every place the threaded server
 * blocks becomes a place this one returns to the loop, and everything it
 * knew has to be written down first: how much of the request has arrived,
 * how much of the response has gone out, which file, which offset. That is
 * "stack ripping" -- the control flow of the threaded version, torn into
 * fragments and reassembled around a table of state. Ch. 33's argument is
 * that you buy something real with it (no per-connection stack, no
 * synchronisation, no head-of-line blocking behind a slow client) and pay
 * for it in exactly this: the code no longer reads like the protocol.
 *
 * There is no locking anywhere in this file and there does not need to be,
 * because there is one thread. That is the second half of what the model
 * buys, and it is why helgrind has nothing to say about this server -- the
 * harness runs memcheck over it instead, which has plenty to say about
 * buffer arithmetic.
 *
 * THE PARTIAL WRITE IS THE PART THAT BITES. A big enough response does not
 * fit in a socket buffer. write() takes what fits and returns a smaller
 * number than you asked for; the rest has to wait for select() to say the
 * socket is writable again. A server that assumes write() writes everything
 * hangs up part way through.
 *
 * "Big enough" is worth a number, because it is bigger than it looks: a
 * 256 KB response goes into a socket send buffer whole on a machine like
 * this one, even when the receiver's buffer is 4 KB and the receiver is
 * asleep. The harness's case for this fetches an 8 MB fixture -- larger than
 * the kernel's maximum send buffer -- through a client that asks for a 4 KB
 * receive buffer and then waits 300 ms before reading.
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
	char *out;              /* header + body, malloc'd */
	size_t outlen;
	size_t sent;
};

static struct conn conns[WS_MAXCONN];
static const char *docroot;

static void conn_close(struct conn *c)
{
	if (c->state == ST_FREE)
		return;
	close(c->fd);           /* exactly once, on every path */
	free(c->out);
	c->out = NULL;
	c->state = ST_FREE;
}

static struct conn *conn_new(int fd)
{
	int i;

	for (i = 0; i < WS_MAXCONN; i++) {
		if (conns[i].state == ST_FREE) {
			conns[i].state = ST_READING;
			conns[i].fd = fd;
			conns[i].reqlen = 0;
			conns[i].out = NULL;
			conns[i].outlen = 0;
			conns[i].sent = 0;
			return &conns[i];
		}
	}
	return NULL;
}

/* Assemble the whole response in one buffer. Simple, and it means the write
 * side has exactly one thing to track: how much of `out` has gone. */
static int set_response(struct conn *c, int status, const char *mime,
			const char *body, long bodylen)
{
	char head[512];
	int hn = http_header(head, sizeof head, status, mime, bodylen);

	if (hn < 0)
		return -1;
	c->out = malloc((size_t)hn + (size_t)bodylen);
	if (!c->out)
		return -1;
	memcpy(c->out, head, (size_t)hn);
	memcpy(c->out + hn, body, (size_t)bodylen);
	c->outlen = (size_t)hn + (size_t)bodylen;
	c->sent = 0;
	c->state = ST_WRITING;
	return 0;
}

static int set_error(struct conn *c, int status)
{
	const char *body = http_error_body(status);

	return set_response(c, status, "text/plain", body,
			    (long)strlen(body));
}

static int set_file(struct conn *c, const char *path, const char *full)
{
	long len, got = 0;
	int fd = http_open_file(full, &len);
	char *body;

	if (fd < 0)
		return set_error(c, 404);
	body = malloc((size_t)len ? (size_t)len : 1);
	if (!body) {
		close(fd);
		return -1;
	}
	while (got < len) {
		ssize_t n = read(fd, body + got, (size_t)(len - got));

		if (n <= 0)
			break;
		got += n;
	}
	close(fd);
	if (got != len) {
		free(body);
		return set_error(c, 404);
	}
	if (set_response(c, 200, http_mime(path), body, len) < 0) {
		free(body);
		return -1;
	}
	free(body);
	return 0;
}

/* The request is complete: decide what the response is. */
static void respond(struct conn *c, int rc, const char *path)
{
	char full[WS_MAXPATH * 2];
	int r;

	if (rc == HTTP_BAD)
		r = set_error(c, 400);
	else if (rc == HTTP_NOTGET)
		r = set_error(c, 501);
	else if (http_resolve(docroot, path, full, sizeof full) < 0)
		r = set_error(c, 403);
	else
		r = set_file(c, path, full);
	if (r < 0)
		conn_close(c);
}

/* Readable. Take what is there and go back to the loop; do NOT loop until
 * the request is complete, because "until" is how one client stops all the
 * others. */
static void on_readable(struct conn *c)
{
	char path[WS_MAXPATH];
	ssize_t n;
	int rc;

	if (c->reqlen == sizeof c->req) {
		respond(c, HTTP_BAD, NULL);
		return;
	}
	n = read(c->fd, c->req + c->reqlen, sizeof c->req - c->reqlen);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		conn_close(c);
		return;
	}
	if (n == 0) {                   /* client hung up mid-request */
		conn_close(c);
		return;
	}
	c->reqlen += (size_t)n;
	rc = http_parse_request(c->req, c->reqlen, path, sizeof path);
	if (rc == HTTP_INCOMPLETE)
		return;                 /* come back when there is more */
	respond(c, rc, path);
}

/* Writable. Send what fits, remember how much that was, and come back. */
static void on_writable(struct conn *c)
{
	ssize_t n = write(c->fd, c->out + c->sent, c->outlen - c->sent);

	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		conn_close(c);
		return;
	}
	c->sent += (size_t)n;
	if (c->sent == c->outlen)
		conn_close(c);
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

	listenfd = http_listen(port, &bound);
	if (listenfd < 0)
		return 1;
	http_set_nonblocking(listenfd);
	http_announce(bound);

	for (;;) {
		fd_set rd, wr;
		int i, maxfd = listenfd, n;

		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_SET(listenfd, &rd);
		for (i = 0; i < WS_MAXCONN; i++) {
			struct conn *c = &conns[i];

			if (c->state == ST_READING)
				FD_SET(c->fd, &rd);
			else if (c->state == ST_WRITING)
				FD_SET(c->fd, &wr);
			else
				continue;
			if (c->fd > maxfd)
				maxfd = c->fd;
		}

		n = select(maxfd + 1, &rd, &wr, NULL, NULL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("select");
			return 1;
		}

		if (FD_ISSET(listenfd, &rd)) {
			for (;;) {
				int fd = accept(listenfd, NULL, NULL);

				if (fd < 0)
					break;  /* EAGAIN: no more waiting */
				http_set_nonblocking(fd);
				if (!conn_new(fd)) {
					/* Full. Refusing now is better than
					 * accepting and never answering. */
					close(fd);
				}
			}
		}

		for (i = 0; i < WS_MAXCONN; i++) {
			struct conn *c = &conns[i];

			if (c->state == ST_READING && FD_ISSET(c->fd, &rd))
				on_readable(c);
			else if (c->state == ST_WRITING && FD_ISSET(c->fd, &wr))
				on_writable(c);
		}
	}
}
