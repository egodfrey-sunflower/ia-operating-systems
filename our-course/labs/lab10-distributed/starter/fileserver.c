/* fileserver.c -- Lab 10 Part 4: a stateless file server.  STARTER.
 *
 * Interface (fixed):
 *
 *   fileserver <exportdir> [port=<P>]
 *       binds 127.0.0.1:<P> (default 0 = ephemeral), prints "port=N",
 *       serves FS_* calls on the export directory until killed.
 *
 * fsproto.h is the fixed contract: the procedures, their argument
 * layouts, and what a file handle must MEAN -- read its header comment
 * before writing a line here.  The design rule of this part, from
 * ch. 49: the server holds no state between requests.  Every request
 * carries everything the server needs, every operation is idempotent,
 * and any handle the server has ever issued keeps working after this
 * process is replaced by a fresh one.
 *
 * What you implement is marked TODO below.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fsproto.h"
#include "msg.h"
#include "rpc.h"

static const char *exportdir;

/* Given: what LOOKUP accepts as a name. */
static int name_ok(const char *name)
{
	if (name[0] == '\0' || strlen(name) > FS_MAXNAME)
		return 0;
	if (strchr(name, '/') != NULL)
		return 0;
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return 0;
	return 1;
}

static int do_lookup(mbuf *am, mbuf *rm)
{
	(void)am; (void)rm; (void)name_ok;
	/* TODO (Part 4): u32 create + blob name in; decide what a file
	 * handle IS (fsproto.h says what it must mean), and reply u64 fh.
	 * create != 0 creates the file, empty, if absent. */
	return RPC_EPROC;       /* TODO */
}

static int do_getattr(mbuf *am, mbuf *rm)
{
	(void)am; (void)rm;
	/* TODO (Part 4): u64 fh in; reply u64 size | u64 mtime_sec |
	 * u32 mtime_nsec (stat's st_size and st_mtim). */
	return RPC_EPROC;       /* TODO */
}

static int do_read(mbuf *am, mbuf *rm)
{
	(void)am; (void)rm;
	/* TODO (Part 4): u64 fh | u64 off | u32 len in (len <= FS_MAXDATA);
	 * reply blob data -- short or empty past end of file. */
	return RPC_EPROC;       /* TODO */
}

static int do_write(mbuf *am, mbuf *rm)
{
	(void)am; (void)rm;
	/* TODO (Part 4): u64 fh | u64 off | blob data in; write at exactly
	 * that offset; reply u32 bytes_written. */
	return RPC_EPROC;       /* TODO */
}

static int handler(uint32_t proc, const uint8_t *args, size_t alen,
                   uint8_t *reply, size_t rmax, size_t *rlen, void *ud)
{
	(void)ud;
	mbuf am, rm;
	mb_rinit(&am, (uint8_t *)args, alen);
	mb_winit(&rm, reply, rmax);
	int status;
	switch (proc) {
	case FS_LOOKUP:  status = do_lookup(&am, &rm); break;
	case FS_GETATTR: status = do_getattr(&am, &rm); break;
	case FS_READ:    status = do_read(&am, &rm); break;
	case FS_WRITE:   status = do_write(&am, &rm); break;
	default:         status = RPC_EPROC; break;
	}
	*rlen = (status == RPC_OK) ? rm.len : 0;
	return status;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: fileserver <exportdir> [port=<P>]\n");
		return 2;
	}
	exportdir = argv[1];
	long port = 0;
	for (int i = 2; i < argc; i++) {
		if (strncmp(argv[i], "port=", 5) == 0)
			port = strtol(argv[i] + 5, NULL, 10);
	}
	struct stat st;
	if (stat(exportdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
		fprintf(stderr, "fileserver: '%s' is not a directory\n",
		        exportdir);
		return 2;
	}

	rpc_server *s = rpc_server_open((int)port);
	if (s == NULL)
		return 1;
	printf("port=%d\n", rpc_server_port(s));
	fflush(stdout);
	rpc_server_run(s, handler, NULL);       /* until killed */
	rpc_server_close(s);
	return 0;
}
