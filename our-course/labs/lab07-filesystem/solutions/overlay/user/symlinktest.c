// symlinktest -- exercise the symlink() system call and open()'s handling
// of symbolic links.
//
// SELF-CHECKING, and SILENT ON FAILURE. Each check that fails prints a
// "symlinktest: FAIL ..." line and exits(1) without the success token. The
// token "symlinktest: ok" is printed only after the final check passes.
//
// Semantics under test (see the handout):
//   * open(path) follows a symbolic link to its (possibly chained) target.
//   * open(path, O_NOFOLLOW) opens the LINK itself; reading it returns the
//     stored target path.
//   * a link to a missing target dangles: following it fails, O_NOFOLLOW
//     opens it.
//   * a cycle of links is refused (open returns -1) rather than followed
//     forever. A kernel without a depth limit hangs here instead, which the
//     grader's per-command timeout reports as this test not completing.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static void
fail(char *msg)
{
  printf("symlinktest: FAIL %s\n", msg);
  exit(1);
}

static void
cleanup(void)
{
  unlink("stf");
  unlink("stl");
  unlink("stl2");
  unlink("sta");
  unlink("stb");
  unlink("stdang");
}

int
main(void)
{
  int fd, n;
  char buf[128];
  struct stat st;

  cleanup();

  // A target file with known contents.
  fd = open("stf", O_CREATE | O_RDWR);
  if(fd < 0)
    fail("cannot create target file stf");
  if(write(fd, "hello", 5) != 5)
    fail("cannot write target file");
  close(fd);

  // Create the link stl -> stf.
  if(symlink("stf", "stl") != 0)
    fail("symlink(\"stf\", \"stl\") returned nonzero");

  // open() follows the link to stf; a read returns stf's contents.
  fd = open("stl", O_RDONLY);
  if(fd < 0)
    fail("open through a symbolic link failed");
  n = read(fd, buf, sizeof(buf));
  if(n != 5 || memcmp(buf, "hello", 5) != 0)
    fail("open followed the link but read the wrong data");
  close(fd);

  // Writing through the link reaches the target file, not the link.
  fd = open("stl", O_RDWR);
  if(fd < 0)
    fail("open(link, O_RDWR) failed");
  if(write(fd, "J", 1) != 1)
    fail("write through the link failed");
  close(fd);
  fd = open("stf", O_RDONLY);
  if(fd < 0)
    fail("reopen of the target failed");
  n = read(fd, buf, sizeof(buf));
  if(n != 5 || buf[0] != 'J')
    fail("write through the link did not reach the target");
  close(fd);

  // O_NOFOLLOW opens the link itself: it is a T_SYMLINK whose contents are
  // the target path "stf".
  fd = open("stl", O_RDONLY | O_NOFOLLOW);
  if(fd < 0)
    fail("O_NOFOLLOW open of a symbolic link failed");
  if(fstat(fd, &st) < 0)
    fail("fstat on the link failed");
  if(st.type != T_SYMLINK)
    fail("O_NOFOLLOW did not open the link itself (type is not T_SYMLINK)");
  n = read(fd, buf, sizeof(buf));
  if(n != 3 || memcmp(buf, "stf", 3) != 0)
    fail("the link's contents are not the stored target path");
  close(fd);

  // Chained links: stl2 -> stl -> stf. open(stl2) must reach stf.
  if(symlink("stl", "stl2") != 0)
    fail("symlink(\"stl\", \"stl2\") returned nonzero");
  fd = open("stl2", O_RDONLY);
  if(fd < 0)
    fail("open through a chain of links failed");
  n = read(fd, buf, sizeof(buf));
  if(n != 5 || buf[0] != 'J')
    fail("chained link read the wrong data");
  close(fd);

  // A dangling link: following it fails, O_NOFOLLOW opens it.
  if(symlink("no_such_target", "stdang") != 0)
    fail("symlink to a missing target returned nonzero");
  fd = open("stdang", O_RDONLY);
  if(fd >= 0)
    fail("open followed a dangling link instead of failing");
  fd = open("stdang", O_RDONLY | O_NOFOLLOW);
  if(fd < 0)
    fail("O_NOFOLLOW open of a dangling link failed");
  close(fd);

  // A cycle must be refused, not followed forever: sta -> stb -> sta.
  if(symlink("stb", "sta") != 0)
    fail("symlink(\"stb\", \"sta\") returned nonzero");
  if(symlink("sta", "stb") != 0)
    fail("symlink(\"sta\", \"stb\") returned nonzero");
  fd = open("sta", O_RDONLY);
  if(fd >= 0)
    fail("open of a symbolic-link cycle returned a descriptor instead of -1");

  cleanup();

  printf("symlinktest: ok\n");
  exit(0);
}
