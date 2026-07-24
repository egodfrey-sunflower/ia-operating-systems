// Lab 4, Part 4: copy-on-write fork.
//
// Self-checking. Every failed check prints one "cowtest: FAIL <reason>" line;
// "cowtest: OK" is printed ONLY if every check passed, and from nowhere else,
// so a run that crashes or is killed cannot be mistaken for a pass. Children
// signal their result through their exit status; the parent prints.
//
// The four checks, and what each is the ONLY one to catch:
//   simple    -- a COW write is private (isolation) and is not lost.
//   threeway  -- the reference count is right when THREE processes share.
//   bigfork   -- fork of a near-full address space succeeds: impossible if
//                fork copied eagerly, so this is the presence-of-COW test.
//   leak      -- free-page count returns to baseline across fork/exit cycles:
//                the only check that sees a reference count that is never
//                decremented (a leak) or double-decremented (a double free).

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"       // PGSIZE
#include "user/user.h"

static int
countfree(void)
{
  uint64 sz0 = (uint64)sbrk(0);
  int n = 0;
  while(1){
    char *a = sbrk(PGSIZE);
    if(a == SBRK_ERROR)
      break;
    *(volatile char*)a = 1;
    n++;
  }
  sbrk(-(int)((uint64)sbrk(0) - sz0));
  return n;
}

// A COW write must be private (parent unaffected) and must not be lost.
static int
test_simple(void)
{
  int fails = 0;
  int st;
  int n = 16 * PGSIZE;
  char *m = sbrk(n);
  if(m == SBRK_ERROR){
    printf("cowtest: FAIL sbrk failed in simple\n");
    return 1;
  }
  for(int i = 0; i < n; i++)
    m[i] = (char)(i % 251);          // parent's pattern

  int pid = fork();
  if(pid < 0){
    printf("cowtest: FAIL fork failed in simple\n");
    sbrk(-n);
    return 1;
  }
  if(pid == 0){
    // Child: the pages are shared, so it must first read the PARENT's pattern.
    for(int i = 0; i < n; i++)
      if(m[i] != (char)(i % 251))
        exit(1);                      // shared read is wrong
    // Now write its own pattern (this is what triggers the copies) ...
    for(int i = 0; i < n; i++)
      m[i] = (char)((i + 1) % 251);
    // ... and read it back: a COW fault that loses the write fails here.
    for(int i = 0; i < n; i++)
      if(m[i] != (char)((i + 1) % 251))
        exit(2);                      // write was lost
    exit(0);
  }
  wait(&st);
  if(st != 0){
    printf("cowtest: FAIL the child's COW read or write failed (status %d): "
           "either a shared page read wrong, or a write was lost\n", st);
    fails++;
  }
  // Parent's pages must be untouched by the child's writes.
  for(int i = 0; i < n; i++){
    if(m[i] != (char)(i % 251)){
      printf("cowtest: FAIL the child's writes changed the PARENT's pages; "
             "fork did not give the child private copies\n");
      fails++;
      break;
    }
  }
  sbrk(-n);
  return fails;
}

// Three processes sharing the same pages: the reference count must be right
// for a page mapped in more than two page tables.
static int
test_threeway(void)
{
  int fails = 0;
  int st;
  int n = 8 * PGSIZE;
  char *m = sbrk(n);
  if(m == SBRK_ERROR){
    printf("cowtest: FAIL sbrk failed in threeway\n");
    return 1;
  }
  for(int i = 0; i < n; i++)
    m[i] = (char)0xaa;

  int a = fork();
  if(a < 0){
    printf("cowtest: FAIL fork failed in threeway\n");
    sbrk(-n);
    return 1;
  }
  if(a == 0){
    // Child A forks grandchild B BEFORE writing, so three processes share.
    int b = fork();
    if(b < 0)
      exit(1);
    if(b == 0){
      for(int i = 0; i < n; i++) if(m[i] != (char)0xaa) exit(3);
      for(int i = 0; i < n; i++) m[i] = (char)0xbb;
      for(int i = 0; i < n; i++) if(m[i] != (char)0xbb) exit(4);
      exit(0);
    }
    for(int i = 0; i < n; i++) if(m[i] != (char)0xaa) exit(5);
    for(int i = 0; i < n; i++) m[i] = (char)0xcc;
    for(int i = 0; i < n; i++) if(m[i] != (char)0xcc) exit(6);
    int bst;
    wait(&bst);
    exit(bst == 0 ? 0 : 7);
  }
  wait(&st);
  if(st != 0){
    printf("cowtest: FAIL a page shared by three processes lost isolation or "
           "a write (status %d)\n", st);
    fails++;
  }
  for(int i = 0; i < n; i++){
    if(m[i] != (char)0xaa){
      printf("cowtest: FAIL the parent's pages changed after three-way COW "
             "sharing\n");
      fails++;
      break;
    }
  }
  sbrk(-n);
  return fails;
}

