// Aggregated lock-contention statistics, filled by the statistics() system
// call (Lab 5). Both the kernel (sys_statistics) and the user test programs
// (kalloctest, bcachetest) include this header so they agree on the layout.
//
// The counts are cumulative since boot. A test measures the contention caused
// by its own workload by reading statistics() before and after and taking the
// difference.
struct lockstat {
  uint64 kmem_n;     // total acquire() calls on the kmem lock(s)
  uint64 kmem_nts;   // contended acquisitions on the kmem lock(s)
  uint64 bcache_n;   // total acquire() calls on the bcache lock(s)
  uint64 bcache_nts; // contended acquisitions on the bcache lock(s)
};
