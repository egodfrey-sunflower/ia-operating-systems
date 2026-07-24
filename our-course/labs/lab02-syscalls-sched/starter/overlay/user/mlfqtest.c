// mlfqtest [samples] -- the Part 5 probe.
//
// GIVEN. Do not change it: tests/run.sh reads its output line by line, and
// compares the file itself against the copy in starter/overlay/.
// Build the kernel with POLICY=mlfq before running it.
//
// Two CPU-bound children, sampled once a tick through getpinfo(), plus the
// sampling parent itself -- which blocks in pause() every tick and is
// therefore the interactive process the five rules exist to serve. Seven
// things have to be visible. (The second and third are graded as one case: a
// boost that lifts nobody and a boost that lifts one process of two are the
// same rule broken, seen twice.)
//
//   demotion   a child that never blocks works its way down to the bottom
//              queue and is seen there (Rule 4);
//   the boost  and then, WITHOUT ever having blocked, is later seen above
//              the bottom queue again. Nothing but a periodic boost can do
//              that, so this is the check that fails on an MLFQ with no
//              Rule 5 at all;
//   every process  BOTH children are seen above the bottom queue in the SAME
//              sample. Rule 5 lifts every process, not only the one that
//              happens to be running when it fires -- and only one of two
//              spinning children is ever the running one;
//   no charge for blocking  the parent, which gives the CPU up before its
//              tick is over on every single tick, is never charged an
//              allotment and so stays at the top level. A kernel that
//              charges the allotment where a process is CHOSEN rather than
//              where it gives the CPU up demotes the parent instead, which
//              is precisely backwards;
//   the allotments  a child takes 1+2+4 = 7 of its own ticks to sink after
//              each boost, not 3. A kernel that demotes on every tick and
//              never looks at MLFQ_ALLOTMENT still sinks and still comes
//              back up; it just gets to the bottom in a third of the time,
//              and spends a third as long above it.
//
//   Rule 3     and a process forked afterwards, into the process-table slot
//              a sunk child has just given back, starts at the TOP level
//              rather than inheriting the level that slot was left at.
//
//   the counter  and, from Part 4 rather than Part 5: this parent's own
//              ticks[] rises about once a sample, because it is chosen once
//              a sample. It is the one process in the lab for which "was
//              chosen" and "used a whole tick" are different events, so it
//              is the only place a ticks[] counted in yield() -- ticks
//              consumed -- can be told from one counted where scheduler()
//              sets a process RUNNING.
//
// With two spinners sharing the CPU, a child spends 2*(1+2+4) = 14 of every
// MLFQ_BOOST=40 ticks above the bottom queue, so roughly one sample in three
// catches it. The thresholds below are far looser than that: they are set to
// separate a working rule from a missing one, not to pin down a number.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/sched.h"
#include "kernel/pinfo.h"
#include "user/user.h"

#define MIN_ABOVE 3       // samples with child A above the bottom, after sinking
#define MIN_BOTH  5       // samples with BOTH children above it, after both sank
#define MIN_ABOVE_PCT 15  // per cent of all samples with child A above the
                          // bottom queue. The allotments put it near 28 per
                          // cent -- 55 or 56 samples of the default 200 -- and
                          // a kernel that demotes on every tick near 8 per cent
#define MIN_TOP_PCT   25  // per cent of samples with the sampling parent --
                          // which blocks every tick -- still at the top
                          // level. It should be nearly all of them

static int
prio_of(struct pinfo *pi, int pid)
{
  int i;

  for(i = 0; i < NPINFO; i++)
    if(pi->inuse[i] && pi->pid[i] == pid)
      return pi->prio[i];
  return -1;
}

static int
ticks_of(struct pinfo *pi, int pid)
{
  int i;

  for(i = 0; i < NPINFO; i++)
    if(pi->inuse[i] && pi->pid[i] == pid)
      return pi->ticks[i];
  return -1;
}

