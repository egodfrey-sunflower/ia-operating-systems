// barrier.c — a reusable (cyclic) barrier. *** STARTER SKELETON ***
//
// INVARIANT you must enforce: no thread returns from barrier_wait() for round k
// until all T threads have called it for round k. The barrier must be REUSABLE:
// after all threads pass, it re-arms automatically for the next round.
//
// CLASSIC BUG TO AVOID: the reuse race. A barrier that merely counts arrivals
// works for one round and then falls apart: think about what a fast thread can
// do to the barrier's state while slower threads are still waking up from the
// previous round. The stress test runs thousands of rounds to catch exactly
// this.
//
// YOUR JOB: implement barrier_wait() with a mutex + condition variable, adding
// to barrier_t whatever state surviving reuse demands. As shipped,
// barrier_wait() is a no-op, so threads race across round boundaries and the
// stress test reports FAIL.

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define T 8
#define ROUNDS 2000

typedef struct {
  pthread_mutex_t mu;
  pthread_cond_t cv;
  int n;         // threads that must rendezvous
  int count;     // threads arrived in the current round
  // TODO: add whatever further state you need
} barrier_t;

static void
barrier_init(barrier_t *b, int n)
{
  pthread_mutex_init(&b->mu, NULL);
  pthread_cond_init(&b->cv, NULL);
  b->n = n;
  b->count = 0;
}

// Should block until all n threads have called it, then release them all and
// re-arm. Returns 1 to exactly one thread per round (the last arriver), 0 to
// the rest — useful for letting one thread run a serial check.
static int
barrier_wait(barrier_t *b)
{
  // TODO: block until all n threads have arrived, release them all, and
  // re-arm for the next round.
  //
  // BROKEN starter behavior: no rendezvous at all — return immediately.
  (void)b;
  return 0;
}

static barrier_t bar;

// Instrumentation: how many threads have arrived at the current round, and
// which round the forming cohort believes it is in. If the barrier is correct,
// all threads cross a round together and no thread's round ever runs ahead of
// another's.
static int arrived;          // threads arrived at the pre-barrier checkpoint
static int cur_round;        // round the cohort is working on
static pthread_mutex_t imu = PTHREAD_MUTEX_INITIALIZER;
static volatile int failed;

static void *
worker(void *arg)
{
  long id = (long)arg;
  (void)id;
  for (int r = 0; r < ROUNDS; r++) {
    // Checkpoint BEFORE the barrier: count how many are in this round.
    pthread_mutex_lock(&imu);
    if (arrived == 0)
      cur_round = r;           // first arriver names the round
    if (cur_round != r)        // must never be in a different round than cohort
      failed = 1;
    arrived++;
    pthread_mutex_unlock(&imu);

    int last = barrier_wait(&bar);

    // Exactly one thread (the last arriver) resets the checkpoint. Because the
    // barrier guarantees all T have passed, `arrived` must equal T here.
    if (last) {
      pthread_mutex_lock(&imu);
      if (arrived != T)
        failed = 1;
      arrived = 0;
      pthread_mutex_unlock(&imu);
    }
    // Re-synchronize so the reset above is visible before the next round's
    // checkpoint reads `arrived`.
    barrier_wait(&bar);
  }
  return NULL;
}

int
main(void)
{
  pthread_t th[T];
  barrier_init(&bar, T);

  for (long i = 0; i < T; i++)
    assert(pthread_create(&th[i], NULL, worker, (void *)i) == 0);
  for (int i = 0; i < T; i++)
    pthread_join(th[i], NULL);

  if (!failed) {
    printf("barrier: PASS (%d threads x %d rounds, all cohorts aligned)\n",
           T, ROUNDS);
    return 0;
  }
  printf("barrier: FAIL (a thread raced across a round boundary)\n");
  return 1;
}
