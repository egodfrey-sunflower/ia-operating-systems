/* rpc.c -- Lab 10 Part 3: the RPC run-time library.  REFERENCE.
 *
 * Client: stop-and-wait.  One outstanding call; retransmit the identical
 * request on timeout; ignore any reply that is not for the call in flight.
 *
 * Server: dispatch each call to the handler EXACTLY once.  A reply cache,
 * keyed on (client, seq), holds the last reply sent to each client; a
 * retransmitted request is answered from the cache without re-executing.
 * That is what makes a non-idempotent procedure safe under reply loss.
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
	uint32_t seq;
	int timeout_ms;
	int max_retries;
	long calls, retrans;
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

	uint8_t req[MAXPKT];
	mbuf m;
	mb_winit(&m, req, sizeof req);
	mb_put_u32(&m, RPC_MAGIC);
	mb_put_u32(&m, c->id);
	mb_put_u32(&m, c->seq);
	mb_put_u32(&m, proc);
	mb_put_blob(&m, args, alen);
	size_t reqlen = m.len;

	net_send(c->nt, req, reqlen, &c->srv);
	long tries = 0;
	for (;;) {
		uint8_t rp[MAXPKT];
		long r = net_recv(c->nt, rp, sizeof rp, NULL, c->timeout_ms);
		if (r == -1) {                          /* timeout */
			if (tries >= c->max_retries)
				return -1;
			net_send(c->nt, req, reqlen, &c->srv);
			tries++;
			c->retrans++;
			continue;
		}
		if (r < 16)
			continue;
		mbuf rm;
		mb_rinit(&rm, rp, (size_t)r);
		uint32_t magic = mb_get_u32(&rm);
		uint32_t rid = mb_get_u32(&rm);
		uint32_t rseq = mb_get_u32(&rm);
		uint32_t status = mb_get_u32(&rm);
		if (magic != RPC_MAGIC || rid != c->id || rseq != c->seq)
			continue;       /* not the call in flight: discard */
		uint8_t blob[RPC_MAXPAYLOAD];
		size_t blen = mb_get_blob(&rm, blob, sizeof blob);
		if (!mb_ok(&rm))
			continue;
		if (blen > rmax)
			blen = rmax;
		memcpy(reply, blob, blen);
		*rlen = blen;
		return (int)status;
	}
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

#define CACHE_SLOTS 64

struct cache_ent {
	int used;
	uint32_t client;
	uint32_t seq;           /* seq of the cached (most recent) reply */
	uint8_t reply[MAXPKT];
	size_t len;
};

struct rpc_server {
	net *nt;
	struct cache_ent cache[CACHE_SLOTS];
	int evict;              /* round-robin eviction cursor */
	long handled, dups;
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

static struct cache_ent *cache_find(rpc_server *s, uint32_t client)
{
	for (int i = 0; i < CACHE_SLOTS; i++)
		if (s->cache[i].used && s->cache[i].client == client)
			return &s->cache[i];
	return NULL;
}

static struct cache_ent *cache_take(rpc_server *s, uint32_t client)
{
	for (int i = 0; i < CACHE_SLOTS; i++)
		if (!s->cache[i].used) {
			s->cache[i].used = 1;
			s->cache[i].client = client;
			return &s->cache[i];
		}
	struct cache_ent *e = &s->cache[s->evict];
	s->evict = (s->evict + 1) % CACHE_SLOTS;
	e->used = 1;
	e->client = client;
	return e;
}

void rpc_server_run(rpc_server *s, rpc_handler h, void *ud)
{
	for (;;) {
		uint8_t req[MAXPKT];
		struct sockaddr_in from;
		long r = net_recv(s->nt, req, sizeof req, &from, -1);
		if (r < 16)
			continue;
		mbuf m;
		mb_rinit(&m, req, (size_t)r);
		uint32_t magic = mb_get_u32(&m);
		uint32_t client = mb_get_u32(&m);
		uint32_t seq = mb_get_u32(&m);
		uint32_t proc = mb_get_u32(&m);
		uint8_t args[RPC_MAXPAYLOAD];
		size_t alen = mb_get_blob(&m, args, sizeof args);
		if (magic != RPC_MAGIC || !mb_ok(&m))
			continue;

		/* A retry of the most recent call from this client is
		 * answered from the cache -- never re-executed. */
		struct cache_ent *e = cache_find(s, client);
		if (e != NULL && e->seq == seq) {
			net_send(s->nt, e->reply, e->len, &from);
			s->dups++;
			continue;
		}
		if (e != NULL && seq < e->seq)
			continue;       /* older than the cached call: a
			                   ghost; nothing useful to say */

		uint8_t result[RPC_MAXPAYLOAD];
		size_t rlen = 0;
		int status = h(proc, args, alen, result, sizeof result,
		               &rlen, ud);
		int halt = (status == RPC_HALT);
		if (halt)
			status = RPC_OK;

		if (e == NULL)
			e = cache_take(s, client);
		mbuf rm;
		mb_winit(&rm, e->reply, sizeof e->reply);
		mb_put_u32(&rm, RPC_MAGIC);
		mb_put_u32(&rm, client);
		mb_put_u32(&rm, seq);
		mb_put_u32(&rm, (uint32_t)status);
		mb_put_blob(&rm, result, rlen);
		e->len = rm.len;
		e->seq = seq;

		net_send(s->nt, e->reply, e->len, &from);
		s->handled++;
		if (halt)
			return;
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
