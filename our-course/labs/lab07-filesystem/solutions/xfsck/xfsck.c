// xfsck -- a structural checker for an xv6 file-system image.  REFERENCE.
//
// It reads an image (never mounts or boots it) and verifies the static
// structural invariants of the on-disk format:
//
//   * the super block's magic is right and the root inode (1) is a directory;
//   * every data block is EITHER free in the bitmap OR reachable from exactly
//     one inode -- never both, never neither, never two;
//   * every allocated inode is named by at least one directory entry;
//   * every directory entry names an allocated inode;
//   * each inode's link count equals the number of directory entries that
//     refer to it (a directory's own "." is not counted, matching the kernel);
//   * every directory has "." naming itself and ".." naming its parent.
//
// A clean image prints exactly one line, "xfsck: clean", and exits 0. Any
// violation is printed as "FAIL <class>: <detail>" naming the inode or block
// at fault, and xfsck exits 1.
//
// It does NOT check crash consistency, the log, or any temporal property --
// those are a journaling checker's job (Lab 9), not a structural one.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "xv6fs.h"

static xv6img img;
static uint  *owner;     // owner[b] = inode that references block b, else 0
static short *itype;     // itype[i] = inode i's type (0 = free)
static uint  *refcount;  // refcount[i] = # directory entries naming inode i
static uint  *parent;    // parent[i] = parent directory of dir inode i
static uint   firstdata; // first block in the data region
static int    nviol;

static void viol(const char *cls, const char *fmt, ...)
  __attribute__((format(printf, 2, 3)));

static void
viol(const char *cls, const char *fmt, ...)
{
  va_list ap;
  nviol++;
  printf("FAIL %s: ", cls);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
}

// The disk block holding file-block fbn of inode `din`, or 0 if unallocated.
// Read-only: it never allocates. Handles all three index levels.
static uint
block_addr(struct dinode *din, uint fbn)
{
  unsigned char *bp;

  if(fbn >= MAXFILE)
    return 0;   // a hostile size field must not index past the image buffer
  if(fbn < NDIRECT)
    return din->addrs[fbn];
  fbn -= NDIRECT;
  if(fbn < NINDIRECT){
    if(din->addrs[NDIRECT] == 0 || (bp = img_block(&img, din->addrs[NDIRECT])) == 0)
      return 0;
    return ((uint *)bp)[fbn];
  }
  fbn -= NINDIRECT;
  if(din->addrs[NDIRECT+1] == 0 || (bp = img_block(&img, din->addrs[NDIRECT+1])) == 0)
    return 0;
  uint l1 = ((uint *)bp)[fbn / NINDIRECT];
  if(l1 == 0 || (bp = img_block(&img, l1)) == 0)
    return 0;
  return ((uint *)bp)[fbn % NINDIRECT];
}

// Read the directory entry at byte offset `off` of directory `din`. A dirent
// is 16 bytes and 16 divides the 1024-byte block, so it never straddles a
// block boundary. Returns 0 on success, -1 if the block is missing.
static int
read_dirent(struct dinode *din, uint off, struct dirent *de)
{
  uint ba = block_addr(din, off / BSIZE);
  unsigned char *bp;
  if(ba == 0 || (bp = img_block(&img, ba)) == 0){
    memset(de, 0, sizeof(*de));
    return -1;
  }
  memmove(de, bp + (off % BSIZE), sizeof(*de));
  return 0;
}

static int
in_data_region(uint bno)
{
  return bno >= firstdata && bno < img.sb.size;
}

// Record that inode `inum` references block `bno`, reporting the block if it
// is out of the data region or already claimed by another inode.
static void
claim(uint bno, uint inum)
{
  if(bno == 0)
    return;   // an unused addrs[] slot, not a reference
  if(!in_data_region(bno)){
    viol("range", "inode %u references block %u, which is outside the data "
         "region [%u, %u)", inum, bno, firstdata, img.sb.size);
    return;
  }
  if(owner[bno] != 0){
    viol("block-double-claim", "block %u is claimed by both inode %u and "
         "inode %u", bno, owner[bno], inum);
    return;
  }
  owner[bno] = inum;
}

