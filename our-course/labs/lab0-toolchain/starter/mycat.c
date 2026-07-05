/* mycat.c -- concatenate files to standard output.
 *
 * Usage: mycat [FILE...]
 *   With no FILE, or when a FILE is "-", read standard input. ("-" support
 *   is required -- the spec and the rubric both check it.)
 *
 * RULES:
 *   - Raw syscalls only: open(2)/read(2)/write(2)/close(2).
 *   - NO stdio: no printf/fputs/fopen/getchar/... Diagnostics go to fd 2
 *     via write(2). malloc(3) and strerror(3) are fine (not stdio).
 *
 * Suggested behaviour (see the handout for the exact contract):
 *   - copy each FILE to fd 1 in order; "-" or no args means stdin;
 *   - on a file that cannot be opened, print an error to fd 2, keep going,
 *     and exit with status 1;
 *   - handle short writes and EINTR.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* TODO: implement mycat. Delete the two lines below when you do. */
	write(2, "mycat: not implemented\n", 23);
	return 1;
}
