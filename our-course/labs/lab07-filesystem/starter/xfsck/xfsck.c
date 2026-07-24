// xfsck -- a structural checker for an xv6 file-system image.
//
// YOUR WORK (Part 4). Read the image with the accessors in xv6fs.h / xv6img.c
// and verify the file system's structural invariants:
//
//   * the super block's magic is right and the root inode (1) is a directory;
//   * every data block is EITHER free in the bitmap OR reachable from exactly
//     one inode -- never both, never neither, never two;
//   * every allocated inode is named by at least one directory entry;
//   * every directory entry names an allocated inode;
//   * each inode's link count equals the number of directory entries that
//     refer to it (a directory's own "." is not counted -- see how the kernel
//     manages nlink);
//   * every directory has "." naming itself and ".." naming its parent.
//
// Report each violation you find as one line of the form
//
//   FAIL <class>: <detail naming the inode or block at fault>
//
// and exit non-zero. The grader looks for the class tokens LITERALLY; the
// eight graded classes (one per invariant, tabulated in the handout) are
//
//   block-free-but-used   bitmap-leak    block-double-claim   orphan-inode
//   dangling-entry        link-count     dotdot               root
//
// The detail after the colon is your own wording, and you may add classes of
// your own for anything outside the eight. On a clean image print exactly one
// line -- "xfsck: clean" -- and exit 0.
//
// The reserved metadata blocks (boot block, super block, log, inode blocks,
// bitmap) are always in use and belong to no inode; img_first_data_block()
// tells you where the data region begins. An inode may legitimately be named
// by several directory entries (a hard link) -- that is not an error.
//
// This checker validates STATIC structure only. It does not, and should not,
// say anything about crash recovery or the log; that is a later lab.

#include <stdio.h>
#include "xv6fs.h"

int
main(int argc, char *argv[])
{
  xv6img img;

  if(argc != 2){
    fprintf(stderr, "usage: xfsck <image>\n");
    return 2;
  }
  if(img_open(&img, argv[1]) < 0)
    return 2;

  // TODO: check the invariants above. Until you do, xfsck prints nothing and
  // therefore fails the clean-image case (no "xfsck: clean") and every
  // corrupted-image case (no violation reported).

  img_close(&img);
  return 0;
}
