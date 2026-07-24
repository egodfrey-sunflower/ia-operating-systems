/*
 * cases.c -- the Part 1 test driver.
 *
 *      cases <case-name>
 *
 * Runs exactly one case, in its own process, and exits 0 if it passed, 1 if
 * it failed, 2 if the name is not a case. Complaints go to stderr, four
 * spaces in.
 *
 * Two kinds of case live here. The TRANSCRIPT cases print a line every time
 * something interesting happens and exit 0 regardless; their verdict is a
 * byte-for-byte diff against a file in ../fixtures, done by run.sh. The
 * ASSERTION cases check something a transcript cannot show -- register
 * contents, stack contents, return values -- and decide their own verdict.
 *
 * Cooperative threads are what makes the first kind possible: a fixed pattern
 * of yields against a FIFO ready queue has exactly one legal output, so most
 * of Part 1 can be tested like ordinary code. Enjoy it; Parts 2 and 3 are not
 * like this.
 *
 * Nothing here looks inside your thread package. Every case goes through
 * uthread.h.
 */

#define _POSIX_C_SOURCE 200809L

#include "uthread.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Provided by regprobe.S. Returns a bitmask of callee-saved registers that
 * did not survive a call to uthread_yield(). */
unsigned long uthread_regprobe(void);

static int failures;

static void fail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "    ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	failures++;
}

#define CHECK(cond, ...) do { if (!(cond)) fail(__VA_ARGS__); } while (0)

/* ==================================================================
 * transcript cases
 * ================================================================== */

static void say(void *a)
{
	printf("hello from %s\n", (const char *)a);
}

static void c_smoke(void)
{
	uthread_init();
	printf("before run\n");
	if (uthread_create(say, (void *)"A") < 0)
		fail("uthread_create returned -1 for the first thread");
	uthread_run();
	printf("after run\n");
}

/* Three threads, three rounds each. Round robin over a FIFO queue has one
 * legal answer and this is it. */
static void spin3(void *a)
{
	const char *n = a;
	int i;

	for (i = 0; i < 3; i++) {
		printf("%s%d\n", n, i);
		uthread_yield();
	}
}

static void c_roundrobin(void)
{
	uthread_init();
	uthread_create(spin3, (void *)"A");
	uthread_create(spin3, (void *)"B");
	uthread_create(spin3, (void *)"C");
	uthread_run();
	printf("done\n");
}

/* Uneven lifetimes. A finishes first, then B, leaving C alone in the queue
 * for three more rounds. A queue that loses its last element, or that fails
 * to clear the tail pointer when it empties, diverges here and nowhere
 * else. */
struct uneven { const char *name; int rounds; };

static void uneven(void *a)
{
	struct uneven *u = a;
	int i;

	for (i = 0; i < u->rounds; i++) {
		printf("%s%d\n", u->name, i);
		uthread_yield();
	}
	printf("%s done\n", u->name);
}

static void c_uneven(void)
{
	static struct uneven us[3] = {
		{ "A", 1 }, { "B", 2 }, { "C", 5 }
	};

	uthread_init();
	uthread_create(uneven, &us[0]);
	uthread_create(uneven, &us[1]);
	uthread_create(uneven, &us[2]);
	uthread_run();
	printf("end\n");
}

/* One thread returns from its function; the other calls uthread_exit()
 * explicitly, with unreachable code after it. */
static void by_return(void *a)
{
	const char *n = a;

	printf("%s enter\n", n);
	uthread_yield();
	printf("%s leave\n", n);
}

static void by_exit(void *a)
{
	const char *n = a;

	printf("%s enter\n", n);
	uthread_yield();
	printf("%s exit\n", n);
	uthread_exit();
	printf("%s CAME BACK FROM uthread_exit\n", n);
}

static void c_exitpaths(void)
{
	uthread_init();
	uthread_create(by_return, (void *)"R");
	uthread_create(by_exit, (void *)"X");
	uthread_run();
	printf("end\n");
}

/* A thread creating threads while the scheduler is already running. */
static void child(void *a)
{
	const char *n = a;

	printf("%s0\n", n);
	uthread_yield();
	printf("%s1\n", n);
}

static void parent(void *a)
{
	(void)a;
	printf("A0\n");
	uthread_yield();
	uthread_create(child, (void *)"B");
	uthread_create(child, (void *)"C");
	printf("A1\n");
	uthread_yield();
	printf("A2\n");
}

static void c_nested(void)
{
	uthread_init();
	uthread_create(parent, NULL);
	uthread_run();
	printf("end\n");
}

/* Twelve threads, five rounds. Same property as c_roundrobin at a size where
 * an off-by-one in a wrapping queue shows up. */
