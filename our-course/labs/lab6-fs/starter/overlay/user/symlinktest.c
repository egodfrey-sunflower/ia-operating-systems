//
// symlinktest: exercises Lab 6 Task 2 (symbolic links).
//
// Subtests:
//   follow     open() of a symlink reaches the target's data
//   nofollow   O_NOFOLLOW opens the link itself (type T_SYMLINK, data = path)
//   dangling   a symlink to a nonexistent target: open() fails
//   chain      a chain of symlinks is followed to the end
//   cycle      a symlink cycle is refused (depth limit), does not hang
//   depthcap   an 11-link chain exceeds the mandated depth cap of 10:
//              open() must return -1 (and must not hang)
//   unlink     removing a symlink leaves the target intact
//
// Prints "symlinktest: ALL TESTS PASSED" iff every subtest passes.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static int nfail = 0;

static void
fail(char *t, char *why)
{
  printf("symlinktest: FAIL %s: %s\n", t, why);
  nfail++;
}

static void
pass(char *t)
{
  printf("symlinktest: ok %s\n", t);
}

// Build the name "sl_d<n>" (1 <= n <= 11) into buf (buf must hold >= 8 bytes).
static char *
dname(char *buf, int n)
{
  buf[0] = 's'; buf[1] = 'l'; buf[2] = '_'; buf[3] = 'd';
  if (n < 10) {
    buf[4] = '0' + n;
    buf[5] = 0;
  } else {
    buf[4] = '1';
    buf[5] = '0' + (n - 10);
    buf[6] = 0;
  }
  return buf;
}

// remove any files this test may have created
static void
cleanup(void)
{
  unlink("sl_target");
  unlink("sl_link");
  unlink("sl_dangle");
  unlink("sl_c1");
  unlink("sl_c2");
  unlink("sl_l1");
  unlink("sl_l2");
  unlink("sl_l3");
  char name[8];
  for (int i = 1; i <= 11; i++)
    unlink(dname(name, i));
}

static void
mkfile(char *path, char *data)
{
  int fd = open(path, O_CREATE | O_WRONLY | O_TRUNC);
  if (fd < 0) {
    fail("setup", "cannot create file");
    return;
  }
  write(fd, data, strlen(data));
  close(fd);
}

static void
test_follow(void)
{
  char buf[64];
  mkfile("sl_target", "hello-target");
  if (symlink("sl_target", "sl_link") != 0) {
    fail("follow", "symlink() returned nonzero");
    return;
  }
  int fd = open("sl_link", O_RDONLY); // should follow to sl_target
  if (fd < 0) {
    fail("follow", "open(link) failed");
    return;
  }
  memset(buf, 0, sizeof(buf));
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0 || strcmp(buf, "hello-target") != 0) {
    fail("follow", "did not read target contents through link");
    return;
  }
  pass("follow");
}

static void
test_nofollow(void)
{
  char buf[64];
  struct stat st;
  int fd = open("sl_link", O_RDONLY | O_NOFOLLOW); // open the link itself
  if (fd < 0) {
    fail("nofollow", "open(O_NOFOLLOW) failed");
    return;
  }
  if (fstat(fd, &st) < 0 || st.type != T_SYMLINK) {
    fail("nofollow", "opened inode is not a T_SYMLINK");
    close(fd);
    return;
  }
  memset(buf, 0, sizeof(buf));
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0 || strcmp(buf, "sl_target") != 0) {
    fail("nofollow", "link data is not the stored target path");
    return;
  }
  pass("nofollow");
}

static void
test_dangling(void)
{
  if (symlink("sl_nonexistent", "sl_dangle") != 0) {
    fail("dangling", "symlink() to missing target returned nonzero");
    return;
  }
  int fd = open("sl_dangle", O_RDONLY); // follow -> missing target -> fail
  if (fd >= 0) {
    fail("dangling", "open() of dangling link unexpectedly succeeded");
    close(fd);
    return;
  }
  // But O_NOFOLLOW should still open the link itself.
  fd = open("sl_dangle", O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    fail("dangling", "O_NOFOLLOW open of dangling link failed");
    return;
  }
  close(fd);
  pass("dangling");
}

static void
test_chain(void)
{
  char buf[64];
  // sl_l1 -> sl_target, sl_l2 -> sl_l1, sl_l3 -> sl_l2
  if (symlink("sl_target", "sl_l1") != 0 || symlink("sl_l1", "sl_l2") != 0 ||
      symlink("sl_l2", "sl_l3") != 0) {
    fail("chain", "symlink() in chain returned nonzero");
    return;
  }
  int fd = open("sl_l3", O_RDONLY); // follow the whole chain
  if (fd < 0) {
    fail("chain", "open of chain head failed");
    return;
  }
  memset(buf, 0, sizeof(buf));
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0 || strcmp(buf, "hello-target") != 0) {
    fail("chain", "chain did not resolve to target contents");
    return;
  }
  pass("chain");
}

static void
test_cycle(void)
{
  // sl_c1 -> sl_c2 -> sl_c1 : must be refused, must not hang.
  if (symlink("sl_c2", "sl_c1") != 0 || symlink("sl_c1", "sl_c2") != 0) {
    fail("cycle", "symlink() in cycle returned nonzero");
    return;
  }
  int fd = open("sl_c1", O_RDONLY);
  if (fd >= 0) {
    fail("cycle", "open() of a symlink cycle unexpectedly succeeded");
    close(fd);
    return;
  }
  pass("cycle");
}

static void
test_depthcap(void)
{
  // sl_d1 -> sl_target, sl_d2 -> sl_d1, ..., sl_d11 -> sl_d10: opening
  // sl_d11 needs 11 dereferences, one more than the mandated cap of 10, so
  // open() must return -1 -- and must do so by counting, not by hanging.
  // (The cycle test alone cannot tell a depth cap from cycle detection; a
  // long straight chain can.)
  char a[8], b[8];
  if (symlink("sl_target", dname(a, 1)) != 0) {
    fail("depthcap", "symlink() in chain returned nonzero");
    return;
  }
  for (int i = 2; i <= 11; i++) {
    if (symlink(dname(b, i - 1), dname(a, i)) != 0) {
      fail("depthcap", "symlink() in chain returned nonzero");
      return;
    }
  }
  int fd = open(dname(a, 11), O_RDONLY);
  if (fd >= 0) {
    fail("depthcap", "open() of an 11-link chain succeeded (depth cap of 10 "
                     "not enforced)");
    close(fd);
    return;
  }
  pass("depthcap");
}

static void
test_unlink(void)
{
  char buf[64];
  // Removing the link must not remove the target.
  if (unlink("sl_link") != 0) {
    fail("unlink", "unlink(link) failed");
    return;
  }
  int fd = open("sl_target", O_RDONLY);
  if (fd < 0) {
    fail("unlink", "target vanished when link was removed");
    return;
  }
  memset(buf, 0, sizeof(buf));
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0 || strcmp(buf, "hello-target") != 0) {
    fail("unlink", "target contents changed");
    return;
  }
  pass("unlink");
}

int
main(void)
{
  cleanup();

  test_follow();
  test_nofollow();
  test_dangling();
  test_chain();
  test_cycle();
  test_depthcap();
  test_unlink();

  cleanup();

  if (nfail == 0) {
    printf("symlinktest: ALL TESTS PASSED\n");
    exit(0);
  }
  printf("symlinktest: %d subtest(s) FAILED\n", nfail);
  exit(1);
}
