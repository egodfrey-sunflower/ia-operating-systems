// trace mask command [args...]
//
// Turn on syscall tracing (via the trace() system call) for the given mask,
// then exec `command`. Because the trace mask is inherited across fork and
// preserved across exec, `command` and any children it forks run with tracing
// enabled. This is the wrapper program; you implement trace() in the kernel.

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')) {
    fprintf(2, "Usage: %s mask command [args]\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }

  for (i = 2; i < argc && i < MAXARG; i++) {
    nargv[i - 2] = argv[i];
  }
  nargv[i - 2] = 0;
  exec(nargv[0], nargv);
  fprintf(2, "%s: exec %s failed\n", argv[0], nargv[0]);
  exit(1);
}
