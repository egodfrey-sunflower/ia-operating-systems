/* mygrep.c -- fixed-string line search.
 *
 * Usage: mygrep PATTERN [FILE...]
 *   With no FILE (or FILE == "-"), read standard input.
 *   PATTERN is a literal string (like `grep -F`), not a regex.
 *
 * RULES:
 *   - Raw syscalls only for I/O: read(2)/write(2)/open(2)/close(2).
 *   - NO stdio. malloc(3), strerror(3), memmem(3)/strstr(3) are allowed.
 *
 * Suggested behaviour (exact contract in the handout):
 *   - print every input line that contains PATTERN as a substring;
 *   - terminate every printed line with '\n' (even if the input line had no
 *     trailing newline);
 *   - when more than one FILE is given, prefix each printed line with
 *     "FILE:" (again matching `grep -F`);
 *   - exit 0 if any line matched, 1 if none matched, 2 on error.
 *
 * HINT: reading the whole input into a grown buffer (malloc/realloc) makes
 * line-splitting much easier than trying to split across read() boundaries.
 * memmem(3) (needs _GNU_SOURCE) does substring search over a length-bounded
 * buffer, which is what you want since lines are not NUL-terminated.
 */

#define _GNU_SOURCE

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

	/* TODO: implement mygrep. Delete the two lines below when you do. */
	write(2, "mygrep: not implemented\n", 24);
	return 1;
}
