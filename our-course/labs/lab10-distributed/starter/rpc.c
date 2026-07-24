/* rpc.c -- Lab 10 Part 3: the RPC run-time library.  STARTER.
 *
 * rpc.h is the fixed contract: the API every tool in Parts 3-5 is built
 * on, and the wire layout of a request and a reply.  Read it first --
 * the autograder compiles this file against its own copy.
 *
 * Two requirements carry the whole part (README.md, Part 3):
 *
 *   - rpc_call returns exactly one result per call, over a network that
 *     loses datagrams in both directions;
 *   - the server dispatches each CALL to the handler exactly once,
 *     however many times that call's request datagram arrives.  A retry
 *     of a call it has already executed is answered with the reply it
 *     already computed -- never by executing the handler again.  Count
 *     those in dup_replies.
 *
 * The plumbing (open/close/counters) is written.  rpc_call and
 * rpc_server_run are yours.
 */

#include "rpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "msg.h"
#include "net.h"

#define MAXPKT (RPC_MAXPAYLOAD + 64)

/* ------------------------------------------------------------------ */
/* Client                                                              */
/* ------------------------------------------------------------------ */

struct rpc_client {
	net *nt;
	struct sockaddr_in srv;
	uint32_t id;
	uint32_t seq;           /* last seq used; first call is seq 1 */
	int timeout_ms;
	int max_retries;
	long calls, retrans;
	/* TODO: whatever else your rpc_call needs */
};

rpc_client *rpc_client_open(int server_port, uint32_t client_id,
                            int timeout_ms, int max_retries)
{
	rpc_client *c = calloc(1, sizeof *c);
	if (c == NULL)
		return NULL;
	c->nt = net_open(0);
	if (c->nt == NULL) {
		free(c);
		return NULL;
	}
	net_target(&c->srv, server_port);
	c->id = client_id;
	c->seq = 0;
	c->timeout_ms = timeout_ms;
	c->max_retries = max_retries;
	return c;
}

int rpc_call(rpc_client *c, uint32_t proc,
             const uint8_t *args, size_t alen,
             uint8_t *reply, size_t rmax, size_t *rlen)
{
	*rlen = 0;
	if (alen > RPC_MAXPAYLOAD)
		return RPC_EARG;
	c->calls++;
	c->seq++;
	(void)proc; (void)args; (void)reply; (void)rmax;

	/* TODO (Part 3): marshal the request (rpc.h gives the layout),
	 * send it, and collect THE reply to THIS call -- retransmitting
	 * the identical datagram on timeout, up to max_retries times, and
	 * disregarding anything that is not the awaited reply.  Return the
	 * reply's status with its blob copied out, or -1 if every retry
	 * went unanswered. */
	return -1;      /* TODO */
}

long rpc_client_calls(const rpc_client *c)   { return c->calls; }
long rpc_client_retrans(const rpc_client *c) { return c->retrans; }

void rpc_client_close(rpc_client *c)
{
	if (c == NULL)
		return;
	net_close(c->nt);
	free(c);
}

/* ------------------------------------------------------------------ */
/* Server                                                              */
/* ------------------------------------------------------------------ */

struct rpc_server {
	net *nt;
	long handled, dups;
	/* TODO: the state that lets a retry be answered without
	 * re-executing the handler. */
};

rpc_server *rpc_server_open(int port)
{
	rpc_server *s = calloc(1, sizeof *s);
	if (s == NULL)
		return NULL;
	s->nt = net_open(port);
	if (s->nt == NULL) {
		free(s);
		return NULL;
	}
	return s;
}

int rpc_server_port(const rpc_server *s)
{
	return net_port(s->nt);
}

void rpc_server_run(rpc_server *s, rpc_handler h, void *ud)
{
	(void)h; (void)ud;
	/* TODO (Part 3): the serve loop.  Receive each request and answer
	 * it as the two requirements above demand, keeping handled and dups
	 * counted.  When the handler returns RPC_HALT, reply RPC_OK and
	 * return. */
	for (;;) {
		uint8_t req[MAXPKT];
		struct sockaddr_in from;
		long r = net_recv(s->nt, req, sizeof req, &from, -1);
		(void)r;
		return;         /* TODO: replace this stub loop */
	}
}

long rpc_server_handled(const rpc_server *s)    { return s->handled; }
long rpc_server_dup_replies(const rpc_server *s) { return s->dups; }

void rpc_server_close(rpc_server *s)
{
	if (s == NULL)
		return;
	net_close(s->nt);
	free(s);
}
