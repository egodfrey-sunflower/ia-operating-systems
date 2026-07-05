// schedload -- a mixed scheduling workload generator (Lab 3, Task 0).
//
//   schedload [ncpu] [nio] [ticks]
//
// Spawns `ncpu` CPU-bound children (tight compute loop) and `nio`
// "I/O-bound" children (short compute, then block on pause(1)), each running
// for about `ticks` clock ticks. Every child reports, at exit, its wall time
// and its accumulated CPU run time (from getpstat), so you can compare how the
// current scheduler shares the CPU. Defaults: 2 CPU-bound, 2 I/O-bound, 30
// ticks.
//
// Use `pstat` in another shell, or just read the per-child report lines, to
// see tick shares under SCHED=RR / PRIO / LOTTERY / MLFQ.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

// Return this process's accumulated run-time ticks, or -1 if getpstat() is
// not implemented yet.
static int
my_rtime(void)
{
  struct pstat st;
  int pid = getpid();

  if (getpstat(&st) < 0)
    return -1;
  for (int i = 0; i < NPROC; i++)
    if (st.inuse[i] && st.pid[i] == pid)
      return st.ticks[i];
  return -1;
}

static void
cpu_child(int dur)
{
  int start = uptime();
  int deadline = start + dur;
  volatile unsigned long x = 0;

  while (uptime() < deadline) {
    for (int i = 0; i < 200000; i++)
      x += i;
  }
  printf("schedload: pid=%d kind=CPU wall=%d rtime=%d\n", getpid(),
         uptime() - start, my_rtime());
  exit(0);
}

static void
io_child(int dur)
{
  int start = uptime();
  int deadline = start + dur;
  volatile unsigned long x = 0;

  while (uptime() < deadline) {
    for (int i = 0; i < 5000; i++) // short compute burst (sub-tick)
      x += i;
    pause(1); // then block, like an I/O wait
  }
  printf("schedload: pid=%d kind=IO  wall=%d rtime=%d\n", getpid(),
         uptime() - start, my_rtime());
  exit(0);
}

int
main(int argc, char **argv)
{
  int ncpu = 2, nio = 2, dur = 30;

  if (argc > 1)
    ncpu = atoi(argv[1]);
  if (argc > 2)
    nio = atoi(argv[2]);
  if (argc > 3)
    dur = atoi(argv[3]);
  if (dur <= 0)
    dur = 1;

  printf("schedload: %d CPU-bound + %d IO-bound children for ~%d ticks\n", ncpu,
         nio, dur);

  for (int i = 0; i < ncpu; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("schedload: fork failed\n");
      exit(1);
    }
    if (pid == 0)
      cpu_child(dur);
  }
  for (int i = 0; i < nio; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("schedload: fork failed\n");
      exit(1);
    }
    if (pid == 0)
      io_child(dur);
  }

  for (int i = 0; i < ncpu + nio; i++)
    wait(0);

  printf("schedload: done\n");
  exit(0);
}
