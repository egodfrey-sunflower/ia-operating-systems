/* myfind.c -- recursively walk a directory tree, printing paths whose final
 * component matches a name exactly.
 *
 * Usage: myfind START NAME
 *
 * For START and every file beneath it, print the path if the entry's own name
 * (its last path component) equals NAME. The target is
 *
 *     find START -name NAME
 *
 * for a literal NAME. The autograder sorts both outputs before diffing, so the
 * order you print paths in is your own.
 *
 *   - Recurse into every subdirectory, to any depth.
 *   - Match the whole final component, not a substring: searching for `foo`
 *     must not report `foobar` -- nor `FOO`; the comparison is byte-exact,
 *     case included. A directory whose name matches is a match too.
 *   - Walk with lstat(2) and do not follow symbolic links -- match a symlink by
 *     its own name, never descend through it. (`find` without -L does the same,
 *     which is what stops a link pointing back up its tree from looping.)
 *   - Skip "." and ".." so the walk terminates.
 *   - A START that does not exist goes to stderr and exits 1.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int
main(int argc, char **argv)
{
	/* TODO: implement myfind (see the block comment above). */
	(void)argc;
	(void)argv;
	return 0;
}
