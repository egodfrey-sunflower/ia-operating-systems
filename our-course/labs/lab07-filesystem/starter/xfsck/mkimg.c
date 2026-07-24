// mkimg -- build a small, valid xv6 file-system image for testing xfsck.
//
// GIVEN CODE. This is a cousin of the kernel's mkfs. mkfs builds a flat
// directory of files; this builds a deterministic little tree that exercises
// every invariant xfsck checks:
//
//   /            (root, a directory, with . and ..)
//   /a           a multi-block regular file
//   /c           a HARD LINK to /a  (so /a's inode has nlink 2, two entries)
//   /b           another regular file
//   /big         a large file: long enough to use direct, singly indirect AND
//                doubly indirect blocks, so xfsck's block walk is exercised
//   /sub         a subdirectory, with its own . and ..
//   /sub/d       a file inside it
//   /sl          a symbolic link (its data is the path "a")
//
// A freshly built image must pass xfsck with no complaints; corrupt.c damages
// a copy of it in named ways, and xfsck must then object.
//
// Byte order: written natively, for a little-endian host (as xv6img.c reads).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "xv6fs.h"

#define NINODES  200
#define LOGBLOCKS 30
#define FSSIZE   1000        // 1 MB image: plenty for the tree above

int nbitmap = FSSIZE / BPB + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGBLOCKS + 1;
int nmeta;
int nblocks;

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

static void die(const char *s){ perror(s); exit(1); }

static void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, (off_t)sec * BSIZE, 0) != (off_t)sec * BSIZE)
    die("lseek");
  if(write(fsfd, buf, BSIZE) != BSIZE)
    die("write");
}

static void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, (off_t)sec * BSIZE, 0) != (off_t)sec * BSIZE)
    die("lseek");
  if(read(fsfd, buf, BSIZE) != BSIZE)
    die("read");
}

static void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  ((struct dinode *)buf)[inum % IPB] = *ip;
  wsect(bn, buf);
}

static void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  *ip = ((struct dinode *)buf)[inum % IPB];
}

static uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;
  memset(&din, 0, sizeof(din));
  din.type = type;
  din.nlink = 1;
  din.size = 0;
  winode(inum, &din);
  return inum;
}

