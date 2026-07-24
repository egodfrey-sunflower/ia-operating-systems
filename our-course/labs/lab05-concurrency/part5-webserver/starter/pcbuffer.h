/*
 * pcbuffer.h -- the contract for Lab 5 Part 3.
 *
 * A bounded producer/consumer buffer of ints, with condition variables. The
 * harness includes THIS header and links against YOUR pcbuffer.c, so nothing
 * declared here may change.
 *
 * The struct is spelled out because the harness has to be able to declare
 * one. It does not read any field: every case goes through the four
 * functions, and what it checks is behaviour -- that every item put comes out
 * exactly once, in order, that a full buffer blocks a producer, that an empty
 * one blocks a consumer, and that a long run does not stall.
 *
 * TWO RULES THE HARNESS ENFORCES THAT POSIX DOES NOT.
 *
 * 1. Signal while you still hold the mutex. POSIX permits releasing the
 *    mutex first, and there are arguments for it, but this lab requires the
 *    signal to be inside the critical section: it is the form OSTEP ch. 30
 *    uses throughout, it removes a whole class of "who wakes whom" reasoning
 *    while you are learning the rest, and helgrind reports the other form.
 *
 * 2. Wait in a `while` loop, never an `if`. That one is not a house rule --
 *    it is Mesa semantics, and Part 3 asks you to break it deliberately
 *    afterwards and watch what happens.
 *
 * Two condition variables, not one. `fill` is where consumers wait for
 * something to consume; `empty` is where producers wait for room. Part 3
 * asks you to collapse them into one afterwards and watch what happens, but
 * the version that is graded has two.
 */
#ifndef PCBUFFER_H
#define PCBUFFER_H

#include <pthread.h>

/* The largest capacity pcb_init() will be asked for. */
#define PCB_MAX_CAPACITY 1024

struct pcbuffer {
	int slots[PCB_MAX_CAPACITY];
	int capacity;           /* as passed to pcb_init */
	int count;              /* items currently in the buffer */
	int head;               /* next slot to take from */
	int tail;               /* next slot to put into */
	pthread_mutex_t lock;
	pthread_cond_t fill;    /* consumers wait here for count > 0 */
	pthread_cond_t empty;   /* producers wait here for count < capacity */
};

typedef struct pcbuffer pcbuffer;

/*
 * Prepare an empty buffer of the given capacity, which is between 1 and
 * PCB_MAX_CAPACITY. Not thread-safe: call it before any thread touches the
 * buffer.
 */
void pcb_init(pcbuffer *b, int capacity);

/*
 * Add an item. If the buffer is full, block until there is room. Items come
 * out of pcb_get in the order they went in.
 */
void pcb_put(pcbuffer *b, int item);

/*
 * Remove and return the oldest item. If the buffer is empty, block until
 * there is one.
 */
int pcb_get(pcbuffer *b);

/*
 * Release the mutex and the condition variables. Not thread-safe: call it
 * after every thread has finished with the buffer.
 */
void pcb_destroy(pcbuffer *b);

#endif /* PCBUFFER_H */
