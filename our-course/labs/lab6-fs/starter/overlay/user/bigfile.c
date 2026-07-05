//
// bigfile: stress the inode block map with a file larger than the
// single-indirect limit. Exercises Lab 6 Task 1 (11 direct + 1 single
// indirect + 1 double indirect).
//
//   11 direct + 256 single-indirect + 256*256 double-indirect = 65803 blocks
//
// On the stock (unmodified) inode the maximum is only 12 + 256 = 268 blocks,
// so this test fails loudly there until you rebalance the inode.
//
// The write/verify/unlink cycle runs NPASS times, and that is what checks
// itrunc(): a kernel that leaks blocks on unlink passes any single pass but
// runs the disk out on the next one. The arithmetic behind NPASS = 2
// (FSSIZE is sized so that two passes suffice):
//
//   free data blocks in a fresh image:  99941 (mkfs: FSSIZE=100000, nmeta
//                                       59) minus the installed binaries,
//                                       so roughly 99,4xx at boot;
//   allocated per pass:                 65803 data + 258 map blocks
//                                       (1 single-indirect + 1 top +
//                                       256 second-level) = 66061;
//   leaked per unlink by an itrunc      65536 data + 257 map = 65793
//   that skips the doubly-indirect arm:
//
//   after 1 leaky pass: ~99,4xx - 65793 = ~33,6xx free -> pass 2 (66061)
//   cannot complete and the test fails, with ~32k blocks of margin. A
//   correct itrunc returns every block, and both passes fit easily.
//
#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// The expected new maximum file size, in blocks.
#define EXPECT 65803
// Write/verify/unlink cycles; see the leak arithmetic above.
#define NPASS 2

char buf[BSIZE];

int
main(void)
{
  int fd, i, blocks, pass;

  printf("bigfile: %d passes of write+verify+unlink of up to %d blocks\n",
         NPASS, EXPECT);
  printf("bigfile: (slow under qemu, please wait)...\n");

  for (pass = 0; pass < NPASS; pass++) {
    int tagbase = pass * 70000; // per-pass tag offset: stale blocks from an
                                // earlier pass can never verify

    fd = open("big.file", O_CREATE | O_WRONLY | O_TRUNC);
    if (fd < 0) {
      printf("bigfile: FAIL cannot open big.file (pass %d)\n", pass + 1);
      exit(1);
    }

    blocks = 0;
    while (blocks <= EXPECT) {
      *(int *)buf = tagbase + blocks; // tag each block with its index
      int cc = write(fd, buf, BSIZE);
      if (cc <= 0)
        break;
      blocks++;
      if (blocks % 10000 == 0)
        printf("bigfile: pass %d ... %d blocks written\n", pass + 1, blocks);
    }
    close(fd);

    printf("bigfile: pass %d wrote %d blocks\n", pass + 1, blocks);

    if (blocks != EXPECT) {
      printf("bigfile: FAIL wrote %d blocks, expected %d (pass %d)\n", blocks,
             EXPECT, pass + 1);
      if (pass > 0)
        printf("bigfile: earlier passes fit -- is itrunc() leaking blocks "
               "on unlink?\n");
      exit(1);
    }

    // Read every block back and check its tag.
    fd = open("big.file", O_RDONLY);
    if (fd < 0) {
      printf("bigfile: FAIL cannot reopen big.file (pass %d)\n", pass + 1);
      exit(1);
    }
    for (i = 0; i < blocks; i++) {
      int cc = read(fd, buf, BSIZE);
      if (cc != BSIZE) {
        printf("bigfile: FAIL short read %d at block %d (pass %d)\n", cc, i,
               pass + 1);
        close(fd);
        exit(1);
      }
      if (*(int *)buf != tagbase + i) {
        printf("bigfile: FAIL block %d carried tag %d (pass %d)\n", i,
               *(int *)buf, pass + 1);
        close(fd);
        exit(1);
      }
    }
    close(fd);

    if (unlink("big.file") != 0) {
      printf("bigfile: FAIL unlink (pass %d)\n", pass + 1);
      exit(1);
    }
    printf("bigfile: pass %d verified and unlinked\n", pass + 1);
  }

  printf("bigfile: OK wrote and verified %d blocks x %d passes\n", EXPECT,
         NPASS);
  exit(0);
}
