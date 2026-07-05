#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "pstat.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// ---------------------------------------------------------------------------
// Lab 3: compile-time scheduler selection.
//
// The Makefile passes exactly one of -DSCHED_RR / -DSCHED_PRIO /
// -DSCHED_LOTTERY / -DSCHED_MLFQ (default RR). RR is the stock round-robin
// loop, preserved byte-for-byte in behaviour. The other three replace the
// pick-a-process logic in scheduler(). Accounting (rtime/ctime/stime/etime)
// and getpstat() are policy-independent and always compiled in.
// ---------------------------------------------------------------------------

#if defined(SCHED_PRIO)
#define SCHED_NAME "PRIO"
#define SCHEDKEY(p) ((p)->priority) // lower = higher priority
#elif defined(SCHED_LOTTERY)
#define SCHED_NAME "LOTTERY"
#elif defined(SCHED_MLFQ)
#define SCHED_NAME "MLFQ"
#define SCHEDKEY(p) ((p)->mlfq_level) // lower level = higher priority
#else
#define SCHED_NAME "RR"
#endif

#if defined(SCHED_PRIO) || defined(SCHED_MLFQ)
// Monotonic dispatch counter. Each time a process is scheduled it stamps
// p->lastrun with the next value; to round-robin among equal-priority (PRIO)
// or same-level (MLFQ) runnable processes we simply pick the one with the
// smallest stamp (i.e. the one that has waited longest). Unlike a single
// shared cursor, this stays fair even when a higher-priority process (e.g. one
// that wakes every tick) is interleaved.
static uint sched_seq = 0;
#endif

#if defined(SCHED_LOTTERY)
// A tiny deterministic xorshift64 PRNG for holding lotteries. Seeded to a
// fixed constant so runs are reproducible.
static uint64 rng_state = 0x2545F4914F6CDD1DULL;

static uint
rand_next(void)
{
  uint64 x = rng_state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rng_state = x;
  return (uint)(x & 0x7fffffff);
}
#endif

#if defined(SCHED_MLFQ)
#define NMLFQ 3        // number of queue levels
#define MLFQ_BOOST 100 // priority-boost period, in ticks

// Time slice (in ticks) for each level: doubles as you go down.
static int
mlfq_quantum(int lvl)
{
  static const int q[NMLFQ] = { 1, 2, 4 };
  if (lvl < 0)
    lvl = 0;
  if (lvl >= NMLFQ)
    lvl = NMLFQ - 1;
  return q[lvl];
}

// Periodically move every process back to the top queue so that jobs starved
// at the bottom get a fresh chance -- this is what un-starves the Task-1-style
// low-priority spinner. Called from scheduler(); guarded so it fires roughly
// once per MLFQ_BOOST ticks.
static void
mlfq_maybe_boost(void)
{
  static uint last = 0;

  if ((uint)ticks - last < MLFQ_BOOST)
    return;
  last = ticks;
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state != UNUSED) {
      p->mlfq_level = 0;
      p->mlfq_used = 0;
    }
    release(&p->lock);
  }
}
#endif

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Lab 3: initialise scheduling policy inputs and accounting. New processes
  // start at the default priority, one ticket, and the top MLFQ level.
  p->priority = 5;
  p->tickets = 1;
  p->mlfq_level = 0;
  p->mlfq_used = 0;
  p->lastrun = 0;
  p->rtime = 0;
  p->ctime = ticks;
  p->stime = -1;
  p->etime = 0;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline,
               PTE_R | PTE_X) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE, (uint64)(p->trapframe),
               PTE_R | PTE_W) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0) {
    if (sz + n > TRAPFRAME) {
      return -1;
    }
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if (n < 0) {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0) {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // Lab 3: children inherit the parent's lottery tickets (as in the MIT lab)
  // and static priority. The MLFQ level is not inherited: a fresh process
  // starts at the top queue.
  np->tickets = p->tickets;
  np->priority = p->priority;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++) {
    if (pp->parent == p) {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  // Lab 3: record completion time for accounting.
  p->etime = ticks;

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;) {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      if (pp->parent == p) {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE) {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p)) {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); //DOC: wait-sleep
  }
}

// ---------------------------------------------------------------------------
// Lab 3 scheduling helpers.
// ---------------------------------------------------------------------------

// Charge the currently-running process one tick of CPU. Called from the timer
// interrupt (trap.c) just before yield(). Under MLFQ it also ages the process:
// once it has spent a full quantum at its level, it is demoted one level.
//
// Runs in interrupt context with no lock held; the running CPU is the only
// writer of its own proc's rtime/mlfq_used, so the lock-free bumps are safe.
void
sched_charge(void)
{
  struct proc *p = myproc();
  if (p == 0)
    return;

  p->rtime++;

#if defined(SCHED_MLFQ)
  p->mlfq_used++;
  if (p->mlfq_used >= mlfq_quantum(p->mlfq_level)) {
    if (p->mlfq_level < NMLFQ - 1)
      p->mlfq_level++;
    p->mlfq_used = 0;
  }
#endif
}

