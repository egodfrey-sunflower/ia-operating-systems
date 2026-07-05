// Information returned by the sysinfo() system call (Lab 2, Task 2).
struct sysinfo {
  uint64 freemem; // amount of free physical memory, in bytes
  uint64 nproc;   // number of processes whose state is not UNUSED
};
