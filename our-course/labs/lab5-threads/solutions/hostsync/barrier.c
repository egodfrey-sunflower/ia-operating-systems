// barrier.c — a reusable (cyclic) barrier from one mutex + one condvar.
//
// INVARIANT: no thread returns from barrier_wait() for round k until all T
// threads have called it for round k. The barrier is REUSABLE: after all
// threads pass, it re-arms automatically for the next round.
//
// CLASSIC BUG TO AVOID: "generation reuse" / the wake-up race. If you only
// track a count and wait `while (count < T)`, the last arriver resets count to
// 0 to re-arm — but a thread that was just woken may see count==0 and block
// again, deadlocking, or a fast thread may lap the others into the next round
// and be miscounted. The fix is a GENERATION counter: each waiter records the
// current generation and waits `while (gen == my_gen)`. The last arriver bumps
// the generation (and resets count) and broadcasts. Waiting on the generation,
// not the count, makes "have we all passed?" immune to the count being reused.

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
  int count;     // threads arrived in the current generation
  uint64_t gen;  // bumped each time the barrier trips
} barrier_t;

static void
barrier_init(barrier_t *b, int n)
{
  pthread_mutex_init(&b->mu, NULL);
  pthread_cond_init(&b->cv, NULL);
  b->n = n;
  b->count = 0;
  b->gen = 0;
}

// Returns 1 to exactly one thread per round (the last arriver), 0 otherwise —
// handy for letting one thread run the serial invariant check.
static int
barrier_wait(barrier_t *b)
{
  pthread_mutex_lock(&b->mu);
  uint64_t my_gen = b->gen;
  if (++b->count == b->n) {
    // Last arriver: trip the barrier and re-arm for the next round.
    b->count = 0;
    b->gen++;
    pthread_cond_broadcast(&b->cv);
    pthread_mutex_unlock(&b->mu);
    return 1;
  }
  while (my_gen == b->gen)          // WAIT ON THE GENERATION, not the count.
    pthread_cond_wait(&b->cv, &b->mu);
  pthread_mutex_unlock(&b->mu);
  return 0;
}

static barrier_t bar;

// Instrumentation: how many threads have arrived at the CURRENT round, and
// which round each thread believes it is in. If the barrier is correct, all
// threads are in the same round when they cross, and no thread's round number
// ever runs more than 1 ahead of another's.
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
    // A thread must never be in a different round than the forming cohort.
    if (cur_round != r)
      failed = 1;
    arrived++;
    pthread_mutex_unlock(&imu);

    int last = barrier_wait(&bar);

    // Exactly one thread (the last arriver) resets the checkpoint. Because
    // the barrier guarantees all T have passed, `arrived` must equal T here.
    if (last) {
      pthread_mutex_lock(&imu);
      if (arrived != T)
        failed = 1;
      arrived = 0;
      pthread_mutex_unlock(&imu);
    }
    // Re-synchronize so the reset above is visible before anyone re-checks
    // `arrived` for the next round.
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
