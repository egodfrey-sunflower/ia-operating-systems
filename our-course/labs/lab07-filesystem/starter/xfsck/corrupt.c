// corrupt -- damage a copy of an xv6 image in one named, specific way, so that
// xfsck can be tested against a fault whose class is known in advance.
//
// GIVEN CODE. You do not write this; Part 5 asks you to run it and confirm
// xfsck catches each fault, and then to describe one fault it cannot catch.
//
//   corrupt <image> <mode>
//
// The image is expected to be one built by mkimg (this directory), whose tree
// is fixed, so each mode can find its target deterministically. corrupt edits
// the image in place -- work on a COPY.
//
// Modes and the violation each is meant to provoke:
//   bitmap-free    a block a file uses is marked free      -> block-free-but-used
//   bitmap-leak    a free block is marked in use           -> bitmap-leak
//   linkcount      a file's nlink is decremented           -> link-count
//   orphan         a file's only directory entry is erased -> orphan-inode
//   dangling       a file's inode is freed under its entry -> dangling-entry
//   double-claim   two files are made to share a block     -> block-double-claim
//   dotdot         a subdirectory's ".." is made wrong     -> dotdot
//   root           the root inode is made a regular file   -> root
//   data           bytes inside a file's data are flipped  -> (undetectable)
//
// Several of these provoke more than one complaint (freeing an inode also
// leaks its blocks, and so on); the mode name gives the class the fault is
// there to exercise. "data" provokes none: that is the point of Part 5.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "xv6fs.h"

static unsigned char *buf;   // the whole image, in memory
static long nbytes;
static struct superblock sb;

static void die(const char *s){ fprintf(stderr, "corrupt: %s\n", s); exit(1); }

static unsigned char *block(uint bno){ return buf + (long)bno * BSIZE; }

static struct dinode *inode(uint inum){
  return (struct dinode *)block(IBLOCK(inum, sb)) + (inum % IPB);
}

static void bitmap_set(uint bno, int v){
  unsigned char *p = &block(BBLOCK(bno, sb))[(bno % BPB) / 8];
  if(v) *p |= (1 << (bno % 8));
  else  *p &= ~(1 << (bno % 8));
}
static int bitmap_get(uint bno){
  return (block(BBLOCK(bno, sb))[(bno % BPB) / 8] >> (bno % 8)) & 1;
}

static uint first_data(void){ return sb.bmapstart + (sb.size / BPB + 1); }

// The disk block holding file-block fbn of an inode (direct + single level is
// enough for everything corrupt needs here).
static uint block_addr(struct dinode *din, uint fbn){
  if(fbn < NDIRECT)
    return din->addrs[fbn];
  fbn -= NDIRECT;
  if(fbn < NINDIRECT && din->addrs[NDIRECT])
    return ((uint *)block(din->addrs[NDIRECT]))[fbn];
  return 0;
}

// Find the entry `name` in directory `dirinum`; return the byte offset of the
// dirent (or -1), and set *inump to the inode it names.
static long find_entry(uint dirinum, const char *name, uint *inump){
  struct dinode *dp = inode(dirinum);
  uint off;
  for(off = 0; off + sizeof(struct dirent) <= dp->size; off += sizeof(struct dirent)){
    uint ba = block_addr(dp, off / BSIZE);
    if(ba == 0)
      continue;
    struct dirent *de = (struct dirent *)(block(ba) + (off % BSIZE));
    if(de->inum != 0 && strncmp(de->name, name, DIRSIZ) == 0){
      if(inump) *inump = de->inum;
      return (long)ba * BSIZE + (off % BSIZE);
    }
  }
  return -1;
}

// Pointer to the dirent named `name` in dir `dirinum`, or NULL.
static struct dirent *entry_ptr(uint dirinum, const char *name){
  uint ino;
  long o = find_entry(dirinum, name, &ino);
  if(o < 0) return 0;
  return (struct dirent *)(buf + o);
}

static uint name_to_inum(uint dirinum, const char *name){
  uint ino = 0;
  if(find_entry(dirinum, name, &ino) < 0)
    die("cannot find an expected entry -- is this a mkimg image?");
  return ino;
}

