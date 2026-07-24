/* mystat.c -- print an inode's metadata, decoded.
 *
 * Usage: mystat PATH...
 *
 * For each PATH, print exactly these five lines, built from the file's inode:
 *
 *     name: <path>
 *     type: <human-readable type>
 *     size: <bytes>
 *     links: <hard-link count>
 *     mode: <octal> <symbolic>
 *
 * The target is byte-for-byte identical to
 *
 *     stat --printf='name: %n\ntype: %F\nsize: %s\nlinks: %h\nmode: %a %A\n' PATH...
 *
 * so you can check any single case yourself by running that `stat` line. The
 * autograder diffs against it directly. (It must be `--printf`: the `-c` form
 * prints the \n's literally and adds its own trailing newline.)
 *
 *   - `type` is stat's %F wording ("regular file", "directory", "symbolic
 *     link", "character special file", ...). Run `stat -c %F` on the fixtures
 *     to see every spelling you must reproduce.
 *   - `mode` is the permission bits in octal, a space, then the ten-character
 *     symbolic form (stat's %A / the first column of `ls -l`), including the
 *     set-user-ID, set-group-ID and sticky bits.
 *   - Report the metadata of the symbolic link itself, not the file it points
 *     to -- as `stat` does by default.
 *   - A PATH that cannot be examined goes to stderr as
 *       mystat: cannot stat '<path>': <strerror(errno)>
 *     after which you carry on with the remaining operands and exit 1.
 *   - With no operands at all, exit 2 (a usage error).
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int
main(int argc, char **argv)
{
	/* TODO: implement mystat (see the block comment above). */
	(void)argc;
	(void)argv;
	return 0;
}
