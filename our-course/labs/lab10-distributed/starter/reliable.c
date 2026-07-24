/* reliable.c -- Lab 10 Part 2: reliable delivery over lossy UDP.  STARTER.
 *
 * Interface (fixed -- the autograder reads these exact lines):
 *
 *   reliable server
 *       "port=N", then one line per message handed to the application:
 *       "deliver payload=%s"
 *       and, once the sender is done:
 *       "server done delivered=%ld dups=%ld"
 *   reliable client <port> n=<N> [timeout=<ms>] [retries=<K>]
 *       sends messages msg-0000 .. msg-<N-1>, then:
 *       "client done sent=%ld acked=%ld retrans=%ld giveups=%ld elapsed_ms=%ld"
 *       exit status 0 iff every message (and the final close) was
 *       acknowledged.
 *
 * The contract (README.md, Part 2), which must hold at any loss rate up
 * to 30% in each direction:
 *   - every message is delivered to the application (= printed as a
 *     "deliver" line) EXACTLY once, in the order sent;
 *   - `dups` counts retransmissions the receiver saw and did not deliver;
 *   - `retrans` counts retransmissions the client sent, `giveups` the
 *     sends it abandoned after K fruitless retries -- a clean run has
 *     giveups=0, and at 0% loss retrans=0;
 *   - both processes terminate cleanly, even when the closing exchange
 *     is itself lossy.
 *
 * The wire format between your client and your server is yours to
 * design.  Ch. 48's ladder -- acknowledgement, timeout, retry, sequence
 * numbers -- is the toolbox.
 *
 * What you implement is marked TODO below.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "msg.h"
#include "net.h"

#define MAXPKT 1400

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

static int server(void)
{
	net *nt = net_open(0);
	if (nt == NULL)
		return 1;
	printf("port=%d\n", net_port(nt));
	fflush(stdout);

	long delivered = 0, dups = 0;

	/* TODO (Part 2): receive; deliver each message exactly once, in
	 * order --
	 *      printf("deliver payload=%s\n", payload); fflush(stdout);
	 * -- acknowledge; count (and do not deliver) retransmissions in
	 * dups; leave the loop once the sender has closed the stream and
	 * the close itself has been seen through. */
	for (;;) {
		uint8_t pkt[MAXPKT];
		struct sockaddr_in from;
		long r = net_recv(nt, pkt, sizeof pkt, &from, -1);
		(void)r;
		break;          /* TODO: replace this stub loop */
	}

	printf("server done delivered=%ld dups=%ld\n", delivered, dups);
	net_close(nt);
	return 0;
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
	(void)timeout; (void)retries;

	net *nt = net_open(0);
	if (nt == NULL)
		return 1;
	struct sockaddr_in srv;
	net_target(&srv, port);
	(void)srv;

	long t0 = now_ms();
	long sent = 0, acked = 0, retrans = 0, giveups = 0;
	for (long i = 0; i < n; i++) {
		char payload[32];
		snprintf(payload, sizeof payload, "msg-%04ld", i);
		sent++;
		/* TODO (Part 2): send payload reliably -- transmit, wait
		 * (bounded), retransmit up to `retries` times; count acked /
		 * retrans / giveups. */
	}
	/* TODO (Part 2): close the stream so the server can report and
	 * exit -- reliably, like everything else on this wire. */

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
