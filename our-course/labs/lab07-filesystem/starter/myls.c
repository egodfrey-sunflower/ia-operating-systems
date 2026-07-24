/* myls.c -- a long directory listing with per-entry metadata.
 *
 * Usage: myls -l [DIR]        (no DIR means ".")
 *
 * Print one line per entry, sorted by raw byte value, omitting every name that
 * begins with '.', in the form
 *
 *     <symbolic-mode> <link-count> <size> <name>
 *
 * The target is, for each entry E in directory D, byte-for-byte
 *
 *     ( cd D && stat -c '%A %h %s %n' E )
 *
 * i.e. the ten-character mode string, the hard-link count, the size in bytes,
 * and the entry's own name. Owner, group and timestamps are deliberately not
 * printed -- they are not reproducible, so there is nothing to diff.
 *
 *   - Read the directory with opendir/readdir/closedir; measure each entry
 *     with lstat(2). A symlink shows as a symlink with its own size.
 *   - Sort by raw byte value (qsort with a strcmp comparator); readdir hands
 *     entries back in filesystem order, which differs between machines.
 *   - Copy each d_name before the next readdir -- the returned struct may be
 *     reused.
 *   - More than one path, or a missing `-l`, is a usage error: exit 2.
 *   - A directory that cannot be opened goes to stderr and exits 2.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int
main(int argc, char **argv)
{
	/* TODO: implement myls -l (see the block comment above). */
	(void)argc;
	(void)argv;
	return 0;
}
