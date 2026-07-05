// bbuffer.c — bounded buffer (producer/consumer) with a mutex + two condvars.
//
// INVARIANT: the ring holds between 0 and CAP items at all times; every item a
// producer enqueues is dequeued by exactly one consumer (item conservation).
//
// CLASSIC BUG TO AVOID: the "lost wakeup". You MUST re-check the wait predicate
// in a `while` loop, never an `if`. pthread_cond_wait can return without the
// condition being true (another thread may have raced in and re-filled/emptied
// the buffer, or a spurious wakeup occurred). Waiting with `if` lets a thread
// proceed on a false premise — writing to a full buffer or reading an empty one.
//
// Design: one mutex protects the ring; not_full wakes blocked producers,
// not_empty wakes blocked consumers. Consumers shut down via a poison-pill
// sentinel (one per consumer) pushed after all producers finish.

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

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

// Result accounting, protected by its own mutex.
static pthread_mutex_t rmu = PTHREAD_MUTEX_INITIALIZER;
static long consumed_count;
static long consumed_sum; // sum of payloads, to catch dup/lost/corrupt items

static void
enqueue(long v)
{
  pthread_mutex_lock(&mu);
  while (count == CAP)               // WHILE, not IF: recheck after wakeup.
    pthread_cond_wait(&not_full, &mu);
  ring[tail] = v;
  tail = (tail + 1) % CAP;
  count++;
  pthread_cond_signal(&not_empty);
  pthread_mutex_unlock(&mu);
}

static long
dequeue(void)
{
  pthread_mutex_lock(&mu);
  while (count == 0)                 // WHILE, not IF.
    pthread_cond_wait(&not_empty, &mu);
  long v = ring[head];
  head = (head + 1) % CAP;
  count--;
  pthread_cond_signal(&not_full);
  pthread_mutex_unlock(&mu);
  return v;
}

static void *
producer(void *arg)
{
  long id = (long)arg;
  // Payloads are unique across all producers: id in [0,NPROD), so
  // value = id*PER_PROD + i + 1 is a distinct positive integer per item.
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
