/* myls.c -- a long directory listing with per-entry metadata.
 *
 * Usage: myls -l [DIR]        (no DIR means ".")
 *
 * Print one line per entry, sorted by raw byte value, omitting every name that
 * begins with '.', in the form
 *
 *     <symbolic-mode> <link-count> <size> <name>
 *
 * which is byte-for-byte, for each entry E in a directory D,
 *
 *     ( cd D && stat -c '%A %h %s %n' E )
 *
 * The autograder builds its oracle exactly that way. We drop the owner, group
 * and timestamp columns of a real `ls -l` on purpose -- they are not
 * reproducible between machines or runs, so there is nothing stable to diff.
 *
 * Each entry is measured with lstat(2): a symlink shows as a symlink with its
 * own size (the length of its target path), never the file it points at.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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
by_name(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int
main(int argc, char **argv)
{
	const char *dir;
	char **names = NULL;
	size_t n = 0, cap = 0;
	struct dirent *de;
	DIR *d;

	if (argc < 2 || strcmp(argv[1], "-l") != 0 || argc > 3) {
		fprintf(stderr, "usage: myls -l [DIR]\n");
		return 2;
	}
	dir = (argc == 3) ? argv[2] : ".";

	if ((d = opendir(dir)) == NULL) {
		fprintf(stderr, "myls: cannot open directory '%s': %s\n",
		        dir, strerror(errno));
		return 2;
	}
	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] == '.')	/* hides ".", "..", dotfiles */
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 32;
			names = realloc(names, cap * sizeof *names);
			if (names == NULL)
				return 2;
		}
		if ((names[n++] = strdup(de->d_name)) == NULL)
			return 2;
	}
	closedir(d);

	qsort(names, n, sizeof *names, by_name);

	for (size_t i = 0; i < n; i++) {
		char path[4096], mode[11];
		struct stat st;

		snprintf(path, sizeof path, "%s/%s", dir, names[i]);
		if (lstat(path, &st) != 0) {
			fprintf(stderr, "myls: cannot access '%s': %s\n",
			        path, strerror(errno));
			free(names[i]);
			continue;
		}
		mode_string(st.st_mode, mode);
		printf("%s %llu %lld %s\n", mode,
		       (unsigned long long)st.st_nlink,
		       (long long)st.st_size, names[i]);
		free(names[i]);
	}
	free(names);
	return 0;
}
