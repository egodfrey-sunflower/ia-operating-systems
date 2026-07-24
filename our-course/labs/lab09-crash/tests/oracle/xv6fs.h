// xv6fs.h -- the on-disk xv6 file-system format, and a small library for
// reading an image from an ordinary userspace program.
//
// GIVEN CODE. You do not need to change this file; it is the format and the
// accessors. The checking logic goes in xfsck.c.
//
// The format here matches the kernel headers after the Part 2 change: NDIRECT
// is 11, and the inode's addrs[] has NDIRECT+2 entries (11 direct, one singly
// indirect, one doubly indirect). An image written by the patched mkfs -- or
// by mkimg in this directory -- has exactly this layout.
//
// Byte order: xv6 runs on little-endian RISC-V and these tools are built for a
// little-endian host, so multi-byte fields are read by a plain struct cast.

#ifndef XV6FS_H
#define XV6FS_H

#include <stdint.h>

typedef uint32_t uint;
typedef uint16_t ushort;
typedef uint8_t  uchar;

#define ROOTINO 1        // root i-number
#define BSIZE   1024     // block size

#define FSMAGIC 0x10203040

// A file's blocks: NDIRECT direct, then one singly indirect block of
// NINDIRECT pointers, then one doubly indirect block whose NINDIRECT pointers
// each name a singly indirect block. addrs[] therefore has NDIRECT+2 entries.
#define NDIRECT    11
#define NINDIRECT  (BSIZE / sizeof(uint))       // 256
#define NDINDIRECT (NINDIRECT * NINDIRECT)       // 65536
#define MAXFILE    (NDIRECT + NINDIRECT + NDINDIRECT)   // 65803
#define NADDR      (NDIRECT + 2)                  // entries in addrs[]

struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

// Inode types.
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_SYMLINK 4   // Symbolic link

// On-disk inode. Exactly 64 bytes, so IPB (inodes per block) is 16.
struct dinode {
  short type;           // File type; 0 means the inode is free
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint  size;           // Size of file (bytes)
  uint  addrs[NADDR];   // Data block addresses
};

#define IPB           (BSIZE / sizeof(struct dinode))       // 16
#define IBLOCK(i, sb) ((i) / IPB + (sb).inodestart)

#define BPB           (BSIZE * 8)                            // bits per block
#define BBLOCK(b, sb) ((b) / BPB + (sb).bmapstart)

#define DIRSIZ 14
struct dirent {
  ushort inum;
  char   name[DIRSIZ];
};

// ------------------------------------------------------------------------
// The reader. Open an image, then use the accessors below. All of them work
// straight off the bytes of the file, read once into memory.
// ------------------------------------------------------------------------

typedef struct {
  unsigned char *data;       // the whole image, in memory
  long           nbytes;     // its length
  struct superblock sb;      // parsed copy of the super block (block 1)
  uint           nblocks_img; // sb.size, the number of blocks the image claims
} xv6img;

// Open `path`, read it into memory, and parse the super block. Returns 0 on
// success, -1 on error (message on stderr). The image is read-only here.
int  img_open(xv6img *im, const char *path);
void img_close(xv6img *im);

// A pointer to the first byte of block `bno` within the image, or 0 if `bno`
// is past the end of the file.
unsigned char *img_block(xv6img *im, uint bno);

// Copy inode number `inum` into *out. Returns 0 on success, -1 if `inum` is
// out of range.
int  img_read_inode(xv6img *im, uint inum, struct dinode *out);

// The value of the free-bitmap bit for block `bno`: 1 if the bitmap marks the
// block in use, 0 if it marks it free.
int  img_bitmap_get(xv6img *im, uint bno);

// The number of the first data block -- the first block that balloc() could
// hand out. Everything below it (boot block, super block, log, inode blocks,
// bitmap blocks) is metadata that is always in use and belongs to no inode.
uint img_first_data_block(xv6img *im);

#endif // XV6FS_H
