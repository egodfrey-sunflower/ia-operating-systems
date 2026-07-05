// Lab 4 measurement: how long does fork()+wait() take when the parent owns a
// few MB of memory? With eager fork the parent's pages are copied on every
// fork; with copy-on-write fork they are merely shared read-only, so a
// fork-then-exit child costs almost nothing. Run this before and after
// implementing COW and compare the reported ticks.
//
// Usage: forkbench [iterations]   (default 100)

#include "kernel/types.h"
#include "user/user.h"

#define MB (1024 * 1024)

int
main(int argc, char *argv[])
{
  int iters = 100;
  int sz = 4 * MB;

  if (argc > 1)
    iters = atoi(argv[1]);

  // Grow and touch ~4MB so the pages are really mapped in the parent.
  char *p = sbrk(sz);
  if (p == (char *)0xffffffffffffffffL) {
    printf("forkbench: sbrk failed\n");
    exit(1);
  }
  for (char *q = p; q < p + sz; q += 4096)
    *q = 1;

  int start = uptime();
  for (int i = 0; i < iters; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("forkbench: fork failed at iter %d\n", i);
      exit(1);
    }
    if (pid == 0) {
      // Classic fork-then-exit: the child never touches the inherited pages.
      exit(0);
    }
    wait(0);
  }
  int end = uptime();

  printf("forkbench: %d fork+wait of %dMB parent took %d ticks\n", iters,
         sz / MB, end - start);
  exit(0);
}