// The headline: fork a process that holds most of physical memory. Eager
// copying would need a second copy of it and run out of memory, so a fork that
// SUCCEEDS here is proof the pages were shared rather than copied.
static int
test_bigfork(void)
{
  int fails = 0;
  int st;

  int free = countfree();
  int want = (free * 3) / 4;              // three-quarters of what's free
  if(want < 8){
    printf("cowtest: FAIL not enough free memory to run bigfork (%d pages)\n",
           free);
    return 1;
  }
  uint64 n = (uint64)want * PGSIZE;
  char *m = sbrk(n);
  if(m == SBRK_ERROR){
    printf("cowtest: FAIL could not allocate %d pages for bigfork\n", want);
    return 1;
  }
  for(uint64 i = 0; i < n; i += PGSIZE)
    m[i] = 1;                             // touch every page

  int pid = fork();
  if(pid < 0){
    printf("cowtest: FAIL fork of a near-full address space failed; fork is "
           "copying pages eagerly instead of sharing them copy-on-write\n");
    sbrk(-n);
    return 1;
  }
  if(pid == 0){
    // Read every page (shared; no copy needed) and leave.
    uint64 sum = 0;
    for(uint64 i = 0; i < n; i += PGSIZE)
      sum += m[i];
    exit(sum == (n / PGSIZE) ? 0 : 1);
  }
  wait(&st);
  if(st != 0){
    printf("cowtest: FAIL the child of bigfork did not read the shared pages "
           "correctly (status %d)\n", st);
    fails++;
  }
  sbrk(-n);
  return fails;
}

// The single highest-value check: after repeated cycles of allocate / fork /
// both-sides-write / exit / free, the free-page count must return to where it
// started. A reference count that is never decremented leaves a page's count
// stuck above zero, so the sbrk(-n) that should free it does not -- and the
// pages pile up. One decremented twice frees a live page. Functional tests
// miss both; this does not.
//
// The cycle deliberately makes BOTH sides write (so both take a copy) and then
// frees the whole region, so a stuck reference count turns straight into lost
// free pages rather than merely a wrong number the test cannot see.
static int
test_leak(void)
{
  int fails = 0;
  int n = 64 * PGSIZE;

  int before = countfree();
  for(int k = 0; k < 8; k++){
    char *m = sbrk(n);
    if(m == SBRK_ERROR){
      printf("cowtest: FAIL sbrk failed in leak cycle %d\n", k);
      fails++;
      break;
    }
    for(int i = 0; i < n; i++)
      m[i] = 1;                           // parent's pages, refcount 1 each

    int pid = fork();                     // now shared, refcount 2 each
    if(pid < 0){
      printf("cowtest: FAIL fork failed in leak cycle %d\n", k);
      fails++;
      break;
    }
    if(pid == 0){
      for(int i = 0; i < n; i++)
        m[i] = (char)(k + 2);             // child copies every page, then goes
      exit(0);
    }
    wait(0);
    for(int i = 0; i < n; i++)
      m[i] = (char)(k + 100);             // parent copies every page too
    sbrk(-n);                             // and frees the whole region
  }
  int after = countfree();
  if(after != before){
    printf("cowtest: FAIL free pages before=%d after=%d across fork/exit "
           "cycles: a physical page's reference count is leaking (never "
           "decremented) or being double-freed\n", before, after);
    fails++;
  }
  return fails;
}

int
main(int argc, char *argv[])
{
  int fails = 0;

  printf("cowtest: starting\n");

  fails += test_simple();
  fails += test_threeway();
  fails += test_bigfork();
  fails += test_leak();

  if(fails == 0)
    printf("cowtest: OK\n");
  // Completion marker, printed only after every check has run. It is NOT a
  // success token (that is "cowtest: OK"); it merely says the program was not
  // killed part way through, so the harness can tell a failed check from a
  // crashed kernel. No abort path reaches it.
  printf("cowtest: done\n");
  exit(0);
}
