/* myls.c -- a minimal `ls -l`, raw syscalls only.
 *
 * Usage: myls [PATH...]   (with no PATH, list ".")
 *
 * Output format (defined precisely; see the lab handout):
 *   For each PATH argument, in the order given:
 *     - if PATH is a directory: read its entries, drop "." and "..",
 *       sort the rest ascending by raw byte value (strcmp), and print one
 *       line per entry (NAME is the entry name);
 *     - otherwise: print one line for PATH itself (NAME is PATH verbatim).
 *   Each line is:  MODE SP SIZE SP NAME LF
 *     MODE : 10 chars = type char + 9 permission chars, like `ls -l`
 *            (type: d/-/l/c/b/p/s/?, perms: rwx triples, '-' where unset;
 *             setuid/setgid/sticky are NOT rendered, plain rwx only).
 *     SP   : one ASCII space (0x20).
 *     SIZE : st_size in bytes, decimal, no padding.
 *     NAME : entry name (directory listing) or PATH verbatim.
 *     LF   : one newline (0x0A).
 *   Entries are stat'd with lstat/fstatat(AT_SYMLINK_NOFOLLOW) so symlinks
 *   are not followed.
 *
 * Directory reading rule: readdir(3) is libc buffered I/O and is NOT used.
 * glibc ships no getdents64 wrapper, so we call it through syscall(2)
 * directly and parse struct linux_dirent64 ourselves.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

static const char *progname = "myls";

/* The kernel's dirent layout for getdents64 (not exposed by glibc). */
struct linux_dirent64 {
	unsigned long long d_ino;
	long long          d_off;
	unsigned short     d_reclen;
	unsigned char      d_type;
	char               d_name[]; /* NUL-terminated */
};

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

/* Format an unsigned value as decimal into buf (which must be big enough);
 * returns the number of characters written (no NUL). */
static size_t
utoa(unsigned long long v, char *buf)
{
	char tmp[24];
	size_t n = 0;
	do {
		tmp[n++] = (char)('0' + (v % 10));
		v /= 10;
	} while (v);
	for (size_t i = 0; i < n; i++)
		buf[i] = tmp[n - 1 - i];
	return n;
}

/* Fill perms[0..9] (no NUL) with the mode string for st_mode. */
static void
mode_string(mode_t m, char *perms)
{
	char t;
	if (S_ISDIR(m))
		t = 'd';
	else if (S_ISLNK(m))
		t = 'l';
	else if (S_ISCHR(m))
		t = 'c';
	else if (S_ISBLK(m))
		t = 'b';
	else if (S_ISFIFO(m))
		t = 'p';
	else if (S_ISSOCK(m))
		t = 's';
	else if (S_ISREG(m))
		t = '-';
	else
		t = '?';
	perms[0] = t;
	static const char *rwx = "rwx";
	for (int i = 0; i < 9; i++)
		perms[1 + i] = (m & (0400 >> i)) ? rwx[i % 3] : '-';
}

/* Print one line "MODE SIZE NAME\n" from an already-stat'd struct stat. */
static void
print_entry(const struct stat *st, const char *name)
{
	char line[64];
	size_t p = 0;
	mode_string(st->st_mode, line + p);
	p += 10;
	line[p++] = ' ';
	p += utoa((unsigned long long)st->st_size, line + p);
	line[p++] = ' ';
	write_all(1, line, p);
	write_all(1, name, strlen(name));
	write_all(1, "\n", 1);
}

static int
name_cmp(const void *a, const void *b)
{
	const char *const *pa = a;
	const char *const *pb = b;
	return strcmp(*pa, *pb);
}

/* List one directory (already known to be a dir). Returns 0 / -1. */
static int
list_dir(const char *path)
{
	int fd = open(path, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		warn_errno(path, errno);
		return -1;
	}

	/* collect entry names */
	char **names = NULL;
	size_t nnames = 0, ncap = 0;
	int rc = 0;
	char buf[65536];

	for (;;) {
		long nread = syscall(SYS_getdents64, fd, buf, sizeof buf);
		if (nread < 0) {
			warn_errno(path, errno);
			rc = -1;
			goto out;
		}
		if (nread == 0)
			break;
		for (long off = 0; off < nread;) {
			struct linux_dirent64 *d =
			    (struct linux_dirent64 *)(buf + off);
			const char *nm = d->d_name;
			if (strcmp(nm, ".") != 0 && strcmp(nm, "..") != 0) {
				if (nnames == ncap) {
					ncap = ncap ? ncap * 2 : 32;
					char **nn = realloc(names,
					    ncap * sizeof *names);
					if (!nn) {
						warn_errno(path, ENOMEM);
						rc = -1;
						goto out;
					}
					names = nn;
				}
				names[nnames] = strdup(nm);
				if (!names[nnames]) {
					warn_errno(path, ENOMEM);
					rc = -1;
					goto out;
				}
				nnames++;
			}
			off += d->d_reclen;
		}
	}

	qsort(names, nnames, sizeof *names, name_cmp);

	for (size_t i = 0; i < nnames; i++) {
		struct stat st;
		if (fstatat(fd, names[i], &st, AT_SYMLINK_NOFOLLOW) < 0) {
			warn_errno(names[i], errno);
			rc = -1;
			continue;
		}
		print_entry(&st, names[i]);
	}

out:
	for (size_t i = 0; i < nnames; i++)
		free(names[i]);
	free(names);
	close(fd);
	return rc;
}

/* Handle one command-line PATH. Returns 0 / -1. */
static int
do_path(const char *path)
{
	struct stat st;
	if (lstat(path, &st) < 0) {
		warn_errno(path, errno);
		return -1;
	}
	if (S_ISDIR(st.st_mode))
		return list_dir(path);
	print_entry(&st, path);
	return 0;
}

int
main(int argc, char **argv)
{
	int status = 0;
	if (argc < 2) {
		if (do_path(".") < 0)
			status = 1;
	} else {
		for (int i = 1; i < argc; i++)
			if (do_path(argv[i]) < 0)
				status = 1;
	}
	return status;
}
