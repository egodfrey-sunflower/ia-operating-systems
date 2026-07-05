// bbuffer.c — bounded buffer (producer/consumer). *** STARTER SKELETON ***
//
// INVARIANT you must enforce: the ring holds between 0 and CAP items at all
// times, and every item a producer enqueues is dequeued by exactly one
// consumer (item conservation — nothing lost, nothing duplicated).
//
// CLASSIC BUG TO AVOID: the "lost wakeup". If the test loses items or a
// thread hangs, ask what may have changed between a wakeup and the moment the
// woken thread acts on it.
//
// YOUR JOB: make enqueue() block while the ring is full and dequeue() block
// while it is empty, using ONE mutex and TWO condition variables (not_full,
// not_empty). See the TODOs below. As shipped, the buffer is UNSYNCHRONIZED
// and non-blocking, so the stress test loses items and reports FAIL.

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NPROD 4
#define NCONS 4
#define CAP 16
#define PER_PROD 50000
#define TOTAL_ITEMS ((long)NPROD * PER_PROD)

// Sentinel value pushed to tell a consumer to exit. Real payloads are >= 1.
#define POISON (-1L)

static long ring[CAP];
static int head;  // next slot to dequeue
static int tail;  // next slot to enqueue
static int count; // items currently in ring

// TODO: declare and initialize one mutex and two condition variables here.

// Result accounting, protected by its own mutex.
static pthread_mutex_t rmu = PTHREAD_MUTEX_INITIALIZER;
static long consumed_count;
static long consumed_sum; // sum of payloads, to catch dup/lost/corrupt items

static void
enqueue(long v)
{
  // TODO: block while the ring is full, then insert and wake anyone who may
  //       now proceed.
  // BROKEN starter behavior: no locking, and drop the item if the ring is
  // full instead of blocking. This loses items -> conservation fails.
  if (count == CAP)
    return; // dropped!
  ring[tail] = v;
  tail = (tail + 1) % CAP;
  count++;
}

static long
dequeue(void)
{
  // TODO: block while the ring is empty, then remove and wake anyone who may
  //       now proceed.
  // BROKEN starter behavior: no locking, and if the ring is empty just
  // pretend the stream ended (return POISON) so consumers exit fast.
  if (count == 0)
    return POISON;
  long v = ring[head];
  head = (head + 1) % CAP;
  count--;
  return v;
}

static void *
producer(void *arg)
{
  long id = (long)arg;
  // Payloads are unique across all producers: value = id*PER_PROD + i + 1 is a
  // distinct positive integer per item, so the consumed sum pins down exactly
  // which items were lost or duplicated.
  long base = id * PER_PROD;
  for (long i = 0; i < PER_PROD; i++)
    enqueue(base + i + 1);
  return NULL;
}

static void *
consumer(void *arg)
{
  (void)arg;
  long local_count = 0;
  long local_sum = 0;
  for (;;) {
    long v = dequeue();
    if (v == POISON)
      break;
    local_count++;
    local_sum += v;
  }
  pthread_mutex_lock(&rmu);
  consumed_count += local_count;
  consumed_sum += local_sum;
  pthread_mutex_unlock(&rmu);
  return NULL;
}

int
main(void)
{
  pthread_t prod[NPROD], cons[NCONS];

  for (long i = 0; i < NCONS; i++)
    assert(pthread_create(&cons[i], NULL, consumer, NULL) == 0);
  for (long i = 0; i < NPROD; i++)
    assert(pthread_create(&prod[i], NULL, producer, (void *)i) == 0);

  for (int i = 0; i < NPROD; i++)
    pthread_join(prod[i], NULL);

  // All real items are now enqueued (each producer joined). Push one poison
  // pill per consumer so every consumer eventually exits.
  for (int i = 0; i < NCONS; i++)
    enqueue(POISON);

  for (int i = 0; i < NCONS; i++)
    pthread_join(cons[i], NULL);

  // Expected sum of 1..TOTAL_ITEMS, since payloads are exactly that set.
  long expect_sum = TOTAL_ITEMS * (TOTAL_ITEMS + 1) / 2;
  int ok = (consumed_count == TOTAL_ITEMS) && (consumed_sum == expect_sum) &&
           (count == 0);

  if (ok) {
    printf("bbuffer: PASS (%ld items conserved, ring drained)\n",
           consumed_count);
    return 0;
  }
  printf("bbuffer: FAIL (count=%ld/%ld sum=%ld/%ld residual=%d)\n",
         consumed_count, TOTAL_ITEMS, consumed_sum, expect_sum, count);
  return 1;
}
