// Lab 4, Part 3: lazy heap allocation.
//
// Self-checking. Every failed check prints one "lazytests: FAIL <reason>"
// line; "lazytests: OK" is printed ONLY if every check passed, so a run that
// crashes cannot be read as a pass. Children signal through their exit status.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"       // PGSIZE
#include "user/user.h"

// Count the free physical pages, by allocating with (eager) sbrk() and
// touching each page until the kernel runs out, then giving them all back.
// The process is left exactly as it was found. This is the same trick
// usertests uses.
static int
countfree(void)
{
  uint64 sz0 = (uint64)sbrk(0);
  int n = 0;
  while(1){
    char *a = sbrk(PGSIZE);
    if(a == SBRK_ERROR)
      break;
    *(volatile char*)a = 1;   // touch it, so the page is really allocated
    n++;
  }
  sbrk(-(int)((uint64)sbrk(0) - sz0));   // restore the break exactly
  return n;
}

// A lazily-grown region must not consume physical pages until it is touched.
static int
test_is_lazy(void)
{
  int fails = 0;
  int npages = 128;

  int f0 = countfree();
  char *a = sbrklazy(npages * PGSIZE);
  if(a == SBRK_ERROR){
    printf("lazytests: FAIL sbrklazy() failed\n");
    return 1;
  }
  int f1 = countfree();                       // WITHOUT touching the region
  sbrklazy(-(npages * PGSIZE));               // give it back untouched

  int drop = f0 - f1;
  if(drop > npages / 2){
    printf("lazytests: FAIL growing the heap lazily consumed %d physical "
           "page(s) before any were touched; sbrk must allocate on the "
           "fault, not on the call\n", drop);
    fails++;
  }
  return fails;
}

// A freshly faulted-in lazy page must be ZEROED. The handout promises it, and
// a BSS or a calloc-style allocation depends on it. Read a fresh page BEFORE
// writing anything to it, so the first access is the load that faults it in; a
// handler that allocates but never memset()s the page returns kalloc()'s junk
// and this reads non-zero.
static int
test_fresh_page_zeroed(void)
{
  int fails = 0;
  int n = 4 * PGSIZE;
  char *a = sbrklazy(n);
  if(a == SBRK_ERROR){
    printf("lazytests: FAIL sbrklazy() failed\n");
    return 1;
  }
  for(int i = 0; i < n; i++){
    if(a[i] != 0){
      printf("lazytests: FAIL a freshly faulted lazy page was not zeroed "
             "(offset %d read 0x%x); the fault handler returned the page "
             "without memset-ing it to zero\n", i, a[i] & 0xff);
      fails++;
      break;
    }
  }
  sbrk(-n);
  return fails;
}

// A lazily-grown region must fault in and read/write correctly when touched.
static int
test_alloc_readback(void)
{
  int fails = 0;
  int n = 64 * PGSIZE;
  char *a = sbrklazy(n);
  if(a == SBRK_ERROR){
    printf("lazytests: FAIL sbrklazy() failed\n");
    return 1;
  }
  for(int i = 0; i < n; i++)
    a[i] = (char)(i * 7 + 3);
  for(int i = 0; i < n; i++){
    if(a[i] != (char)(i * 7 + 3)){
      printf("lazytests: FAIL a lazily-allocated page did not read back what "
             "was written (offset %d)\n", i);
      fails++;
      break;
    }
  }
  sbrk(-n);
  return fails;
}

// A page that has been made valid by sbrklazy() but never touched by the
// process must still work when the FIRST access to it is the kernel's, inside
// a system call (here, write() reading from the buffer via copyin()).
static int
test_syscall_faults_in(void)
{
  int fails = 0;
  int fds[2];
  if(pipe(fds) < 0){
    printf("lazytests: FAIL pipe() failed\n");
    return 1;
  }
  char *buf = sbrklazy(PGSIZE);            // freshly valid, never touched
  if(buf == SBRK_ERROR){
    printf("lazytests: FAIL sbrklazy() failed\n");
    close(fds[0]); close(fds[1]);
    return 1;
  }
  int nw = write(fds[1], buf, 64);         // kernel copyin() touches buf first
  if(nw != 64){
    printf("lazytests: FAIL write() from an untouched lazy page returned %d, "
           "not 64; the kernel's access to it was not faulted in\n", nw);
    fails++;
  }
  char tmp[64];
  read(fds[0], tmp, 64);
  close(fds[0]);
  close(fds[1]);
  sbrk(-PGSIZE);
  return fails;
}

// An access OUTSIDE the process's allocated region must be fatal, not quietly
// satisfied by allocating a page. Run in a child, which must be killed.
static int
test_out_of_range_is_fatal(void)
{
  int fails = 0;
  int st;
  int pid = fork();
  if(pid < 0){
    printf("lazytests: FAIL fork failed\n");
    return 1;
  }
  if(pid == 0){
    // Well above the current break, far below MAXVA: no valid region here.
    volatile char *bad = (volatile char *)((uint64)sbrk(0) + 0x4000000L);
    volatile char c = *bad;                // must fault and kill us
    (void)c;
    exit(0);                               // reached only if it did NOT fault
  }
  wait(&st);
  if(st == 0){
    printf("lazytests: FAIL an access far outside the heap was satisfied "
           "instead of killing the process; the fault handler is not "
           "checking the address against the process size\n");
    fails++;
  }
  return fails;
}

int
main(int argc, char *argv[])
{
  int fails = 0;

  printf("lazytests: starting\n");

  fails += test_is_lazy();
  fails += test_fresh_page_zeroed();
  fails += test_alloc_readback();
  fails += test_syscall_faults_in();
  fails += test_out_of_range_is_fatal();

  if(fails == 0)
    printf("lazytests: OK\n");
  // Completion marker, printed only after every check has run. It is NOT a
  // success token (that is "lazytests: OK"); it merely says the program was not
  // killed part way through, so the harness can tell a failed check from a
  // crashed kernel. No abort path reaches it.
  printf("lazytests: done\n");
  exit(0);
}
