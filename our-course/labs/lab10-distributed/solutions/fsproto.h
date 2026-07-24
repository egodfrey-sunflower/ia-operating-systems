/* fsproto.h -- Lab 10 Parts 4-5: the file-service procedure numbers and
 * argument layouts.  GIVEN, FIXED -- fileserver.c and fileclient.c must
 * agree on these, and the autograder compiles both against its own copy.
 *
 * All blobs are laid out with msg.h.  Every request names its file by
 * HANDLE and carries its own EXPLICIT OFFSET; there is no open-file state,
 * no cursor, and no per-client anything on the server.  A handle names the
 * file itself, not a conversation with the server that issued it: any
 * handle the server has ever returned must still work after the server
 * process is killed and a fresh one is started on the same export
 * directory.
 *
 *   FS_LOOKUP:  args  u32 create | blob name          (name: <= FS_MAXNAME
 *               bytes, one path component -- no '/', not "." or "..";
 *               create != 0 creates the file, empty, if it is absent)
 *               reply u64 fh
 *   FS_GETATTR: args  u64 fh
 *               reply u64 size | u64 mtime_sec | u32 mtime_nsec
 *   FS_READ:    args  u64 fh | u64 off | u32 len      (len <= FS_MAXDATA)
 *               reply blob data      (short or empty at end of file)
 *   FS_WRITE:   args  u64 fh | u64 off | blob data    (<= FS_MAXDATA)
 *               reply u32 bytes_written
 *
 * Errors come back as RPC statuses: RPC_ENOENT (lookup: no such file and
 * create == 0), RPC_ESTALE (no file with that handle exists in the export
 * directory), RPC_EARG, RPC_EIO.
 */
#ifndef FSPROTO_H
#define FSPROTO_H

#define FS_LOOKUP  1
#define FS_GETATTR 2
#define FS_READ    3
#define FS_WRITE   4

#define FS_MAXNAME 200
#define FS_MAXDATA 1024

#endif
