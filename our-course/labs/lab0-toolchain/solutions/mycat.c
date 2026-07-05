/* mycat.c -- concatenate files to stdout, raw syscalls only.
 *
 * Usage: mycat [FILE...]
 *   With no FILE, or when FILE is "-", read standard input.
 *
 * No stdio: all I/O is done with open(2)/read(2)/write(2)/close(2).
 * strerror(3) and malloc(3) are libc but not stdio, so they are allowed;
 * diagnostics are written to fd 2 by hand.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static const char *progname = "mycat";

/* Write exactly len bytes, retrying on short writes and EINTR. */
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

/* Emit "progname: prefix: strerror(err)\n" to fd 2, no stdio. */
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

/* Copy all of fd to stdout. Returns 0 on success, -1 on error. */
static int
cat_fd(int fd, const char *name)
{
	char buf[65536];
	for (;;) {
		ssize_t n = read(fd, buf, sizeof buf);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			warn_errno(name, errno);
			return -1;
		}
		if (n == 0)
			return 0;
		if (write_all(1, buf, (size_t)n) < 0) {
			warn_errno("stdout", errno);
			return -1;
		}
	}
}

int
main(int argc, char **argv)
{
	int status = 0;

	if (argc < 2)
		return cat_fd(0, "stdin") < 0 ? 1 : 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-") == 0) {
			if (cat_fd(0, "stdin") < 0)
				status = 1;
			continue;
		}
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			warn_errno(argv[i], errno);
			status = 1;
			continue;
		}
		if (cat_fd(fd, argv[i]) < 0)
			status = 1;
		close(fd);
	}
	return status;
}
