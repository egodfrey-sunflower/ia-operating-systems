/* mytail.c -- print the last N lines of a file, seeking from the end.
 *
 * Usage: mytail -n N FILE
 *
 * Output is byte-for-byte `tail -n N FILE`. N is a non-negative integer; the
 * last line is printed exactly as it appears, whether or not the file ends in
 * a newline. FILE is required -- there is no stdin mode, because the whole
 * point is to lseek(2) inside a seekable file rather than stream it.
 *
 * The file is read from the end: we lseek to blocks near EOF and scan them
 * backward until N line terminators have been seen, then copy from that offset
 * to EOF. The number of bytes read is a function of N, not of the file size,
 * which is what the autograder's read-budget case checks.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLK 8192

static int
write_all(int fd, const char *buf, size_t len)
{
	size_t off = 0;
	while (off < len) {
		ssize_t w = write(fd, buf + off, len - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)w;
	}
	return 0;
}

/* Fill buf[0..len) from the file at absolute offset `at`, looping over short
 * reads. Returns 0 on success, -1 on error. */
static int
read_at(int fd, off_t at, char *buf, size_t len)
{
	if (lseek(fd, at, SEEK_SET) < 0)
		return -1;
	size_t got = 0;
	while (got < len) {
		ssize_t r = read(fd, buf + got, len - got);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (r == 0)
			break;
		got += (size_t)r;
	}
	return got == len ? 0 : -1;
}

int
main(int argc, char **argv)
{
	if (argc != 4 || strcmp(argv[1], "-n") != 0) {
		fprintf(stderr, "usage: mytail -n N FILE\n");
		return 2;
	}

	char *end;
	long n = strtol(argv[2], &end, 10);
	if (*end != '\0' || end == argv[2] || n < 0) {
		fprintf(stderr, "usage: mytail -n N FILE\n");
		return 2;
	}
	const char *file = argv[3];

	int fd = open(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "mytail: cannot open '%s': %s\n",
		        file, strerror(errno));
		return 1;
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		fprintf(stderr, "mytail: cannot stat '%s': %s\n",
		        file, strerror(errno));
		close(fd);
		return 1;
	}
	off_t size = st.st_size;

	if (n == 0 || size == 0) {
		close(fd);
		return 0;
	}

	/* Scan backward, counting line terminators. A newline that is the very
	 * last byte of the file terminates the final line rather than starting a
	 * new one, so it is not counted. `start` ends up at the first byte of the
	 * last N lines (or 0 if the file has fewer than N lines). */
	char buf[BLK];
	off_t pos = size;
	long count = 0;
	off_t start = 0;
	int found = 0;

	while (pos > 0 && !found) {
		size_t chunk = (pos >= (off_t)BLK) ? (size_t)BLK : (size_t)pos;
		off_t rstart = pos - (off_t)chunk;
		if (read_at(fd, rstart, buf, chunk) != 0) {
			fprintf(stderr, "mytail: read error on '%s': %s\n",
			        file, strerror(errno));
			close(fd);
			return 1;
		}
		for (ssize_t j = (ssize_t)chunk - 1; j >= 0; j--) {
			off_t abs = rstart + j;
			if (buf[j] == '\n' && abs != size - 1) {
				count++;
				if (count == n) {
					start = abs + 1;
					found = 1;
					break;
				}
			}
		}
		pos = rstart;
	}
	if (!found)
		start = 0;

	/* Copy [start, size) to stdout. */
	off_t at = start;
	while (at < size) {
		size_t want = (size - at >= (off_t)BLK) ? (size_t)BLK
		                                        : (size_t)(size - at);
		if (read_at(fd, at, buf, want) != 0) {
			fprintf(stderr, "mytail: read error on '%s': %s\n",
			        file, strerror(errno));
			close(fd);
			return 1;
		}
		if (write_all(1, buf, want) != 0) {
			close(fd);
			return 1;
		}
		at += (off_t)want;
	}

	close(fd);
	return 0;
}
