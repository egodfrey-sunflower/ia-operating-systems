/* mymalloc.c -- Lab 3 reference allocator.
 *
 * SPOILERS. This is the model answer for Parts 1-5.
 *
 * Design, in one paragraph: one arena, mapped from the kernel once with mmap.
 * Every block is a 16-byte header {size, flags} followed by its payload, and
 * the blocks tile the arena exactly -- there are no gaps, so the physical
 * successor of a block is found by adding the header size and the payload
 * size. Free blocks are additionally threaded onto a singly linked list kept
 * in ASCENDING ADDRESS ORDER, whose links live in the first 8 bytes of each
 * free block's own payload. Address order is what makes coalescing easy: the
 * list neighbours of a freed block are the only candidates for being its
 * physical neighbours, so one insertion walk finds both.
 *
 * No libc heap is used: no malloc, no calloc, no realloc, no free, no strdup,
 * no stdio. mmap and getenv only.
 */

#include "mymalloc.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

/* ------------------------------------------------------------------ header */

typedef struct hdr {
	size_t size;    /* payload bytes, always a multiple of MYM_ALIGN */
	size_t flags;   /* MYM_MAGIC in the high bits, bit 0 = in use    */
} hdr_t;

#define MYM_MAGIC     ((size_t)0x4d594d3000000000ULL)
#define MYM_MAGIC_M   ((size_t)0xffffffff00000000ULL)
#define MYM_USED      ((size_t)0x1)

/* Compile-time check that the header really is the size the contract claims;
 * everything about the alignment guarantee depends on it. */
typedef char mym_header_size_check[(sizeof(hdr_t) == MYM_HEADER_BYTES) ? 1 : -1];

static inline int hdr_ok(const hdr_t *h)   { return (h->flags & MYM_MAGIC_M) == MYM_MAGIC; }
static inline int hdr_used(const hdr_t *h) { return (h->flags & MYM_USED) != 0; }

/* Pointer arithmetic is done on char *, never on void * -- void * arithmetic
 * is a GNU extension and -Wpedantic would reject it. */
static inline void *payload_of(hdr_t *h)  { return (void *)((char *)h + MYM_HEADER_BYTES); }
static inline hdr_t *hdr_of(void *p)       { return (hdr_t *)((char *)p - MYM_HEADER_BYTES); }

/* --------------------------------------------------------------- the arena */

static char   *arena;        /* NULL until the first mymalloc() */
static size_t  arena_len;
static hdr_t  *free_head;    /* lowest-addressed free block, or NULL */
static mym_fit_t fit_policy = MYM_FIT_FIRST;
static mym_err_t last_error = MYM_OK;
static int     coalesce_on = 1;

/* The free-list link of a free block lives in the first 8 bytes of its own
 * payload. The payload is 16-byte aligned and at least 16 bytes long, so this
 * is properly aligned storage for a pointer. */
static hdr_t *fl_next(hdr_t *h)                { return *(hdr_t **)payload_of(h); }
static void   fl_set_next(hdr_t *h, hdr_t *n)  { *(hdr_t **)payload_of(h) = n; }

static hdr_t *phys_next(hdr_t *h)
{
	char *p = (char *)h + MYM_HEADER_BYTES + h->size;
	return (p < arena + arena_len) ? (hdr_t *)p : NULL;
}

static size_t round_up(size_t n, size_t a) { return (n + a - 1) / a * a; }

/* getenv + a hand-rolled parse: strtoul would do, but keeping this file free
 * of anything that might touch the libc heap makes the "no libc heap" test
 * trivially true rather than probably true. */
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

	/* mmap returns page-aligned memory, so arena+16 is 16-byte aligned and
	 * so is every payload after it: header and payload sizes are both
	 * multiples of 16, so alignment is preserved by construction rather
	 * than by rounding each returned pointer. */
	/* A design with more per-block overhead than this one has to pay for
	 * it here too: with a boundary tag on every block the opening block is
	 * arena_len - MYM_HEADER_BYTES - MYM_FOOTER_BYTES, and the tag is
	 * written here. This is the one line of arena_init() a student who
	 * takes the footer route must edit. */
	h = (hdr_t *)arena;
	h->size = arena_len - MYM_HEADER_BYTES;
	h->flags = MYM_MAGIC;
	fl_set_next(h, NULL);
	free_head = h;

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

