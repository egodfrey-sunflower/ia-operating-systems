/* fileserver.c -- Lab 10 Part 4: a stateless file server.  REFERENCE.
 *
 * Interface (fixed):
 *
 *   fileserver <exportdir> [port=<P>]
 *       binds 127.0.0.1:<P> (default 0 = ephemeral), prints "port=N",
 *       serves FS_* calls on the export directory until killed.
 *
 * Statelessness is the design rule: the server holds NOTHING between
 * requests -- no open files, no cursors, no per-client tables.  A file
 * handle is the file's inode number, recomputed from the directory on
 * every request, so any handle ever issued keeps working after the process
 * is killed and a fresh one starts on the same directory.  Every operation
 * carries its own offset and is idempotent: executing it twice leaves the
 * same bytes as executing it once.
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

/* Resolve a handle to a path by scanning the export directory for the
 * regular file with that inode number.  O(files), recomputed per request:
 * the handle's validity depends only on the directory's contents, never on
 * this process's history. */
static int fh_to_path(uint64_t fh, char *out, size_t max)
{
	DIR *d = opendir(exportdir);
	if (d == NULL)
		return -1;
	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		char path[4096];
		snprintf(path, sizeof path, "%s/%s", exportdir, de->d_name);
		struct stat st;
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		if ((uint64_t)st.st_ino == fh) {
			snprintf(out, max, "%s", path);
			closedir(d);
			return 0;
		}
	}
	closedir(d);
	return -1;
}

static int do_lookup(mbuf *am, mbuf *rm)
{
	uint32_t create = mb_get_u32(am);
	char name[FS_MAXNAME + 1];
	size_t nlen = mb_get_blob(am, name, FS_MAXNAME);
	if (!mb_ok(am))
		return RPC_EARG;
	name[nlen] = '\0';
	if (!name_ok(name))
		return RPC_EARG;

	char path[4096];
	snprintf(path, sizeof path, "%s/%s", exportdir, name);
	if (create) {
		int fd = open(path, O_RDWR | O_CREAT, 0644);
		if (fd < 0)
			return RPC_EIO;
		close(fd);
	}
	struct stat st;
	if (stat(path, &st) != 0)
		return RPC_ENOENT;
	mb_put_u64(rm, (uint64_t)st.st_ino);
	return RPC_OK;
}

static int do_getattr(mbuf *am, mbuf *rm)
{
	uint64_t fh = mb_get_u64(am);
	if (!mb_ok(am))
		return RPC_EARG;
	char path[4096];
	if (fh_to_path(fh, path, sizeof path) != 0)
		return RPC_ESTALE;
	struct stat st;
	if (stat(path, &st) != 0)
		return RPC_ESTALE;
	mb_put_u64(rm, (uint64_t)st.st_size);
	mb_put_u64(rm, (uint64_t)st.st_mtim.tv_sec);
	mb_put_u32(rm, (uint32_t)st.st_mtim.tv_nsec);
	return RPC_OK;
}

static int do_read(mbuf *am, mbuf *rm)
{
	uint64_t fh = mb_get_u64(am);
	uint64_t off = mb_get_u64(am);
	uint32_t len = mb_get_u32(am);
	if (!mb_ok(am) || len > FS_MAXDATA)
		return RPC_EARG;
	char path[4096];
	if (fh_to_path(fh, path, sizeof path) != 0)
		return RPC_ESTALE;
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return RPC_EIO;
	uint8_t buf[FS_MAXDATA];
	ssize_t n = pread(fd, buf, len, (off_t)off);
	close(fd);
	if (n < 0)
		return RPC_EIO;
	mb_put_blob(rm, buf, (size_t)n);
	return RPC_OK;
}

static int do_write(mbuf *am, mbuf *rm)
{
	uint64_t fh = mb_get_u64(am);
	uint64_t off = mb_get_u64(am);
	uint8_t buf[FS_MAXDATA];
	size_t len = mb_get_blob(am, buf, sizeof buf);
	if (!mb_ok(am))
		return RPC_EARG;
	char path[4096];
	if (fh_to_path(fh, path, sizeof path) != 0)
		return RPC_ESTALE;
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return RPC_EIO;
	ssize_t n = pwrite(fd, buf, len, (off_t)off);
	close(fd);
	if (n < 0 || (size_t)n != len)
		return RPC_EIO;
	mb_put_u32(rm, (uint32_t)n);
	return RPC_OK;
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
