// pstat -- print the per-process scheduling statistics table (Lab 3, Task 0).
//
// Calls getpstat() and prints one row per in-use process slot. Handy for
// eyeballing tick shares, priorities/tickets, and MLFQ levels while a
// workload (e.g. schedload) runs.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

static char *
statename(int s)
{
  switch (s) {
  case 0:  return "unused";
  case 1:  return "used";
  case 2:  return "sleep";
  case 3:  return "runble";
  case 4:  return "run";
  case 5:  return "zombie";
  default: return "?";
  }
}

int
main(void)
{
  struct pstat st;

  if (getpstat(&st) < 0) {
    printf("pstat: getpstat() failed -- is it implemented yet? (Task 0)\n");
    exit(1);
  }

  printf("pid\tstate\tprio\ttickets\tlevel\tticks\tctime\tstime\tetime\n");
  for (int i = 0; i < NPROC; i++) {
    if (!st.inuse[i])
      continue;
    printf("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", st.pid[i],
           statename(st.state[i]), st.priority[i], st.tickets[i], st.level[i],
           st.ticks[i], st.ctime[i], st.stime[i], st.etime[i]);
  }
  exit(0);
}
