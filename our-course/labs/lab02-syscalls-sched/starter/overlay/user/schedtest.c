// schedtest [ticks] -- the Part 4 measurement program.
//
// GIVEN. Do not change it: tests/run.sh reads its output line by line, and
// compares the file itself against the copy in starter/overlay/.
//
// It does three things.
//
//   1. Checks that settickets() rejects a ticket count outside
//      [1, MAX_TICKETS] and accepts one inside it, and that getpinfo()
//      returns -1 rather than 0 for a pointer the kernel may not write
//      through.
//
//   2. Forks a child that does nothing and looks at it through getpinfo()
//      before anything has set its tickets. A child must be born holding
//      DEFAULT_TICKETS at priority 0. fork() deliberately does NOT copy the
//      parent's ticket count -- this parent holds a thousand of them -- so
//      the child's can only have come from allocproc(). A process born with
//      zero tickets can never win a lottery, and this is the only check in
//      the lab that looks at that initialisation directly.
//
//   3. Forks two children that do nothing but spin, gives one 30 tickets and
//      the other 10, lets them run for `ticks` timer ticks, and reports how
//      many times the scheduler chose each. It works on DIFFERENCES between
//      two getpinfo() snapshots, so the few ticks each child spends starting
//      up -- before it has called settickets() and while it still holds the
//      default allocation -- are outside the measurement window and cannot
//      bias the answer.
//
//   4. Does it again with two children holding ONE ticket each, over a
//      shorter window. At 30:10 an off-by-one in the ticket walk moves the
//      share by a few per mille and no test of any length could see it; at
//      1:1 the same off-by-one hands every draw to whichever of the two sits
//      first in the process table and the other is never selected at all.
//      "Both of them ran" is exact and is what this window is for. The SHARE
//      it also prints is not: this window is a quarter of the run length,
//      floored at 40 ticks, so it is the smallest sample the program takes
//      and the noisiest number it prints. At the default it is 100
//      selections; below it, 40, where one standard deviation is 79 per
//      mille and a correct kernel wanders a long way from 500.
//
// The measuring parent is asleep in pause() for the whole window, so it is
// not competing for the CPU in any interesting way. It nonetheless gives
// itself a large ticket count, for a reason worth understanding: a WRONG
// scheduler that simply always runs the highest-ticket runnable process
// looks superficially proportional, and would starve a parent holding ten
// tickets for ever. The run would then die at a deadline with no numbers in
// it, instead of finishing and printing the numbers that show what happened.
// A measuring instrument should not be starved by the thing it measures.
//
// Default is 400 ticks for the 30:10 window and a quarter of that, at least
// 40, for the 1:1 one. A tick is a tenth of a second of emulated time, so the
// whole program is about a minute. Run it shorter and you will see both
// shares wander, the 1:1 one much the further because its window is a quarter
// the size; that wandering is the subject of Part 5's write-up, not a bug.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/sched.h"
#include "kernel/pinfo.h"
#include "user/user.h"

#define TICKETS_A      30
#define TICKETS_B      10
#define TICKETS_PARENT 1000

static int
slot_of(struct pinfo *pi, int pid)
{
  int i;

  for(i = 0; i < NPINFO; i++)
    if(pi->inuse[i] && pi->pid[i] == pid)
      return i;
  return -1;
}

static void
spin(int tickets)
{
  volatile int x = 0;

  if(settickets(tickets) < 0){
    printf("schedtest: FAIL settickets rejected a valid ticket count\n");
    exit(1);
  }
  for(;;)
    x = x + 1;
}

