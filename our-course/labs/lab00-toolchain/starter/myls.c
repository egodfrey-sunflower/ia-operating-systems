/* myls.c -- list a directory's entries, one per line, sorted by name.
 *
 * Usage: myls [PATH]
 *
 * Target behaviour: byte-for-byte identical to `LC_ALL=C ls -1 PATH` on
 * stdout, stderr and exit status. See ../README.md, Part 3.
 *
 * RULES
 *   - opendir/readdir/closedir are the ONE permitted exception to the
 *     no-stdio rule. There is no portable lower-level way to read a
 *     directory, so you are allowed the <dirent.h> interface.
 *   - Everything you print still goes out through write(2). No printf.
 */

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	/* TODO: implement myls.
	 *
	 * 1. No argument means ".". More than one argument is a usage error
	 *    (exit 2) -- this myls takes at most one path.
	 * 2. stat(2) the path. If it fails, write
	 *       myls: cannot access '<path>': <strerror(errno)>\n
	 *    to fd 2 and exit 2. (That wording is ls(1)'s; the grader rewrites
	 *    the leading program name before diffing, nothing else.)
	 * 3. If it is not a directory, print the path back verbatim and exit 0 --
	 *    that is what `ls -1 somefile` does.
	 * 4. If it is a directory: collect the entry names with
	 *    opendir/readdir/closedir, SKIPPING every name that begins with '.'
	 *    (that covers "." and ".." and dotfiles, which `ls -1` also hides),
	 *    sort them, print one per line, exit 0.
	 *
	 * The sort must be by raw byte value -- qsort(3) with a strcmp(3)
	 * comparator. That is what LC_ALL=C ls does, and it is why the diff test
	 * is not flaky. readdir returns entries in whatever order the filesystem
	 * feels like; never rely on it.
	 *
	 * You will need to copy each d_name (strdup) before the next readdir
	 * call: the struct dirent that readdir returns may be reused.
	 */

	(void)argc;
	(void)argv;
	return 0;
}
