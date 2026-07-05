// sysinfotest -- exercises the sysinfo() system call (Lab 2, Task 2).
//
// Adapted from MIT 6.1810's sysinfotest for this course's xv6 fork. Prints
// "sysinfotest: OK" on success; on any failure it prints a line beginning
// "sysinfotest: FAIL" and exits non-zero.

#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

void
sinfo(struct sysinfo *info)
{
  if (sysinfo(info) < 0) {
    printf("sysinfotest: FAIL sysinfo() returned an error\n");
    exit(1);
  }
}

// Free memory should drop when we allocate and use pages, and recover when we
// give them back with a negative sbrk.
void
test_freemem(void)
{
  struct sysinfo info;
  uint64 before, after, freed;
  int npages = 64;
  char *p;
  int i;

  sinfo(&info);
  before = info.freemem;

  p = sbrk(npages * PGSIZE);
  if (p == SBRK_ERROR) {
    printf("sysinfotest: FAIL sbrk failed\n");
    exit(1);
  }
  for (i = 0; i < npages; i++)
    p[i * PGSIZE] = 1; // touch each page (works whether sbrk is eager or lazy)

  sinfo(&info);
  after = info.freemem;

  if (after > before || before - after < (uint64)npages * PGSIZE) {
    printf("sysinfotest: FAIL freemem did not drop by at least %d bytes "
           "(before %ld, after %ld)\n",
           npages * PGSIZE, (long)before, (long)after);
    exit(1);
  }

  sbrk(-npages * PGSIZE);
  sinfo(&info);
  freed = info.freemem;
  if (freed <= after) {
    printf("sysinfotest: FAIL freemem did not recover after free "
           "(before-free %ld, after-free %ld)\n",
           (long)after, (long)freed);
    exit(1);
  }

  printf("sysinfotest: freemem OK\n");
}

// nproc should be strictly larger inside a forked child than it was in the
// parent before the fork.
void
test_nproc(void)
{
  struct sysinfo info;
  uint64 before;
  int pid, st;

  sinfo(&info);
  before = info.nproc;

  pid = fork();
  if (pid < 0) {
    printf("sysinfotest: FAIL fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    sinfo(&info);
    if (info.nproc < before + 1) {
      printf("sysinfotest: FAIL nproc is %ld in child, expected at least %ld\n",
             (long)info.nproc, (long)(before + 1));
      exit(1);
    }
    exit(0);
  }

  wait(&st);
  if (st != 0)
    exit(1); // child already printed the reason
  printf("sysinfotest: nproc OK\n");
}

int
main(void)
{
  test_freemem();
  test_nproc();
  printf("sysinfotest: OK\n");
  exit(0);
}
