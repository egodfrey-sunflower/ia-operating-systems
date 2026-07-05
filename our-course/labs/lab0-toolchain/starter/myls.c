/* myls.c -- a minimal `ls -l`.
 *
 * Usage: myls [PATH...]   (with no PATH, list ".")
 *
 * RULES:
 *   - Raw syscalls only. NO stdio.
 *   - You MUST read directories with getdents64(2), NOT readdir(3):
 *     readdir(3) is libc buffered I/O and is off-limits for this exercise.
 *     glibc has no getdents64 wrapper, so call it via syscall(2):
 *         long n = syscall(SYS_getdents64, fd, buf, sizeof buf);
 *     and parse the buffer as a sequence of struct linux_dirent64 records,
 *     advancing by each record's d_reclen. See `man 2 getdents64`.
 *
 * OUTPUT FORMAT (must be exact -- the autograder compares byte-for-byte):
 *   For each PATH argument, in order:
 *     - if PATH is a directory: list its entries, dropping "." and "..",
 *       sorted ascending by raw byte value (strcmp); NAME is the entry name;
 *     - otherwise: print one line for PATH itself; NAME is PATH verbatim.
 *   Each line:   MODE ' ' SIZE ' ' NAME '\n'
 *     MODE : 10 chars, like `ls -l`: type char (d/-/l/c/b/p/s/?) then 9
 *            permission chars (rwx triples, '-' where the bit is clear).
 *            Do NOT render setuid/setgid/sticky -- plain rwx only.
 *     SIZE : st_size in bytes, decimal, no padding.
 *     NAME : entry name (dir listing) or PATH verbatim.
 *   Use lstat(2) / fstatat(2) with AT_SYMLINK_NOFOLLOW so symlinks are not
 *   followed.
 *
 * struct linux_dirent64 layout (define it yourself; glibc does not):
 *     unsigned long long d_ino;
 *     long long          d_off;
 *     unsigned short     d_reclen;
 *     unsigned char      d_type;
 *     char               d_name[];   // NUL-terminated
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* TODO: implement myls. Delete the two lines below when you do. */
	write(2, "myls: not implemented\n", 22);
	return 1;
}
