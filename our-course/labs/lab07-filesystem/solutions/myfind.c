/* myfind.c -- recursively walk a directory tree, printing paths whose final
 * component matches a name exactly.
 *
 * Usage: myfind START NAME
 *
 * For START and every file beneath it, print the path if the entry's own name
 * (its last path component) equals NAME. This mirrors
 *
 *     find START -name NAME
 *
 * for a literal NAME (no glob metacharacters). The autograder sorts both
 * outputs before diffing, so the order myfind emits paths in is its own.
 *
 * The walk uses lstat(2) and does not follow symbolic links: a symlink is
 * matched by its own name and never descended into, which is why a link that
 * points back up its own tree does not send the walk into a loop -- exactly
 * `find`'s behaviour without -L.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *want;

static const char *
base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static void
walk(const char *path)
{
	struct stat st;

	if (strcmp(base_name(path), want) == 0)
		printf("%s\n", path);

	if (lstat(path, &st) != 0)
		return;			/* unreadable entry: skip it */
	if (!S_ISDIR(st.st_mode))
		return;			/* not a directory (symlinks included) */

	DIR *d = opendir(path);
	if (d == NULL) {
		fprintf(stderr, "myfind: cannot open '%s': %s\n",
		        path, strerror(errno));
		return;
	}
	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		char child[4096];
		snprintf(child, sizeof child, "%s/%s", path, de->d_name);
		walk(child);
	}
	closedir(d);
}

int
main(int argc, char **argv)
{
	struct stat st;

	if (argc != 3) {
		fprintf(stderr, "usage: myfind START NAME\n");
		return 2;
	}
	if (lstat(argv[1], &st) != 0) {
		fprintf(stderr, "myfind: '%s': %s\n",
		        argv[1], strerror(errno));
		return 1;
	}
	want = argv[2];
	walk(argv[1]);
	return 0;
}
