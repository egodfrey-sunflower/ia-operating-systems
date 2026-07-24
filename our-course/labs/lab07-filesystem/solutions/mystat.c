/* mystat.c -- print an inode's metadata, decoded.
 *
 * Usage: mystat PATH...
 *
 * For each PATH, print five labelled lines built from the file's inode:
 *
 *     name: <path>
 *     type: <human-readable type>
 *     size: <bytes>
 *     links: <hard-link count>
 *     mode: <octal> <symbolic>
 *
 * The output is byte-for-byte identical to
 *
 *     stat --printf='name: %n\ntype: %F\nsize: %s\nlinks: %h\nmode: %a %A\n' PATH...
 *
 * which is what the autograder diffs against. (It must be `--printf`: the
 * `-c` form prints the \n's literally and adds its own trailing newline.)
 *
 * The metadata is read with lstat(2), so a symbolic link reports on the link
 * itself -- its own type, size and permissions -- exactly as `stat` does by
 * default. That is why a dangling link still stats: the link is a real inode
 * whether or not its target exists.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* `stat`'s %F: the human-readable type. A zero-length regular file is called
 * a "regular empty file", which is its own line in stat's vocabulary. */
static const char *
type_word(mode_t m, off_t size)
{
	if (S_ISREG(m))
		return size == 0 ? "regular empty file" : "regular file";
	if (S_ISDIR(m))
		return "directory";
	if (S_ISLNK(m))
		return "symbolic link";
	if (S_ISCHR(m))
		return "character special file";
	if (S_ISBLK(m))
		return "block special file";
	if (S_ISFIFO(m))
		return "fifo";
	if (S_ISSOCK(m))
		return "socket";
	return "unknown";
}

/* `stat`'s %A / `ls -l`'s first column: type char plus nine permission
 * characters, with the set-user-ID, set-group-ID and sticky bits folded into
 * the execute slots (s/S, s/S, t/T). */
static void
mode_string(mode_t m, char out[11])
{
	char t = '?';
	if (S_ISREG(m))       t = '-';
	else if (S_ISDIR(m))  t = 'd';
	else if (S_ISLNK(m))  t = 'l';
	else if (S_ISCHR(m))  t = 'c';
	else if (S_ISBLK(m))  t = 'b';
	else if (S_ISFIFO(m)) t = 'p';
	else if (S_ISSOCK(m)) t = 's';

	out[0] = t;
	out[1] = (m & S_IRUSR) ? 'r' : '-';
	out[2] = (m & S_IWUSR) ? 'w' : '-';
	out[3] = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S')
	                       : ((m & S_IXUSR) ? 'x' : '-');
	out[4] = (m & S_IRGRP) ? 'r' : '-';
	out[5] = (m & S_IWGRP) ? 'w' : '-';
	out[6] = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S')
	                       : ((m & S_IXGRP) ? 'x' : '-');
	out[7] = (m & S_IROTH) ? 'r' : '-';
	out[8] = (m & S_IWOTH) ? 'w' : '-';
	out[9] = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T')
	                       : ((m & S_IXOTH) ? 'x' : '-');
	out[10] = '\0';
}

static int
report(const char *path)
{
	struct stat st;
	char mode[11];

	if (lstat(path, &st) != 0) {
		fprintf(stderr, "mystat: cannot stat '%s': %s\n",
		        path, strerror(errno));
		return -1;
	}
	mode_string(st.st_mode, mode);
	printf("name: %s\n", path);
	printf("type: %s\n", type_word(st.st_mode, st.st_size));
	printf("size: %lld\n", (long long)st.st_size);
	printf("links: %llu\n", (unsigned long long)st.st_nlink);
	printf("mode: %o %s\n", (unsigned)(st.st_mode & 07777), mode);
	return 0;
}

int
main(int argc, char **argv)
{
	int rc = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: mystat PATH...\n");
		return 2;
	}
	for (int i = 1; i < argc; i++)
		if (report(argv[i]) != 0)
			rc = 1;
	return rc;
}