int
main(int argc, char **argv)
{
  struct pinfo *p0, *p1;
  int dur = 400, warm, half, eq;
  int a, b, c, d, k, i, da, db;
  int a0, b0, a1, b1;

  if(argc > 1)
    dur = atoi(argv[1]);
  if(dur < 8)
    dur = 8;
  half = dur / 2;

  p0 = malloc(sizeof(*p0));         // 1280 bytes -- too big for the one-page
  p1 = malloc(sizeof(*p1));         // user stack, so it goes on the heap
  if(p0 == 0 || p1 == 0){
    printf("schedtest: FAIL out of memory\n");
    printf("schedtest: done\n");
    exit(1);
  }

  printf("schedtest: start ticks %d ratio %d:%d\n", dur, TICKETS_A, TICKETS_B);

  // 1. the argument check.
  if(settickets(0) != -1 || settickets(-5) != -1 ||
     settickets(MAX_TICKETS + 1) != -1){
    printf("schedtest: FAIL settickets accepted a ticket count outside "
           "[1, MAX_TICKETS]\n");
    printf("schedtest: done\n");
    exit(1);
  }
  if(settickets(TICKETS_PARENT) != 0){
    printf("schedtest: FAIL settickets rejected a valid ticket count\n");
    printf("schedtest: done\n");
    exit(1);
  }

  // getpinfo's trust boundary, the same rule and the same mechanism as
  // sysinfo's: 0x3f00000000 is below MAXVA and far above any heap, so the
  // page-table walk finds nothing there. A getpinfo that calls copyout and
  // ignores what it returns says 0 to this.
  if(getpinfo((struct pinfo *)0x3f00000000L) != -1)
    printf("schedtest: FAIL getpinfo accepted a pointer the kernel may not "
           "write through\n");

  // 2. what a process is born holding.
  if((k = fork()) == 0){
    pause(60);                      // still alive when the parent looks
    exit(0);
  }
  if(k < 0){
    printf("schedtest: FAIL fork failed\n");
    printf("schedtest: done\n");
    exit(1);
  }
  if(getpinfo(p0) != 0){
    printf("schedtest: FAIL getpinfo failed\n");
    kill(k); wait(0);
    printf("schedtest: done\n");
    exit(1);
  }
  if((i = slot_of(p0, k)) < 0){
    printf("schedtest: FAIL getpinfo does not report a newly forked child\n");
    kill(k); wait(0);
    printf("schedtest: done\n");
    exit(1);
  }
  printf("schedtest: newborn pid %d tickets %d prio %d\n",
         k, p0->tickets[i], p0->prio[i]);
  if(p0->tickets[i] != DEFAULT_TICKETS)
    printf("schedtest: FAIL a newly forked child does not hold "
           "DEFAULT_TICKETS\n");
  if(p0->prio[i] != 0)
    printf("schedtest: FAIL a newly forked child does not start at priority "
           "0\n");
  kill(k);
  wait(0);

  // 3. the measurement.
  if((a = fork()) == 0)
    spin(TICKETS_A);
  if((b = fork()) == 0)
    spin(TICKETS_B);
  if(a < 0 || b < 0){
    printf("schedtest: FAIL fork failed\n");
    printf("schedtest: done\n");
    exit(1);
  }
  printf("schedtest: A pid %d tickets %d\n", a, TICKETS_A);
  printf("schedtest: B pid %d tickets %d\n", b, TICKETS_B);

  warm = 10;                        // let both children call settickets
  pause(warm);
  if(getpinfo(p0) != 0){
    printf("schedtest: FAIL getpinfo failed\n");
    kill(a); kill(b);
    printf("schedtest: done\n");
    exit(1);
  }
  a0 = slot_of(p0, a);
  b0 = slot_of(p0, b);
  if(a0 < 0 || b0 < 0){
    printf("schedtest: FAIL getpinfo does not report both children\n");
    kill(a); kill(b);
    printf("schedtest: done\n");
    exit(1);
  }
  a0 = p0->ticks[a0];
  b0 = p0->ticks[b0];

  // A snapshot half way through, so the write-up can compare a short run
  // with a long one.
  pause(half);
  if(getpinfo(p1) == 0){
    a1 = slot_of(p1, a);
    b1 = slot_of(p1, b);
    if(a1 >= 0 && b1 >= 0){
      da = p1->ticks[a1] - a0;
      db = p1->ticks[b1] - b0;
      printf("schedtest: sample after %d ticks A %d B %d share %d per mille\n",
             half, da, db, (da + db) > 0 ? 1000 * da / (da + db) : -1);
    }
  }

  pause(dur - half);
  if(getpinfo(p1) != 0){
    printf("schedtest: FAIL getpinfo failed\n");
    kill(a); kill(b);
    printf("schedtest: done\n");
    exit(1);
  }
  a1 = slot_of(p1, a);
  b1 = slot_of(p1, b);
  if(a1 < 0 || b1 < 0){
    printf("schedtest: FAIL getpinfo does not report both children\n");
    kill(a); kill(b);
    printf("schedtest: done\n");
    exit(1);
  }
  da = p1->ticks[a1] - a0;
  db = p1->ticks[b1] - b0;

  printf("schedtest: result A pid %d tickets %d ticks %d\n", a, TICKETS_A, da);
  printf("schedtest: result B pid %d tickets %d ticks %d\n", b, TICKETS_B, db);
  printf("schedtest: share A %d per mille of %d selections (target %d)\n",
         (da + db) > 0 ? 1000 * da / (da + db) : -1, da + db,
         1000 * TICKETS_A / (TICKETS_A + TICKETS_B));

  kill(a);
  kill(b);
  wait(0);
  wait(0);

  // 4. the equal-ticket window. Same instrument, same difference-of-two-
  // snapshots method, two children holding one ticket each.
  eq = dur / 4;
  if(eq < 40)
    eq = 40;
  if((c = fork()) == 0)
    spin(1);
  if((d = fork()) == 0)
    spin(1);
  if(c < 0 || d < 0){
    printf("schedtest: FAIL fork failed\n");
    printf("schedtest: done\n");
    exit(1);
  }
  printf("schedtest: equal C pid %d D pid %d tickets 1 each, %d ticks\n",
         c, d, eq);

  pause(warm);
  if(getpinfo(p0) != 0){
    printf("schedtest: FAIL getpinfo failed\n");
    kill(c); kill(d);
    printf("schedtest: done\n");
    exit(1);
  }
  a0 = slot_of(p0, c);
  b0 = slot_of(p0, d);
  if(a0 < 0 || b0 < 0){
    printf("schedtest: FAIL getpinfo does not report both children\n");
    kill(c); kill(d);
    printf("schedtest: done\n");
    exit(1);
  }
  a0 = p0->ticks[a0];
  b0 = p0->ticks[b0];

  pause(eq);
  if(getpinfo(p1) != 0){
    printf("schedtest: FAIL getpinfo failed\n");
    kill(c); kill(d);
    printf("schedtest: done\n");
    exit(1);
  }
  a1 = slot_of(p1, c);
  b1 = slot_of(p1, d);
  if(a1 < 0 || b1 < 0){
    printf("schedtest: FAIL getpinfo does not report both children\n");
    kill(c); kill(d);
    printf("schedtest: done\n");
    exit(1);
  }
  da = p1->ticks[a1] - a0;
  db = p1->ticks[b1] - b0;

  printf("schedtest: result C pid %d tickets 1 ticks %d\n", c, da);
  printf("schedtest: result D pid %d tickets 1 ticks %d\n", d, db);
  printf("schedtest: share C %d per mille of %d selections (target 500)\n",
         (da + db) > 0 ? 1000 * da / (da + db) : -1, da + db);
  if(da + db > 0 && (da == 0 || db == 0))
    printf("schedtest: FAIL one of two children holding one ticket each was "
           "never selected\n");

  kill(c);
  kill(d);
  wait(0);
  wait(0);
  printf("schedtest: done\n");
  exit(0);
}
