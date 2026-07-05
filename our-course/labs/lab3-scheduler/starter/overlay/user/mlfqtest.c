// mlfqtest -- exercises the multi-level feedback queue scheduler (Lab 3, Task
// 3).
//
//   mlfqtest [window]
//
// Prints two self-verifying verdict lines (window defaults to 60 ticks each):
//
//   MLFQ-RESPONSE ... VERDICT=PASS|FAIL   an I/O-bound process stays in a
//                                         higher queue than a CPU hog, so it
//                                         gets better response.
//   MLFQ-BOOST    ... VERDICT=PASS|FAIL   a CPU hog that sinks to the bottom
//                                         queue keeps accruing ticks (the
//                                         periodic priority boost un-starves
//                                         it), rather than being frozen out.
//
// Build the kernel with `make SCHED=MLFQ`. Under any other scheduler (or the
// stubbed starter) getpstat() returns -1 and both lines report FAIL.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

// Index of process `pid` in the pstat table, or -1.
static int
find(struct pstat *st, int pid)
{
  for (int i = 0; i < NPROC; i++)
    if (st->inuse[i] && st->pid[i] == pid)
      return i;
  return -1;
}

static void
cpu_hog(void)
{
  volatile unsigned long x = 0;
  for (;;)
    x += 1;
}

static void
io_proc(void)
{
  volatile unsigned long x = 0;
  for (;;) {
    for (int i = 0; i < 3000; i++) // sub-tick compute
      x += 1;
    pause(1); // block, so it stays near the top queue
  }
}

int
main(int argc, char **argv)
{
  int win = 60;
  if (argc > 1)
    win = atoi(argv[1]);

  // ---------------- Part A: I/O response vs CPU hog ----------------
  int hog = fork();
  if (hog == 0)
    cpu_hog();
  int io = fork();
  if (io == 0)
    io_proc();

  pause(win);

  struct pstat st;
  int ok = (getpstat(&st) == 0);
  int io_level = -1, hog_level = -1, io_ticks = -1, hog_ticks = -1;
  // Sample the queue levels TWICE, a few ticks apart, judging the hog by the
  // deepest (max) level seen and the I/O proc by the shallowest (min). A
  // single sample can land in the instant right after a periodic priority
  // boost, when even a lifelong CPU hog momentarily sits in the top queue --
  // that was a rare (~1-in-1e3) source of false FAILs.
  for (int s = 0; ok && s < 2; s++) {
    if (s > 0) {
      pause(3);
      if (getpstat(&st) != 0)
        break;
    }
    int hi = find(&st, hog), ii = find(&st, io);
    if (hi >= 0) {
      if (st.level[hi] > hog_level)
        hog_level = st.level[hi];
      hog_ticks = st.ticks[hi];
    }
    if (ii >= 0) {
      if (io_level < 0 || st.level[ii] < io_level)
        io_level = st.level[ii];
      io_ticks = st.ticks[ii];
    }
  }
  kill(hog);
  kill(io);
  wait(0);
  wait(0);

  if (!ok) {
    printf("MLFQ-RESPONSE VERDICT=FAIL (getpstat unimplemented)\n");
  } else {
    // I/O proc should sit in a strictly higher-priority queue (lower level)
    // than the demoted CPU hog.
    int passA = (io_level >= 0 && hog_level >= 1 && io_level < hog_level);
    printf("MLFQ-RESPONSE io_level=%d hog_level=%d io_ticks=%d hog_ticks=%d "
           "VERDICT=%s\n",
           io_level, hog_level, io_ticks, hog_ticks, passA ? "PASS" : "FAIL");
  }

  // ---------------- Part B: starvation recovery via boost ----------------
  // A CPU hog (`low`) sinks to the bottom queue. Two I/O-ish children provide
  // higher-priority pressure. With periodic boosts, `low` must keep making
  // progress and be observed at the bottom queue (i.e. it really was demoted).
  int low = fork();
  if (low == 0)
    cpu_hog();
  int p1 = fork();
  if (p1 == 0)
    io_proc();
  int p2 = fork();
  if (p2 == 0)
    io_proc();

  int bottom_seen = 0;
  int low_start = -1, low_end = -1;
  int nsamp = 6;
  int step = win / nsamp;
  if (step <= 0)
    step = 1;
  for (int s = 0; s < nsamp; s++) {
    pause(step);
    if (getpstat(&st) == 0) {
      int li = find(&st, low);
      if (li >= 0) {
        if (st.level[li] >= 2)
          bottom_seen = 1;
        if (low_start < 0)
          low_start = st.ticks[li];
        low_end = st.ticks[li];
      }
    }
  }
  kill(low);
  kill(p1);
  kill(p2);
  wait(0);
  wait(0);
  wait(0);

  if (low_start < 0) {
    printf("MLFQ-BOOST VERDICT=FAIL (getpstat unimplemented)\n");
  } else {
    // Demoted to the bottom queue at some point, yet still gaining ticks by
    // the end of the run: the boost keeps it from starving.
    int passB = (bottom_seen && low_end > low_start);
    printf("MLFQ-BOOST bottom_seen=%d low_start=%d low_end=%d VERDICT=%s\n",
           bottom_seen, low_start, low_end, passB ? "PASS" : "FAIL");
  }

  exit(0);
}
