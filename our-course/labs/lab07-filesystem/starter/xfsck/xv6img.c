// xv6img.c -- the image-reader library declared in xv6fs.h.
//
// GIVEN CODE. Parsing the image is a tedious hour that teaches nothing; the
// checking logic in xfsck.c is the exercise. Read this if you are curious,
// but you should not need to change it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "xv6fs.h"

int
img_open(xv6img *im, const char *path)
{
  int fd;
  off_t sz;

  memset(im, 0, sizeof(*im));

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(stderr, "xfsck: cannot open image '%s'\n", path);
    return -1;
  }
  if((sz = lseek(fd, 0, SEEK_END)) < 0 || lseek(fd, 0, SEEK_SET) < 0){
    fprintf(stderr, "xfsck: cannot size image '%s'\n", path);
    close(fd);
    return -1;
  }
  im->nbytes = (long)sz;
  im->data = malloc(im->nbytes ? im->nbytes : 1);
  if(im->data == 0){
    fprintf(stderr, "xfsck: out of memory reading '%s'\n", path);
    close(fd);
    return -1;
  }

  long got = 0;
  while(got < im->nbytes){
    ssize_t r = read(fd, im->data + got, im->nbytes - got);
    if(r < 0){
      fprintf(stderr, "xfsck: read error on '%s'\n", path);
      close(fd);
      free(im->data);
      im->data = 0;
      return -1;
    }
    if(r == 0)
      break;
    got += r;
  }
  close(fd);

  if(im->nbytes < 2 * BSIZE){
    fprintf(stderr, "xfsck: image '%s' is too small to be an xv6 image\n",
            path);
    free(im->data);
    im->data = 0;
    return -1;
  }

  // The super block is block 1.
  memmove(&im->sb, im->data + BSIZE, sizeof(im->sb));
  im->nblocks_img = im->sb.size;
  return 0;
}

void
img_close(xv6img *im)
{
  if(im->data)
    free(im->data);
  im->data = 0;
}

unsigned char *
img_block(xv6img *im, uint bno)
{
  long off = (long)bno * BSIZE;
  if(off < 0 || off + BSIZE > im->nbytes)
    return 0;
  return im->data + off;
}

int
img_read_inode(xv6img *im, uint inum, struct dinode *out)
{
  unsigned char *bp;
  struct dinode *dip;

  if(inum >= im->sb.ninodes)
    return -1;
  bp = img_block(im, IBLOCK(inum, im->sb));
  if(bp == 0)
    return -1;
  dip = (struct dinode *)bp + (inum % IPB);
  memmove(out, dip, sizeof(*out));
  return 0;
}

int
img_bitmap_get(xv6img *im, uint bno)
{
  unsigned char *bp = img_block(im, BBLOCK(bno, im->sb));
  if(bp == 0)
    return 0;
  return (bp[(bno % BPB) / 8] >> (bno % 8)) & 1;
}

uint
img_first_data_block(xv6img *im)
{
  // The bitmap occupies ceil(size / BPB) blocks, starting at bmapstart; the
  // first data block is just past it. This equals mkfs's nmeta.
  uint nbitmap = im->sb.size / BPB + 1;
  return im->sb.bmapstart + nbitmap;
}
