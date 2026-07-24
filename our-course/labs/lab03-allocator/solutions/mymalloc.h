/* mymalloc.h -- Lab 3, the allocator's public interface.
 *
 * This file is the contract. The test harness includes THIS header and links
 * against YOUR mymalloc.c, so the declarations below may not change: no
 * renaming, no extra required arguments, no different return types. You may
 * add your own declarations for internal use, but everything already here has
 * to keep working exactly as specified.
 *
 * Thread safety is out of scope. Nothing here is safe to call from two threads
 * at once and nothing needs to be.
 */
#ifndef MYMALLOC_H
#define MYMALLOC_H

#include <stddef.h>

/* Every pointer mymalloc() returns is aligned to this many bytes. */
#define MYM_ALIGN 16

/* Every block carries a header of exactly this many bytes, immediately before
 * the payload the caller sees. The first word of it is the payload size and
 * the second is the flags -- a magic number in the top 32 bits, the in-use bit
 * at the bottom. That much is fixed, and the harness relies on it in the five
 * cases that damage a heap on purpose.
 *
 * Nothing else about your block layout is fixed. If your design carries more
 * than this header -- a boundary tag at the end of every block, say -- that is
 * accepted, and no test assumes a request for n bytes costs exactly n + 16.
 * What the extra bytes must do is show up in mym_get_stats(): the invariant
 * below still has to hold exactly. */
#define MYM_HEADER_BYTES 16

/* The smallest payload any block may have. A free block stores its free-list
 * link inside its own payload, so the payload can never be smaller than a
 * pointer; rounding that up to MYM_ALIGN gives 16. */
#define MYM_MIN_PAYLOAD 16

/* ------------------------------------------------------------------ the API */

/* Return a pointer to at least `size` writable bytes, aligned to MYM_ALIGN,
 * or NULL if the arena cannot satisfy the request.
 *
 * mymalloc(0) returns a valid unique pointer to a minimum-sized block. It
 * aliases no other live allocation and may be passed to myfree().
 *
 * The returned memory is NOT zeroed and its contents are unspecified. */
void *mymalloc(size_t size);

/* Release a block obtained from mymalloc().
 *
 * myfree(NULL) does nothing at all -- not an error, not a diagnostic.
 *
 * Given anything else that did not come from mymalloc(), or a pointer that is
 * already free, myfree() must leave the heap untouched and record why in
 * mym_last_error(). It must not abort and it must not corrupt anything.
 *
 * Three corners that are easy to guess wrong, all of them tested:
 *
 *   - a non-NULL pointer passed before the arena exists is MYM_ERR_NOT_OURS.
 *     There is no arena, so the pointer cannot be inside one; do not create an
 *     arena just to answer the question.
 *   - a pointer that IS inside the arena but is not a multiple of MYM_ALIGN is
 *     MYM_ERR_NOT_A_BLOCK, not MYM_ERR_NOT_OURS. NOT_OURS means "outside the
 *     arena" and nothing else; anything inside the arena without a valid header
 *     sixteen bytes in front of it is NOT_A_BLOCK, whatever is wrong with it --
 *     misaligned, no magic, an implausible size, or a size that would run off
 *     the end of the arena.
 *   - the order of the checks therefore matters: bounds first, then alignment
 *     and header validity, then the in-use bit. A pointer that is both
 *     misaligned and already free reports NOT_A_BLOCK, because it never reaches
 *     the in-use bit. */
void myfree(void *ptr);

/* ---------------------------------------------------------- fit policy (P4) */

typedef enum {
	MYM_FIT_FIRST = 0,   /* the lowest-address free block that fits      */
	MYM_FIT_BEST  = 1,   /* the smallest free block that fits            */
	MYM_FIT_WORST = 2    /* the largest free block, if it fits           */
} mym_fit_t;

/* Both BEST and WORST break ties by taking the lowest-address candidate. The
 * Part 4 cases lay out two free blocks of each candidate size for exactly this
 * reason, so the rule is one the tests can and do fail you on. */

void      mym_set_fit(mym_fit_t policy);   /* default is MYM_FIT_FIRST */
mym_fit_t mym_get_fit(void);

/* -------------------------------------------------------- measurement (P4) */

typedef struct {
	size_t arena_bytes;         /* total bytes mapped from the kernel      */
	size_t bytes_in_use;        /* sum of the payload sizes of live blocks */
	size_t free_bytes;          /* sum of the payload sizes of free blocks */
	size_t header_bytes;        /* every byte of the arena that is neither
	                             * a live payload nor a free payload: the
	                             * headers, and any other metadata your
	                             * design charges per block                */
	size_t free_blocks;         /* how many free blocks exist              */
	size_t largest_free_block;  /* biggest payload among them, 0 if none   */
} mym_stats_t;

/* Fill *out by walking the heap. Every byte of the arena belongs to exactly
 * one block, so this invariant must hold on every call:
 *
 *     bytes_in_use + free_bytes + header_bytes == arena_bytes
 *
 * Before the arena exists, every field is 0. */
void mym_get_stats(mym_stats_t *out);

/* ------------------------------------------------------------- debug (P5) */

/* Walk the whole heap and check it against itself. Return 0 if it is
 * consistent, otherwise the number of problems found (any non-zero value
 * counts as "broken"). Must be safe to call on a heap that has already been
 * corrupted: bound the walk and validate before dereferencing.
 *
 * On damage it sets last_error to MYM_ERR_CORRUPT. On a clean heap it does
 * NOT touch last_error -- it neither sets it nor clears it, so an error
 * recorded by an earlier myfree() is still readable after a clean check.
 * mym_clear_error() is the only thing that clears it, apart from mym_reset(),
 * which throws the whole heap away and discards everything with it.
 *
 * Each of the checks Part 5 asks for -- the magic, the sizes, the tiling, "no
 * two free blocks physically adjacent when coalescing is on", and the free
 * list agreeing with the physical walk -- has a case built around a heap that
 * only that check can see through. Leaving any of them out costs a case. The
 * last of them is really two checks, and there are two cases: every entry the
 * list holds must be a block the physical walk agrees is free, AND the list
 * must hold exactly as many entries as the walk found free blocks. A free
 * block that has fallen off the list breaks only the count. */
int mym_check_heap(void);

typedef enum {
	MYM_OK = 0,
	MYM_ERR_NOT_OURS,       /* pointer is not inside the arena at all      */
	MYM_ERR_NOT_A_BLOCK,    /* inside the arena, but no valid header there */
	MYM_ERR_DOUBLE_FREE,    /* a real block that is already free           */
	MYM_ERR_CORRUPT         /* mym_check_heap() found damage               */
} mym_err_t;

mym_err_t mym_last_error(void);
void      mym_clear_error(void);

/* ------------------------------------------------------------------ arena */

/* Throw the arena away: unmap it, forget every block, reset the free list.
 * The next mymalloc() maps a fresh one. Every pointer handed out before the
 * call is dangling afterwards.
 *
 * This exists for the tests, which need a known-empty heap per scenario. */
void mym_reset(void);

/* The arena is created on the first mymalloc(). Its size comes from the
 * environment variable MYM_ARENA_BYTES if that is set to a decimal number,
 * clamped to [64 KiB, 64 MiB] and rounded up to a page; otherwise it is 1 MiB.
 * The arena never grows: when it is full, mymalloc() returns NULL.
 *
 * If MYM_COALESCE is set to "0", myfree() must NOT merge adjacent free blocks.
 * That switch exists so you can watch the Part 3 tests fail without it. */

#endif /* MYMALLOC_H */
