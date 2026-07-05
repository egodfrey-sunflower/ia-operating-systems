// lotto -- exercises the lottery scheduler (Lab 3, Task 2).
//
//   lotto [window]
//
// Forks two identical CPU-bound spinners holding 30 and 10 lottery tickets:
// spinner A *inherits* its 30 tickets across fork() (the parent calls
// settickets(30) before forking and A never calls settickets itself), so a
// kfork() that fails to copy the parent's ticket count is caught here;
// spinner B sets its own 10. After `window` ticks (default 150) it reports
// the measured tick-share ratio, which should converge near 3:1:
//
//   LOTTERY ... ratio100=<r> VERDICT=PASS|FAIL   PASS iff 2.0 <= r/100 <= 4.5.
//
// Build the kernel with `make SCHED=LOTTERY`. Scheduling is stochastic, so use
// a long-enough window; under any other scheduler getpstat() returns -1 and
// the line reports FAIL.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

static int
ticks_of(int pid)
{
  struct pstat st;

  if (getpstat(&st) < 0)
    return -2;
  for (int i = 0; i < NPROC; i++)
    if (st.inuse[i] && st.pid[i] == pid)
      return st.ticks[i];
  return -1;
}

static void
spin_forever(void)
{
  volatile unsigned long x = 0;
  for (;;)
    x += 1;
}

int
main(int argc, char **argv)
{
  int win = 150;
  int t1 = 30, t2 = 10;

  if (argc > 1)
    win = atoi(argv[1]);

  // Spinner A INHERITS its t1 tickets: set them on ourselves BEFORE forking
  // and never call settickets() in the child. This deliberately exercises
  // kfork()'s ticket copy -- a kernel that gives forked children a default
  // ticket count instead of the parent's collapses A's share to
  // ~default:t2 and the ratio check below fails.
  settickets(t1);
  int a = fork();
  if (a == 0) {
    spin_forever();
    exit(0);
  }

  // Back in the parent: hold a large ticket count ourselves so that, once we
  // wake from pause(), we win the lottery promptly and can sample/stop the
  // spinners. While we sleep we hold no share, so the 3:1 contest is strictly
  // between the children. (Set AFTER forking A, so A keeps the inherited 30.)
  settickets(1000);

  int b = fork();
  if (b == 0) {
    settickets(t2);
    spin_forever();
    exit(0);
  }

  pause(win);
  int ca = ticks_of(a);
  int cb = ticks_of(b);

  kill(a);
  kill(b);
  wait(0);
  wait(0);

  if (ca < 0 || cb < 0) {
    printf("LOTTERY t1=%d t2=%d VERDICT=FAIL (getpstat unimplemented)\n", t1,
           t2);
    exit(0);
  }

  int r100 = cb > 0 ? (ca * 100) / cb : 9999;
  int pass = (r100 >= 200 && r100 <= 450);
  printf("LOTTERY t1=%d t2=%d ticks1=%d ticks2=%d ratio100=%d VERDICT=%s\n", t1,
         t2, ca, cb, r100, pass ? "PASS" : "FAIL");
  exit(0);
}
