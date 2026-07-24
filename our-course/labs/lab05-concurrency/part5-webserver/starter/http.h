/*
 * http.h -- supplied helpers and the contract for Lab 5 Part 5.
 *
 * Parsing HTTP is not the lesson, so http.c is given to you complete: the
 * request parser, the path resolver, the header formatter, the listening
 * socket. What is left is the concurrency, which is all of it.
 *
 * You write two servers against this header:
 *
 *   wsthread.c  ->  webserver-threaded : an accept loop, a bounded work
 *                   queue (Part 3's, supplied in pcbuffer.c), and a fixed
 *                   pool of WS_POOL_THREADS worker threads.
 *
 *   wsevent.c   ->  webserver-event    : ONE thread, one select() loop, and
 *                   a state machine per connection. No second thread, and
 *                   the harness checks that -- it reads Threads: from
 *                   /proc/<pid>/status while your server is serving.
 *
 * THE COMMAND LINE, WHICH IS PART OF THE CONTRACT.
 *
 *      ./webserver-threaded <docroot> <port>
 *      ./webserver-event    <docroot> <port>
 *
 * A port of 0 means "any free port", which is what the harness always
 * passes: a fixed port collides with whatever else is running on the machine
 * and turns somebody else's process into a mysterious test failure. Once the
 * socket is listening, and BEFORE serving anything, print
 *
 *      listening on <port>\n
 *
 * to stdout and flush it. That line is how the harness -- and you, from a
 * shell -- find out where the server went. http_announce() does it for you.
 *
 * THE PROTOCOL, WHICH IS ALSO PART OF THE CONTRACT.
 *
 *   - GET only. Any other method: 501.
 *   - A request is over when "\r\n\r\n" has arrived. It may arrive in
 *     pieces, and in the event server it will.
 *   - "/" means "/index.html".
 *   - A path containing ".." is 403, always, before touching the disk.
 *   - A path that does not resolve to a readable file is 404.
 *   - Anything unparseable is 400.
 *   - Every response carries Content-Length and Content-Type, and the
 *     server closes the connection afterwards. This is HTTP/1.0 with no
 *     keep-alive, which is one less thing to get right.
 *   - Bodies are bytes, not strings. The fixture set contains a file with
 *     NUL bytes in it, a 256 KB file that will not fit in one read(), and an
 *     8 MB one that will not fit in one write() on a NON-BLOCKING socket.
 *     (On a blocking socket Linux waits rather than returning short, so the
 *     8 MB fixture only exercises the event server's write path.)
 */
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* The pool size and queue depth are fixed by the contract because the
 * harness tests a consequence of them: with more stalled clients than
 * workers, a thread-pool server has nothing left to serve anyone else with.
 * That is head-of-line blocking, it is the honest behaviour of a bounded
 * pool, and Part 5 exists to make you see it happen. */
#define WS_POOL_THREADS 4
#define WS_QUEUE_CAP    16

#define WS_MAXPATH      512     /* longest request path, including the '/' */
#define WS_REQ_MAX      8192    /* longest request the servers accept */
#define WS_MAXCONN      64      /* connections the event loop must handle */

/* http_parse_request return values. */
#define HTTP_OK          0
#define HTTP_INCOMPLETE (-1)    /* no "\r\n\r\n" yet: read more */
#define HTTP_BAD        (-2)    /* 400 */
#define HTTP_NOTGET     (-3)    /* 501 */

/*
 * A socket bound to 127.0.0.1 on `port`, listening. Pass 0 for `port` to
 * get any free one; the port actually bound is written to *bound_port.
 * Returns the listening fd, or -1 with a message on stderr.
 */
int http_listen(int port, int *bound_port);

/* Print "listening on <port>" to stdout and flush it. */
void http_announce(int port);

/*
 * Look for a complete request in buf[0..len). Returns HTTP_OK and copies
 * the requested path into path[0..pathsz), or one of the three negative
 * codes above. Does not modify buf.
 */
int http_parse_request(const char *buf, size_t len, char *path, size_t pathsz);

/*
 * Turn a request path into a filesystem path under docroot. Returns 0 on
 * success, -1 if the path is forbidden (a "..", a missing leading '/', or
 * one long enough to overflow), which is a 403.
 */
int http_resolve(const char *docroot, const char *path, char *out, size_t outsz);

/* "text/html", "text/plain", "application/octet-stream", ... */
const char *http_mime(const char *path);

/*
 * Format a complete response header into buf and return its length, or -1
 * if it would not fit. Pass the body length, which must be known before the
 * header goes out -- that is what Content-Length means.
 */
int http_header(char *buf, size_t bufsz, int status, const char *mime,
		long body_len);

/* The one-line body served with a 400/403/404/501. Never NULL. */
const char *http_error_body(int status);

/* O_NONBLOCK on fd. Returns 0, or -1 on failure. */
int http_set_nonblocking(int fd);

/*
 * Open a file for reading and report its size. Returns the fd, or -1 if it
 * does not exist or is not a regular file -- which is a 404.
 */
int http_open_file(const char *fullpath, long *len);

#endif /* HTTP_H */