int
main(int argc, char *argv[])
{
  int fd;
  const char *mode;

  if(argc != 3){
    fprintf(stderr, "usage: corrupt <image> <mode>\n");
    return 2;
  }
  mode = argv[2];

  if((fd = open(argv[1], O_RDWR)) < 0)
    die("cannot open image");
  nbytes = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  buf = malloc(nbytes);
  if(!buf || read(fd, buf, nbytes) != nbytes)
    die("cannot read image");
  memmove(&sb, buf + BSIZE, sizeof(sb));
  if(sb.magic != FSMAGIC)
    die("not an xv6 image (bad magic)");

  if(strcmp(mode, "bitmap-free") == 0){
    // Free a block that file "b" actually uses.
    uint ino = name_to_inum(ROOTINO, "b");
    uint blk = block_addr(inode(ino), 0);
    if(blk == 0) die("file b has no data block");
    bitmap_set(blk, 0);
    printf("corrupt: marked block %u (used by inode %u) free in the bitmap\n", blk, ino);

  } else if(strcmp(mode, "bitmap-leak") == 0){
    // Mark a currently-free data block as in use.
    uint b, chosen = 0;
    for(b = first_data(); b < sb.size; b++)
      if(!bitmap_get(b)){ chosen = b; break; }
    if(chosen == 0) die("no free block to leak");
    bitmap_set(chosen, 1);
    printf("corrupt: marked free block %u in use in the bitmap\n", chosen);

  } else if(strcmp(mode, "linkcount") == 0){
    // Decrement the link count of file "a" (which has two links: a and c).
    uint ino = name_to_inum(ROOTINO, "a");
    inode(ino)->nlink--;
    printf("corrupt: decremented inode %u nlink to %d\n", ino, inode(ino)->nlink);

  } else if(strcmp(mode, "orphan") == 0){
    // Erase file "b"'s only directory entry, orphaning its inode.
    uint ino = name_to_inum(ROOTINO, "b");
    struct dirent *de = entry_ptr(ROOTINO, "b");
    de->inum = 0;
    printf("corrupt: erased the only directory entry for inode %u\n", ino);

  } else if(strcmp(mode, "dangling") == 0){
    // Free file "b"'s inode while its directory entry still names it.
    uint ino = name_to_inum(ROOTINO, "b");
    inode(ino)->type = 0;
    printf("corrupt: freed inode %u, leaving its directory entry dangling\n", ino);

  } else if(strcmp(mode, "double-claim") == 0){
    // Make file "b" point its first block at a block file "a" already owns.
    uint ia = name_to_inum(ROOTINO, "a");
    uint ib = name_to_inum(ROOTINO, "b");
    uint shared = block_addr(inode(ia), 0);
    if(shared == 0) die("file a has no data block");
    inode(ib)->addrs[0] = shared;
    printf("corrupt: made inode %u share block %u with inode %u\n", ib, shared, ia);

  } else if(strcmp(mode, "dotdot") == 0){
    // Point subdirectory "sub"'s ".." at the wrong inode.
    uint isub = name_to_inum(ROOTINO, "sub");
    uint ia = name_to_inum(ROOTINO, "a");
    struct dirent *de = entry_ptr(isub, "..");
    if(!de) die("sub has no .. entry");
    de->inum = ia;
    printf("corrupt: set inode %u's '..' to %u (should be %u)\n", isub, ia, ROOTINO);

  } else if(strcmp(mode, "root") == 0){
    // Make the root inode a regular file.
    inode(ROOTINO)->type = T_FILE;
    printf("corrupt: changed the root inode's type to T_FILE\n");

  } else if(strcmp(mode, "data") == 0){
    // Flip bytes inside file "a"'s data. Structure is untouched -- a purely
    // structural checker cannot see this. This is the Part 5 point.
    uint ino = name_to_inum(ROOTINO, "a");
    uint blk = block_addr(inode(ino), 0);
    if(blk == 0) die("file a has no data block");
    block(blk)[0] ^= 0xff;
    block(blk)[1] ^= 0xff;
    printf("corrupt: flipped two data bytes in block %u of inode %u\n", blk, ino);

  } else {
    fprintf(stderr, "corrupt: unknown mode '%s'\n", mode);
    return 2;
  }

  lseek(fd, 0, SEEK_SET);
  if(write(fd, buf, nbytes) != nbytes)
    die("cannot write image back");
  close(fd);
  free(buf);
  return 0;
}