// settickets(n): set the calling process's lottery-ticket count (>= 1).
int
ksettickets(int n)
{
  struct proc *p = myproc();
  if (n < 1)
    n = 1;
  acquire(&p->lock);
  p->tickets = n;
  release(&p->lock);
  return 0;
}

// setpriority(pid, prio): set process `pid`'s static priority (0 high .. 9
// low). Returns 0 on success, -1 if prio is out of range or no such process.
int
ksetpriority(int pid, int prio)
{
  struct proc *p;

  if (prio < 0 || prio > 9)
    return -1;
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state != UNUSED && p->pid == pid) {
      p->priority = prio;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// getpstat(): fill a user struct pstat with a snapshot of every slot's
// scheduling stats. `addr` is the user-space destination pointer. The staging
// buffer is a file-static (it is ~2.5 KB, too big for the kernel stack).
static struct pstat kstat;

int
kgetpstat(uint64 addr)
{
  struct proc *p = myproc();
  struct proc *q;
  int i;

  for (i = 0, q = proc; q < &proc[NPROC]; q++, i++) {
    acquire(&q->lock);
    kstat.inuse[i] = (q->state != UNUSED);
    kstat.pid[i] = q->pid;
    kstat.state[i] = q->state;
    kstat.ticks[i] = q->rtime;
    kstat.priority[i] = q->priority;
    kstat.tickets[i] = q->tickets;
    kstat.level[i] = q->mlfq_level;
    kstat.ctime[i] = q->ctime;
    kstat.stime[i] = q->stime;
    kstat.etime[i] = q->etime;
    release(&q->lock);
  }

  if (copyout(p->pagetable, addr, (char *)&kstat, sizeof(kstat)) < 0)
    return -1;
  return 0;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  static int announced = 0;
  if (!announced) {
    announced = 1;
    printk("scheduler: %s\n", SCHED_NAME);
  }

  c->proc = 0;
  for (;;) {
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    intr_on();
    intr_off();

    int found = 0;

#if defined(SCHED_MLFQ)
    mlfq_maybe_boost();
#endif

#if defined(SCHED_PRIO) || defined(SCHED_MLFQ)
    // Pass 1: find the best (lowest) key among RUNNABLE procs.
    int best = -1;
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        int key = SCHEDKEY(p);
        if (best < 0 || key < best)
          best = key;
      }
      release(&p->lock);
    }
    // Pass 2: among procs with that key, choose the least-recently-run one
    // (smallest lastrun stamp) for round-robin fairness within the class.
    if (best >= 0) {
      int bestidx = -1;
      uint bestseq = 0;
      for (int idx = 0; idx < NPROC; idx++) {
        p = &proc[idx];
        acquire(&p->lock);
        if (p->state == RUNNABLE && SCHEDKEY(p) == best) {
          if (bestidx < 0 || (uint)p->lastrun < bestseq) {
            bestidx = idx;
            bestseq = (uint)p->lastrun;
          }
        }
        release(&p->lock);
      }
      if (bestidx >= 0) {
        p = &proc[bestidx];
        acquire(&p->lock);
        if (p->state == RUNNABLE && SCHEDKEY(p) == best) {
          p->lastrun = ++sched_seq;
          if (p->stime < 0)
            p->stime = ticks;
          p->state = RUNNING;
          c->proc = p;
          swtch(&c->context, &p->context);
          c->proc = 0;
          found = 1;
        }
        release(&p->lock);
      }
    }
#elif defined(SCHED_LOTTERY)
    // Sum the tickets held by RUNNABLE procs, draw a winner, then run it.
    int total = 0;
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
        total += (p->tickets > 0 ? p->tickets : 1);
      release(&p->lock);
    }
    if (total > 0) {
      int draw = (int)(rand_next() % (uint)total);
      for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state == RUNNABLE) {
          int t = (p->tickets > 0 ? p->tickets : 1);
          if (draw < t) {
            if (p->stime < 0)
              p->stime = ticks;
            p->state = RUNNING;
            c->proc = p;
            swtch(&c->context, &p->context);
            c->proc = 0;
            found = 1;
            release(&p->lock);
            break;
          }
          draw -= t;
        }
        release(&p->lock);
      }
    }
#else // SCHED_RR: stock round robin (unchanged behaviour).
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        if (p->stime < 0)
          p->stime = ticks;
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
#endif

    if (found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched RUNNING");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){"/init", 0});
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

#if defined(SCHED_MLFQ)
  // Lab 3: a process that blocks voluntarily (e.g. for I/O) before using up
  // its quantum stays at its current level -- clear the partial quantum so it
  // is not penalised. This is what keeps interactive/I/O-bound jobs near the
  // top of the MLFQ.
  p->mlfq_used = 0;
#endif

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    if (p != myproc()) {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING) {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst) {
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src) {
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
    // clang-format off
    [UNUSED]    "unused",
    [USED]      "used",
    [SLEEPING]  "sleep ",
    [RUNNABLE]  "runble",
    [RUNNING]   "run   ",
    [ZOMBIE]    "zombie"
    // clang-format on
  };
  struct proc *p;
  char *state;

  printk("\n");
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printk("%d %s %s", p->pid, state, p->name);
    printk("\n");
  }
}
