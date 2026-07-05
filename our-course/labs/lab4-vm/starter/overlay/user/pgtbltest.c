// Lab 4 page-table tests: vmprintme() and ugetpid().
// Adapted from MIT 6.1810's pgtbltest.c for this xv6 fork.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void print_pgtbl_test();
void ugetpid_test();

char *testname = "???";

void
err(char *why)
{
  printf("pgtbltest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

// Task 1: ask the kernel to dump this process's page table. There is nothing
// to assert in user space -- the grader inspects the printed tree -- so this
// just triggers the dump and reports that it ran.
void
print_pgtbl_test()
{
  testname = "print_pgtbl";
  printf("print_pgtbl starting\n");
  vmprintme();
  printf("print_pgtbl: OK\n");
}

// Task 2: in every child, the pid read from the USYSCALL page (ugetpid())
// must match the pid returned by the getpid() system call.
void
ugetpid_test()
{
  int i;

  testname = "ugetpid_test";
  printf("ugetpid_test starting\n");

  for (i = 0; i < 64; i++) {
    int ret = fork();
    if (ret != 0) {
      int xstatus;
      wait(&xstatus);
      if (xstatus != 0)
        exit(1); // a child failed; err() already printed why
      continue;
    }
    if (getpid() != ugetpid())
      err("ugetpid() != getpid()");
    exit(0);
  }
  printf("ugetpid_test: OK\n");
}

int
main(int argc, char *argv[])
{
  print_pgtbl_test();
  ugetpid_test();
  printf("pgtbltest: all tests succeeded\n");
  exit(0);
}
