/* mygrep.c -- fixed-string line search, raw syscalls only.
 *
 * Usage: mygrep PATTERN [FILE...]
 *   With no FILE, or when FILE is "-", read standard input.
 *   PATTERN is a literal (fixed) string, like `grep -F`.
 *
 * Prints every input line that contains PATTERN as a substring. When more
 * than one FILE is given, each printed line is prefixed with "FILE:" (again
 * matching `grep -F`). Output lines are always terminated with '\n', even if
 * the matching input line had no trailing newline.
 *
 * Exit status: 0 if at least one line matched, 1 if none matched,
 *              2 on any error (matching grep(1)).
 *
 * No stdio: I/O via read(2)/write(2). The whole input is slurped into a
 * grown malloc buffer so we can split on '\n' without straddling reads.
 */

#define _GNU_SOURCE /* for memmem(3) */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static const char *progname = "mygrep";

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
warn_errno(const char *prefix, int err)
{
	const char *m = strerror(err);
	write_all(2, progname, strlen(progname));
	write_all(2, ": ", 2);
	write_all(2, prefix, strlen(prefix));
	write_all(2, ": ", 2);
	write_all(2, m, strlen(m));
	write_all(2, "\n", 1);
}

/* Read all of fd into a malloc'd buffer. On success returns 0 and sets
 * out/outlen (caller frees the buffer). On error returns -1. */
static int
slurp(int fd, char **out, size_t *outlen)
{
	size_t cap = 65536, len = 0;
	char *buf = malloc(cap);
	if (!buf)
		return -1;
	for (;;) {
		if (len == cap) {
			size_t ncap = cap * 2;
			char *nb = realloc(buf, ncap);
			if (!nb) {
				free(buf);
				return -1;
			}
			buf = nb;
			cap = ncap;
		}
		ssize_t n = read(fd, buf + len, cap - len);
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
	*out = buf;
	*outlen = len;
	return 0;
}

/* Search one already-slurped buffer. Prints matching lines, prefixing with
 * "label:" when label != NULL. Returns 1 if any line matched, else 0. */
static int
grep_buf(const char *pat, size_t patlen, const char *buf, size_t len,
         const char *label)
{
	int matched = 0;
	size_t i = 0;
	while (i < len) {
		size_t j = i;
		while (j < len && buf[j] != '\n')
			j++;
		/* line is buf[i..j) (j is at '\n' or end) */
		size_t linelen = j - i;
		int hit;
		if (patlen == 0)
			hit = 1;
		else
			hit = memmem(buf + i, linelen, pat, patlen) != NULL;
		if (hit) {
			matched = 1;
			if (label) {
				write_all(1, label, strlen(label));
				write_all(1, ":", 1);
			}
			write_all(1, buf + i, linelen);
			write_all(1, "\n", 1);
		}
		if (j < len)
			j++; /* skip the '\n' */
		i = j;
	}
	return matched;
}

/* Grep a single named source ("-"/NULL means stdin). Returns 1 match,
 * 0 no match, -1 error. */
static int
grep_one(const char *pat, size_t patlen, const char *path, const char *label)
{
	int fd;
	int close_it = 0;
	if (path == NULL || strcmp(path, "-") == 0) {
		fd = 0;
	} else {
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			warn_errno(path, errno);
			return -1;
		}
		close_it = 1;
	}
	char *buf;
	size_t len;
	if (slurp(fd, &buf, &len) < 0) {
		warn_errno(path ? path : "stdin", errno);
		if (close_it)
			close(fd);
		return -1;
	}
	if (close_it)
		close(fd);
	int m = grep_buf(pat, patlen, buf, len, label);
	free(buf);
	return m;
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		const char *u = "usage: mygrep PATTERN [FILE...]\n";
		write_all(2, u, strlen(u));
		return 2;
	}

	const char *pat = argv[1];
	size_t patlen = strlen(pat);
	int nfiles = argc - 2;
	int any_match = 0;
	int any_error = 0;

	if (nfiles == 0) {
		int r = grep_one(pat, patlen, NULL, NULL);
		if (r < 0)
			any_error = 1;
		else if (r)
			any_match = 1;
	} else {
		for (int i = 2; i < argc; i++) {
			/* prefix filename only when more than one file */
			const char *label = (nfiles > 1) ? argv[i] : NULL;
			int r = grep_one(pat, patlen, argv[i], label);
			if (r < 0)
				any_error = 1;
			else if (r)
				any_match = 1;
		}
	}

	if (any_error)
		return 2;
	return any_match ? 0 : 1;
}
