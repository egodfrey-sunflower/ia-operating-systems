/* rpcdemo.c -- Lab 10 Part 3: a counter service over the RPC library.
 * STARTER.
 *
 * Interface (fixed; the autograder reads these exact lines):
 *
 *   rpcdemo server
 *       "port=N"; serves until a shutdown call, then
 *       "server done handled=%ld executed_inc=%ld dup_replies=%ld"
 *   rpcdemo client <port> id=<I> [timeout=<ms>] [retries=<K>] <op>
 *     op = ping n=<N>          "ping done calls=%d ok=%d retrans=%ld"
 *          inc n=<N>           "inc done calls=%d ok=%d value=%u retrans=%ld"
 *          get                 "get value=%u"
 *          slowping ms=<M> n=<N>
 *                              "slowping done slow_ok=%d pings_ok=%d retrans=%ld"
 *          shutdown            "shutdown ok"
 *       exit status 0 iff every call succeeded.
 *
 * Procedures: ping echoes its argument blob.  inc(u32 amount) advances a
 * single shared counter and returns its new value -- deliberately NOT
 * idempotent; executing one call twice is a wrong answer forever after.
 * get returns the counter.  slow(u32 ms) sleeps, then replies "slow" --
 * long enough for a caller to have retried in the meantime.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "msg.h"
#include "rpc.h"

#define PROC_PING     1
#define PROC_INC      2
#define PROC_GET      3
#define PROC_SLOW     4
#define PROC_SHUTDOWN 5

static int kv(const char *arg, const char *key, long *out)
{
	size_t klen = strlen(key);
	if (strncmp(arg, key, klen) == 0 && arg[klen] == '=') {
		*out = strtol(arg + klen + 1, NULL, 10);
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Server                                                              */
/* ------------------------------------------------------------------ */

struct counter_state {
	uint32_t value;
	long executed_inc;
};

static int handler(uint32_t proc, const uint8_t *args, size_t alen,
                   uint8_t *reply, size_t rmax, size_t *rlen, void *ud)
{
	struct counter_state *st = ud;
	mbuf am, rm;
	mb_rinit(&am, (uint8_t *)args, alen);
	mb_winit(&rm, reply, rmax);

	switch (proc) {
	case PROC_PING:
		/* Worked example: echo the argument blob back. */
		mb_put_bytes(&rm, args, alen);
		*rlen = rm.len;
		return RPC_OK;
	case PROC_INC:
		/* TODO (Part 3): u32 amount in; advance st->value by it,
		 * count the execution in st->executed_inc, reply with the
		 * new value (u32). */
		(void)st;
		return RPC_EPROC;       /* TODO */
	case PROC_GET:
		/* TODO (Part 3): reply with the counter (u32). */
		return RPC_EPROC;       /* TODO */
	case PROC_SLOW:
		/* TODO (Part 3): u32 ms in (reject > 5000 as RPC_EARG);
		 * usleep that long, then reply with the 4 bytes "slow". */
		return RPC_EPROC;       /* TODO */
	case PROC_SHUTDOWN:
		return RPC_HALT;
	default:
		return RPC_EPROC;
	}
}

