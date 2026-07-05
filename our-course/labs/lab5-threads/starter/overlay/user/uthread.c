// User-level cooperative threads (Lab 5, Part A).
//
// A tiny round-robin scheduler that multiplexes several threads onto one
// kernel thread (one process). Threads run until they voluntarily call
// thread_yield(); there is no timer, no preemption. Switching between two
// user threads is just like the kernel's swtch(): save the callee-saved
// registers (plus ra and sp) of the outgoing thread, load those of the
// incoming one, and return.
//
// YOUR JOB (two TODOs below, plus writing user/uthread_switch.S):
//   1. thread_create(): set up the new thread's saved context so that the
//      FIRST time it is switched to, it "returns" into its start function on
//      its own stack.
//   2. thread_schedule(): actually switch from the current thread to the next
//      one by calling thread_switch().
//
// Handout questions to answer: why do we only save the callee-saved
// registers? where does a brand-new thread's first `ret` jump to? what does a
// process (kernel) context switch save that this one does NOT, and why is
// this switch so much cheaper?

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_THREAD 4
#define STACK_SIZE 8192

// Saved registers for a user-thread context switch. This MUST match the
// layout that user/uthread_switch.S saves and restores. Only callee-saved
// registers are here (ra and sp, then s0..s11) -- see the handout question.
struct context {
  uint64 ra;
  uint64 sp;
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

enum tstate { FREE, RUNNING, RUNNABLE };

struct thread {
  struct context ctx;         // saved registers (must be first is not required)
  char stack[STACK_SIZE];     // this thread's private stack
  enum tstate state;          // FREE, RUNNING or RUNNABLE
};

static struct thread all_thread[MAX_THREAD];
static struct thread *current_thread;

// Implemented in user/uthread_switch.S (YOU write it). Saves the current
// registers into *old and loads them from *new, then returns -- so control
// resumes in whatever thread *new describes.
extern void thread_switch(struct context *old, struct context *new);

void
thread_init(void)
{
  // main() is thread 0, which will make the first call to thread_schedule().
  // It needs a stack so that the first thread_switch() has somewhere to save
  // the current registers -- but that already happens on main's real stack,
  // so we just mark thread 0 RUNNING.
  current_thread = &all_thread[0];
  current_thread->state = RUNNING;
}

void
thread_schedule(void)
{
  struct thread *t, *next_thread;

  // Find the next runnable thread, round-robin starting after current.
  next_thread = 0;
  t = current_thread + 1;
  for (int i = 0; i < MAX_THREAD; i++) {
    if (t >= all_thread + MAX_THREAD)
      t = all_thread;
    if (t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {
    // No RUNNABLE thread anywhere. Every worker has finished (state FREE);
    // only main (thread 0, state RUNNING) is left. We are done. (main's very
    // first call also lands here if no threads were ever created.)
    printf("uthread: all threads finished\n");
    exit(0);
  }

  if (next_thread != current_thread) {
    next_thread->state = RUNNING;
    struct thread *prev = current_thread;
    current_thread = next_thread;

    // TODO (2): call thread_switch() so that, afterwards, prev's registers
    // are preserved in its saved context and execution continues wherever
    // next_thread last left off. Until you do this, the scheduler picks a
    // thread but never actually runs it, so `uthread` prints nothing useful
    // and exits.
    (void)prev; // silence unused warning until you add the call
  }
}

void
thread_create(void (*func)(void))
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE)
      break;
  }
  if (t >= all_thread + MAX_THREAD) {
    printf("thread_create: out of threads\n");
    return;
  }

  t->state = RUNNABLE;

  // TODO (1): set up t->ctx so that the first thread_switch() into this
  // thread starts running func() on this thread's own stack (t->stack).
  // Work out what thread_switch() does with a saved context on the way *in*
  // to a thread -- kernel/swtch.S is the model to read first -- and remember
  // which direction a RISC-V stack grows. Deciding exactly what to plant
  // where, and why, is handout written question 2; answer it before coding.
  (void)func;
}

// Yield the CPU to another thread: mark ourselves runnable and schedule.
void
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

// --- the test workload: three threads that count to a target, yielding ---

static int a_n, b_n, c_n; // how many times each thread has run

void
thread_a(void)
{
  printf("thread_a started\n");
  a_n = 0;
  while (a_n < 100) {
    printf("thread_a %d\n", a_n);
    a_n += 1;
    thread_yield();
  }
  printf("thread_a: exit after %d\n", a_n);
  current_thread->state = FREE;
  thread_schedule();
}

void
thread_b(void)
{
  printf("thread_b started\n");
  b_n = 0;
  while (b_n < 100) {
    printf("thread_b %d\n", b_n);
    b_n += 1;
    thread_yield();
  }
  printf("thread_b: exit after %d\n", b_n);
  current_thread->state = FREE;
  thread_schedule();
}

void
thread_c(void)
{
  printf("thread_c started\n");
  c_n = 0;
  while (c_n < 100) {
    printf("thread_c %d\n", c_n);
    c_n += 1;
    thread_yield();
  }
  printf("thread_c: exit after %d\n", c_n);
  current_thread->state = FREE;
  thread_schedule();
}

int
main(void)
{
  thread_init();
  thread_create(thread_a);
  thread_create(thread_b);
  thread_create(thread_c);
  // Hand control to the scheduler. In a correct implementation control never
  // comes back here: thread_schedule() itself exit()s once every thread has
  // finished. (Before you wire up thread_switch it returns immediately, and
  // we just exit.)
  thread_schedule();
  exit(0);
}
