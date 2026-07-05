// priotest -- exercises the static-priority scheduler (Lab 3, Task 1).
//
//   priotest [window]
//
// Prints two self-verifying verdict lines (window defaults to 40 ticks):
//
//   PRIOSTARVE ... VERDICT=PASS|FAIL   high-priority spinner starves a
//                                      low-priority spinner (one CPU).
//   PRIOEQUAL  ... VERDICT=PASS|FAIL   two equal-priority spinners get
//                                      roughly equal shares (round robin).
//
// Build the kernel with `make SCHED=PRIO`. Under any other scheduler (or the
// stubbed starter) getpstat() returns -1 and both lines report FAIL.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

// Return the run-time ticks of process `pid`, -1 if not found, or -2 if
// getpstat() is unimplemented.
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

// Fork two spinners with priorities pa/pb, let them run for `win` ticks, then
// sample their tick counts and stop them.
static void
run_pair(int pa, int pb, int win, int *ta, int *tb)
{
  int a = fork();
  if (a == 0) {
    setpriority(getpid(), pa);
    spin_forever();
    exit(0);
  }
  int b = fork();
  if (b == 0) {
    setpriority(getpid(), pb);
    spin_forever();
    exit(0);
  }

  pause(win);
  *ta = ticks_of(a);
  *tb = ticks_of(b);

  kill(a);
  kill(b);
  wait(0);
  wait(0);
}

int
main(int argc, char **argv)
{
  int win = 40;
  if (argc > 1)
    win = atoi(argv[1]);

  // Run at the highest priority ourselves so we always wake up to sample and
  // to stop the spinners, rather than being starved by the high-priority one.
  setpriority(getpid(), 0);

  // --- Starvation: priority 1 (high) vs priority 9 (low) ---
  int ha = 0, lb = 0;
  run_pair(1, 9, win, &ha, &lb);
  if (ha < 0 || lb < 0) {
    printf("PRIOSTARVE high=%d low=%d VERDICT=FAIL (getpstat unimplemented)\n",
           ha, lb);
  } else {
    int total = ha + lb;
    int pct = total > 0 ? (ha * 100) / total : 0;
    int pass = (pct >= 90) && (ha > lb);
    printf("PRIOSTARVE high=%d low=%d high_share=%d%% VERDICT=%s\n", ha, lb, pct,
           pass ? "PASS" : "FAIL");
  }

  // --- Round robin among equals: both priority 4 ---
  int ea = 0, eb = 0;
  run_pair(4, 4, win, &ea, &eb);
  if (ea < 0 || eb < 0) {
    printf("PRIOEQUAL a=%d b=%d VERDICT=FAIL (getpstat unimplemented)\n", ea,
           eb);
  } else {
    int hi = ea > eb ? ea : eb;
    int lo = ea > eb ? eb : ea;
    int pass = (lo > 0) && (hi <= lo * 3); // roughly balanced
    printf("PRIOEQUAL a=%d b=%d VERDICT=%s\n", ea, eb, pass ? "PASS" : "FAIL");
  }

  exit(0);
}
