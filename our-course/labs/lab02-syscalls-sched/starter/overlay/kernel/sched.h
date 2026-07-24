// Scheduler policy selection and constants.
//
// GIVEN. You do not need to change anything in this file; the tests and the
// reference solution both assume these exact numbers.
//
// The policy is chosen at COMPILE TIME, not at run time:
//
//     make POLICY=rr        round robin, the stock xv6 scheduler
//     make POLICY=lottery   Part 4
//     make POLICY=mlfq      Part 5
//
// The Makefile turns that into -DSCHED_POLICY=SCHED_LOTTERY (or whichever),
// so in kernel/proc.c you select between the three with #if.
//
// A CFLAGS change does not make xv6's Makefile rebuild anything. Always
// `make clean` before switching policy. tests/run.sh does.

#ifndef SCHED_H
#define SCHED_H

#define SCHED_RR       0
#define SCHED_LOTTERY  1
#define SCHED_MLFQ     2

#ifndef SCHED_POLICY
#define SCHED_POLICY SCHED_RR
#endif

#if SCHED_POLICY == SCHED_LOTTERY
#define SCHED_POLICY_NAME "lottery"
#elif SCHED_POLICY == SCHED_MLFQ
#define SCHED_POLICY_NAME "mlfq"
#elif SCHED_POLICY == SCHED_RR
#define SCHED_POLICY_NAME "rr"
#else
#error "SCHED_POLICY must be SCHED_RR, SCHED_LOTTERY or SCHED_MLFQ"
#endif

// ---- Part 4: lottery ------------------------------------------------------

// Every process starts with this many tickets, including the first user
// process and every child of a fork that never calls settickets().
#define DEFAULT_TICKETS 10

// settickets(n) must reject anything outside [1, MAX_TICKETS] with -1.
#define MAX_TICKETS     100000

// A pseudo-random number generator, since xv6 has none and writing one is
// not the exercise. Defined in kernel/rand.c. Returns a full 64-bit value;
// take it modulo the total ticket count.
uint64 rand(void);

// ---- Part 5: MLFQ ---------------------------------------------------------

// Number of priority levels. Level 0 is the highest priority (shortest
// allotment); level NPRIO-1 is the lowest.
#define NPRIO        4

// Timer ticks between priority boosts (Rule 5).
#define MLFQ_BOOST   40

// Allotment, in timer ticks, that a process at level `prio` may consume
// before it is demoted one level: 1, 2, 4, 8.
#define MLFQ_ALLOTMENT(prio)  (1 << (prio))

#endif // SCHED_H
