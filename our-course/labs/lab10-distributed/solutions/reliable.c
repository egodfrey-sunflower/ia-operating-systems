/* reliable.c -- Lab 10 Part 2: reliable delivery over lossy UDP.  REFERENCE.
 *
 * Interface (fixed; the autograder reads these exact lines):
 *
 *   reliable server
 *       "port=N", then one line per message handed to the application:
 *       "deliver payload=%s"
 *       and, once the sender is done:
 *       "server done delivered=%ld dups=%ld"
 *   reliable client <port> n=<N> [timeout=<ms>] [retries=<K>]
 *       sends messages msg-0000 .. msg-<N-1>, reliably and in order, then:
 *       "client done sent=%ld acked=%ld retrans=%ld giveups=%ld elapsed_ms=%ld"
 *       exit status 0 iff every message (and the final close) was
 *       acknowledged.
 *
 * The contract this tool must meet at any loss rate up to 30%:
 *   - every message is delivered to the application EXACTLY once,
 *     in the order sent;
 *   - both processes terminate cleanly, even when the closing exchange
 *     is itself lossy.
 *
 * Design: stop-and-wait.  DATA(seq) -> ACK(seq); the client retransmits on
 * a timeout; the receiver delivers seq == expected, discards and re-acks
 * anything older.  A FIN closes the stream; the server re-acks FIN retries
 * until the line has been silent for a drain period, then reports.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "msg.h"
#include "net.h"

#define MAXPKT 1400
#define T_DATA 1
#define T_ACK  2
#define T_FIN  3
#define REL_MAGIC 0x52454C31u   /* "REL1" */
#define FIN_DRAIN_MS 400

static int kv(const char *arg, const char *key, long *out)
{
	size_t klen = strlen(key);
	if (strncmp(arg, key, klen) == 0 && arg[klen] == '=') {
		*out = strtol(arg + klen + 1, NULL, 10);
		return 1;
	}
	return 0;
}

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static size_t pack(uint8_t *pkt, uint32_t type, uint32_t seq,
                   const void *payload, size_t plen)
{
	mbuf m;
	mb_winit(&m, pkt, MAXPKT);
	mb_put_u32(&m, REL_MAGIC);
	mb_put_u32(&m, type);
	mb_put_u32(&m, seq);
	if (plen > 0)
		mb_put_bytes(&m, payload, plen);
	return m.len;
}

static int server(void)
{
	net *nt = net_open(0);
	if (nt == NULL)
		return 1;
	printf("port=%d\n", net_port(nt));
	fflush(stdout);

	uint32_t expected = 0;
	long delivered = 0, dups = 0;
	int fin_seen = 0;
	uint32_t fin_seq = 0;
	struct sockaddr_in peer;

	for (;;) {
		uint8_t pkt[MAXPKT];
		struct sockaddr_in from;
		/* Before FIN: block forever.  After FIN: wait out the drain
		 * window, re-acking any FIN retries that still arrive. */
		long r = net_recv(nt, pkt, sizeof pkt, &from,
		                  fin_seen ? FIN_DRAIN_MS : -1);
		if (r == -1) {
			if (fin_seen)
				break;          /* line quiet: we are done */
			continue;
		}
		if (r < 12)
			continue;
		mbuf m;
		mb_rinit(&m, pkt, (size_t)r);
		uint32_t magic = mb_get_u32(&m);
		uint32_t type = mb_get_u32(&m);
		uint32_t seq = mb_get_u32(&m);
		if (magic != REL_MAGIC)
			continue;
		peer = from;

		if (type == T_DATA) {
			if (seq == expected) {
				char payload[MAXPKT];
				size_t plen = mb_remain(&m);
				if (plen >= sizeof payload)
					plen = sizeof payload - 1;
				mb_get_bytes(&m, payload, plen);
				payload[plen] = '\0';
				printf("deliver payload=%s\n", payload);
				fflush(stdout);
				delivered++;
				expected++;
			} else if (seq < expected) {
				dups++;         /* retransmission: ack again */
			} else {
				continue;       /* future seq: impossible under
				                   stop-and-wait; drop it */
			}
			uint8_t ack[MAXPKT];
			size_t alen = pack(ack, T_ACK, seq, NULL, 0);
			net_send(nt, ack, alen, &peer);
		} else if (type == T_FIN) {
			if (!fin_seen) {
				fin_seen = 1;
				fin_seq = seq;
			}
			uint8_t ack[MAXPKT];
			size_t alen = pack(ack, T_ACK, fin_seq, NULL, 0);
			net_send(nt, ack, alen, &peer);
		}
	}
	printf("server done delivered=%ld dups=%ld\n", delivered, dups);
	net_close(nt);
	return 0;
}