static void spin5(void *a)
{
	long id = (long)a;
	int i;

	for (i = 0; i < 5; i++) {
		printf("%ld.%d\n", id, i);
		uthread_yield();
	}
}

static void c_order(void)
{
	long i;

	uthread_init();
	for (i = 0; i < 12; i++)
		uthread_create(spin5, (void *)i);
	uthread_run();
	printf("end\n");
}

/* ==================================================================
 * assertion cases
 * ================================================================== */

/* uthread_self() is the id create() handed back, and stays that way across
 * switches. */
#define NSELF 5
static int self_tid[NSELF];
static int self_seen[NSELF];

static void selfcheck(void *a)
{
	long idx = (long)a;
	int i;

	for (i = 0; i < 4; i++) {
		if (uthread_self() != self_tid[idx]) {
			fail("thread %ld: uthread_self() is %d on round %d, "
			     "but uthread_create returned %d",
			     idx, uthread_self(), i, self_tid[idx]);
			return;
		}
		uthread_yield();
	}
	self_seen[idx] = 1;
}

static void c_self(void)
{
	long i;

	uthread_init();
	CHECK(uthread_self() == 0,
	      "uthread_self() is %d in the main context before uthread_run(); "
	      "the header says 0", uthread_self());
	for (i = 0; i < NSELF; i++) {
		self_tid[i] = uthread_create(selfcheck, (void *)i);
		CHECK(self_tid[i] >= 1,
		      "uthread_create returned %d for thread %ld; ids are >= 1",
		      self_tid[i], i);
	}
	for (i = 1; i < NSELF; i++)
		CHECK(self_tid[i] != self_tid[i - 1],
		      "threads %ld and %ld were both given id %d",
		      i - 1, i, self_tid[i]);
	uthread_run();
	for (i = 0; i < NSELF; i++)
		CHECK(self_seen[i], "thread %ld never finished", i);
	CHECK(uthread_self() == 0,
	      "uthread_self() is %d in the main context after uthread_run()",
	      uthread_self());
}

/* The six callee-saved registers, checked from assembly. */
static const char *const regname[6] = {
	"%rbx", "%rbp", "%r12", "%r13", "%r14", "%r15"
};
static int reg_done;

static void regs(void *a)
{
	long id = (long)a;
	int round;

	for (round = 0; round < 3; round++) {
		unsigned long bad = uthread_regprobe();
		int b;

		for (b = 0; b < 6; b++) {
			if (bad & (1UL << b))
				fail("thread %ld round %d: %s did not survive "
				     "uthread_yield(). uthread_swtch() must "
				     "save it into *save and reload it from "
				     "*load.", id, round, regname[b]);
		}
		if (bad)
			return;
	}
	reg_done++;
}

static void c_regs(void)
{
	long i;

	uthread_init();
	for (i = 0; i < 3; i++)
		uthread_create(regs, (void *)i);
	uthread_run();
	CHECK(reg_done == 3, "%d of 3 threads finished the register probe",
	      reg_done);
}

/* Each thread scribbles a pattern over 2 KiB of its own stack, yields, and
 * checks it is still there. Threads sharing a stack, or a stack pointer that
 * comes back off by a few bytes, corrupt this and nothing else notices. */
#define NSTACK   8
#define PATBYTES 2048
static int stack_done;

static void stacks(void *a)
{
	unsigned char buf[PATBYTES];
	long id = (long)a;
	int round, i;

	for (round = 0; round < 10; round++) {
		for (i = 0; i < PATBYTES; i++)
			buf[i] = (unsigned char)(id * 7 + i + round);
		uthread_yield();
		for (i = 0; i < PATBYTES; i++) {
			if (buf[i] != (unsigned char)(id * 7 + i + round)) {
				fail("thread %ld round %d: stack byte %d is "
				     "0x%02x, was 0x%02x before the yield",
				     id, round, i, buf[i],
				     (unsigned char)(id * 7 + i + round));
				return;
			}
		}
	}
	stack_done++;
}

static void c_stacks(void)
{
	long i;

	uthread_init();
	for (i = 0; i < NSTACK; i++)
		uthread_create(stacks, (void *)i);
	uthread_run();
	CHECK(stack_done == NSTACK, "%d of %d threads kept their stack intact",
	      stack_done, NSTACK);
}

/* Slots come back. Two full batches of UTHREAD_MAX threads, one after the
 * other: a package that never marks a finished thread's slot free runs the
 * first batch and none of the second. */
static int ran_count;

static void tick(void *a)
{
	(void)a;
	uthread_yield();
	ran_count++;
}

