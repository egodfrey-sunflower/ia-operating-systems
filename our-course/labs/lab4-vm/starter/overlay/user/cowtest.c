// Lab 4 Task 3: copy-on-write fork tests.
// Adapted from MIT 6.1810's cowtest.c for this xv6 fork (sbrk() here is
// eager by default, so the pages really are mapped before the fork).

#include "kernel/types.h"
#include "kernel/memlayout.h"
#include "user/user.h"

// Allocate more than half of physical memory, fill it, then fork. Without
// copy-on-write the fork would need a second copy and run out of memory.
void
simpletest()
{
  uint64 phys_size = PHYSTOP - KERNBASE;
  int sz = (phys_size / 3) * 2;

  printf("simple: ");

  char *p = sbrk(sz);
  if (p == (char *)0xffffffffffffffffL) {
    printf("sbrk(%d) failed\n", sz);
    exit(-1);
  }

  for (char *q = p; q < p + sz; q += 4096) {
    *(int *)q = getpid();
  }

  int pid = fork();
  if (pid < 0) {
    printf("fork() failed\n");
    exit(-1);
  }

  if (pid == 0)
    exit(0);

  wait(0);

  if (sbrk(-sz) == (char *)0xffffffffffffffffL) {
    printf("sbrk(-%d) failed\n", sz);
    exit(-1);
  }

  printf("ok\n");
}

// Three processes all write copy-on-write memory. This allocates more than
// half of physical memory, so it also checks that copied pages are freed.
void
threetest()
{
  uint64 phys_size = PHYSTOP - KERNBASE;
  int sz = phys_size / 4;
  int pid1, pid2;

  printf("three: ");

  char *p = sbrk(sz);
  if (p == (char *)0xffffffffffffffffL) {
    printf("sbrk(%d) failed\n", sz);
    exit(-1);
  }

  pid1 = fork();
  if (pid1 < 0) {
    printf("fork failed\n");
    exit(-1);
  }
  if (pid1 == 0) {
    pid2 = fork();
    if (pid2 < 0) {
      printf("fork failed\n");
      exit(-1);
    }
    if (pid2 == 0) {
      for (char *q = p; q < p + (sz / 5) * 4; q += 4096) {
        *(int *)q = getpid();
      }
      for (char *q = p; q < p + (sz / 5) * 4; q += 4096) {
        if (*(int *)q != getpid()) {
          printf("wrong content\n");
          exit(-1);
        }
      }
      exit(-1);
    }
    for (char *q = p; q < p + (sz / 2); q += 4096) {
      *(int *)q = 9999;
    }
    exit(0);
  }

  for (char *q = p; q < p + sz; q += 4096) {
    *(int *)q = getpid();
  }

  wait(0);

  pause(1);

  for (char *q = p; q < p + sz; q += 4096) {
    if (*(int *)q != getpid()) {
      printf("wrong content\n");
      exit(-1);
    }
  }

  if (sbrk(-sz) == (char *)0xffffffffffffffffL) {
    printf("sbrk(-%d) failed\n", sz);
    exit(-1);
  }

  printf("ok\n");
}

char junk1[4096];
int fds[2];
char junk2[4096];
char buf[4096];
char junk3[4096];

// Test that copyout() breaks COW: the kernel writing into a child's COW page
// (via read() into buf) must not disturb the parent's copy of buf.
void
filetest()
{
  printf("file: ");

  buf[0] = 99;

  for (int i = 0; i < 4; i++) {
    if (pipe(fds) != 0) {
      printf("pipe() failed\n");
      exit(-1);
    }
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(-1);
    }
    if (pid == 0) {
      pause(1);
      if (read(fds[0], buf, sizeof(i)) != sizeof(i)) {
        printf("error: read failed\n");
        exit(1);
      }
      pause(1);
      int j = *(int *)buf;
      if (j != i) {
        printf("error: read the wrong value\n");
        exit(1);
      }
      exit(0);
    }
    if (write(fds[1], &i, sizeof(i)) != sizeof(i)) {
      printf("error: write failed\n");
      exit(-1);
    }
  }

  int xstatus = 0;
  for (int i = 0; i < 4; i++) {
    wait(&xstatus);
    if (xstatus != 0)
      exit(1);
  }

  if (buf[0] != 99) {
    printf("error: child overwrote parent\n");
    exit(1);
  }

  printf("ok\n");
}

int
main(int argc, char *argv[])
{
  simpletest();

  // check that simpletest() freed its physical memory.
  threetest();
  threetest();
  threetest();

  filetest();

  printf("ALL COW TESTS PASSED\n");

  exit(0);
}
