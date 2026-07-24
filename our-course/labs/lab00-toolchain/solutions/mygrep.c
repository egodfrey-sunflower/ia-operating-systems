/* mygrep.c -- print lines containing a fixed substring. No regex.
 *
 * Usage: mygrep PATTERN [FILE...]   (no FILE means standard input)
 *
 * Behaves like `grep -F`: with two or more FILEs each hit is prefixed
 * "FILE:", and the exit status is 0 if anything matched, 1 if nothing did,
 * 2 if any file could not be read.
 *
 * No stdio. The whole input is slurped into a grown buffer and then split on
 * '\n', which is the cheap way to dodge the real trap: a line straddling two
 * read(2) returns.
 */

#define _GNU_SOURCE		/* memmem */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
write_all(int fd, const char *buf, size_t len)
{
	size_t off = 0;
	while (off < len) {
		ssize_t n = write(fd, buf + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)n;
	}
	return 0;
}

static void
warn(const char *what, int err)
{
	write_all(2, "mygrep: ", 8);
	write_all(2, what, strlen(what));
	write_all(2, ": ", 2);
	write_all(2, strerror(err), strlen(strerror(err)));
	write_all(2, "\n", 1);
}

/* Read all of fd into the caller's buf/len. Returns 0, or -1 (errno set). */
static int
slurp(int fd, char **bufp, size_t *lenp)
{
	char *buf = NULL;
	size_t len = 0, cap = 0;
	for (;;) {
		ssize_t n;
		if (len == cap) {
			cap = cap ? cap * 2 : 65536;
			if ((buf = realloc(buf, cap)) == NULL)
				return -1;
		}
		n = read(fd, buf + len, cap - len);	/* may be short! */
		if (n < 0) {
			if (errno == EINTR)
				continue;
			free(buf);
			return -1;
		}
		if (n == 0)
			break;
		len += (size_t)n;
	}
	*bufp = buf;
	*lenp = len;
	return 0;
}

/* Scan one slurped input. Returns 1 if anything matched, else 0. */
static int
scan(const char *buf, size_t len, const char *pat, const char *prefix)
{
	size_t plen = strlen(pat), pos = 0;
	int hit = 0;

	while (pos < len) {
		const char *nl = memchr(buf + pos, '\n', len - pos);
		size_t llen = nl ? (size_t)(nl - (buf + pos)) : len - pos;
		if (memmem(buf + pos, llen, pat, plen) != NULL) {
			hit = 1;
			if (prefix) {
				write_all(1, prefix, strlen(prefix));
				write_all(1, ":", 1);
			}
			write_all(1, buf + pos, llen);
			write_all(1, "\n", 1);	/* always terminated */
		}
		pos += llen + 1;
	}
	return hit;
}

int
main(int argc, char **argv)
{
	int matched = 0, failed = 0;

	if (argc < 2) {
		write_all(2, "usage: mygrep PATTERN [FILE...]\n", 32);
		return 2;
	}
	if (argc == 2) {		/* pattern only: read stdin */
		char *buf;
		size_t len;
		if (slurp(0, &buf, &len) < 0) {
			warn("-", errno);
			return 2;
		}
		matched = scan(buf, len, argv[1], NULL);
		free(buf);
		return matched ? 0 : 1;
	}
	for (int i = 2; i < argc; i++) {
		char *buf;
		size_t len;
		int dash = (argv[i][0] == '-' && argv[i][1] == '\0');
		int fd = dash ? 0 : open(argv[i], O_RDONLY);

		if (fd < 0 || slurp(fd, &buf, &len) < 0) {
			warn(argv[i], errno);
			failed = 1;
			if (fd > 0)
				close(fd);
			continue;
		}
		if (!dash)
			close(fd);
		if (scan(buf, len, argv[1], (argc > 3) ? argv[i] : NULL))
			matched = 1;
		free(buf);
	}
	return failed ? 2 : (matched ? 0 : 1);
}