// Return the data-block number holding file-block fbn of inode din, allocating
// that block and any index blocks on the way to it. Mirrors the kernel's bmap.
static uint
bmap_alloc(struct dinode *din, uint fbn)
{
  uint ind[NINDIRECT], l1[NINDIRECT], l2[NINDIRECT];

  if(fbn < NDIRECT){
    if(din->addrs[fbn] == 0)
      din->addrs[fbn] = freeblock++;
    return din->addrs[fbn];
  }
  fbn -= NDIRECT;

  if(fbn < NINDIRECT){
    if(din->addrs[NDIRECT] == 0)
      din->addrs[NDIRECT] = freeblock++;
    rsect(din->addrs[NDIRECT], ind);
    if(ind[fbn] == 0){
      ind[fbn] = freeblock++;
      wsect(din->addrs[NDIRECT], ind);
    }
    return ind[fbn];
  }
  fbn -= NINDIRECT;

  // Doubly indirect.
  if(din->addrs[NDIRECT+1] == 0)
    din->addrs[NDIRECT+1] = freeblock++;
  rsect(din->addrs[NDIRECT+1], l1);
  if(l1[fbn / NINDIRECT] == 0){
    l1[fbn / NINDIRECT] = freeblock++;
    wsect(din->addrs[NDIRECT+1], l1);
  }
  rsect(l1[fbn / NINDIRECT], l2);
  if(l2[fbn % NINDIRECT] == 0){
    l2[fbn % NINDIRECT] = freeblock++;
    wsect(l1[fbn / NINDIRECT], l2);
  }
  return l2[fbn % NINDIRECT];
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Append n bytes of xp to inode inum's data.
static void
iappend(uint inum, const void *xp, int n)
{
  const char *p = (const char *)xp;
  struct dinode din;
  char buf[BSIZE];
  uint off, fbn, x, n1;

  rinode(inum, &din);
  off = din.size;
  while(n > 0){
    fbn = off / BSIZE;
    x = bmap_alloc(&din, fbn);
    n1 = MIN((uint)n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    memcpy(buf + (off - fbn * BSIZE), p, n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = off;
  winode(inum, &din);
}

// Append a directory entry (name -> inum) to directory dirinum.
static void
direntadd(uint dirinum, const char *name, uint inum)
{
  struct dirent de;
  memset(&de, 0, sizeof(de));
  de.inum = inum;
  strncpy(de.name, name, DIRSIZ);
  iappend(dirinum, &de, sizeof(de));
}

// Round a directory's size up to a whole block, the way mkfs does for root.
// The entries all fit in the first block, so no new block is needed.
static void
dirfinish(uint dirinum)
{
  struct dinode din;
  rinode(dirinum, &din);
  din.size = ((din.size / BSIZE) + 1) * BSIZE;
  winode(dirinum, &din);
}

static void
setnlink(uint inum, short nlink)
{
  struct dinode din;
  rinode(inum, &din);
  din.nlink = nlink;
  winode(inum, &din);
}

// Mark bits [0, used) of the bitmap in use. Every block this tool allocates is
// handed out sequentially from block 0, so the in-use blocks are exactly
// [0, freeblock) and one bitmap block holds them all.
static void
mark_bitmap(int used)
{
  uchar buf[BSIZE];
  int i;
  assert(used < BPB);
  memset(buf, 0, sizeof(buf));
  for(i = 0; i < used; i++)
    buf[i / 8] |= (0x1 << (i % 8));
  wsect(sb.bmapstart, buf);
}

int
main(int argc, char *argv[])
{
  uint rootino, subino, ino_a, ino_b, ino_big, ino_d, ino_sl;
  int i;
  char *fill;

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  if(argc != 2){
    fprintf(stderr, "usage: mkimg <image>\n");
    exit(1);
  }
  if((fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
    die(argv[1]);

  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = FSSIZE - nmeta;

  memset(&sb, 0, sizeof(sb));
  sb.magic = FSMAGIC;
  sb.size = FSSIZE;
  sb.nblocks = nblocks;
  sb.ninodes = NINODES;
  sb.nlog = nlog;
  sb.logstart = 2;
  sb.inodestart = 2 + nlog;
  sb.bmapstart = 2 + nlog + ninodeblocks;

  freeblock = nmeta;

  // Zero the whole image.
  memset(zeroes, 0, sizeof(zeroes));
  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  // Super block at block 1.
  {
    char buf[BSIZE];
    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);
  }

  // Root.
  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);
  direntadd(rootino, ".", rootino);
  direntadd(rootino, "..", rootino);

  // A subdirectory.
  subino = ialloc(T_DIR);
  direntadd(subino, ".", subino);
  direntadd(subino, "..", rootino);
  direntadd(rootino, "sub", subino);
  setnlink(rootino, 2);   // base entry + one child's ".." pointing back

  // /a, a few blocks, and a hard link /c to it.
  ino_a = ialloc(T_FILE);
  fill = malloc(3000);
  memset(fill, 'A', 3000);
  iappend(ino_a, fill, 3000);
  free(fill);
  direntadd(rootino, "a", ino_a);
  direntadd(rootino, "c", ino_a);
  setnlink(ino_a, 2);     // two directory entries name this inode

  // /b.
  ino_b = ialloc(T_FILE);
  fill = malloc(1500);
  memset(fill, 'B', 1500);
  iappend(ino_b, fill, 1500);
  free(fill);
  direntadd(rootino, "b", ino_b);

  // /big -- 270 blocks, so it reaches into the doubly indirect region.
  ino_big = ialloc(T_FILE);
  {
    int nb = 270;
    char *blk = malloc(BSIZE);
    for(i = 0; i < nb; i++){
      memset(blk, i & 0xff, BSIZE);
      iappend(ino_big, blk, BSIZE);
    }
    free(blk);
  }
  direntadd(rootino, "big", ino_big);

  // /sub/d.
  ino_d = ialloc(T_FILE);
  fill = malloc(500);
  memset(fill, 'D', 500);
  iappend(ino_d, fill, 500);
  free(fill);
  direntadd(subino, "d", ino_d);

  // /sl -> "a", a symbolic link.
  ino_sl = ialloc(T_SYMLINK);
  iappend(ino_sl, "a", 1);
  direntadd(rootino, "sl", ino_sl);

  dirfinish(rootino);
  dirfinish(subino);

  mark_bitmap(freeblock);

  if(freeblock >= (uint)FSSIZE){
    fprintf(stderr, "mkimg: image too small\n");
    exit(1);
  }
  printf("mkimg: wrote %s (%d blocks used of %d, %d inodes)\n",
         argv[1], freeblock, FSSIZE, freeinode - 1);
  exit(0);
}
