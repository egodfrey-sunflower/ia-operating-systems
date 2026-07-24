// sysinfotest -- exercises the Part 3 sysinfo() system call.
//
// GIVEN. Do not change it: tests/run.sh matches its output.
//
// Unlike tracetest, this one can check itself: everything it needs to know
// comes back through the struct it passed in. It prints one
// "sysinfotest: FAIL <reason>" line per failure and "sysinfotest: OK" only
// if there were none.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

#define PGSIZE   4096
#define NPAGES   64      // 256 KiB -- far more than any rounding slack
#define NKIDS     5

static int fails = 0;

static void
bad(char *why)
{
  printf("sysinfotest: FAIL %s\n", why);
  fails++;
}

int
main(void)
{
  struct sysinfo a, b, c;
  char *p;
  int i;

  printf("sysinfotest: start\n");

  // 1 -- the plain call works and the numbers are not nonsense.
  if(sysinfo(&a) != 0){
    bad("sysinfo returned non-zero for a valid pointer");
  } else {
    printf("sysinfotest: freemem %d nproc %d\n", (int)a.freemem, (int)a.nproc);
    if(a.freemem == 0)
      bad("freemem is zero");
    if(a.freemem % PGSIZE != 0)
      bad("freemem is not a multiple of the page size");
    if(a.nproc < 2 || a.nproc > NPROC)
      bad("nproc is outside 2..NPROC");
  }

  // 2 -- allocating memory must be visible in freemem.
  if(sysinfo(&a) != 0)
    bad("sysinfo failed before the allocation");
  p = sbrk(NPAGES * PGSIZE);
  if(p == SBRK_ERROR){
    bad("sbrk failed; cannot test freemem");
  } else {
    for(i = 0; i < NPAGES; i++)     // touch each page, in case of lazy sbrk
      p[i * PGSIZE] = 1;
    if(sysinfo(&b) != 0)
      bad("sysinfo failed after the allocation");
    // Unsigned arithmetic: check the direction before taking a difference,
    // or a rise in freemem wraps round and looks like an enormous fall.
    else if(b.freemem > a.freemem)
      bad("freemem went UP after allocating memory");
    else if(a.freemem - b.freemem < (uint64)NPAGES * PGSIZE)
      bad("freemem fell by less than the memory just allocated");

    // 3 -- and freeing it must be visible too.
    sbrk(-(NPAGES * PGSIZE));
    if(sysinfo(&c) != 0)
      bad("sysinfo failed after freeing");
    else if(c.freemem <= b.freemem)
      bad("freemem did not rise again after the memory was freed");
  }

  // 4 -- nproc counts processes, exactly. Exact equality is only safe
  // because nothing else on this machine creates or reaps a process while
  // the test runs: the harness boots one kernel, runs one program, and the
  // only other processes are init and the shell, both of which are asleep.
  // A later part that adds background processes must relax this to a range.
  if(sysinfo(&a) != 0)
    bad("sysinfo failed before forking");
  for(i = 0; i < NKIDS; i++){
    int pid = fork();
    if(pid < 0){
      bad("fork failed");
      break;
    }
    if(pid == 0){
      pause(20);
      exit(0);
    }
  }
  pause(2);                          // let them all reach the process table
  if(sysinfo(&b) != 0)
    bad("sysinfo failed with children running");
  else if(b.nproc != a.nproc + NKIDS)
    bad("nproc did not rise by exactly the number of children forked");
  for(i = 0; i < NKIDS; i++)
    wait(0);
  pause(2);
  if(sysinfo(&c) != 0)
    bad("sysinfo failed after the children exited");
  else if(c.nproc != a.nproc)
    bad("nproc did not return to its old value once the children were reaped");

  // 5 -- the trust boundary. A user pointer is a number the process chose,
  // and the kernel has to treat it as one. None of these may succeed, and
  // none of them may take the kernel down.
  if(sysinfo((struct sysinfo *)0xffffffffffff0000L) != -1)
    bad("an address above the top of the address space was accepted");
  // 0x3f00000000 is below MAXVA (1L << 38) and far above any p->sz, so the
  // walk finds an invalid PTE. Do NOT use 0x3fffffe000 here: that address is
  // exactly TRAPFRAME, which is MAPPED in every user page table -- it is
  // rejected on PTE_U, not on validity, which is a different lesson.
  if(sysinfo((struct sysinfo *)0x3f00000000L) != -1)
    bad("an unmapped address below MAXVA was accepted");
  if(sysinfo((struct sysinfo *)0) != -1)
    bad("address 0 -- the read-only text page -- was accepted");

  if(fails == 0)
    printf("sysinfotest: OK\n");
  else
    printf("sysinfotest: %d failure(s)\n", fails);
  printf("sysinfotest: done\n");
  exit(fails == 0 ? 0 : 1);
}
