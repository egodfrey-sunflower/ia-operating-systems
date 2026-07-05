// ppidtest -- exercises the getppid() system call (Lab 2, Task 3).
//
// Two checks:
//   basic  -- a child's getppid() equals its parent's getpid().
//   adopt  -- when a process's parent exits, the process is reparented to
//             init (pid 1), so getppid() must eventually return 1.
//
// Prints "ppidtest: basic OK" and "ppidtest: adopt OK" on success, and a line
// containing "FAIL" on failure.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
test_basic(void)
{
  int parent = getpid();
  int pid = fork();

  if (pid < 0) {
    printf("ppidtest: basic FAIL fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    int pp = getppid();
    if (pp == parent)
      printf("ppidtest: basic OK\n");
    else
      printf("ppidtest: basic FAIL got ppid %d, expected %d\n", pp, parent);
    exit(0);
  }
  wait(0);
}

void
test_adopt(void)
{
  int pid = fork();

  if (pid < 0) {
    printf("ppidtest: adopt FAIL fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    // We are the "child". Fork a "grandchild", then exit immediately so the
    // grandchild is orphaned and must be adopted by init.
    int gpid = fork();
    if (gpid < 0) {
      printf("ppidtest: adopt FAIL nested fork failed\n");
      exit(1);
    }
    if (gpid == 0) {
      // Grandchild: wait for our parent to exit and init to adopt us. Poll
      // getppid() until it reports pid 1 (init), or give up.
      int pp = -1, i;
      for (i = 0; i < 100; i++) {
        pp = getppid();
        if (pp == 1)
          break;
        pause(2);
      }
      if (pp == 1)
        printf("ppidtest: adopt OK\n");
      else
        printf("ppidtest: adopt FAIL got ppid %d, expected 1 (init)\n", pp);
      exit(0);
    }
    // Child exits right away, orphaning the grandchild.
    exit(0);
  }

  // Original process: reap the child, then give the grandchild time to run
  // and report before we finish (init reaps the grandchild itself).
  wait(0);
  pause(50);
}

int
main(void)
{
  test_basic();
  test_adopt();
  exit(0);
}
