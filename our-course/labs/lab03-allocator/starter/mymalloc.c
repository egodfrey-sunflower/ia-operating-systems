/* mymalloc.c -- Lab 3 starter.
 *
 * What is written for you: getting the arena from the kernel, throwing it
 * away again, the header layout, and the two conversions between a header and
 * the payload the caller sees. None of that is the lesson, and all of it is
 * fiddly enough to eat an evening.
 *
 * What is yours: everything that decides which bytes go where. mymalloc(),
 * myfree(), the free list, splitting, coalescing, the fit policies, the stats
 * and the heap walker.
 *
 * Read mymalloc.h first. It is the contract, and the tests hold you to it.
 */

#include "mymalloc.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

/* ------------------------------------------------------------------ header
 *
 * Sixteen bytes, immediately before every payload, allocated or free:
 *
 *      +--------+--------+---------------------------+
 *      |  size  | flags  |  payload: size bytes      |
 *      +--------+--------+---------------------------+
 *      ^                 ^
 *      the header        what mymalloc() returns
 *
 * `size` is the payload size in bytes, not counting this header, and is always
 * a multiple of MYM_ALIGN. `flags` carries a magic number in its top 32 bits
 * -- so that a walker can tell a header from ordinary data -- and the in-use
 * bit at the bottom.
 *
 * Sixteen bytes is not an accident. mmap hands back a page-aligned address, so
 * if every header and every payload is a multiple of 16 bytes then every
 * payload lands on a 16-byte boundary automatically, and the alignment
 * guarantee costs nothing at run time.
 */

typedef struct hdr {
	size_t size;    /* payload bytes, always a multiple of MYM_ALIGN */
	size_t flags;   /* MYM_MAGIC in the high bits, bit 0 = in use    */
} hdr_t;

#define MYM_MAGIC     ((size_t)0x4d594d3000000000ULL)
#define MYM_MAGIC_M   ((size_t)0xffffffff00000000ULL)
#define MYM_USED      ((size_t)0x1)

/* Fails to compile if the header is ever not the size the contract promises. */
typedef char mym_header_size_check[(sizeof(hdr_t) == MYM_HEADER_BYTES) ? 1 : -1];

static inline int hdr_ok(const hdr_t *h)   { return (h->flags & MYM_MAGIC_M) == MYM_MAGIC; }
static inline int hdr_used(const hdr_t *h) { return (h->flags & MYM_USED) != 0; }

/* Cast to char * before doing arithmetic. Arithmetic on void * is a GNU
 * extension, it is a byte at a time only by accident, and this lab is built
 * with -Werror. */
static inline void *payload_of(hdr_t *h)  { return (void *)((char *)h + MYM_HEADER_BYTES); }
static inline hdr_t *hdr_of(void *p)       { return (hdr_t *)((char *)p - MYM_HEADER_BYTES); }

/* --------------------------------------------------------------- the arena */

static char   *arena;        /* NULL until the first mymalloc() */
static size_t  arena_len;
static hdr_t  *free_head;    /* your free list; shape is up to you */
static mym_fit_t fit_policy = MYM_FIT_FIRST;
static mym_err_t last_error = MYM_OK;
static int     coalesce_on = 1;

static size_t round_up(size_t n, size_t a) { return (n + a - 1) / a * a; }

static size_t env_size(const char *name, size_t dflt)
{
	const char *s = getenv(name);
	size_t v = 0;
	if (!s || !*s)
		return dflt;
	for (; *s; s++) {
		if (*s < '0' || *s > '9')
			return dflt;
		if (v > (size_t)-1 / 10)
			return dflt;
		v = v * 10 + (size_t)(*s - '0');
	}
	return v;
}

/* Map the arena and make it one enormous free block. Returns 0, or -1 if the
 * kernel refused. Call it from mymalloc() when `arena` is still NULL. */
