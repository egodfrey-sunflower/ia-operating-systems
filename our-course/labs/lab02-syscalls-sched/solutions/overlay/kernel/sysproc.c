#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "sched.h"
#include "sysinfo.h"
#include "pinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// ---------------------------------------------------------------------------
// Lab 2. Four new system calls.
//
// The shape is the same every time: pull the arguments out of the trapframe
// with argint()/argaddr(), do the work, and hand anything that has to go
// back to the user to copyout(). argaddr() returns a NUMBER the user process
// chose. It is not a pointer the kernel may follow.
// ---------------------------------------------------------------------------

// trace(mask) -- Part 2. Record the mask; kernel/syscall.c does the printing.
uint64
sys_trace(void)
{
  int mask;

  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}

// sysinfo(struct sysinfo *) -- Part 3.
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  uint64 addr;

  argaddr(0, &addr);
  info.freemem = kfreemem();
  info.nproc = knproc();

  // The whole point of the part. `addr` is a number chosen by a user
  // process; it may be unmapped, it may be someone else's, it may be above
  // MAXVA, it may be the read-only text page. copyout() walks the process's
  // own page table to answer all of those, and returns -1 rather than
  // faulting in the kernel -- where a fault is a panic, not a signal.
  if(copyout(myproc()->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}

// settickets(n) -- Part 4.
uint64
sys_settickets(void)
{
  int n;

  argint(0, &n);
  if(n < 1 || n > MAX_TICKETS)
    return -1;

  acquire(&myproc()->lock);
  myproc()->tickets = n;
  release(&myproc()->lock);
  return 0;
}

// getpinfo(struct pinfo *) -- Part 4.
uint64
sys_getpinfo(void)
{
  struct pinfo *pi;
  uint64 addr;
  int r;

  argaddr(0, &addr);

  // struct pinfo is 1280 bytes and a kernel stack is one 4096-byte page,
  // most of it already spoken for by the trap path. Borrow a page instead.
  if((pi = (struct pinfo *)kalloc()) == 0)
    return -1;

  kgetpinfo(pi);
  r = copyout(myproc()->pagetable, addr, (char *)pi, sizeof(*pi));
  kfree((void *)pi);              // including on the error path
  return r < 0 ? -1 : 0;
}
