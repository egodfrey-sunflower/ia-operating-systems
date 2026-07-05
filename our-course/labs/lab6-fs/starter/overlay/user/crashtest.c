//
// crashtest: the user half of the Lab 6 Task 3 crash-consistency experiment.
//
// It runs in two phases, driven by the autograder:
//
//   crashtest phase1
//     1. create "crashf", fill its first block with 'A', sync -> durable
//        baseline.
//     2. arm the crashpoint with crashnow(1).
//     3. overwrite the block with 'B'. The commit of THIS write hits the
//        compiled-in crashpoint (if any) and the kernel freezes. On a kernel
//        built without CRASH= the write simply completes.
//
//   crashtest phase2   (run after rebooting the SAME disk image)
//     read "crashf" back and print a machine-readable verdict:
//        VERDICT old    -> block still 'A': the 'B' write was lost
//                          (crash happened BEFORE the log header committed)
//        VERDICT new    -> block is 'B': the 'B' write survived
//                          (crash happened AFTER the header committed; recovery
//                           replayed it)
//        VERDICT absent -> "crashf" is missing entirely (should never happen
//                          here: the baseline was synced before arming)
//        VERDICT corrupt/mixed/short -> inconsistency: the log failed to keep
//                          the file all-old-or-all-new.
//
// The whole point: whatever the crash instant, the file is NEVER a torn
// mixture. That all-or-nothing property is exactly what the write-ahead log
// buys, and where the header write is the atomic commit point.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define BSZ 1024
char buf[BSZ];

static int
phase1(void)
{
  int fd = open("crashf", O_CREATE | O_WRONLY | O_TRUNC);
  if (fd < 0) {
    printf("crashtest: phase1 cannot create crashf\n");
    return 1;
  }
  memset(buf, 'A', BSZ);
  if (write(fd, buf, BSZ) != BSZ) {
    printf("crashtest: phase1 baseline write failed\n");
    return 1;
  }
  close(fd);
  sync(); // baseline ('A') is now durable on disk
  printf("crashtest: phase1 baseline durable\n");

  crashnow(1); // arm the crashpoint

  fd = open("crashf", O_WRONLY); // no O_TRUNC: this open touches no FS blocks
  if (fd < 0) {
    printf("crashtest: phase1 cannot reopen crashf\n");
    return 1;
  }
  memset(buf, 'B', BSZ);
  write(fd, buf, BSZ); // <-- the commit of this write hits the crashpoint
  // We only get here on a kernel built WITHOUT a crashpoint.
  close(fd);
  crashnow(0);
  printf("crashtest: phase1 completed with no crash (built without CRASH=)\n");
  return 0;
}

static int
phase2(void)
{
  int fd = open("crashf", O_RDONLY);
  if (fd < 0) {
    printf("crashtest: VERDICT absent\n");
    return 0;
  }
  memset(buf, 0, BSZ);
  int n = read(fd, buf, BSZ);
  close(fd);
  if (n != BSZ) {
    printf("crashtest: VERDICT short n=%d\n", n);
    return 0;
  }
  int nA = 0, nB = 0, nother = 0;
  for (int i = 0; i < BSZ; i++) {
    if (buf[i] == 'A')
      nA++;
    else if (buf[i] == 'B')
      nB++;
    else
      nother++;
  }
  if (nother)
    printf("crashtest: VERDICT corrupt other=%d\n", nother);
  else if (nA == BSZ)
    printf("crashtest: VERDICT old\n");
  else if (nB == BSZ)
    printf("crashtest: VERDICT new\n");
  else
    printf("crashtest: VERDICT mixed nA=%d nB=%d\n", nA, nB);
  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("usage: crashtest phase1|phase2\n");
    exit(1);
  }
  if (strcmp(argv[1], "phase1") == 0)
    exit(phase1());
  if (strcmp(argv[1], "phase2") == 0)
    exit(phase2());
  printf("crashtest: unknown phase %s\n", argv[1]);
  exit(1);
}
