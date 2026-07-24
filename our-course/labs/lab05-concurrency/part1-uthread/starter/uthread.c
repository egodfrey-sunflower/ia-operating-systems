/*
 * uthread.c -- a user-level thread package. Lab 5, Part 1.
 *
 * A thread here is exactly three things: a stack, a saved register set, and a
 * link in a queue. There is no kernel involvement anywhere in this file. The
 * process has one kernel thread throughout; what changes is which stack the
 * one kernel thread is standing on.
 *
 * The shape is fixed for you, because it is the shape that makes freeing an
 * exited thread's stack safe: threads switch to the SCHEDULER, never directly
 * to each other. uthread_run() owns the main context, is the only code that
 * takes threads off the queue, and is standing on the main stack when it
 * releases a dead thread's stack.
 *
 * Work through the TODOs in order: the queue, then create, then yield, then
 * the scheduler, then exit. Nothing runs until all five are there, so build
 * them together and expect the first run to crash.
 */

#include "uthread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum tstate {
	T_FREE = 0,     /* slot unused */
	T_RUNNABLE,     /* on the ready queue, or just came off it */
	T_RUNNING,      /* switched to; this is `current` */
	T_ZOMBIE        /* called uthread_exit(); stack awaiting release */
};

struct uthread {
	struct uthread_ctx ctx;
	char *stack;
	void (*fn)(void *);
	void *arg;
	int state;
	int tid;
	struct uthread *next;           /* ready-queue link */
};

static struct uthread table[UTHREAD_MAX];
static struct uthread *qhead, *qtail;
static struct uthread *current;         /* NULL while the scheduler runs */
static struct uthread_ctx sched_ctx;    /* the main context's saved registers */
static int next_tid = 1;

/* ------------------------------------------------------------------ queue */
/*
 * The ready queue is FIFO, and the harness can see that it is: create appends
 * to the tail, yield sends the caller to the tail, the head runs next. Any
 * structure with that behaviour is fine -- a singly linked list through
 * ->next, an array of pointers, a ring buffer.
 *
 * The failure to look out for is the empty queue. Remove the last element and
 * the tail pointer still points at it; append the next thread and it links
 * onto something that is off the queue. Threads then vanish, or the queue
 * becomes a loop and the scheduler never returns.
 */

static void enqueue(struct uthread *t)
{
	(void)t;
	/* TODO: put t at the tail. */
}

static struct uthread *dequeue(void)
{
	/* TODO: take the thread at the head and return it, or NULL if the
	 * queue is empty. */
	return NULL;
}

/* ------------------------------------------------------------------- init */

void uthread_init(void)
{
	memset(table, 0, sizeof table);
	qhead = qtail = NULL;
	current = NULL;
	memset(&sched_ctx, 0, sizeof sched_ctx);
	next_tid = 1;
}

/* ------------------------------------------------------------ thread entry */
/*
 * Where a brand-new thread starts running. It is reached not by a call but by
 * the `ret` at the bottom of uthread_swtch(), which pops this function's
 * address off the new stack -- see uthread_create() for how it gets there.
 *
 * Because it was not called, there is no meaningful return address below it,
 * so it must never return. It does not: if fn returns, it calls uthread_exit()
 * itself, which is what the header promises.
 *
 * Given to you, complete.
 */
static void uthread_entry(void)
{
	struct uthread *t = current;

	t->fn(t->arg);
	uthread_exit();
}

/* ----------------------------------------------------------------- create */

int uthread_create(void (*fn)(void *arg), void *arg)
{
	(void)fn;
	(void)arg;
	(void)next_tid;

	/* TODO: find a slot in `table` whose state is T_FREE. Return -1 if
	 * there is none. */

	/* TODO: give the thread UTHREAD_STACK bytes of stack. malloc is fine;
	 * so is mmap. */

	/*
	 * TODO: build the initial frame, so that the FIRST uthread_swtch()
	 * into this thread arrives at uthread_entry().
	 *
	 * Stacks grow down, so start at the high end of the allocation and
	 * round DOWN to a multiple of 16. Call that `top`. Then:
	 *
	 *   top-16 -> [ &uthread_entry ]   <- ctx.rsp points here
	 *   top-8  -> [ 0             ]    a fake return address, never used
	 *   top    -> (one past the end of the allocation)
	 *
	 * uthread_swtch() loads ctx.rsp and executes `ret`. `ret` pops the top
	 * word into the program counter -- so it "returns" into uthread_entry
	 * -- and leaves %rsp at top-8.
	 *
	 * top-8 is not a typo and not slack. The ABI says a function finds
	 * %rsp eight bytes below a multiple of 16 on entry, because its
	 * caller's `call` pushed a return address there. Give it a multiple of
	 * 16 instead and the thread runs fine until something inside printf
	 * uses an aligned SSE store, and then dies a long way from the
	 * mistake.
	 *
	 * Zero the rest of ctx. Nothing needs a particular value, but a
	 * context full of stack garbage makes the first crash unreadable.
	 */

	/* TODO: fill in fn, arg, tid (>= 1, and not one you have used before
	 * in this run), state, and put the thread on the queue. Return the
	 * tid. */

	return -1;
}

/* ------------------------------------------------------------------ yield */

void uthread_yield(void)
{
	/* TODO: if there is no current thread we are in the main context and
	 * there is nothing to switch to; return.
	 *
	 * Otherwise mark the caller runnable and switch to sched_ctx. The
	 * scheduler puts it back on the queue. This call returns -- later. */
}

/* ------------------------------------------------------------------- exit */

void uthread_exit(void)
{
	/* TODO: mark the caller a zombie and switch to sched_ctx, which will
	 * free its stack.
	 *
	 * This function does not return. Say so loudly if it ever does: at
	 * that point you are running on a stack that has been handed back to
	 * malloc, and the next thing that happens is unpredictable. */
	fprintf(stderr, "uthread: a dead thread was rescheduled\n");
	abort();
}

/* ------------------------------------------------------------------- self */

int uthread_self(void)
{
	/* TODO: the current thread's tid, or 0 in the main context. */
	return 0;
}

/* -------------------------------------------------------------- scheduler */

void uthread_run(void)
{
	/*
	 * TODO: the whole scheduler. Loop:
	 *
	 *   - take the thread at the head of the queue; if there is none,
	 *     return -- every thread has finished;
	 *   - make it current, and switch from sched_ctx into its context;
	 *   - when control comes back, it is because that thread yielded or
	 *     exited. If it is runnable, put it at the tail. If it is a
	 *     zombie, free its stack and mark the slot free.
	 *
	 * Clear `current` as soon as the switch returns: between switches you
	 * are the main context, and uthread_self() has to say 0.
	 */
	(void)sched_ctx;
	(void)uthread_entry;
	(void)enqueue;
	(void)dequeue;
}
