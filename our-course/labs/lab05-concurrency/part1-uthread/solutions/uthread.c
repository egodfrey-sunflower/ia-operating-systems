/*
 * uthread.c -- reference user-level thread package, Lab 5 Part 1.
 *
 * A thread here is exactly three things: a stack, a saved register set, and a
 * link in a queue. There is no kernel involvement anywhere in this file. The
 * process has one kernel thread throughout; what changes is which stack the
 * one kernel thread is standing on.
 *
 * Shape: threads switch to the SCHEDULER, never directly to each other.
 * uthread_run() owns the main context and is the only place that touches the
 * queue's head, which makes "who is on the ready queue" easy to reason about
 * and makes freeing an exited thread's stack safe -- the scheduler is not
 * standing on it.
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
	struct uthread_ctx ctx;         /* must be first; nothing depends on it,
	                                 * but it is the field the switch writes */
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

static void enqueue(struct uthread *t)
{
	t->next = NULL;
	if (qtail)
		qtail->next = t;
	else
		qhead = t;
	qtail = t;
}

static struct uthread *dequeue(void)
{
	struct uthread *t = qhead;

	if (!t)
		return NULL;
	qhead = t->next;
	if (!qhead)
		qtail = NULL;
	t->next = NULL;
	return t;
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
 * address off the new stack -- see uthread_create() for how it got there.
 *
 * Because it was not called, there is no return address below it that means
 * anything, so it must never return. It does not: if fn returns, we call
 * uthread_exit() ourselves, which is what the header promises.
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
	struct uthread *t = NULL;
	unsigned long top;
	unsigned long *sp;
	int i;

	for (i = 0; i < UTHREAD_MAX; i++) {
		if (table[i].state == T_FREE) {
			t = &table[i];
			break;
		}
	}
	if (!t)
		return -1;

	t->stack = malloc(UTHREAD_STACK);
	if (!t->stack)
		return -1;

	/*
	 * The initial frame. Stacks grow down, so start at the high end and
	 * round DOWN to a multiple of 16.
	 *
	 *   top-16 -> [ &uthread_entry ]   <- ctx.rsp points here
	 *   top-8  -> [ 0             ]    fake return address, never used
	 *   top    -> (one past the end of the stack)
	 *
	 * uthread_swtch() loads ctx.rsp and executes `ret`, which pops
	 * &uthread_entry into the program counter and leaves %rsp at top-8.
	 * That is not an accident: the ABI says a function finds %rsp eight
	 * bytes below a multiple of 16 on entry, because its caller's `call`
	 * pushed a return address. Getting this wrong gives you a thread that
	 * runs fine until something inside printf uses an aligned SSE store,
	 * and then dies a long way from the mistake.
	 */
	top = ((unsigned long)t->stack + UTHREAD_STACK) & ~15UL;
	sp = (unsigned long *)(top - 16);
	sp[0] = (unsigned long)uthread_entry;
	sp[1] = 0;

	memset(&t->ctx, 0, sizeof t->ctx);
	t->ctx.rsp = (unsigned long)sp;

	t->fn = fn;
	t->arg = arg;
	t->tid = next_tid++;
	t->state = T_RUNNABLE;
	enqueue(t);
	return t->tid;
}

/* ------------------------------------------------------------------ yield */

void uthread_yield(void)
{
	struct uthread *t = current;

	if (!t)                 /* main context: there is nothing to switch to */
		return;

	t->state = T_RUNNABLE;
	uthread_swtch(&t->ctx, &sched_ctx);
}

/* ------------------------------------------------------------------- exit */

void uthread_exit(void)
{
	struct uthread *t = current;

	if (!t)
		return;

	t->state = T_ZOMBIE;
	uthread_swtch(&t->ctx, &sched_ctx);

	/* The scheduler never switches back into a zombie, so control does not
	 * reach here. If it does, the thread table is corrupt and carrying on
	 * would run off the end of a freed stack. */
	fprintf(stderr, "uthread: a dead thread was rescheduled\n");
	abort();
}

/* ------------------------------------------------------------------- self */

int uthread_self(void)
{
	return current ? current->tid : 0;
}

/* -------------------------------------------------------------- scheduler */

void uthread_run(void)
{
	for (;;) {
		struct uthread *t = dequeue();

		if (!t)
			return;

		t->state = T_RUNNING;
		current = t;
		uthread_swtch(&sched_ctx, &t->ctx);
		current = NULL;

		/* Back on the main stack. The thread either yielded, in which
		 * case it goes to the tail, or exited, in which case its stack
		 * can be released -- safely, because we are not on it. */
		if (t->state == T_RUNNABLE) {
			enqueue(t);
		} else {
			free(t->stack);
			t->stack = NULL;
			t->state = T_FREE;
		}
	}
}
