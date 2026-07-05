// rwlock.c — a readers/writers lock with WRITER PREFERENCE, built from a
// single mutex + two condition variables (no pthread_rwlock_t allowed).
//
// INVARIANT: at any instant either (a) any number of readers hold the lock and
// no writer does, or (b) exactly one writer holds it and no readers do. Never
// both. WRITER PREFERENCE: once a writer is waiting, new readers queue behind
// it, so a steady stream of readers cannot starve a writer.
//
// CLASSIC BUG TO AVOID: writer starvation. The naive readers/writers lock lets
// a reader in whenever no writer is *currently* active. Under a continuous
// reader stream there is always a reader active, so a waiting writer never
// runs. The fix: readers must also block while a writer is WAITING
// (waiting_writers > 0), not only while one is active. (Symmetrically, this
// can starve readers if writers stream forever — the accepted tradeoff of
// writer preference, and why the test also checks readers still share.)

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
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ok_to_read = PTHREAD_COND_INITIALIZER;
static pthread_cond_t ok_to_write = PTHREAD_COND_INITIALIZER;
static int active_readers;
static int active_writer;    // 0 or 1
static int waiting_writers;

// Writer-starvation metric, guarded by `mu` (the lock's own mutex) so it is
// sampled atomically with the lock state. `total_admitted` counts every reader
// that passes read_lock; in write_lock we snapshot it the instant the writer
// registers as waiting and again the instant it is granted. The difference is
// exactly the readers let in *while the writer was waiting* — with writer
// preference that is 0 (once waiting_writers>0, read_lock blocks).
static long total_admitted;
static long max_wait_admissions;

static void
read_lock(void)
{
  pthread_mutex_lock(&mu);
  // Writer preference: yield to any active OR waiting writer.
  while (active_writer || waiting_writers > 0)
    pthread_cond_wait(&ok_to_read, &mu);
  active_readers++;
  total_admitted++;
  pthread_mutex_unlock(&mu);
}

static void
read_unlock(void)
{
  pthread_mutex_lock(&mu);
  if (--active_readers == 0 && waiting_writers > 0)
    pthread_cond_signal(&ok_to_write);
  pthread_mutex_unlock(&mu);
}

static void
write_lock(void)
{
  pthread_mutex_lock(&mu);
  waiting_writers++;
  long before = total_admitted;
  while (active_writer || active_readers > 0)
    pthread_cond_wait(&ok_to_write, &mu);
  long during = total_admitted - before;  // admitted WHILE we waited: must be 0
  if (during > max_wait_admissions)
    max_wait_admissions = during;
  waiting_writers--;
  active_writer = 1;
  pthread_mutex_unlock(&mu);
}

static void
write_unlock(void)
{
  pthread_mutex_lock(&mu);
  active_writer = 0;
  // Prefer the next writer; only release readers if no writer is queued.
  if (waiting_writers > 0)
    pthread_cond_signal(&ok_to_write);
  else
    pthread_cond_broadcast(&ok_to_read);
  pthread_mutex_unlock(&mu);
}

// ---- instrumentation ------------------------------------------------------
// `imu` guards ONLY the reader-concurrency bookkeeping, so it does not itself
// serialize readers (which would mask a missing rwlock).
static pthread_mutex_t imu = PTHREAD_MUTEX_INITIALIZER;
static int cur_readers;       // readers currently in the critical section
static int peak_readers;      // max concurrent readers ever observed

// Exclusion probe: writers do a NON-ATOMIC read-modify-write of shared_counter
// under the write lock. If the write lock truly serializes writers, no update
// is lost and the final value is exactly EXPECT_WRITES. A broken lock loses
// updates. protected_value is touched only by writers, guarded only by the
// rwlock — no other mutex — so this genuinely tests the lock, not imu.
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

    for (volatile int s = 0; s < 40; s++) {}  // brief overlap window

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
    // Non-atomic RMW: read, delay, write back. Correct exclusion => no lost
    // update. The wide delay is what a broken lock would race in; kept few in
    // number (WRITE_OPS) so the writer phase ends early and readers then share.
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

  // Bound on writer starvation: readers let in while a writer waited. With
  // writer preference this is ~0; a reader-preference lock shows thousands.
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
