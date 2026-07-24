/* mytail.c -- print the last N lines of a file, seeking from the end.
 *
 * Usage: mytail -n N FILE
 *
 * Output is byte-for-byte `tail -n N FILE`.
 *
 *   - N is a non-negative integer. N = 0 prints nothing; N larger than the
 *     file's line count prints the whole file.
 *   - The last line must come out exactly as it is stored, whether or not the
 *     file ends in a newline.
 *   - FILE is required; there is no stdin mode. The point of this tool is to
 *     use lseek(2): reach the last N lines by seeking near the end of the file
 *     and reading backward, NOT by reading the file from the beginning. The
 *     autograder measures how much of the file you pull in -- whether by
 *     read, pread, readv or mmap -- and fails a tool that takes in far more
 *     of the file than the last N lines occupy.
 *   - A FILE that cannot be opened goes to stderr and exits 1.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	/* TODO: implement mytail -n N (see the block comment above). */
	(void)argc;
	(void)argv;
	return 0;
}