/* Send one packet reliably: transmit, wait for ACK(seq), retransmit on
 * timeout.  Returns retransmission count, or -1 on giveup. */
static long send_reliably(net *nt, const struct sockaddr_in *srv,
                          const uint8_t *pkt, size_t len, uint32_t seq,
                          long timeout, long retries)
{
	long retrans = 0;
	net_send(nt, pkt, len, srv);
	for (;;) {
		uint8_t ack[MAXPKT];
		long r = net_recv(nt, ack, sizeof ack, NULL, (int)timeout);
		if (r >= 12) {
			mbuf m;
			mb_rinit(&m, ack, (size_t)r);
			uint32_t magic = mb_get_u32(&m);
			uint32_t type = mb_get_u32(&m);
			uint32_t aseq = mb_get_u32(&m);
			if (magic == REL_MAGIC && type == T_ACK && aseq == seq)
				return retrans;
			continue;       /* stale ack: keep waiting */
		}
		if (r != -1)
			continue;       /* socket hiccup */
		if (retrans >= retries)
			return -1;
		net_send(nt, pkt, len, srv);
		retrans++;
	}
}

static int client(int argc, char **argv)
{
	int port = atoi(argv[2]);
	long n = 50, timeout = 100, retries = 32;
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "timeout", &v)) timeout = v;
		else if (kv(argv[i], "retries", &v)) retries = v;
	}

	net *nt = net_open(0);
	if (nt == NULL)
		return 1;
	struct sockaddr_in srv;
	net_target(&srv, port);

	long t0 = now_ms();
	long sent = 0, acked = 0, retrans = 0, giveups = 0;
	for (long i = 0; i < n; i++) {
		char payload[32];
		int plen = snprintf(payload, sizeof payload, "msg-%04ld", i);
		uint8_t pkt[MAXPKT];
		size_t len = pack(pkt, T_DATA, (uint32_t)i, payload,
		                  (size_t)plen);
		sent++;
		long r = send_reliably(nt, &srv, pkt, len, (uint32_t)i,
		                       timeout, retries);
		if (r < 0)
			giveups++;
		else {
			acked++;
			retrans += r;
		}
	}
	/* Close the stream: FIN carries the next seq. */
	uint8_t fin[MAXPKT];
	size_t flen = pack(fin, T_FIN, (uint32_t)n, NULL, 0);
	long fr = send_reliably(nt, &srv, fin, flen, (uint32_t)n,
	                        timeout, retries);
	if (fr < 0)
		giveups++;
	else
		retrans += fr;

	printf("client done sent=%ld acked=%ld retrans=%ld giveups=%ld "
	       "elapsed_ms=%ld\n",
	       sent, acked, retrans, giveups, now_ms() - t0);
	net_close(nt);
	return (acked == sent && giveups == 0) ? 0 : 1;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "server") == 0)
		return server();
	if (argc >= 3 && strcmp(argv[1], "client") == 0)
		return client(argc, argv);
	fprintf(stderr,
	        "usage: reliable server\n"
	        "       reliable client <port> [n=..] [timeout=..] [retries=..]\n");
	return 2;
}