/* Return the block the current policy chooses for a payload of `need` bytes,
 * with *prev_out set to its predecessor on the free list (NULL if it is the
 * head). The list is address-ordered, so scanning it forwards visits
 * candidates from low address to high: taking the first match gives first
 * fit, and using a strict < / > comparison for the others breaks ties towards
 * the lowest address, exactly as the header promises.
 */
static hdr_t *find_block(size_t need, hdr_t **prev_out)
{
	hdr_t *prev = NULL, *cur = free_head;
	hdr_t *best = NULL, *best_prev = NULL;

	while (cur) {
		if (cur->size >= need) {
			if (fit_policy == MYM_FIT_FIRST) {
				*prev_out = prev;
				return cur;
			}
			if (!best ||
			    (fit_policy == MYM_FIT_BEST  && cur->size < best->size) ||
			    (fit_policy == MYM_FIT_WORST && cur->size > best->size)) {
				best = cur;
				best_prev = prev;
			}
		}
		prev = cur;
		cur = fl_next(cur);
	}
	*prev_out = best_prev;
	return best;
}

/* ---------------------------------------------------------------- mymalloc */

void *mymalloc(size_t size)
{
	hdr_t *b, *prev, *rem;
	size_t need;

	if (!arena && arena_init() != 0)
		return NULL;

	/* malloc(0) hands back a real, unique, freeable minimum-sized block.
	 * Returning NULL would be legal for libc but is a poor choice here:
	 * callers cannot tell it from out-of-memory. */
	need = round_up(size ? size : 1, MYM_ALIGN);
	if (need < MYM_MIN_PAYLOAD)
		need = MYM_MIN_PAYLOAD;
	/* Catch a request so large that rounding it up wrapped around. */
	if (need < size)
		return NULL;

	b = find_block(need, &prev);
	if (!b)
		return NULL;

	/* Split only if what is left over can be a block in its own right: a
	 * header plus the minimum payload. A smaller remainder would be
	 * unaddressable, so it stays with the allocation as internal waste.
	 * This threshold is a design parameter -- raising it trades internal
	 * fragmentation for fewer, larger free blocks. */
	if (b->size >= need + MYM_HEADER_BYTES + MYM_MIN_PAYLOAD) {
		rem = (hdr_t *)((char *)payload_of(b) + need);
		rem->size = b->size - need - MYM_HEADER_BYTES;
		rem->flags = MYM_MAGIC;
		fl_set_next(rem, fl_next(b));
		b->size = need;
		if (prev)
			fl_set_next(prev, rem);
		else
			free_head = rem;
	} else {
		if (prev)
			fl_set_next(prev, fl_next(b));
		else
			free_head = fl_next(b);
	}

	b->flags = MYM_MAGIC | MYM_USED;
	return payload_of(b);
}

/* ------------------------------------------------------------------ myfree */

void myfree(void *ptr)
{
	hdr_t *h, *prev, *cur;

	if (!ptr)
		return;                       /* free(NULL) is a no-op */

	if (!arena ||
	    (char *)ptr <= arena ||
	    (char *)ptr >= arena + arena_len) {
		last_error = MYM_ERR_NOT_OURS;
		return;
	}

	h = hdr_of(ptr);
	if (((uintptr_t)h % MYM_ALIGN) != 0 || !hdr_ok(h) ||
	    h->size == 0 || h->size % MYM_ALIGN != 0 ||
	    (char *)h + MYM_HEADER_BYTES + h->size > arena + arena_len) {
		/* An interior pointer lands here: the 16 bytes in front of it are
		 * the caller's own data, not a header, so the magic is wrong. */
		last_error = MYM_ERR_NOT_A_BLOCK;
		return;
	}
	if (!hdr_used(h)) {
		last_error = MYM_ERR_DOUBLE_FREE;
		return;
	}

	h->flags = MYM_MAGIC;

	/* Insert into the address-ordered list. */
	prev = NULL;
	cur = free_head;
	while (cur && cur < h) {
		prev = cur;
		cur = fl_next(cur);
	}
	fl_set_next(h, cur);
	if (prev)
		fl_set_next(prev, h);
	else
		free_head = h;

	if (!coalesce_on)
		return;

	/* Merge forwards, then backwards. `cur` is the next free block by
	 * address and `prev` the previous one, so the physical-neighbour test
	 * is a single pointer comparison in each direction. Doing forwards
	 * first matters: it grows h, and the backward merge then absorbs the
	 * whole run in one step. */
	if (cur && phys_next(h) == cur) {
		h->size += MYM_HEADER_BYTES + cur->size;
		fl_set_next(h, fl_next(cur));
	}
	if (prev && phys_next(prev) == h) {
		prev->size += MYM_HEADER_BYTES + h->size;
		fl_set_next(prev, fl_next(h));
	}
}

