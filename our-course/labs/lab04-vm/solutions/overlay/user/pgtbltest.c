// Lab 4, Part 2: the read-only per-process page and its user-space accessor.
//
// Self-checking. Every failed check prints one "pgtbltest: FAIL <reason>"
// line; the final "pgtbltest: OK" is printed ONLY if every check passed, and
// from nowhere else, so a run that crashes part way through cannot be mistaken
// for a pass. Children signal their result through their exit status; only the
// parent prints.
//
// The mapping is checked by reading the USYSCALL page DIRECTLY at its fixed
// address -- not through the student's ugetpid() -- so a correct verdict does
// not depend on student code that could sidestep the page. ugetpid() itself is
// only checked for the value it returns: whether it reads the pid without a
// system call (the point of Part 2) cannot be observed from user space, so that
// property is marked by hand against the source, not here.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"       // PGSIZE, MAXVA (used by memlayout.h)
#include "kernel/memlayout.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fails = 0;
  int st;
  int pid;

  printf("pgtbltest: starting\n");

  int gp = getpid();

  // 1. The USYSCALL page is mapped, readable, and holds this process's pid.
  //    Read it DIRECTLY at the fixed address, casting the constant and
  //    dereferencing -- this proves the kernel mapped the page and stored the
  //    right pid, with no dependence on ugetpid(). If the page is not mapped
  //    this load page-faults and the kernel kills the whole program before
  //    "pgtbltest: done", so the harness force-fails every Part 2 case: an
  //    absent page cannot pass by omission.
  struct usyscall *u = (struct usyscall *)USYSCALL;
  int page_pid = u->pid;
  int page_ok = (page_pid == gp);
  if(!page_ok){
    printf("pgtbltest: FAIL the USYSCALL page holds pid %d but getpid() "
           "returned %d\n", page_pid, gp);
    fails++;
  }

  // 2. ugetpid() returns the right pid. That it reads the page WITHOUT a system
  //    call is the whole point of Part 2, but user space cannot tell whether a
  //    trap happened -- a wrapper that just calls getpid() is indistinguishable
  //    from here -- so syscall-avoidance is marked BY HAND against the source.
  //    This check only confirms the accessor returns the correct value.
  int up = ugetpid();
  if(up != gp){
    printf("pgtbltest: FAIL ugetpid returned %d but getpid returned %d\n",
           up, gp);
    fails++;
  }

  // 3. the page is per-process: a child reads ITS own pid, not the parent's,
  //    straight from the page (again directly, not via ugetpid). fork() does
  //    not copy the parent's USYSCALL contents; the child gets its own page,
  //    filled by the kernel when it was born.
  int parent = gp;
  pid = fork();
  if(pid < 0){
    printf("pgtbltest: FAIL fork failed\n");
    fails++;
  } else if(pid == 0){
    // exit(0) only if the page holds the child's own pid and not the parent's.
    struct usyscall *cu = (struct usyscall *)USYSCALL;
    if(cu->pid == getpid() && cu->pid != parent)
      exit(0);
    exit(1);
  } else {
    wait(&st);
    if(st != 0){
      printf("pgtbltest: FAIL the pid page is not per-process; a child read "
             "the wrong pid\n");
      fails++;
    }
  }

  // 4. the page is READ-ONLY. A user store to it must fault (the child is
  //    killed) rather than succeed. This case only means anything if the page
  //    is also mapped and readable -- an unmapped page faults on a store too --
  //    so it is graded only when check 1 (the DIRECT page read) passed; the
  //    harness enforces that gate as well.
  if(page_ok){
    pid = fork();
    if(pid < 0){
      printf("pgtbltest: FAIL fork failed\n");
      fails++;
    } else if(pid == 0){
      // If USYSCALL is writable this store succeeds and we exit(0), which the
      // parent reads as a failure. If it is read-only the store page-faults
      // and the kernel kills us before we reach exit(0).
      struct usyscall *wu = (struct usyscall *)USYSCALL;
      wu->pid = 999;
      exit(0);
    } else {
      wait(&st);
      if(st == 0){
        printf("pgtbltest: FAIL the pid page is writable; a user store to it "
               "succeeded\n");
        fails++;
      }
    }
  }

  if(fails == 0)
    printf("pgtbltest: OK\n");
  // Completion marker, printed only after every check has run. It is NOT a
  // success token (that is "pgtbltest: OK"); it merely says the program was not
  // killed part way through, so the harness can tell a failed check from a
  // crashed kernel. No abort path reaches it.
  printf("pgtbltest: done\n");
  exit(0);
}
