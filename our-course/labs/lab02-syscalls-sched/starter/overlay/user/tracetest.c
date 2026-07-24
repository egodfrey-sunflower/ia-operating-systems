// tracetest -- exercises the Part 2 trace() system call.
//
// GIVEN. Do not change it: tests/run.sh matches its output line by line.
//
// This program makes a fixed, deterministic sequence of system calls and
// prints markers around each group. It cannot check the trace lines itself --
// they are printed by the KERNEL, straight to the console, and no user
// process can read them back. So the checking happens outside, in the
// harness, which scrapes the console between the markers.
//
// Note what is deliberately never traced here: write(). xv6's user-space
// printf calls write() once PER CHARACTER, so a mask containing SYS_write
// turns every marker into a wall of trace lines. Try it by hand once -- it is
// a good way to see that the trace hook really is in the dispatch path -- but
// not in this program.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(void)
{
  int pid, cpid;

  pid = getpid();
  printf("tracetest: start pid %d\n", pid);

  // A -- nothing has been traced yet. Every system call below must be
  // silent. A kernel that prints unconditionally fails here.
  printf("tracetest: A begin\n");
  getpid();
  getpid();
  close(99);
  printf("tracetest: A end\n");

  // B -- trace getpid only. Exactly three lines, all naming this pid, all
  // with getpid's return value, which for getpid is this pid again.
  //
  // This is also the one place trace()'s OWN return value is checked: the
  // specification says it returns 0, and a call whose result nobody looks at
  // is a part of the specification nobody enforces.
  if(trace(1 << SYS_getpid) != 0)
    printf("tracetest: FAIL trace returned non-zero\n");
  printf("tracetest: B begin\n");
  getpid();
  getpid();
  getpid();
  printf("tracetest: B end\n");

  // C -- trace close only. Two things at once: getpid must now be silent
  // (the mask is a mask, not a switch), and close(99) fails, so this is
  // where a NEGATIVE return value gets printed. A kernel that prints the
  // first argument instead of the result says 99 here.
  trace(1 << SYS_close);
  printf("tracetest: C begin\n");
  getpid();
  close(99);
  printf("tracetest: C end\n");

  // D -- the mask survives fork. The parent makes no getpid call inside this
  // block, so the single line that must appear is the CHILD's.
  trace(1 << SYS_getpid);
  printf("tracetest: D begin\n");
  cpid = fork();
  if(cpid < 0){
    printf("tracetest: FAIL fork failed\n");
    exit(1);
  }
  if(cpid == 0){
    int me = getpid();            // traced -- prints the child's pid
    printf("tracetest: D child pid %d\n", me);
    exit(0);
  }
  wait(0);
  printf("tracetest: D end\n");

  // E -- an empty mask turns tracing off again.
  trace(0);
  printf("tracetest: E begin\n");
  getpid();
  getpid();
  close(99);
  printf("tracetest: E end\n");

  // F -- the mask is PER PROCESS. Everything above this point has only ever
  // had one mask in play at a time, so "inherited" and "shared" look
  // identical. Here the child changes its own mask after the fork, and the
  // parent's must be untouched. A single kernel-global mask -- no field in
  // struct proc, no copy in fork -- passes every block above and fails this
  // one, which is the whole reason it exists.
  trace(1 << SYS_getpid);
  printf("tracetest: F begin\n");
  cpid = fork();
  if(cpid < 0){
    printf("tracetest: FAIL fork failed\n");
    exit(1);
  }
  if(cpid == 0){
    trace(1 << SYS_close);        // changes the CHILD's mask, and only it
    close(99);                    // -> one close line, from the child
    exit(0);
  }
  wait(0);
  printf("tracetest: F child pid %d\n", cpid);
  getpid();                       // -> one getpid line, from the parent
  close(99);                      // the parent's mask says nothing of close
  printf("tracetest: F end\n");

  printf("tracetest: done\n");
  exit(0);
}