static int arena_init(void)
{
	size_t len = env_size("MYM_ARENA_BYTES", (size_t)1 << 20);
	long pg = sysconf(_SC_PAGESIZE);
	const char *c;
	hdr_t *h;
	void *m;

	if (pg <= 0)
		pg = 4096;
	if (len < (size_t)64 * 1024)
		len = (size_t)64 * 1024;
	if (len > (size_t)64 * 1024 * 1024)
		len = (size_t)64 * 1024 * 1024;
	len = round_up(len, (size_t)pg);

	m = mmap(NULL, len, PROT_READ | PROT_WRITE,
	         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (m == MAP_FAILED)
		return -1;

	arena = (char *)m;
	arena_len = len;

	/* This one line is the exception to "arena_init() is written for you".
	 * It says the arena's single opening block spends exactly one header
	 * on metadata, which is true of the design this starter assumes. If
	 * you give every block a boundary tag as well, this becomes
	 *     h->size = arena_len - MYM_HEADER_BYTES - MYM_FOOTER_BYTES;
	 * (and the tag has to be written here too), or the very first block
	 * overlaps the end of the arena and the tiling test fails on your
	 * first allocation. Any per-block overhead you add has to be paid for
	 * here as well as in split, free and merge. */
	h = (hdr_t *)arena;
	h->size = arena_len - MYM_HEADER_BYTES;
	h->flags = MYM_MAGIC;
	free_head = h;
	/* TODO (Part 2): whatever else your free list needs initialising. */

	c = getenv("MYM_COALESCE");
	coalesce_on = !(c && c[0] == '0' && c[1] == '\0');
	return 0;
}

void mym_reset(void)
{
	if (arena)
		munmap(arena, arena_len);
	arena = NULL;
	arena_len = 0;
	free_head = NULL;
	last_error = MYM_OK;
}

/* ----------------------------------------------------------- fit policies */

void      mym_set_fit(mym_fit_t p) { fit_policy = p; }
mym_fit_t mym_get_fit(void)        { return fit_policy; }

/* ---------------------------------------------------------------- mymalloc */

void *mymalloc(size_t size)
{
	if (!arena && arena_init() != 0)
		return NULL;

	/* TODO (Part 1). Work out how many payload bytes this request really
	 * needs: round it up to MYM_ALIGN, never below MYM_MIN_PAYLOAD, and
	 * decide what mymalloc(0) does. mymalloc.h says what it must do; do
	 * that. Check that the rounding did not wrap around.
	 *
	 * In Part 1 you do not need a free list at all. Keep a pointer to the
	 * first unused byte of the arena, carve blocks off the front, write a
	 * header in front of each one, and return the payload. Nothing is ever
	 * reused. That is enough to pass every Part 1 test and it is the whole
	 * of ch. 14's two-level story: you asked the kernel once, and now you
	 * are retailing what it gave you.
	 *
	 * TODO (Part 2). Replace the bump pointer with a search of the free
	 * list, and split the block you find when the remainder is big enough
	 * to be a block in its own right (a header plus MYM_MIN_PAYLOAD). What
	 * counts as "big enough" is a design parameter -- name it, so you can
	 * argue about it later.
	 *
	 * TODO (Part 4). Make the search obey fit_policy. mymalloc.h defines
	 * all three precisely, including how ties break; the tests hold you to
	 * those definitions and not to the shape of your list.
	 */
	(void)size;
	return NULL;
}

/* ------------------------------------------------------------------ myfree */

void myfree(void *ptr)
{
	/* TODO (Part 1). myfree(NULL) does nothing. Otherwise find the header
	 * in front of the pointer and clear its in-use bit. That is all Part 1
	 * needs -- the space is not reusable yet, and it should not be.
	 *
	 * TODO (Part 2). Put the block back on the free list.
	 *
	 * TODO (Part 3). Merge it with its neighbours. The question to answer
	 * first, before you write any code: given a block, how do you find the
	 * block that physically precedes and follows it in the arena? Its free
	 * list neighbours are not the same thing, and merging list neighbours
	 * that are not physically adjacent will corrupt the heap in a way that
	 * shows up thousands of operations later. There is more than one good
	 * answer. Pick one and write down why.
	 *
	 * Honour MYM_COALESCE=0 (`coalesce_on` above is already set for you):
	 * with it set, do not merge. You will want that switch to see what
	 * coalescing is actually buying you, and the tests use it too.
	 *
	 * TODO (Part 5). Before trusting the pointer at all, check it: inside
	 * the arena, a real header where a header should be, not already free.
	 * Set last_error and return, leaving the heap untouched, if not.
	 *
	 * Those three go in that order and the order is part of the contract:
	 * bounds, then alignment and header validity, then the in-use bit. A
	 * pointer that is both invalid and already free is NOT_A_BLOCK, because
	 * it never reaches the in-use bit -- and there is a case that hands you
	 * exactly such a pointer and checks which answer you give.
	 *
	 * "A real header" means the magic number, not just a plausible size. A
	 * caller that keeps a length and a flag word at the front of its own
	 * buffer has written sixteen bytes that pass every range check you can
	 * think of; only the magic tells them from a header, and there is a
	 * case that hands you exactly those bytes.
	 */
	(void)ptr;
}

/* ------------------------------------------------------------------- stats */

void mym_get_stats(mym_stats_t *out)
{
	memset(out, 0, sizeof(*out));
	if (!arena)
		return;

	/* TODO (Part 1). Walk the heap and fill this in. Every byte of the
	 * arena belongs to exactly one block, so the three totals plus the
	 * headers must come to arena_bytes exactly -- there is a test for that,
	 * and it is the one that catches a split which forgot to charge for the
	 * new header. */
	out->arena_bytes = arena_len;
}

/* -------------------------------------------------------------- the walker */

int mym_check_heap(void)
{
	if (!arena)
		return 0;

	/* TODO (Part 5). Walk the heap and check it against itself. Ideas, in
	 * rough order of how much they catch:
	 *
	 *   - every header has the magic number (this is the one that catches
	 *     an overflow: writing past the end of a block lands in the next
	 *     block's header);
	 *   - sizes are non-zero multiples of MYM_ALIGN, and the blocks tile
	 *     the arena exactly, with no gap and no overshoot at the end;
	 *   - no two free blocks are physically adjacent -- if two are, a merge
	 *     was missed (skip this one when coalescing is switched off);
	 *   - the free list stays inside the arena, every entry on it really is
	 *     free, and it has exactly as many entries as the physical walk
	 *     found free blocks.
	 *
	 * Every one of those has a case built around a heap that only it can
	 * see through, so none of them is optional. The last one is two checks
	 * wearing one bullet point and it has two cases: "every entry is really
	 * free" catches a block left on the list after being handed out, and
	 * "as many entries as free blocks" catches a free block that fell off
	 * the list. Neither can see what the other sees.
	 *
	 * This runs on a heap that may already be damaged, so validate before
	 * you dereference and put a bound on the walk. A checker that loops for
	 * ever on a corrupt heap is worse than no checker: the test harness
	 * reports it as a timeout with no idea why. One case builds exactly
	 * that heap -- a size field that makes the physical walk a cycle -- and
	 * it is the bound, plus refusing a size the arena cannot contain, that
	 * gets you out of it.
	 *
	 * Return 0 for a clean heap, the number of problems otherwise, and set
	 * last_error to MYM_ERR_CORRUPT when you find any.
	 */
	return 0;
}

/* ------------------------------------------------------------------ errors */

mym_err_t mym_last_error(void) { return last_error; }
void      mym_clear_error(void) { last_error = MYM_OK; }