static void c_slots(void)
{
	int batch, i, made;

	uthread_init();
	for (batch = 0; batch < 2; batch++) {
		made = 0;
		for (i = 0; i < UTHREAD_MAX; i++) {
			if (uthread_create(tick, NULL) >= 1)
				made++;
		}
		CHECK(made == UTHREAD_MAX,
		      "batch %d: only %d of %d creates succeeded", batch, made,
		      UTHREAD_MAX);
		uthread_run();
	}
	CHECK(ran_count == 2 * UTHREAD_MAX,
	      "%d threads ran; %d were created", ran_count, 2 * UTHREAD_MAX);
}

/* Ids are handed out; they are not recycled. Two full batches of UTHREAD_MAX
 * threads again, but this time the ids create() returned are kept and compared
 * with each other.
 *
 * The two batches are the point. Within one batch, an id that is really the
 * slot index is already distinct, so a single batch proves nothing; it is the
 * second batch, running in the same slots the first batch gave back, that
 * separates "a fresh number every time" from "whatever slot you landed in".
 *
 * Note what is NOT checked: nothing here says the ids are contiguous, or
 * increasing, or unrelated to the slot -- and slots are still expected to come
 * back, which is why both batches have to fill. Only reuse of an id is a
 * failure. */
static int tid_seen[2 * UTHREAD_MAX];

static void c_tids(void)
{
	int batch, i, j, n = 0;

	uthread_init();
	for (batch = 0; batch < 2; batch++) {
		for (i = 0; i < UTHREAD_MAX; i++) {
			int tid = uthread_create(tick, NULL);

			if (tid < 1) {
				fail("batch %d: create %d returned %d. Both batches "
				     "have to fill: a slot is expected to come back "
				     "once the thread in it has finished.",
				     batch, i, tid);
				return;
			}
			tid_seen[n++] = tid;
		}
		uthread_run();
	}
	for (i = 0; i < n; i++) {
		for (j = i + 1; j < n; j++) {
			if (tid_seen[i] == tid_seen[j]) {
				fail("thread %d of %d and thread %d were both "
				     "given id %d. uthread.h says an id is never "
				     "reused within a run, so an id cannot be the "
				     "slot the thread landed in -- slots are "
				     "reused, ids are not. Hand them out of a "
				     "counter that only ever goes up.",
				     i + 1, n, j + 1, tid_seen[i]);
				return;
			}
		}
	}
}

/* Asking for more than the table holds is refused, not fatal. */
static int over_count;

static void tickover(void *a)
{
	(void)a;
	over_count++;
}

static void c_limit(void)
{
	int i, ok = 0, refused = 0, tid;

	uthread_init();
	for (i = 0; i < UTHREAD_MAX + 8; i++) {
		tid = uthread_create(tickover, NULL);
		if (tid >= 1)
			ok++;
		else if (tid == -1)
			refused++;
		else
			fail("uthread_create returned %d, which is neither an "
			     "id (>= 1) nor -1", tid);
	}
	CHECK(ok == UTHREAD_MAX, "%d creates succeeded before the table filled; "
	      "UTHREAD_MAX is %d", ok, UTHREAD_MAX);
	CHECK(refused == 8, "%d creates were refused; 8 were over the limit",
	      refused);
	uthread_run();
	CHECK(over_count == ok, "%d of the %d created threads ran", over_count,
	      ok);
}

/* The main context is not a thread. Yielding and asking who you are from it
 * must be harmless, before the scheduler starts and after it stops. */
static void c_maincontext(void)
{
	uthread_init();
	uthread_yield();
	CHECK(uthread_self() == 0, "uthread_self() is %d in the main context",
	      uthread_self());
	uthread_create(tick, NULL);
	uthread_run();
	uthread_yield();
	CHECK(uthread_self() == 0,
	      "uthread_self() is %d in the main context after uthread_run()",
	      uthread_self());
	CHECK(ran_count == 1, "the one thread created ran %d times", ran_count);
}

/* ================================================================== */

struct testcase {
	const char *name;
	void (*fn)(void);
};

static const struct testcase cases[] = {
	{ "p1_smoke",       c_smoke },
	{ "p1_roundrobin",  c_roundrobin },
	{ "p1_uneven",      c_uneven },
	{ "p1_exitpaths",   c_exitpaths },
	{ "p1_nested",      c_nested },
	{ "p1_order",       c_order },
	{ "p1_self",        c_self },
	{ "p1_regs",        c_regs },
	{ "p1_stacks",      c_stacks },
	{ "p1_slots",       c_slots },
	{ "p1_tids",        c_tids },
	{ "p1_limit",       c_limit },
	{ "p1_maincontext", c_maincontext },
};

int main(int argc, char **argv)
{
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "usage: cases <case-name>\n");
		return 2;
	}
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		if (strcmp(argv[1], cases[i].name) == 0) {
			cases[i].fn();
			fflush(stdout);
			return failures ? 1 : 0;
		}
	}
	fprintf(stderr, "cases: no such case '%s'\n", argv[1]);
	return 2;
}