// Claim every block an inode reaches: its direct blocks, its indirect blocks
// (and the index blocks themselves), and its doubly indirect blocks.
static void
walk_inode(uint inum, struct dinode *din)
{
  unsigned char *bp;
  uint i, j;

  for(i = 0; i < NDIRECT; i++)
    claim(din->addrs[i], inum);

  // Singly indirect: the index block, then the data blocks it names.
  uint ib = din->addrs[NDIRECT];
  claim(ib, inum);
  if(in_data_region(ib) && (bp = img_block(&img, ib)) != 0){
    uint *a = (uint *)bp;
    for(i = 0; i < NINDIRECT; i++)
      claim(a[i], inum);
  }

  // Doubly indirect: the top index block, each singly indirect block it names,
  // and the data blocks those name.
  uint db = din->addrs[NDIRECT+1];
  claim(db, inum);
  if(in_data_region(db) && (bp = img_block(&img, db)) != 0){
    uint *a = (uint *)bp;
    for(i = 0; i < NINDIRECT; i++){
      uint l1 = a[i];
      claim(l1, inum);
      if(in_data_region(l1)){
        unsigned char *bp2 = img_block(&img, l1);
        if(bp2){
          uint *a2 = (uint *)bp2;
          for(j = 0; j < NINDIRECT; j++)
            claim(a2[j], inum);
        }
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  struct dinode din, root;
  uint inum, b, off;

  if(argc != 2){
    fprintf(stderr, "usage: xfsck <image>\n");
    return 2;
  }
  if(img_open(&img, argv[1]) < 0)
    return 2;

  // --- the super block -------------------------------------------------
  if(img.sb.magic != FSMAGIC){
    viol("super", "bad magic 0x%x (expected 0x%x); this is not an xv6 image",
         img.sb.magic, FSMAGIC);
    printf("xfsck: %d problem(s) found\n", nviol);
    return 1;   // nothing below can be trusted
  }
  firstdata = img_first_data_block(&img);

  owner    = calloc(img.sb.size, sizeof(uint));
  itype    = calloc(img.sb.ninodes, sizeof(short));
  refcount = calloc(img.sb.ninodes, sizeof(uint));
  parent   = calloc(img.sb.ninodes, sizeof(uint));
  if(!owner || !itype || !refcount || !parent){
    fprintf(stderr, "xfsck: out of memory\n");
    return 2;
  }

  // --- the root inode --------------------------------------------------
  if(img_read_inode(&img, ROOTINO, &root) < 0 || root.type != T_DIR)
    viol("root", "inode %d must be an allocated directory (type is %d)",
         ROOTINO, root.type);

  // --- pass 1: inode types, and every block each inode claims ----------
  for(inum = 1; inum < img.sb.ninodes; inum++){
    if(img_read_inode(&img, inum, &din) < 0)
      continue;
    itype[inum] = din.type;
    if(din.type == 0)
      continue;
    walk_inode(inum, &din);
  }

  // --- the bitmap must agree with reachability, in the data region -----
  for(b = firstdata; b < img.sb.size; b++){
    int inuse = img_bitmap_get(&img, b);
    int owned = owner[b] != 0;
    if(inuse && !owned)
      viol("bitmap-leak", "block %u is marked in use but no inode references "
           "it", b);
    else if(!inuse && owned)
      viol("block-free-but-used", "block %u is referenced by inode %u but "
           "marked free in the bitmap", b, owner[b]);
  }

  // --- pass 2: directory entries -> reference counts, parents, validity -
  parent[ROOTINO] = ROOTINO;   // the root is its own parent
  for(inum = 1; inum < img.sb.ninodes; inum++){
    if(itype[inum] != T_DIR)
      continue;
    if(img_read_inode(&img, inum, &din) < 0)
      continue;
    for(off = 0; off + sizeof(struct dirent) <= din.size; off += sizeof(struct dirent)){
      struct dirent de;
      char name[DIRSIZ+1];
      if(read_dirent(&din, off, &de) < 0)
        continue;
      if(de.inum == 0)
        continue;   // an empty slot
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0;
      if(de.inum >= img.sb.ninodes){
        viol("entry", "directory %u has entry '%s' naming out-of-range inode "
             "%u", inum, name, de.inum);
        continue;
      }
      refcount[de.inum]++;
      if(itype[de.inum] == 0)
        viol("dangling-entry", "entry '%s' in directory %u names free inode "
             "%u", name, inum, de.inum);
      // Record the parent of a subdirectory (from its real name, not ..).
      if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
         itype[de.inum] == T_DIR)
        parent[de.inum] = inum;
    }
  }

  // --- "." and ".." on every directory --------------------------------
  for(inum = 1; inum < img.sb.ninodes; inum++){
    struct dirent de;
    if(itype[inum] != T_DIR)
      continue;
    if(img_read_inode(&img, inum, &din) < 0)
      continue;
    if(read_dirent(&din, 0, &de) < 0 ||
       strncmp(de.name, ".", DIRSIZ) != 0 || de.inum != inum)
      viol("dot", "directory %u's first entry is not '.' naming itself "
           "(inode %u)", inum, (uint)de.inum);
    if(read_dirent(&din, sizeof(struct dirent), &de) < 0 ||
       strncmp(de.name, "..", DIRSIZ) != 0 || de.inum != parent[inum])
      viol("dotdot", "directory %u's '..' is %u but its parent is %u",
           inum, (uint)de.inum, parent[inum]);
  }

  // --- orphans, and link counts ---------------------------------------
  for(inum = 1; inum < img.sb.ninodes; inum++){
    if(itype[inum] == 0)
      continue;
    if(refcount[inum] == 0){
      if(inum != ROOTINO)
        viol("orphan-inode", "inode %u (type %d) is allocated but no "
             "directory entry names it", inum, itype[inum]);
      continue;   // link count is meaningless for an unreferenced inode
    }
    // The kernel does not count a directory's own "." toward its link count.
    uint expected = refcount[inum];
    if(itype[inum] == T_DIR)
      expected -= 1;
    if(img_read_inode(&img, inum, &din) == 0 && (uint)din.nlink != expected)
      viol("link-count", "inode %u (type %d) has nlink %d but %u directory "
           "entr%s refer%s to it", inum, itype[inum], din.nlink, expected,
           expected == 1 ? "y" : "ies", expected == 1 ? "s" : "");
  }

  img_close(&img);
  free(owner); free(itype); free(refcount); free(parent);

  if(nviol == 0){
    printf("xfsck: clean\n");
    return 0;
  }
  printf("xfsck: %d problem(s) found\n", nviol);
  return 1;
}
