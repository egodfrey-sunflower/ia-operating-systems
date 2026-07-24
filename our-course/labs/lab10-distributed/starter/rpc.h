/* rpc.h -- Lab 10 Part 3: the RPC library interface.  GIVEN, FIXED.
 *
 * This header is the contract between your rpc.c and every tool built on it
 * (rpcdemo, fileserver, fileclient).  The autograder compiles your rpc.c
 * against ITS OWN copy of this header, so the interface cannot drift: add
 * anything private you need inside rpc.c, but do not change these
 * declarations.
 *
 * The wire format is fixed too, so a request and its retransmission are
 * recognisably the same call.  Every request and every reply is one
 * datagram, laid out with msg.h (big-endian):
 *
 *   request:  u32 RPC_MAGIC | u32 client | u32 seq | u32 proc   | blob args
 *   reply:    u32 RPC_MAGIC | u32 client | u32 seq | u32 status | blob result
 *
 * `client` is the caller's client id -- every client instance picks its own,
 * and two live clients never share one.  `seq` numbers that client's calls
 * 1, 2, 3, ...  A retransmitted request is byte-identical to the original:
 * same client, same seq, same proc, same args.
 */
#ifndef RPC_H
#define RPC_H

#include <stddef.h>
#include <stdint.h>

#define RPC_MAGIC      0x52504331u      /* "RPC1" */
#define RPC_MAXPAYLOAD 1200             /* max args / result blob, bytes */

/* Reply status codes.  0 is success; the rest are application errors --
 * delivered to the caller as the return value of rpc_call, exactly like an
 * errno coming back from a system call. */
#define RPC_OK      0
#define RPC_EPROC   1   /* no such procedure */
#define RPC_EARG    2   /* malformed arguments */
#define RPC_EIO     3   /* server-side I/O failure */
#define RPC_ENOENT  4   /* no such file (Part 4) */
#define RPC_ESTALE  5   /* file handle no longer names a file (Part 4) */

/* A handler returns one of the RPC_* statuses above, or RPC_HALT to make
 * rpc_server_run send an RPC_OK reply and then return (a clean shutdown,
 * used by rpcdemo). */
#define RPC_HALT    255

/* ------------------------------------------------------------------ */
/* Client side                                                         */
/* ------------------------------------------------------------------ */

typedef struct rpc_client rpc_client;

/* Connect to 127.0.0.1:server_port.  client_id identifies this client
 * instance on the wire.  timeout_ms bounds each wait for a reply;
 * max_retries bounds how many times one call may be retransmitted before
 * rpc_call gives up.  Returns NULL on error. */
rpc_client *rpc_client_open(int server_port, uint32_t client_id,
                            int timeout_ms, int max_retries);

/* Perform one call: send proc(args), wait, retransmit on timeout, and
 * return the reply's status (an RPC_* value >= 0) with the result blob
 * copied into reply[0..rmax) and its length in *rlen.  Returns -1 only if
 * every retry went unanswered (the hard-failure case).  The caller sees
 * exactly one result per call, however many datagrams that took. */
int rpc_call(rpc_client *c, uint32_t proc,
             const uint8_t *args, size_t alen,
             uint8_t *reply, size_t rmax, size_t *rlen);

long rpc_client_calls(const rpc_client *c);     /* rpc_call invocations */
long rpc_client_retrans(const rpc_client *c);   /* retransmissions issued */
void rpc_client_close(rpc_client *c);

/* ------------------------------------------------------------------ */
/* Server side                                                         */
/* ------------------------------------------------------------------ */

typedef struct rpc_server rpc_server;

/* Handle one call: args[0..alen) in, result into reply[0..rmax) with its
 * length in *rlen (initialised to 0), return an RPC_* status.  ud is the
 * pointer given to rpc_server_run. */
typedef int (*rpc_handler)(uint32_t proc, const uint8_t *args, size_t alen,
                           uint8_t *reply, size_t rmax, size_t *rlen,
                           void *ud);

/* Bind 127.0.0.1:port (0 = ephemeral; read back with rpc_server_port). */
rpc_server *rpc_server_open(int port);
int rpc_server_port(const rpc_server *s);

/* Serve requests until a handler returns RPC_HALT.  Dispatches each call
 * to h exactly once, however many times its request datagram arrives. */
void rpc_server_run(rpc_server *s, rpc_handler h, void *ud);

long rpc_server_handled(const rpc_server *s);      /* calls executed */
long rpc_server_dup_replies(const rpc_server *s);  /* retries answered
                                                      without re-executing */
void rpc_server_close(rpc_server *s);

#endif