/* ------------------------------------------------------------------- stats */

void mym_get_stats(mym_stats_t *out)
{
	hdr_t *h;

	memset(out, 0, sizeof(*out));
	if (!arena)
		return;

	out->arena_bytes = arena_len;
	for (h = (hdr_t *)arena; h; h = phys_next(h)) {
		out->header_bytes += MYM_HEADER_BYTES;
		if (hdr_used(h)) {
			out->bytes_in_use += h->size;
		} else {
			out->free_bytes += h->size;
			out->free_blocks++;
			if (h->size > out->largest_free_block)
				out->largest_free_block = h->size;
		}
	}
}

/* -------------------------------------------------------------- the walker */

int mym_check_heap(void)
{
	size_t phys_free = 0, list_free = 0, steps = 0, maxsteps;
	hdr_t *h, *prev_h = NULL, *cur;
	char *p;
	int bad = 0;

	if (!arena)
		return 0;

	/* Every step consumes at least a header plus the minimum payload, so
	 * this bound cannot be reached by a sane heap and stops a corrupt one
	 * from looping forever. */
	maxsteps = arena_len / (MYM_HEADER_BYTES + MYM_MIN_PAYLOAD) + 1;

	/* 1. the physical walk: blocks must tile the arena exactly. */
	p = arena;
	while (p < arena + arena_len) {
		if (++steps > maxsteps) {
			bad++;
			break;
		}
		if ((size_t)(arena + arena_len - p) < MYM_HEADER_BYTES) {
			bad++;
			break;
		}
		h = (hdr_t *)p;
		if (!hdr_ok(h)) {                 /* smashed by an overflow */
			bad++;
			break;
		}
		if (h->size == 0 || h->size % MYM_ALIGN != 0 ||
		    h->size > arena_len ||
		    (char *)h + MYM_HEADER_BYTES + h->size > arena + arena_len) {
			bad++;
			break;
		}
		if (!hdr_used(h)) {
			phys_free++;
			/* Two free blocks side by side mean a merge was missed. */
			if (coalesce_on && prev_h && !hdr_used(prev_h))
				bad++;
		}
		prev_h = h;
		p = (char *)h + MYM_HEADER_BYTES + h->size;
	}
	if (bad)
		goto done;                        /* the list walk would be unsafe */

	/* 2. the free list: inside the arena, strictly ascending, all free,
	 *    and exactly as many entries as the physical walk found. */
	steps = 0;
	prev_h = NULL;
	for (cur = free_head; cur; cur = fl_next(cur)) {
		if (++steps > maxsteps) {
			bad++;
			break;
		}
		if ((char *)cur < arena ||
		    (char *)cur + MYM_HEADER_BYTES > arena + arena_len ||
		    ((uintptr_t)cur % MYM_ALIGN) != 0) {
			bad++;
			break;
		}
		if (!hdr_ok(cur) || hdr_used(cur)) {
			bad++;
			break;
		}
		if (prev_h && cur <= prev_h) {
			bad++;
			break;
		}
		prev_h = cur;
		list_free++;
	}
	if (list_free != phys_free)
		bad++;

done:
	if (bad)
		last_error = MYM_ERR_CORRUPT;
	return bad;
}

/* ------------------------------------------------------------------ errors */

mym_err_t mym_last_error(void) { return last_error; }
void      mym_clear_error(void) { last_error = MYM_OK; }
