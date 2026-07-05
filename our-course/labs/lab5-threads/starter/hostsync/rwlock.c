// rwlock.c — readers/writers lock with WRITER PREFERENCE. *** STARTER SKELETON ***
//
// INVARIANT you must enforce: at any instant either (a) any number of readers
// hold the lock and no writer does, or (b) exactly one writer holds it and no
// readers do — never both. WRITER PREFERENCE: once a writer is waiting, new
// readers queue behind it, so a steady stream of readers cannot starve a
// writer.
//
// CLASSIC BUG TO AVOID: writer starvation. The naive lock admits a reader
// whenever no writer is *currently active*. Under a continuous reader stream
// there is always a reader active, so a waiting writer never runs. Writer
// preference is the cure. (Its cost: writers streaming forever would starve
// readers — the accepted tradeoff; the test also checks readers still share.)
//
// YOUR JOB: build read_lock/read_unlock/write_lock/write_unlock from a mutex
// and condition variables, enforcing the invariant and the preference rule.
// As shipped, the lock provides NO mutual exclusion, so concurrent writers
// lose updates to the shared counter and the test FAILs.

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NREADERS 8
#define NWRITERS 2
#define READ_OPS 40000  // per reader thread
#define WRITE_OPS 500   // per writer thread (few ops, long critical section)
#define EXPECT_WRITES ((long)NWRITERS * WRITE_OPS)

// ---- the lock -------------------------------------------------------------
// TODO: add a mutex and your condition variables, and guard all of the state
// below with the mutex.
static int active_readers;
static int active_writer;    // 0 or 1
static int waiting_writers;

// Writer-starvation metric — part of the test contract, so maintain it exactly
// (guard with the lock's mutex once you add one): read_lock() increments
// total_admitted once per admitted reader; write_lock() samples total_admitted
// when it starts waiting and again when it is admitted, keeping the largest
// difference seen in max_wait_admissions (readers admitted while it waited).
static long total_admitted;
static long max_wait_admissions;

static void
read_lock(void)
{
  // TODO: admit this reader only when the invariant AND the preference rule
  //       allow it; count the admission in total_admitted.
  // BROKEN starter behavior: no exclusion, just bump the counters racily.
  active_readers++;
  total_admitted++;
}

static void
read_unlock(void)
{
  // TODO: retire this reader and wake whoever may now proceed.
  active_readers--;
}

static void
write_lock(void)
{
  // TODO: admit this writer only when it can be exclusive; maintain
  //       waiting_writers (the preference rule depends on it) and the
  //       max_wait_admissions metric described above.
  // BROKEN starter behavior: no exclusion, just claim the writer flag.
  // (waiting_writers/max_wait_admissions are unused until you implement this.)
  (void)waiting_writers;
  (void)max_wait_admissions;
  active_writer = 1;
}

static void
write_unlock(void)
{
  // TODO: retire the writer and wake whoever may now proceed — think about
  //       who, and how many.
  active_writer = 0;
}

// ---- instrumentation ------------------------------------------------------
static pthread_mutex_t imu = PTHREAD_MUTEX_INITIALIZER;
static int cur_readers;       // readers currently in the critical section
static int peak_readers;      // max concurrent readers ever observed

// Exclusion probe: writers do a NON-ATOMIC read-modify-write of shared_counter
// under the write lock. With real exclusion no update is lost (final value ==
// EXPECT_WRITES); the broken starter lock loses updates.
static long shared_counter;

static void *
reader(void *arg)
{
  (void)arg;
  for (int i = 0; i < READ_OPS; i++) {
    read_lock();
    pthread_mutex_lock(&imu);
    cur_readers++;
    if (cur_readers > peak_readers)
      peak_readers = cur_readers;
    pthread_mutex_unlock(&imu);

    for (volatile int s = 0; s < 40; s++) {}

    pthread_mutex_lock(&imu);
    cur_readers--;
    pthread_mutex_unlock(&imu);
    read_unlock();
  }
  return NULL;
}

static void *
writer(void *arg)
{
  (void)arg;
  for (int i = 0; i < WRITE_OPS; i++) {
    write_lock();
    // Non-atomic RMW under the (missing) write lock. The wide window means two
    // writers routinely read the same value and one increment is lost.
    long v = shared_counter;
    for (volatile int s = 0; s < 20000; s++) {}
    shared_counter = v + 1;
    write_unlock();
  }
  return NULL;
}

int
main(void)
{
  pthread_t rd[NREADERS], wr[NWRITERS];

  for (long i = 0; i < NWRITERS; i++)
    assert(pthread_create(&wr[i], NULL, writer, (void *)i) == 0);
  for (long i = 0; i < NREADERS; i++)
    assert(pthread_create(&rd[i], NULL, reader, (void *)i) == 0);

  for (int i = 0; i < NREADERS; i++)
    pthread_join(rd[i], NULL);
  for (int i = 0; i < NWRITERS; i++)
    pthread_join(wr[i], NULL);

  long starvation_bound = NREADERS;

  int shared_ok = (peak_readers > 1);                 // readers really share
  int excl_ok = (shared_counter == EXPECT_WRITES);    // writers were exclusive
  int bounded = (max_wait_admissions <= starvation_bound);  // writer not starved

  if (shared_ok && excl_ok && bounded) {
    printf("rwlock: PASS (peak_readers=%d, writes=%ld/%ld, max readers "
           "admitted during a writer wait=%ld, bound=%ld)\n",
           peak_readers, shared_counter, EXPECT_WRITES, max_wait_admissions,
           starvation_bound);
    return 0;
  }
  printf("rwlock: FAIL (peak_readers=%d shared_ok=%d excl_ok=%d[%ld/%ld] "
         "max_wait=%ld bound=%ld)\n",
         peak_readers, shared_ok, excl_ok, shared_counter, EXPECT_WRITES,
         max_wait_admissions, starvation_bound);
  return 1;
}
