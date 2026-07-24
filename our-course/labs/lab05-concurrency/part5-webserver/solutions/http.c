/*
 * http.c -- supplied. You should not need to change anything in here.
 *
 * Request parsing, path resolution, MIME types, header formatting and the
 * listening socket. None of it is concurrent and none of it is the point of
 * Part 5; it is here so that the two servers differ only in how they are
 * scheduled.
 *
 * Two things in this file are worth reading anyway, because the servers
 * depend on their exact behaviour:
 *
 *   http_parse_request returns HTTP_INCOMPLETE until "\r\n\r\n" has
 *   arrived. In the threaded server you can loop on read() until it stops
 *   saying that. In the event server you cannot -- you have to go back to
 *   select() and come back later, which is the whole of "stack ripping".
 *
 *   http_resolve rejects "..", always, before the filesystem is touched.
 *   Path traversal is not a concurrency bug but it is a real one, and a
 *   server that hands out /etc/passwd is not a server.
 */

#define _GNU_SOURCE   /* memmem */

#include "http.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int http_listen(int port, int *bound_port)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof addr;
	int fd, on = 1;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* 127.0.0.1 only */
	addr.sin_port = htons((unsigned short)port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		perror("bind");
		close(fd);
		return -1;
	}
	if (listen(fd, 64) < 0) {
		perror("listen");
		close(fd);
		return -1;
	}
	if (getsockname(fd, (struct sockaddr *)&addr, &len) < 0) {
		perror("getsockname");
		close(fd);
		return -1;
	}
	if (bound_port)
		*bound_port = ntohs(addr.sin_port);
	return fd;
}

void http_announce(int port)
{
	printf("listening on %d\n", port);
	fflush(stdout);
}

int http_parse_request(const char *buf, size_t len, char *path, size_t pathsz)
{
	const char *end, *p, *sp;
	size_t n;

	/* The request is complete only when the blank line has arrived. */
	end = memmem(buf, len, "\r\n\r\n", 4);
	if (!end)
		return HTTP_INCOMPLETE;

	if (len < 5 || memcmp(buf, "GET ", 4) != 0) {
		/* Distinguish "a method we do not do" from "not a request at
		 * all", because they are a 501 and a 400. */
		for (p = buf; p < buf + len && *p != ' ' && *p != '\r'; p++)
			;
		if (p < buf + len && *p == ' ' && p > buf)
			return HTTP_NOTGET;
		return HTTP_BAD;
	}
	p = buf + 4;
	sp = memchr(p, ' ', (size_t)(end - p));
	if (!sp || sp == p)
		return HTTP_BAD;
	n = (size_t)(sp - p);
	if (n >= pathsz)
		return HTTP_BAD;
	memcpy(path, p, n);
	path[n] = '\0';
	if (path[0] != '/')
		return HTTP_BAD;
	/* "GET / HTTP/1.0" -- anything that is not HTTP/1.x is still served;
	 * being liberal about the version costs nothing here. The bound check
	 * matters: sp points inside [buf, end), so without it memcmp could read
	 * up to five bytes past the request -- in bounds of the 8 KB buffer, but
	 * past what was written -- for a request like "GET /x \r\n\r\n". */
	if (sp + 6 > end || memcmp(sp + 1, "HTTP/", 5) != 0)
		return HTTP_BAD;
	return HTTP_OK;
}

int http_resolve(const char *docroot, const char *path, char *out, size_t outsz)
{
	int n;

	if (path[0] != '/')
		return -1;
	if (strstr(path, ".."))
		return -1;                      /* 403, before touching disk */
	if (strcmp(path, "/") == 0)
		path = "/index.html";
	n = snprintf(out, outsz, "%s%s", docroot, path);
	if (n < 0 || (size_t)n >= outsz)
		return -1;
	return 0;
}

const char *http_mime(const char *path)
{
	const char *dot = strrchr(path, '.');

	if (!dot)
		return "application/octet-stream";
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html";
	if (strcmp(dot, ".txt") == 0)
		return "text/plain";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	return "application/octet-stream";
}

static const char *reason(int status)
{
	switch (status) {
	case 200: return "OK";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 501: return "Not Implemented";
	default:  return "Unknown";
	}
}

int http_header(char *buf, size_t bufsz, int status, const char *mime,
		long body_len)
{
	int n = snprintf(buf, bufsz,
			 "HTTP/1.0 %d %s\r\n"
			 "Content-Length: %ld\r\n"
			 "Content-Type: %s\r\n"
			 "Connection: close\r\n"
			 "\r\n",
			 status, reason(status), body_len, mime);

	if (n < 0 || (size_t)n >= bufsz)
		return -1;
	return n;
}

const char *http_error_body(int status)
{
	switch (status) {
	case 400: return "400 bad request\n";
	case 403: return "403 forbidden\n";
	case 404: return "404 not found\n";
	case 501: return "501 not implemented\n";
	default:  return "error\n";
	}
}

int http_set_nonblocking(int fd)
{
	int fl = fcntl(fd, F_GETFL, 0);

	if (fl < 0)
		return -1;
	return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

int http_open_file(const char *fullpath, long *len)
{
	struct stat st;
	int fd;

	fd = open(fullpath, O_RDONLY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
		close(fd);
		return -1;
	}
	if (len)
		*len = (long)st.st_size;
	return fd;
}