int
main(int argc, char **argv)
{
  struct pinfo *pi;
  volatile int x = 0;
  int nsamples = 200;
  int a, b, i, pa, pb, ps, failed = 0;
  int seen_bottom = 0, above_after = 0, minprio = NPRIO, maxprio = -1;
  int both_above = 0, either_above = 0, self_top = 0, taken = 0;
  int sank_b = 0;
  int self_first = -1, self_last = -1, self_picked = 0;
  int at[NPRIO];

  if(argc > 1)
    nsamples = atoi(argv[1]);
  if(nsamples < 80)
    nsamples = 80;
  for(i = 0; i < NPRIO; i++)
    at[i] = 0;

  pi = malloc(sizeof(*pi));
  if(pi == 0){
    printf("mlfqtest: FAIL out of memory\n");
    exit(1);
  }

  if((a = fork()) == 0){
    for(;;)
      x = x + 1;
  }
  if((b = fork()) == 0){
    for(;;)
      x = x + 1;
  }
  if(a < 0 || b < 0){
    printf("mlfqtest: FAIL fork failed\n");
    printf("mlfqtest: done\n");
    exit(1);
  }
  printf("mlfqtest: start children %d %d samples %d\n", a, b, nsamples);

  for(i = 0; i < nsamples; i++){
    pause(1);
    if(getpinfo(pi) != 0){
      printf("mlfqtest: FAIL getpinfo failed\n");
      kill(a); kill(b);
      wait(0); wait(0);
      printf("mlfqtest: done\n");
      exit(1);
    }
    pa = prio_of(pi, a);
    pb = prio_of(pi, b);
    ps = prio_of(pi, getpid());
    // This process is chosen at least once between one sample and the next --
    // it is asleep in pause() until the tick that wakes it, and it runs to
    // take this sample. So its own ticks[] has to rise about once a sample,
    // and it is the only process here for which "chosen" and "used a whole
    // tick" differ: it blocks again before its tick is out, every time.
    self_last = ticks_of(pi, getpid());
    if(self_first < 0)
      self_first = self_last;
    taken++;
    if(ps == 0)
      self_top++;
    if(pa < 0)
      continue;
    if(pa < minprio)
      minprio = pa;
    if(pa > maxprio)
      maxprio = pa;
    if(pa < NPRIO)
      at[pa]++;
    if(pa == NPRIO - 1)
      seen_bottom = 1;
    else if(seen_bottom)
      above_after++;
    if(pb == NPRIO - 1)
      sank_b = 1;
    // Both children have to have sunk before "both are above the bottom"
    // means anything: at the very start of the run neither has sunk yet.
    if(pb >= 0 && seen_bottom && sank_b){
      if(pa < NPRIO - 1 && pb < NPRIO - 1)
        both_above++;
      else if(pa < NPRIO - 1 || pb < NPRIO - 1)
        either_above++;
    }
  }

  kill(a);
  kill(b);
  wait(0);
  wait(0);

  printf("mlfqtest: prio seen min %d max %d; samples above the bottom queue after sinking: %d\n",
         minprio, maxprio, above_after);
  printf("mlfqtest: samples at each level 0..%d:", NPRIO - 1);
  for(i = 0; i < NPRIO; i++)
    printf(" %d", at[i]);
  printf("\n");
  printf("mlfqtest: both children above the bottom queue in %d of %d samples (only one of them in %d); the sampling parent was at the top level in %d\n",
         both_above, taken, either_above, self_top);
  if(self_first >= 0 && self_last >= self_first)
    self_picked = self_last - self_first;
  printf("mlfqtest: the sampling parent was selected %d time(s) over %d samples\n",
         self_picked, taken);

  // Rule 3: a new process enters at the TOP level. The children just reaped
  // ended up at the bottom, and allocproc hands out the lowest free slot, so
  // the fork below almost certainly takes one of those slots back. A kernel
  // that does not reset the level in allocproc reports the dead child's
  // level here -- which is the one moment in the lab where a stale field in
  // a recycled proc slot is visible at all.
  if((i = fork()) == 0){
    pause(60);
    exit(0);
  }
  if(i < 0){
    printf("mlfqtest: FAIL fork failed\n");
    printf("mlfqtest: done\n");
    exit(1);
  }
  if(getpinfo(pi) != 0){
    printf("mlfqtest: FAIL getpinfo failed\n");
    kill(i); wait(0);
    printf("mlfqtest: done\n");
    exit(1);
  }
  pa = prio_of(pi, i);
  printf("mlfqtest: newborn pid %d prio %d\n", i, pa);
  if(pa != 0){
    printf("mlfqtest: FAIL a newly forked process does not start at the top level\n");
    failed = 1;
  }
  kill(i);
  wait(0);

  if(self_top * 100 < taken * MIN_TOP_PCT){
    printf("mlfqtest: FAIL a process that blocks before its tick is up was demoted anyway\n");
    printf("mlfqtest: this program blocks in pause() on every one of its %d samples and so is almost never charged a whole tick, yet it was at the top level in only %d of them. Rule 4 charges the allotment where a process GIVES THE CPU UP, not where it is chosen.\n",
           taken, self_top);
    failed = 1;
  }
  if(self_picked * 4 < taken){
    printf("mlfqtest: FAIL the sampling parent's ticks[] barely moved, so ticks[] is not counting scheduling decisions\n");
    printf("mlfqtest: this program is chosen at least once for each of its %d samples -- it sleeps in pause() until a tick wakes it and then runs to take the sample -- yet getpinfo reports it selected only %d time(s). ticks[i] counts how many times scheduler() has CHOSEN process i: increment it where scheduler() sets a process RUNNING. Incremented in yield() instead, it counts whole ticks CONSUMED, and a process that gives the CPU up early never gets there.\n",
           taken, self_picked);
    failed = 1;
  }
  if(!seen_bottom){
    printf("mlfqtest: FAIL a process that never blocks was never demoted to the bottom queue\n");
    printf("mlfqtest: it was never seen at level %d; the furthest down it got was level %d. Rule 4 -- the allotment and the demotion -- is charged in yield().\n",
           NPRIO - 1, maxprio);
    printf("mlfqtest: done\n");
    exit(1);
  }
  if(above_after < MIN_ABOVE){
    printf("mlfqtest: FAIL once at the bottom the process never came back up\n");
    printf("mlfqtest: %d samples above the bottom queue after it sank, need %d. Nothing but Rule 5, the periodic boost, can lift a process that never blocks.\n",
           above_after, MIN_ABOVE);
    printf("mlfqtest: done\n");
    exit(1);
  }
  if(both_above < MIN_BOTH){
    printf("mlfqtest: FAIL the boost never lifted both children out of the bottom queue at once\n");
    printf("mlfqtest: %d samples with both above it, need %d; %d samples had exactly one of them above it, so something is lifting processes. Rule 5 lifts EVERY process, and only one of two spinning children is ever the one that is running when the boost fires.\n",
           both_above, MIN_BOTH, either_above);
    failed = 1;
  }
  if(above_after * 100 < taken * MIN_ABOVE_PCT){
    printf("mlfqtest: FAIL a process sinks to the bottom queue faster than its allotments allow\n");
    printf("mlfqtest: above the bottom queue in %d of %d samples, need %d per cent. The allotments are %d, %d and %d ticks, so after every boost a process that never blocks owns %d of its own ticks before it reaches the bottom -- twice that in samples here, because two children are sharing the CPU. A kernel that demotes on every tick regardless of MLFQ_ALLOTMENT gets there in %d.\n",
           above_after, taken, MIN_ABOVE_PCT,
           MLFQ_ALLOTMENT(0), MLFQ_ALLOTMENT(1), MLFQ_ALLOTMENT(2),
           MLFQ_ALLOTMENT(0) + MLFQ_ALLOTMENT(1) + MLFQ_ALLOTMENT(2),
           NPRIO - 1);
    failed = 1;
  }

  if(failed){
    printf("mlfqtest: done\n");
    exit(1);
  }

  printf("mlfqtest: OK\n");
  printf("mlfqtest: done\n");
  exit(0);
}
