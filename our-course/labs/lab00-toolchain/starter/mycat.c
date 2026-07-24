/* mycat.c -- concatenate files to standard output.
 *
 * Usage: mycat [FILE...]
 *
 * Target behaviour: byte-for-byte identical to cat(1) on stdout, stderr and
 * exit status, for the cases in tests/run.sh. See ../README.md, Part 3.
 *
 * RULES
 *   - Allowed: open, read, write, close, and libc that is not stdio
 *     (strerror, malloc, free, strlen, memchr, ...).
 *   - Banned:  printf and every other stdio call. The autograder inspects the
 *              binary's undefined symbols, so you cannot sneak one in.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	/* TODO: implement mycat.
	 *
	 * 1. No arguments (or an argument that is exactly "-"): copy fd 0 to
	 *    fd 1 until read() returns 0.
	 * 2. Otherwise open(2) each argument in turn and copy it to fd 1.
	 * 3. If a file cannot be opened: write
	 *       mycat: <path>: <strerror(errno)>\n
	 *    to fd 2, carry on with the remaining files, and exit 1 at the end.
	 * 4. A file that opens but fails to READ -- a directory is the case you
	 *    will meet -- is reported the same way, with the same exit 1.
	 * 5. Otherwise exit 0.
	 *
	 * Two traps, both tested:
	 *   - read() may return FEWER bytes than you asked for without being at
	 *     end of file. Only a return of 0 means end of input.
	 *   - write() may also write fewer bytes than you asked. Loop until the
	 *     whole buffer is out.
	 * Do not treat the buffer as a C string: the fixtures contain NUL bytes.
	 */

	(void)argc;
	(void)argv;
	return 0;
}
