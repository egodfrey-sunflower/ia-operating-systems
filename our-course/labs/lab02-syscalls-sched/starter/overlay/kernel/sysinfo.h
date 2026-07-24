// The structure sysinfo(2) fills in. GIVEN -- do not change the layout; the
// supplied user program user/sysinfotest.c depends on it.
//
// This header is included by BOTH kernel and user code, so it may not
// include anything itself: whoever includes it has already included
// kernel/types.h.

struct sysinfo {
  uint64 freemem;   // bytes of free physical memory
  uint64 nproc;     // processes whose state is not UNUSED
};
