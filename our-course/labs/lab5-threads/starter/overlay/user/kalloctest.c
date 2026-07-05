// kalloctest -- Lab 5, Part B, task 1 (per-CPU kmem free lists).
//
// Two checks:
//   test1  Several processes hammer kalloc()/kfree() at the same time (via
//          sbrk up and down). With the stock single kmem lock they serialise
//          and the "kmem" contended-acquire count is huge; with per-CPU free
//          lists it should be tiny. This also checks correctness: if a child
//          cannot get memory (e.g. because your stealing code is broken) its
//          sbrk() fails and the test FAILS.
//   test2  One process grabs ALL free memory, frees it, and grabs it all
//          again. The second grab must recover essentially everything -- which
//          is only possible if free pages are reachable from any CPU, i.e.
//          stealing works.
//
// It prints "kalloctest: OK" iff both pass. The autograder keys off that line.

#include "kernel/types.h"
#include "kernel/lockstat.h"
#include "user/user.h"

#define PGSIZE 4096

// How much contention (contended kmem acquisitions) test1 tolerates. The stock
// single-lock kernel blows way past this under load; a correct per-CPU design
// stays far below it (only occasional cross-CPU steals contend).
#define KMEM_NTS_MAX 500

#define NCHILD 4
#define CHUNK  64   // pages grabbed/released per iteration
#define ITERS  40

static void
churn(void)
{
  for (int j = 0; j < ITERS; j++) {
    char *p = sbrk(CHUNK * PGSIZE);
    if (p == (char *)-1) {
      printf("kalloctest: FAIL -- sbrk(%d pages) failed (out of memory or "
             "broken cross-CPU stealing)\n", CHUNK);
      exit(1);
    }
    // Touch one word per page so the pages are really used.
    for (int k = 0; k < CHUNK; k++)
      p[k * PGSIZE] = (char)k;
    sbrk(-CHUNK * PGSIZE);
  }
}

static int
test1(void)
{
  struct lockstat s0, s1;

  printf("start test1\n");
  if (statistics(&s0) < 0) {
    printf("test1: FAIL -- statistics() syscall not working\n");
    return 1;
  }

  int fail = 0;
  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("test1: FAIL -- fork failed\n");
      return 1;
    }
    if (pid == 0) {
      churn();
      exit(0);
    }
  }
  for (int i = 0; i < NCHILD; i++) {
    int st = -1;
    wait(&st);
    if (st != 0)
      fail = 1;
  }

  statistics(&s1);
  uint64 dn = s1.kmem_n - s0.kmem_n;
  uint64 dnts = s1.kmem_nts - s0.kmem_nts;
  printf("test1: kmem #acquire = %d, #test-and-set (contended) = %d\n",
         (int)dn, (int)dnts);

  if (fail) {
    printf("test1: FAIL -- a child could not allocate memory\n");
    return 1;
  }
  if (dnts > KMEM_NTS_MAX) {
    printf("test1: FAIL -- kmem still heavily contended (%d > %d)\n",
           (int)dnts, KMEM_NTS_MAX);
    return 1;
  }
  printf("test1: OK\n");
  return 0;
}

// Grab every free page (until sbrk fails), then release it all. Returns how
// many pages were grabbed.
static int
grab_all(void)
{
  int n = 0;
  while (sbrk(PGSIZE) != (char *)-1)
    n++;
  if (n > 0)
    sbrk(-(n * PGSIZE));
  return n;
}

static int
test2(void)
{
  printf("start test2\n");
  int n1 = grab_all();
  int n2 = grab_all();
  printf("test2: first grab = %d pages, second grab = %d pages\n", n1, n2);
  if (n1 < 1000) {
    printf("test2: FAIL -- could not allocate much memory (%d pages)\n", n1);
    return 1;
  }
  if (n2 < n1 - n1 / 10) {
    printf("test2: FAIL -- memory not fully recovered (%d < %d)\n", n2, n1);
    return 1;
  }
  printf("test2: OK\n");
  return 0;
}

int
main(void)
{
  int bad = 0;
  bad |= test1();
  bad |= test2();
  if (bad) {
    printf("kalloctest: FAILED\n");
    exit(1);
  }
  printf("kalloctest: OK\n");
  exit(0);
}
