// Mutual exclusion lock.
struct spinlock {
  uint locked; // Is the lock held?

  // For debugging:
  char *name;      // Name of lock.
  struct cpu *cpu; // The cpu holding the lock.

  // Contention instrumentation (Lab 5). These are ALWAYS-ON counters,
  // maintained by acquire(). They let the statistics() system call report
  // how often a lock was contended, so you can measure the effect of the
  // per-CPU kmem and bucketed bcache redesigns.
  uint64 n;   // total number of acquire() calls on this lock
  uint64 nts; // number of times the atomic test-and-set found it held
              // (i.e. a contended acquisition -- someone else had the lock)
};
