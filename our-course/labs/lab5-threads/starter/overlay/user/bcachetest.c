// bcachetest -- Lab 5, Part B, task 2 (bucketed buffer cache).
//
// Two checks:
//   test0  Several processes each repeatedly read their OWN private file at
//          the same time, generating lots of bget()/brelse() traffic on
//          disjoint blocks. With the stock single bcache lock every one of
//          those calls serialises on that lock and the "bcache" contended-
//          acquire count is huge; with per-bucket locks each process mostly
//          touches its own bucket(s), so the count is far lower. Each process
//          verifies the bytes it reads, so a broken cache FAILS. (The workload
//          is read-only: writes would hammer the shared log/bitmap/inode
//          blocks and hide the effect of bucketing.)
//   test1  One process reads and re-reads many DISTINCT blocks -- more than
//          NBUF -- forcing eviction, and checks the data every time. This
//          catches bugs in your LRU/eviction and cross-bucket handling.
//
// Prints "bcachetest: OK" iff both pass. The autograder keys off that line.

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/lockstat.h"
#include "user/user.h"

#define BSIZE 1024

// How much bcache contention test0 tolerates. Stock single-lock blows past it
// by two orders of magnitude; a correct per-bucket design stays well below.
// (Some contention is unavoidable on the shared metadata every open() reads --
// the root directory and inode blocks -- so this is not zero.)
#define BCACHE_NTS_MAX 1000

#define NCHILD 4
#define NBLK   5  // blocks per file (NCHILD*NBLK stays < NBUF so it all fits)
#define ROUNDS 60 // times each child re-reads its file

static char rbuf[BSIZE];

static void
fname(char *name, int id)
{
  name[0] = 'b';
  name[1] = (char)('0' + id);
  name[2] = 0;
}

// Create file b<id> with NBLK blocks of a known per-file pattern.
static int
make_file(int id)
{
  char name[3];
  fname(name, id);
  unlink(name);
  int fd = open(name, O_CREATE | O_RDWR);
  if (fd < 0)
    return -1;
  for (int b = 0; b < NBLK; b++) {
    for (int i = 0; i < BSIZE; i++)
      rbuf[i] = (char)(id * 31 + b * 7 + i);
    if (write(fd, rbuf, BSIZE) != BSIZE) {
      close(fd);
      return -1;
    }
  }
  close(fd);
  return 0;
}

// Re-read file b<id> ROUNDS times, verifying the pattern each time.
static int
read_file(int id)
{
  char name[3];
  fname(name, id);
  for (int r = 0; r < ROUNDS; r++) {
    int fd = open(name, O_RDONLY);
    if (fd < 0)
      return -1;
    for (int b = 0; b < NBLK; b++) {
      if (read(fd, rbuf, BSIZE) != BSIZE) {
        close(fd);
        return -1;
      }
      for (int i = 0; i < BSIZE; i++)
        if (rbuf[i] != (char)(id * 31 + b * 7 + i)) {
          close(fd);
          return -1;
        }
    }
    close(fd);
  }
  return 0;
}

static int
test0(void)
{
  struct lockstat s0, s1;

  printf("start test0\n");
  for (int i = 0; i < NCHILD; i++) {
    if (make_file(i) != 0) {
      printf("test0: FAIL -- setup could not create file %d\n", i);
      return 1;
    }
  }

  if (statistics(&s0) < 0) {
    printf("test0: FAIL -- statistics() syscall not working\n");
    return 1;
  }

  int fail = 0;
  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("test0: FAIL -- fork failed\n");
      return 1;
    }
    if (pid == 0)
      exit(read_file(i) == 0 ? 0 : 1);
  }
  for (int i = 0; i < NCHILD; i++) {
    int st = -1;
    wait(&st);
    if (st != 0)
      fail = 1;
  }

  statistics(&s1);
  uint64 dn = s1.bcache_n - s0.bcache_n;
  uint64 dnts = s1.bcache_nts - s0.bcache_nts;
  printf("test0: bcache #acquire = %d, #test-and-set (contended) = %d\n",
         (int)dn, (int)dnts);

  for (int i = 0; i < NCHILD; i++) {
    char name[3];
    fname(name, i);
    unlink(name);
  }

  if (fail) {
    printf("test0: FAIL -- a child saw wrong data or an I/O error\n");
    return 1;
  }
  if (dnts > BCACHE_NTS_MAX) {
    printf("test0: FAIL -- bcache still heavily contended (%d > %d)\n",
           (int)dnts, BCACHE_NTS_MAX);
    return 1;
  }
  printf("test0: OK\n");
  return 0;
}

// Read many distinct blocks repeatedly, forcing eviction, checking integrity.
static int
test1(void)
{
  printf("start test1\n");
  int nblk = 40; // > NBUF (30) so eviction is forced
  int fd = open("bcbig", O_CREATE | O_RDWR | O_TRUNC);
  if (fd < 0) {
    printf("test1: FAIL -- cannot create file\n");
    return 1;
  }
  for (int b = 0; b < nblk; b++) {
    for (int i = 0; i < BSIZE; i++)
      rbuf[i] = (char)(b * 7 + i);
    if (write(fd, rbuf, BSIZE) != BSIZE) {
      printf("test1: FAIL -- write error\n");
      close(fd);
      return 1;
    }
  }
  close(fd);

  for (int pass = 0; pass < 3; pass++) {
    fd = open("bcbig", O_RDONLY);
    if (fd < 0) {
      printf("test1: FAIL -- cannot reopen file\n");
      return 1;
    }
    for (int b = 0; b < nblk; b++) {
      if (read(fd, rbuf, BSIZE) != BSIZE) {
        printf("test1: FAIL -- read error at block %d\n", b);
        close(fd);
        return 1;
      }
      for (int i = 0; i < BSIZE; i++) {
        if (rbuf[i] != (char)(b * 7 + i)) {
          printf("test1: FAIL -- wrong data at block %d (bad eviction?)\n", b);
          close(fd);
          return 1;
        }
      }
    }
    close(fd);
  }
  unlink("bcbig");
  printf("test1: OK\n");
  return 0;
}

int
main(void)
{
  int bad = 0;
  bad |= test0();
  bad |= test1();
  if (bad) {
    printf("bcachetest: FAILED\n");
    exit(1);
  }
  printf("bcachetest: OK\n");
  exit(0);
}
