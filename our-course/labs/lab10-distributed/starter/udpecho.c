/* udpecho.c -- Lab 10 Part 1: a UDP client and server.  STARTER.
 *
 * Interface (fixed -- the autograder reads these exact lines):
 *
 *   udpecho server
 *       prints "port=N" and echoes checksummed datagrams forever.
 *   udpecho client <port> n=<N> [corrupt=<i>] [timeout=<ms>]
 *       sends N payloads, waits for each echo, and prints
 *       "echo done n=%d ok=%d bad=%d mismatch=%d lost=%d"
 *
 * Packet layout: u32 checksum | payload bytes.
 *
 * The contract (README.md, Part 1):
 *   - payload of message i is "ping-%04d" (i from 0);
 *   - the server verifies the checksum of everything it receives; a
 *     packet that fails gets the reply payload "BAD" (itself correctly
 *     checksummed) instead of an echo;
 *   - corrupt=i makes the client flip one payload byte of message i
 *     AFTER computing the checksum (pkt[5] ^= 0x40) -- corruption you
 *     inject, which the server must catch;
 *   - the client classifies each reply against the ORIGINAL payload it
 *     sent: an exact echo -> ok, a "BAD" reply -> bad, anything else
 *     -> mismatch; no reply within the timeout -> lost;
 *   - the client must terminate and print its summary at any loss rate.
 *
 * What you implement is marked TODO below.  net.h/msg.h are the given
 * plumbing; see README.md, Part 1.
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

/* TODO (Part 1): a 32-bit checksum over the payload.  The algorithm is
 * yours, but it must catch any single flipped byte. */
static uint32_t cksum(const uint8_t *b, size_t n)
{
	(void)b; (void)n;
	return 0;       /* TODO */
}

/* Build checksum|payload into pkt; returns the packet length. */
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
		uint8_t pkt[MAXPKT];
		struct sockaddr_in from;
		long r = net_recv(nt, pkt, sizeof pkt, &from, -1);
		if (r < 4)
			continue;
		(void)pack;
		/* TODO (Part 1): verify the checksum; echo the packet back
		 * verbatim if it is good, reply "BAD" (via pack) if not. */
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
		(void)plen; (void)corrupt; (void)timeout; (void)srv;

		/* TODO (Part 1): pack and send the payload (flip pkt[5] if
		 * i == corrupt), wait for the reply -- with a bound -- and
		 * classify it: ok / bad / mismatch / lost. */
		lost++;         /* TODO: replace */
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
