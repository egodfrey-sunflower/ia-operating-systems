// A pseudo-random number generator for the lottery scheduler.
//
// GIVEN. xv6 has none, and writing one is not the exercise.
//
// This is splitmix64: advance a 64-bit counter by a fixed odd stride, then
// run the result through a mixing function of shifts, xors and odd
// multiplies. It is not cryptographically anything; it is a cheap, well
// behaved source of uniform bits, which is all a lottery draw needs.
//
// WHY THIS ONE, since it matters here and the reason is not obvious.
//
// The scheduler uses the value modulo the total ticket count, so the draw is
// decided by the LOW bits of what this function returns. Plenty of famous
// small generators -- the xorshift family in particular -- have excellent
// high bits and visibly correlated low ones, and multiplying the output by a
// constant does not fix that, because multiplication by an odd number is a
// permutation of the low k bits and so preserves any structure they had.
// The effect is not a wrong average: over a few hundred thousand draws such
// a generator gives exactly the share you asked for. It is a wrong ANSWER
// over the few hundred draws a test actually makes, reproducibly, because
// the state is seeded to a constant at every boot. splitmix64's mixing step
// is designed so that every output bit depends on every state bit, and its
// low bits are as good as its high ones.
//
// It keeps its state in a plain global with no lock. That is safe here only
// because this lab runs single-CPU (CPUS=1) and scheduler() calls it with
// interrupts off. If you take this to a multiprocessor kernel you will need
// per-CPU state or a lock -- and you will also want to notice that a shared
// generator makes two CPUs' draws correlated.

#include "types.h"
#include "sched.h"

static uint64 rstate = 88172645463325252ULL;

uint64
rand(void)
{
  uint64 z;

  rstate += 0x9E3779B97F4A7C15ULL;   // 2^64 / phi, an odd stride
  z = rstate;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
