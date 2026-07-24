/* mycat.c -- concatenate files to stdout using raw system calls.
 *
 * Usage: mycat [FILE...]     (no FILE, or "-", means standard input)
 *
 * No stdio anywhere: open/read/write/close only. strerror(3) is libc but
 * not stdio, so it is allowed; diagnostics are assembled by hand on fd 2.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* Write exactly len bytes. write(2) may write fewer than asked; loop. */
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

/* "mycat: <what>: <strerror(err)>\n" on fd 2. */
static void
warn(const char *what, int err)
{
	write_all(2, "mycat: ", 7);
	write_all(2, what, strlen(what));
	write_all(2, ": ", 2);
	write_all(2, strerror(err), strlen(strerror(err)));
	write_all(2, "\n", 1);
}

/* Copy everything readable on fd to stdout. 0 on success, -1 on error. */
static int
cat_fd(int fd, const char *name)
{
	char buf[65536];
	for (;;) {
		ssize_t n = read(fd, buf, sizeof buf);
		if (n < 0) {
			if (errno == EINTR)
				continue;	/* not an EOF; retry */
			warn(name, errno);
			return -1;
		}
		if (n == 0)
			return 0;	/* only 0 means end of input */
		if (write_all(1, buf, (size_t)n) < 0) {
			warn("write error", errno);
			return -1;
		}
	}
}

int
main(int argc, char **argv)
{
	int status = 0;

	if (argc < 2)
		return cat_fd(0, "-") < 0 ? 1 : 0;

	for (int i = 1; i < argc; i++) {
		int fd;
		if (strcmp(argv[i], "-") == 0) {
			if (cat_fd(0, "-") < 0)
				status = 1;
			continue;
		}
		if ((fd = open(argv[i], O_RDONLY)) < 0) {
			warn(argv[i], errno);
			status = 1;	/* report, but keep going */
			continue;
		}
		if (cat_fd(fd, argv[i]) < 0)
			status = 1;
		close(fd);
	}
	return status;
}
