// bigfile -- write a file past the old singly-indirect ceiling, read every
// block back byte-for-byte, then delete it and confirm the disk got its blocks
// back.
//
//   bigfile [nblocks]
//
// The default size is well past the old maximum of 268 blocks (12 direct plus
// a 256-entry singly indirect block) -- deep enough into the doubly indirect
// index that a wrong bmap or a leaky itrunc shows up -- but small enough to
// run in a couple of minutes under
// emulation. Pass a larger nblocks for a stress run (the grader's --stress mode
// passes a big one); do not go past MAXFILE = 65803.
//
// SELF-CHECKING, and SILENT ON FAILURE. Every failed check prints a
// "bigfile: FAIL ..." line and exits(1) WITHOUT the success token. The token
// "bigfile: ok" is printed only after the last check passes, so a grader that
// keys on it cannot be fooled by a run that died early.
//
// Two bugs it is built to catch:
//   * a bmap that does not reach the doubly indirect level -- the write of the
//     first block past NDIRECT + NINDIRECT fails (or the kernel panics);
//   * an itrunc that frees the direct and singly indirect blocks but leaks the
//     doubly indirect ones -- the write and read-back both pass, but after the
//     file is unlinked the free-block count has not returned to what it was, so
//     the last check fails. (This is why the file is deleted and the count
//     re-taken, rather than trusting that a clean read-back means a clean free.)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define BSIZE 1024
#define NB_DEFAULT 4000
#define NAME "big.dat"

char buf[BSIZE];

// A pattern that depends on the block index, so a block read back from the
// wrong place, or never written, shows up as wrong data rather than passing by
// luck.
static void
fill(int blk)
{
  for(int i = 0; i < BSIZE; i++)
    buf[i] = (char)(blk * 3 + i * 7 + (i & blk));
}

int
main(int argc, char *argv[])
{
  int nb = NB_DEFAULT;
  int fd, blk, i, base, after;

  if(argc >= 2)
    nb = atoi(argv[1]);
  if(nb <= 0){
    printf("bigfile: FAIL nblocks must be positive\n");
    exit(1);
  }

  // Warm up the directory entry: create and remove NAME once so that any
  // one-time growth of the directory to hold the entry happens BEFORE we take
  // the baseline free-block count. Otherwise that growth would look like a leak.
  fd = open(NAME, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("bigfile: FAIL cannot create %s\n", NAME);
    exit(1);
  }
  close(fd);
  unlink(NAME);

  base = freeblocks();

  // ---- write nb blocks -------------------------------------------------
  fd = open(NAME, O_CREATE | O_RDWR | O_TRUNC);
  if(fd < 0){
    printf("bigfile: FAIL cannot create %s\n", NAME);
    exit(1);
  }
  for(blk = 0; blk < nb; blk++){
    fill(blk);
    if(write(fd, buf, BSIZE) != BSIZE){
      printf("bigfile: FAIL write of block %d failed (disk full, or bmap did "
             "not reach the doubly indirect level)\n", blk);
      exit(1);
    }
  }
  close(fd);

  // ---- read every block back and check the bytes -----------------------
  fd = open(NAME, O_RDONLY);
  if(fd < 0){
    printf("bigfile: FAIL cannot reopen %s for reading\n", NAME);
    exit(1);
  }
  for(blk = 0; blk < nb; blk++){
    if(read(fd, buf, BSIZE) != BSIZE){
      printf("bigfile: FAIL short read at block %d\n", blk);
      exit(1);
    }
    for(i = 0; i < BSIZE; i++){
      if(buf[i] != (char)(blk * 3 + i * 7 + (i & blk))){
        printf("bigfile: FAIL block %d byte %d wrong\n", blk, i);
        exit(1);
      }
    }
  }
  if(read(fd, buf, BSIZE) != 0){
    printf("bigfile: FAIL file is longer than the %d blocks written\n", nb);
    exit(1);
  }
  close(fd);

  // ---- delete it and confirm the blocks came back ----------------------
  unlink(NAME);
  after = freeblocks();
  if(after != base){
    printf("bigfile: FAIL itrunc leaked blocks: %d free before, %d free after "
           "(%d blocks not reclaimed)\n", base, after, base - after);
    exit(1);
  }

  printf("bigfile: ok (%d blocks written, verified, and fully reclaimed)\n", nb);
  exit(0);
}