static int server(void)
{
	rpc_server *s = rpc_server_open(0);
	if (s == NULL)
		return 1;
	printf("port=%d\n", rpc_server_port(s));
	fflush(stdout);

	struct counter_state st = { 0, 0 };
	rpc_server_run(s, handler, &st);
	printf("server done handled=%ld executed_inc=%ld dup_replies=%ld\n",
	       rpc_server_handled(s), st.executed_inc,
	       rpc_server_dup_replies(s));
	rpc_server_close(s);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Client                                                              */
/* ------------------------------------------------------------------ */

static int op_ping(rpc_client *c, long n)
{
	int ok = 0;
	for (long i = 0; i < n; i++) {
		char want[32];
		int wlen = snprintf(want, sizeof want, "ping-%03ld", i);
		uint8_t reply[RPC_MAXPAYLOAD];
		size_t rlen;
		int st = rpc_call(c, PROC_PING, (const uint8_t *)want,
		                  (size_t)wlen, reply, sizeof reply, &rlen);
		if (st == RPC_OK && rlen == (size_t)wlen &&
		    memcmp(reply, want, rlen) == 0)
			ok++;
	}
	printf("ping done calls=%ld ok=%d retrans=%ld\n",
	       n, ok, rpc_client_retrans(c));
	return ok == n ? 0 : 1;
}

static int op_inc(rpc_client *c, long n)
{
	int ok = 0;
	uint32_t value = 0;
	for (long i = 0; i < n; i++) {
		uint8_t arg[4], reply[RPC_MAXPAYLOAD];
		mbuf m;
		mb_winit(&m, arg, sizeof arg);
		mb_put_u32(&m, 1);
		size_t rlen;
		int st = rpc_call(c, PROC_INC, arg, m.len,
		                  reply, sizeof reply, &rlen);
		if (st == RPC_OK && rlen == 4) {
			mbuf rm;
			mb_rinit(&rm, reply, rlen);
			value = mb_get_u32(&rm);
			ok++;
		}
	}
	printf("inc done calls=%ld ok=%d value=%u retrans=%ld\n",
	       n, ok, value, rpc_client_retrans(c));
	return ok == n ? 0 : 1;
}

static int op_get(rpc_client *c)
{
	uint8_t reply[RPC_MAXPAYLOAD];
	size_t rlen;
	int st = rpc_call(c, PROC_GET, NULL, 0, reply, sizeof reply, &rlen);
	if (st != RPC_OK || rlen != 4) {
		fprintf(stderr, "get failed (status %d)\n", st);
		return 1;
	}
	mbuf m;
	mb_rinit(&m, reply, rlen);
	printf("get value=%u\n", mb_get_u32(&m));
	return 0;
}

static int op_slowping(rpc_client *c, long ms, long n)
{
	uint8_t arg[4], reply[RPC_MAXPAYLOAD];
	mbuf m;
	mb_winit(&m, arg, sizeof arg);
	mb_put_u32(&m, (uint32_t)ms);
	size_t rlen;
	int st = rpc_call(c, PROC_SLOW, arg, m.len,
	                  reply, sizeof reply, &rlen);
	int slow_ok = (st == RPC_OK && rlen == 4 &&
	               memcmp(reply, "slow", 4) == 0);

	/* The slow call's late duplicate replies are still queued on our
	 * socket.  These pings only succeed if rpc_call refuses to take a
	 * stale reply as the answer to a new call. */
	int pings_ok = 0;
	for (long i = 0; i < n; i++) {
		char want[32];
		int wlen = snprintf(want, sizeof want, "ping-%03ld", i);
		st = rpc_call(c, PROC_PING, (const uint8_t *)want,
		              (size_t)wlen, reply, sizeof reply, &rlen);
		if (st == RPC_OK && rlen == (size_t)wlen &&
		    memcmp(reply, want, rlen) == 0)
			pings_ok++;
	}
	printf("slowping done slow_ok=%d pings_ok=%d retrans=%ld\n",
	       slow_ok, pings_ok, rpc_client_retrans(c));
	return (slow_ok && pings_ok == n) ? 0 : 1;
}

static int op_shutdown(rpc_client *c)
{
	uint8_t reply[RPC_MAXPAYLOAD];
	size_t rlen;
	int st = rpc_call(c, PROC_SHUTDOWN, NULL, 0,
	                  reply, sizeof reply, &rlen);
	if (st != RPC_OK) {
		fprintf(stderr, "shutdown failed (status %d)\n", st);
		return 1;
	}
	printf("shutdown ok\n");
	return 0;
}

static int client(int argc, char **argv)
{
	int port = atoi(argv[2]);
	long id = 1, timeout = 100, retries = 32;
	long n = 10, ms = 0;
	const char *op = NULL;
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "id", &v)) id = v;
		else if (kv(argv[i], "timeout", &v)) timeout = v;
		else if (kv(argv[i], "retries", &v)) retries = v;
		else if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "ms", &v)) ms = v;
		else op = argv[i];
	}
	if (op == NULL) {
		fprintf(stderr, "rpcdemo: no operation given\n");
		return 2;
	}

	rpc_client *c = rpc_client_open(port, (uint32_t)id,
	                                (int)timeout, (int)retries);
	if (c == NULL)
		return 1;
	int rc;
	if (strcmp(op, "ping") == 0) rc = op_ping(c, n);
	else if (strcmp(op, "inc") == 0) rc = op_inc(c, n);
	else if (strcmp(op, "get") == 0) rc = op_get(c);
	else if (strcmp(op, "slowping") == 0) rc = op_slowping(c, ms, n);
	else if (strcmp(op, "shutdown") == 0) rc = op_shutdown(c);
	else {
		fprintf(stderr, "rpcdemo: unknown op '%s'\n", op);
		rc = 2;
	}
	rpc_client_close(c);
	return rc;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "server") == 0)
		return server();
	if (argc >= 3 && strcmp(argv[1], "client") == 0)
		return client(argc, argv);
	fprintf(stderr,
	        "usage: rpcdemo server\n"
	        "       rpcdemo client <port> id=<I> [timeout=..] [retries=..] "
	        "<ping n=..|inc n=..|get|slowping ms=.. n=..|shutdown>\n");
	return 2;
}
