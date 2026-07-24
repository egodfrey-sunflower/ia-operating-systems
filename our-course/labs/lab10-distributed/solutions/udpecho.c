/* udpecho.c -- Lab 10 Part 1: a UDP client and server.  REFERENCE.
 *
 * Interface (fixed; the autograder reads these exact lines):
 *
 *   udpecho server
 *       prints "port=N" and echoes checksummed datagrams forever.
 *   udpecho client <port> n=<N> [corrupt=<i>] [timeout=<ms>]
 *       sends N payloads, waits for each echo, and prints
 *       "echo done n=%d ok=%d bad=%d mismatch=%d lost=%d"
 *
 * Packet: u32 checksum | payload bytes.  The server verifies the checksum;
 * a corrupt packet gets the reply payload "BAD" (correctly checksummed)
 * instead of an echo.  corrupt=i makes the client flip one payload byte of
 * message i AFTER computing the checksum -- injected corruption the server
 * must catch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* FNV-1a over the payload: catches any single flipped byte. */
static uint32_t cksum(const uint8_t *b, size_t n)
{
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 16777619u;
	}
	return h;
}

static size_t pack(uint8_t *pkt, const uint8_t *payload, size_t n)
{
	mbuf m;
	mb_winit(&m, pkt, MAXPKT);
	mb_put_u32(&m, cksum(payload, n));
	mb_put_bytes(&m, payload, n);
	return m.len;
}

static int server(void)
{
	net *nt = net_open(0);
	if (nt == NULL)
		return 1;
	printf("port=%d\n", net_port(nt));
	fflush(stdout);

	for (;;) {
		uint8_t pkt[MAXPKT], out[MAXPKT];
		struct sockaddr_in from;
		long r = net_recv(nt, pkt, sizeof pkt, &from, -1);
		if (r < 4)
			continue;
		mbuf m;
		mb_rinit(&m, pkt, (size_t)r);
		uint32_t sum = mb_get_u32(&m);
		const uint8_t *payload = pkt + 4;
		size_t plen = (size_t)r - 4;
		if (sum != cksum(payload, plen)) {
			fprintf(stderr, "server: bad checksum on %zu-byte payload\n",
			        plen);
			size_t olen = pack(out, (const uint8_t *)"BAD", 3);
			net_send(nt, out, olen, &from);
			continue;
		}
		net_send(nt, pkt, (size_t)r, &from);      /* echo, verbatim */
	}
}

static int client(int argc, char **argv)
{
	int port = atoi(argv[2]);
	long n = 8, corrupt = -1, timeout = 1000;
	for (int i = 3; i < argc; i++) {
		long v;
		if (kv(argv[i], "n", &v)) n = v;
		else if (kv(argv[i], "corrupt", &v)) corrupt = v;
		else if (kv(argv[i], "timeout", &v)) timeout = v;
	}

	net *nt = net_open(0);
	if (nt == NULL)
		return 1;
	struct sockaddr_in srv;
	net_target(&srv, port);

	long ok = 0, bad = 0, mismatch = 0, lost = 0;
	for (long i = 0; i < n; i++) {
		char payload[32];
		int plen = snprintf(payload, sizeof payload, "ping-%04ld", i);

		uint8_t pkt[MAXPKT];
		size_t len = pack(pkt, (const uint8_t *)payload, (size_t)plen);
		if (i == corrupt)
			pkt[4 + 1] ^= 0x40;     /* flip a payload byte, after
			                           the checksum was computed */
		net_send(nt, pkt, len, &srv);

		uint8_t rp[MAXPKT];
		long r = net_recv(nt, rp, sizeof rp, NULL, (int)timeout);
		if (r < 4) {
			lost++;
			continue;
		}
		mbuf m;
		mb_rinit(&m, rp, (size_t)r);
		uint32_t sum = mb_get_u32(&m);
		const uint8_t *rpl = rp + 4;
		size_t rlen = (size_t)r - 4;
		if (sum != cksum(rpl, rlen)) {
			mismatch++;             /* reply corrupted in flight */
		} else if (rlen == 3 && memcmp(rpl, "BAD", 3) == 0) {
			bad++;                  /* server rejected our packet */
		} else if (rlen == (size_t)plen &&
		           memcmp(rpl, payload, rlen) == 0) {
			ok++;                   /* echo of the ORIGINAL payload */
		} else {
			mismatch++;
		}
	}
	printf("echo done n=%ld ok=%ld bad=%ld mismatch=%ld lost=%ld\n",
	       n, ok, bad, mismatch, lost);
	net_close(nt);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "server") == 0)
		return server();
	if (argc >= 3 && strcmp(argv[1], "client") == 0)
		return client(argc, argv);
	fprintf(stderr,
	        "usage: udpecho server\n"
	        "       udpecho client <port> [n=..] [corrupt=..] [timeout=..]\n");
	return 2;
}
