#ifndef PSTAT_H
#define PSTAT_H

// Per-process scheduling statistics returned by getpstat() (Lab 3, Task 0).
//
// Every array is indexed by process-table slot (0 .. NPROC-1); use inuse[i]
// to tell whether slot i currently holds a process. NPROC must be visible
// before this header is included: include "kernel/param.h" first (both the
// kernel and the user-space pstat tool do this).
struct pstat {
  int inuse[NPROC];    // 1 if this slot holds a process (state != UNUSED)
  int pid[NPROC];      // process id
  int state[NPROC];    // procstate enum value (UNUSED..ZOMBIE)
  int ticks[NPROC];    // CPU ticks the process has been scheduled (run time)
  int priority[NPROC]; // static priority (Task 1): 0 = high .. 9 = low
  int tickets[NPROC];  // lottery tickets (Task 2)
  int level[NPROC];    // MLFQ queue level (Task 3): 0 = top (highest priority)
  int ctime[NPROC];    // creation time, in ticks
  int stime[NPROC];    // first-scheduled time, in ticks (-1 if never run)
  int etime[NPROC];    // completion (exit) time, in ticks (0 if still alive)
};

#endif // PSTAT_H
