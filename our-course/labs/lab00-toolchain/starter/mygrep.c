/* mygrep.c -- print the lines of each file that contain a fixed substring.
 *
 * Usage: mygrep PATTERN [FILE...]
 *
 * Target behaviour: byte-for-byte identical to `grep -F PATTERN [FILE...]`
 * on stdout, stderr and exit status. See ../README.md, Part 3.
 *
 * PLAIN SUBSTRING MATCHING ONLY. No regular expressions -- not '.', not '*',
 * not anchors. `grep -F` is the reference precisely because it does the same.
 *
 * RULES
 *   - Allowed: open, read, write, close, malloc/realloc/free, memchr,
 *     memmem, strlen, strerror.
 *   - Banned:  stdio, and regcomp/regexec.
 */

#define _GNU_SOURCE		/* memmem(3) */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	/* TODO: implement mygrep.
	 *
	 * 1. argv[1] is the pattern. Fewer than 2 arguments: write a usage line
	 *    to fd 2 and exit 2.
	 * 2. No FILE operands, or an operand that is exactly "-": read stdin.
	 *    An EMPTY pattern matches every line -- do not special-case it into
	 *    matching nothing.
	 * 3. For each input, print every line that CONTAINS the pattern
	 *    anywhere. Always terminate the printed line with '\n', even if the
	 *    source line had no trailing newline -- grep does.
	 * 4. With TWO OR MORE FILE operands, prefix each printed line with
	 *    "<file>:". With one file, or with stdin, no prefix.
	 * 5. A file that will not open: write
	 *       mygrep: <path>: <strerror(errno)>\n
	 *    to fd 2 and carry on with the remaining files.
	 * 6. Exit status, in this order of precedence:
	 *       2  some file could not be read
	 *       0  at least one line matched
	 *       1  nothing matched
	 *
	 * The trap here is a line that straddles two read() calls. The blunt fix
	 * is to read the whole input into a malloc'd buffer that you realloc as
	 * it fills, and only then split on '\n'; the fixture tree includes a
	 * 200000-byte file to make sure you cannot get away with one read().
	 *
	 * Lines are not NUL-terminated, so match with memmem(haystack, hlen,
	 * needle, nlen) rather than strstr.
	 */

	(void)argc;
	(void)argv;
	return 0;
}
