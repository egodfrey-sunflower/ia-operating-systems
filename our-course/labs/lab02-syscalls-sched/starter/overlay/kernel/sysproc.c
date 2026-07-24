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
// Lab 2. Four new system calls. Each one is a stub that fails; make it work.
//
// Reminders that apply to all four:
//   * arguments come out of the trapframe with argint()/argaddr()/argstr(),
//     never by dereferencing anything yourself;
//   * argaddr() gives you a NUMBER the user process chose. It is not a
//     pointer you may follow. copyout() is how the kernel writes to it.
// ---------------------------------------------------------------------------

// trace(mask) -- Part 2.
//
// Record the mask in the calling process and return 0. The printing itself
// belongs in syscall(), in kernel/syscall.c, not here: the whole point is
// that ONE hook in the dispatch path covers every system call there is.
uint64
sys_trace(void)
{
  // TODO Part 2
  return -1;
}

// sysinfo(struct sysinfo *) -- Part 3.
//
// Fill in freemem and nproc and copy the struct out to the user's address.
// Return 0, or -1 if the address is not one the kernel may write to --
// which is copyout()'s answer, not yours to work out.
//
// You need two counts that the kernel does not currently keep:
//
//   free memory  walk kmem.freelist in kernel/kalloc.c and multiply by
//                PGSIZE, holding kmem.lock. That has to be a new function in
//                kalloc.c, because kmem is static to that file.
//   processes    walk proc[] in kernel/proc.c counting states other than
//                UNUSED. LOCKING, GIVEN, because xv6's locking rules are a
//                later chapter and deriving this is not the exercise:
//
//                    for(p = proc; p < &proc[NPROC]; p++){
//                      acquire(&p->lock);
//                      if(p->state != UNUSED)
//                        n++;
//                      release(&p->lock);
//                    }
//
//                Acquire and release one p->lock at a time. Never hold two
//                at once, and never call this while already holding one --
//                that is a deadlock, and it is the reason the loop looks
//                like this rather than acquiring them all up front.
uint64
sys_sysinfo(void)
{
  // TODO Part 3
  return -1;
}

// settickets(n) -- Part 4.
//
// Set the calling process's ticket count. Return 0, or -1 if n is outside
// [1, MAX_TICKETS].
uint64
sys_settickets(void)
{
  // TODO Part 4
  return -1;
}

// getpinfo(struct pinfo *) -- Part 4.
//
// One entry per slot of the process table, in table order, copied out to the
// user. Return 0, or -1 on a bad address.
//
// struct pinfo is 1280 bytes and the kernel stack is a single 4096-byte
// page, most of which is already spoken for. Build the struct in a page from
// kalloc() and kfree() it before you return -- including on the error path.
//
// Same locking rule as sysinfo: one p->lock at a time.
uint64
sys_getpinfo(void)
{
  // TODO Part 4
  return -1;
}
