// The structure getpinfo(2) fills in: one entry per slot of the kernel's
// process table, in table order. GIVEN -- do not change the layout; the
// supplied user programs user/schedtest.c and user/mlfqtest.c depend on it.
//
// Included by BOTH kernel and user code, so it includes nothing itself.

#define NPINFO 64        // must equal NPROC in kernel/param.h

struct pinfo {
  int inuse[NPINFO];     // 1 if this slot's state is not UNUSED
  int pid[NPINFO];       // process id, or 0
  int tickets[NPINFO];   // lottery tickets held
  int ticks[NPINFO];     // times the scheduler has chosen this process
  int prio[NPINFO];      // MLFQ priority level; 0 under the other policies
};
