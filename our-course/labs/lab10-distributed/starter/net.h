/* net.h -- Lab 10, GIVEN CODE (complete, do not modify).
 *
 * One UDP endpoint on 127.0.0.1, with a deterministic, seeded loss simulator
 * wired into the send path.  Every tool in this lab talks through this file,
 * and the autograder compiles its OWN copy of it, so the simulator is part of
 * the ground the lab stands on, not something a submission can soften.
 *
 * The simulator: each net endpoint owns one PRNG stream (xorshift32), seeded
 * from $LOSS_SEED (default 1).  Every net_send() consumes exactly one draw;
 * the datagram is silently discarded -- "lost by the network" -- iff
 * draw % 100 < $LOSS_RATE (percent, default 0).  net_send() returns success
 * either way: the sender cannot tell, which is the whole point of ch. 48.
 * Two processes given different seeds therefore drop independently in the two
 * directions, and a run is exactly reproducible from (seed, rate, workload).
 *
 * Set NET_STATS=1 and net_close() reports sent/dropped/received counts on
 * stderr -- the autograder uses this to prove a run really was lossy.
 */
#ifndef NET_H
#define NET_H

#include <netinet/in.h>
#include <stddef.h>

typedef struct net net;

/* Bind a UDP socket to 127.0.0.1:port.  port == 0 asks the kernel for an
 * ephemeral port (read it back with net_port) -- the tools in this lab
 * always do this and print "port=N", so nothing ever squats on a fixed
 * port.  A nonzero port (used only when a server must come back on the
 * address a client already holds) binds with SO_REUSEADDR.
 * Returns NULL on error (message on stderr). */
net *net_open(int port);

/* The locally bound port number. */
int net_port(const net *n);

/* Fill *a with 127.0.0.1:port, ready to pass to net_send. */
void net_target(struct sockaddr_in *a, int port);

/* Send len bytes to *to -- through the loss simulator.  Returns (long)len,
 * whether or not the simulator dropped the datagram (a real network gives
 * no more warning than that), or -1 on a genuine socket error. */
long net_send(net *n, const void *buf, size_t len, const struct sockaddr_in *to);

/* Receive one datagram.  timeout_ms < 0 blocks forever; timeout_ms >= 0
 * waits at most that long.  Returns the byte count (>= 0), -1 if the
 * timeout expired with nothing to read, or -2 on a socket error.  If from
 * is non-NULL it receives the sender's address. */
long net_recv(net *n, void *buf, size_t max, struct sockaddr_in *from,
              int timeout_ms);

/* Close the socket; with NET_STATS=1, report the endpoint's counters on
 * stderr first:  "net: sent=S dropped=D recvd=R". */
void net_close(net *n);

#endif
