/* net.c -- Lab 10, GIVEN CODE (complete, do not modify).  See net.h. */

#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct net {
	int fd;
	int port;
	int rate;               /* $LOSS_RATE, percent 0..99 */
	uint32_t rng;           /* xorshift32 state, from $LOSS_SEED */
	long sent, dropped, recvd;
	int stats;              /* $NET_STATS */
};

static uint32_t xs32(uint32_t *s)
{
	uint32_t x = *s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return *s = x;
}

static long env_long(const char *name, long dflt)
{
	const char *v = getenv(name);
	if (v == NULL || *v == '\0')
		return dflt;
	return strtol(v, NULL, 10);
}

net *net_open(int port)
{
	net *n = calloc(1, sizeof *n);
	if (n == NULL)
		return NULL;

	n->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (n->fd < 0) {
		perror("net: socket");
		free(n);
		return NULL;
	}
	if (port != 0) {
		int one = 1;
		setsockopt(n->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	}

	struct sockaddr_in a;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port = htons((uint16_t)port);
	if (bind(n->fd, (struct sockaddr *)&a, sizeof a) < 0) {
		perror("net: bind");
		close(n->fd);
		free(n);
		return NULL;
	}
	socklen_t alen = sizeof a;
	if (getsockname(n->fd, (struct sockaddr *)&a, &alen) < 0) {
		perror("net: getsockname");
		close(n->fd);
		free(n);
		return NULL;
	}
	n->port = ntohs(a.sin_port);

	long rate = env_long("LOSS_RATE", 0);
	if (rate < 0)
		rate = 0;
	if (rate > 99)
		rate = 99;
	n->rate = (int)rate;

	uint32_t seed = (uint32_t)env_long("LOSS_SEED", 1);
	n->rng = (seed * 2654435761u) ^ 0x9E3779B9u;
	if (n->rng == 0)
		n->rng = 1;

	n->stats = env_long("NET_STATS", 0) != 0;
	return n;
}

int net_port(const net *n)
{
	return n->port;
}

void net_target(struct sockaddr_in *a, int port)
{
	memset(a, 0, sizeof *a);
	a->sin_family = AF_INET;
	a->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a->sin_port = htons((uint16_t)port);
}

long net_send(net *n, const void *buf, size_t len, const struct sockaddr_in *to)
{
	n->sent++;
	/* One draw per send, unconditionally, so the drop pattern at a given
	 * seed does not depend on the rate of any earlier run. */
	uint32_t draw = xs32(&n->rng) % 100;
	if ((int)draw < n->rate) {
		n->dropped++;
		return (long)len;       /* "sent"; the network ate it */
	}
	ssize_t r = sendto(n->fd, buf, len, 0,
	                   (const struct sockaddr *)to, sizeof *to);
	if (r < 0)
		return -1;
	return (long)r;
}

long net_recv(net *n, void *buf, size_t max, struct sockaddr_in *from,
              int timeout_ms)
{
	if (timeout_ms >= 0) {
		struct pollfd p = { n->fd, POLLIN, 0 };
		int r;
		do {
			r = poll(&p, 1, timeout_ms);
		} while (r < 0 && errno == EINTR);
		if (r == 0)
			return -1;      /* timeout */
		if (r < 0)
			return -2;
	}
	socklen_t sl = from ? sizeof *from : 0;
	ssize_t r = recvfrom(n->fd, buf, max, 0,
	                     (struct sockaddr *)from, from ? &sl : NULL);
	if (r < 0)
		return -2;
	n->recvd++;
	return (long)r;
}

void net_close(net *n)
{
	if (n == NULL)
		return;
	if (n->stats)
		fprintf(stderr, "net: sent=%ld dropped=%ld recvd=%ld\n",
		        n->sent, n->dropped, n->recvd);
	close(n->fd);
	free(n);
}
