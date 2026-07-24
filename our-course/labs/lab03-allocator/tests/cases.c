/* cases.c -- Lab 3 correctness cases.
 *
 *     cases <case-name>
 *
 * Runs one case against the allocator it is linked with and exits 0 if it
 * passed, 1 if it failed, 2 if the case name is unknown. Every complaint goes
 * to stderr; run.sh shows them when a case fails.
 *
 * One process per case, so a segfault in one case cannot take the rest of the
 * suite with it, and each case gets its own timeout and its own environment.
 *
 * Nothing here reaches into the allocator's internals. Every assertion is made
 * through mymalloc.h: returned addresses, contents, mym_get_stats(),
 * mym_check_heap() and mym_last_error(). How you represent a free block is
 * your business.
 */

#include "mymalloc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void fail(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

static void fail(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fputs("    ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	failures++;
}

#define CHECK(cond, ...) do { if (!(cond)) fail(__VA_ARGS__); } while (0)

/* Give up early when carrying on would be meaningless (or unsafe). */
#define REQUIRE(cond, ...) do { \
	if (!(cond)) { fail(__VA_ARGS__); return; } \
} while (0)

static void use_arena(const char *bytes)
{
	setenv("MYM_ARENA_BYTES", bytes, 1);
}

/* Fill a block with a recognisable pattern and check it later. */
static void paint(void *p, size_t n, unsigned char tag)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;
	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(tag + (unsigned char)(i & 0x1f));
}

static int painted(const void *p, size_t n, unsigned char tag)
{
	const unsigned char *b = (const unsigned char *)p;
	size_t i;
	for (i = 0; i < n; i++)
		if (b[i] != (unsigned char)(tag + (unsigned char)(i & 0x1f)))
			return 0;
	return 1;
}

static void check_invariant(const char *when)
{
	mym_stats_t s;
	mym_get_stats(&s);
	CHECK(s.bytes_in_use + s.free_bytes + s.header_bytes == s.arena_bytes,
	      "%s: the stats do not account for the arena: "
	      "in_use %zu + free %zu + headers %zu = %zu, arena is %zu",
	      when, s.bytes_in_use, s.free_bytes, s.header_bytes,
	      s.bytes_in_use + s.free_bytes + s.header_bytes, s.arena_bytes);
}

/* --------------------------------------------------- design-neutral helpers
 *
 * The contract fixes one 16-byte header immediately before every payload. It
 * does NOT fix what else a design may charge: a boundary tag at the end of
 * every block is an accepted alternative and costs more per block. So no case
 * here assumes that a request for n bytes consumes exactly n + 16. Where a
 * case needs "the biggest block this arena can still hand out", it asks for
 * largest_free_block and steps the request down until one fits.
 */

/* Allocate the largest block the arena can still serve. *got, if given, is the
 * size that succeeded. NULL if nothing fits. */
static void *alloc_largest(size_t *got)
{
	mym_stats_t s;
	size_t want;

	mym_get_stats(&s);
	for (want = s.largest_free_block; want >= MYM_ALIGN; want -= MYM_ALIGN) {
		void *p = mymalloc(want);
		if (p) {
			if (got)
				*got = want;
			return p;
		}
	}
	if (got)
		*got = 0;
	return NULL;
}

/* Leave the arena with no free bytes at all, whatever the per-block cost.
 * Returns 0 if some free space could not be handed out. */
static int fill_arena(void)
{
	mym_stats_t s;
	int guard;

	for (guard = 0; guard < 64; guard++) {
		mym_get_stats(&s);
		if (s.free_bytes == 0)
			return 1;
		if (!alloc_largest(NULL))
			return 0;
	}
	return 0;
}

/* The header of the block whose payload is p. The contract puts it exactly
 * MYM_HEADER_BYTES in front, with the payload size in the first word and the
 * flags -- magic in the top 32 bits, in-use in bit 0 -- in the second. Five
 * Part 5 cases damage a heap deliberately and need to say where. */
static size_t *header_of(void *p)
{
	return (size_t *)(void *)((char *)p - MYM_HEADER_BYTES);
}

/* ------------------------------------------------------------------ Part 1 */

static void p1_align(void)
{
	static const size_t odd[] = { 0, 1, 7, 8, 15, 16, 17, 31, 33, 100,
	                              101, 255, 1000, 4095 };
	void *p;
	size_t i;

	use_arena("262144");
	for (i = 0; i < sizeof(odd) / sizeof(odd[0]); i++) {
		p = mymalloc(odd[i]);
		REQUIRE(p != NULL, "mymalloc(%zu) returned NULL on an empty arena",
		        odd[i]);
		CHECK(((unsigned long)(size_t)p % MYM_ALIGN) == 0,
		      "mymalloc(%zu) returned %p, which is not %d-byte aligned",
		      odd[i], p, MYM_ALIGN);
	}
	for (i = 1; i <= 64; i++) {
		p = mymalloc(i);
		REQUIRE(p != NULL, "mymalloc(%zu) returned NULL", i);
		CHECK(((unsigned long)(size_t)p % MYM_ALIGN) == 0,
		      "mymalloc(%zu) returned %p, which is not %d-byte aligned",
		      i, p, MYM_ALIGN);
	}
}

#define NBLK 64

static void p1_no_overlap(void)
{
	void *p[NBLK];
	size_t sz[NBLK];
	int i;

	use_arena("262144");
	for (i = 0; i < NBLK; i++) {
		sz[i] = (size_t)(17 + i * 7);
		p[i] = mymalloc(sz[i]);
		REQUIRE(p[i] != NULL, "mymalloc(%zu) returned NULL at block %d",
		        sz[i], i);
		paint(p[i], sz[i], (unsigned char)(i + 1));
	}
	/* Every byte of every block must still hold its own pattern. If two
	 * blocks overlap -- or one was handed out shorter than requested --
	 * the later paint() has scribbled on the earlier one. */
	for (i = 0; i < NBLK; i++)
		CHECK(painted(p[i], sz[i], (unsigned char)(i + 1)),
		      "block %d (%zu bytes at %p) was overwritten by a later "
		      "allocation: the blocks overlap",
		      i, sz[i], p[i]);
}

static void p1_survives(void)
{
	void *keep, *junk;
	int i;

	use_arena("262144");
	keep = mymalloc(300);
	REQUIRE(keep != NULL, "mymalloc(300) returned NULL on an empty arena");
	paint(keep, 300, 0x40);

	for (i = 0; i < 40; i++) {
		junk = mymalloc((size_t)(32 + i * 13));
		REQUIRE(junk != NULL, "mymalloc failed at intervening block %d", i);
		paint(junk, (size_t)(32 + i * 13), 0x91);
	}
	CHECK(painted(keep, 300, 0x40),
	      "the first block's contents did not survive 40 later allocations");
}

static void p1_zero(void)
{
	mym_stats_t before, after;
	void *a, *b, *c;

	use_arena("262144");
	(void)mymalloc(16);              /* so the arena already exists */
	mym_get_stats(&before);
	a = mymalloc(0);
	mym_get_stats(&after);
	b = mymalloc(0);
	c = mymalloc(64);
	CHECK(a != NULL, "mymalloc(0) returned NULL; the contract says it "
	                 "returns a unique freeable block");
	/* "Minimum-sized", not merely non-NULL: a zero-byte request that comes
	 * back with a kilobyte attached to it is a leak with a returned
	 * pointer. The bound is loose on purpose -- it is there to catch a
	 * size class, not to police anyone's per-block overhead. */
	CHECK(after.bytes_in_use - before.bytes_in_use <= 256,
	      "mymalloc(0) charged the arena %zu bytes of payload; the contract "
	      "says a zero-byte request gets a minimum-sized block",
	      after.bytes_in_use - before.bytes_in_use);
	CHECK(b != NULL, "the second mymalloc(0) returned NULL");
	CHECK(a != b, "two mymalloc(0) calls returned the same pointer %p", a);
	CHECK(a != c && b != c,
	      "a zero-sized block aliases a real one");
	mym_clear_error();
	myfree(a);
	CHECK(mym_last_error() == MYM_OK,
	      "myfree() of a mymalloc(0) block reported error %d",
	      (int)mym_last_error());
}

static void p1_free_null(void)
{
	void *p;

	use_arena("262144");
	mym_clear_error();
	myfree(NULL);                    /* before the arena even exists */
	CHECK(mym_last_error() == MYM_OK,
	      "myfree(NULL) on a cold allocator reported error %d",
	      (int)mym_last_error());

	p = mymalloc(64);
	REQUIRE(p != NULL, "mymalloc(64) returned NULL");
	paint(p, 64, 0x7);
	myfree(NULL);
	CHECK(mym_last_error() == MYM_OK,
	      "myfree(NULL) reported error %d", (int)mym_last_error());
	CHECK(painted(p, 64, 0x7), "myfree(NULL) disturbed a live block");
	check_invariant("after myfree(NULL)");
	CHECK(mymalloc(64) != NULL, "the allocator stopped working after "
	                            "myfree(NULL)");
}

static void p1_stats_account(void)
{
	void *p[16];
	mym_stats_t s;
	int i;

	use_arena("262144");
	mym_get_stats(&s);
	CHECK(s.bytes_in_use == 0 && s.free_bytes == 0 && s.arena_bytes == 0,
	      "the stats are not all zero before the arena exists "
	      "(arena %zu, in use %zu, free %zu)",
	      s.arena_bytes, s.bytes_in_use, s.free_bytes);

	for (i = 0; i < 16; i++) {
		p[i] = mymalloc((size_t)(100 + i * 100));
		REQUIRE(p[i] != NULL, "mymalloc failed at block %d", i);
	}
	check_invariant("after 16 allocations");

	mym_get_stats(&s);
	CHECK(s.arena_bytes >= 262144,
	      "MYM_ARENA_BYTES=262144 but arena_bytes is %zu", s.arena_bytes);
	CHECK(s.bytes_in_use >= 13600,
	      "16 blocks totalling 13600 requested bytes are recorded as only "
	      "%zu bytes in use", s.bytes_in_use);
	CHECK(s.header_bytes >= 17 * MYM_HEADER_BYTES,
	      "17 blocks exist but only %zu bytes of header are accounted for",
	      s.header_bytes);

	for (i = 0; i < 16; i += 2)
		myfree(p[i]);
	check_invariant("after 8 frees");

	for (i = 0; i < 16; i++)
		REQUIRE(mymalloc(64) != NULL, "mymalloc(64) failed after the frees");
	check_invariant("after 16 more allocations");
}

static void p1_too_big(void)
{
	void *p;
	mym_stats_t s;

	use_arena("65536");
	p = mymalloc(64);
	REQUIRE(p != NULL, "mymalloc(64) returned NULL on an empty arena");

	mym_get_stats(&s);
	CHECK(s.arena_bytes < 200000,
	      "the arena is %zu bytes; MYM_ARENA_BYTES asked for 65536 and the "
	      "arena is not supposed to grow", s.arena_bytes);

	CHECK(mymalloc(200000) == NULL,
	      "a 200000-byte request succeeded in a 65536-byte arena");
	CHECK(mymalloc((size_t)-1) == NULL,
	      "a SIZE_MAX request succeeded; the size rounding wrapped around");
	check_invariant("after two impossible requests");
	CHECK(mymalloc(1024) != NULL,
	      "a failed request left the allocator unable to serve a small one");

	/* The other end of the same rule: MYM_ARENA_BYTES is clamped to at
	 * least 64 KiB, so a silly request for a 1 KiB arena still gets one big
	 * enough to hold a free list. */
	mym_reset();
	use_arena("1024");
	REQUIRE(mymalloc(64) != NULL, "mymalloc(64) failed with "
	                              "MYM_ARENA_BYTES=1024");
	mym_get_stats(&s);
	CHECK(s.arena_bytes >= 65536,
	      "MYM_ARENA_BYTES=1024 produced a %zu-byte arena; the contract "
	      "clamps it to at least 64 KiB", s.arena_bytes);
}

/* ------------------------------------------------------------------ Part 2 */

static void p2_reuse(void)
{
	void *a, *b;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	a = mymalloc(128);
	REQUIRE(a != NULL, "mymalloc(128) returned NULL on an empty arena");
	myfree(a);
	b = mymalloc(128);
	REQUIRE(b != NULL, "mymalloc(128) returned NULL after freeing an "
	                   "identical block");
	CHECK(a == b,
	      "the freed block at %p was not reused: the next identical request "
	      "returned %p instead. First fit on an empty arena must hand back "
	      "the same address.", a, b);
	check_invariant("after reuse");
}

static void p2_churn(void)
{
	enum { LIVE = 4, ROUNDS = 2000 };
	void *p[LIVE] = { NULL, NULL, NULL, NULL };
	size_t sz[LIVE] = { 0, 0, 0, 0 };
	int i, slot;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);

	/* Sizes run 64, 128, ... 448 and back, so the mean is 256 bytes: 2000
	 * rounds is 500 KiB of traffic through a 64 KiB arena. It only fits if
	 * freed blocks come back. */
	for (i = 0; i < ROUNDS; i++) {
		slot = i % LIVE;
		if (p[slot]) {
			if (!painted(p[slot], sz[slot], (unsigned char)slot)) {
				fail("round %d: the block in slot %d was corrupted "
				     "while it was live", i, slot);
				return;
			}
			myfree(p[slot]);
		}
		sz[slot] = (size_t)(64 + (i % 7) * 64);
		p[slot] = mymalloc(sz[slot]);
		if (!p[slot]) {
			fail("round %d of %d: mymalloc(%zu) returned NULL. The "
			     "arena is 64 KiB and only %d blocks are live, so "
			     "freed space is not coming back.",
			     i, ROUNDS, sz[slot], LIVE);
			return;
		}
		paint(p[slot], sz[slot], (unsigned char)slot);
	}
	check_invariant("after 2000 rounds");

	/* Four blocks are live, and they have to be live INSIDE the arena --
	 * an allocator that quietly gets memory from somewhere else would
	 * otherwise sail through everything above. */
	{
		mym_stats_t s;
		mym_get_stats(&s);
		CHECK(s.bytes_in_use >= 64,
		      "four blocks are live but the arena reports only %zu bytes "
		      "in use: the memory being handed out is not coming from the "
		      "arena", s.bytes_in_use);
	}
}

/* A remainder of exactly MYM_HEADER_BYTES has nowhere to put its own header.
 * Splitting it off produces a block with a zero-length payload -- and then
 * writing a free-list link into that payload writes into the NEXT block's
 * header. The damage surfaces thousands of operations later.
 *
 * Build the situation exactly: one free block hemmed in by live blocks so
 * nothing merges, and a request for all of it but sixteen bytes. Sixteen bytes
 * cannot be a block under any design -- a block is a header plus at least
 * MYM_MIN_PAYLOAD -- so no free block may come out of the split. */
static void p2_tiny_remainder(void)
{
	mym_stats_t before, after;
	void *a, *b, *p;
	size_t hole, ask;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	a = mymalloc(256);
	b = mymalloc(64);
	REQUIRE(a && b, "the layout allocations failed");
	REQUIRE(fill_arena(), "could not fill the rest of the arena");

	myfree(a);
	mym_get_stats(&before);
	REQUIRE(before.free_blocks == 1,
	        "expected exactly one free block, got %zu block(s) totalling "
	        "%zu bytes", before.free_blocks, before.free_bytes);
	hole = before.free_bytes;
	REQUIRE(hole >= 256,
	        "freeing the 256-byte block left a hole of only %zu bytes", hole);

	ask = hole - MYM_HEADER_BYTES;
	p = mymalloc(ask);
	REQUIRE(p != NULL, "a %zu-byte request failed against a %zu-byte hole",
	        ask, hole);
	CHECK(p == a, "the request was not served from the only free block");

	mym_get_stats(&after);
	CHECK(after.free_blocks == 0,
	      "splitting a %zu-byte block for a %zu-byte request left %zu free "
	      "block(s). The 16 bytes over are exactly one header with no room "
	      "for a payload; a block cannot be made out of them.",
	      hole, ask, after.free_blocks);
	CHECK(after.free_bytes == 0,
	      "%zu bytes are still recorded as free", after.free_bytes);
	check_invariant("after the too-small remainder");
}

static void p2_split(void)
{
	enum { N = 8 };
	void *whole, *q[N];
	mym_stats_t s;
	size_t big, sz;
	int i;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	REQUIRE(mymalloc(16) != NULL, "mymalloc(16) returned NULL");

	mym_get_stats(&s);
	REQUIRE(s.largest_free_block > 8192,
	        "largest_free_block is %zu in a 64 KiB arena",
	        s.largest_free_block);

	whole = alloc_largest(&big);
	REQUIRE(whole != NULL,
	        "no request could take the largest free block (%zu bytes)",
	        s.largest_free_block);
	CHECK(mymalloc(16) == NULL,
	      "the arena claimed to be full but served another 16 bytes");
	myfree(whole);

	/* One free block of `big` bytes, and nothing else. Eight requests of a
	 * bit under an eighth of it each must all fit -- which they only can if
	 * the block is cut up rather than handed over whole. */
	sz = big / N - 64;
	for (i = 0; i < N; i++) {
		q[i] = mymalloc(sz);
		if (!q[i]) {
			fail("block %d of %d (%zu bytes) failed although one free "
			     "block of %zu bytes was available: the block is not "
			     "being split", i, N, sz, big);
			return;
		}
		paint(q[i], sz, (unsigned char)(i + 1));
	}
	for (i = 0; i < N; i++)
		CHECK(painted(q[i], sz, (unsigned char)(i + 1)),
		      "block %d was overwritten: the split produced overlapping "
		      "blocks", i);
	check_invariant("after splitting");
}

/* ------------------------------------------------------------------ Part 3 */

/* Carve the arena into four adjacent 1 KiB blocks with nothing free left
 * anywhere else, free them in the given order, and demand a block that only
 * exists if all four merged.
 *
 * 4 x 1024 payload + the 3 headers between them = 4144 bytes once merged.
 * Three merged blocks come to 3104. So a request for 4096 succeeds only when
 * every one of the three joins was made.
 */
static void merge_pattern(const int *order)
{
	void *a[4], *big;
	mym_stats_t s;
	int i;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);

	for (i = 0; i < 4; i++) {
		a[i] = mymalloc(1024);
		REQUIRE(a[i] != NULL, "mymalloc(1024) failed for block %d", i);
	}
	REQUIRE(fill_arena(), "could not fill the rest of the arena");
	mym_get_stats(&s);
	REQUIRE(s.free_bytes == 0,
	        "the arena should be full but %zu free bytes remain in %zu "
	        "block(s); the rest of this case would not mean anything",
	        s.free_bytes, s.free_blocks);

	for (i = 0; i < 4; i++)
		myfree(a[order[i]]);
	check_invariant("after the four frees");

	big = mymalloc(4096);
	CHECK(big != NULL,
	      "the four adjacent 1 KiB blocks did not merge: a 4096-byte request "
	      "failed although 4144 contiguous bytes are free");
	if (big)
		CHECK(big == a[0],
		      "the merged block should start where the lowest of the four "
		      "did (%p), but the request was served from %p",
		      a[0], big);
	check_invariant("after the merged allocation");
}

static void p3_merge_up(void)   { static const int o[4] = {0,1,2,3}; merge_pattern(o); }
static void p3_merge_down(void) { static const int o[4] = {3,2,1,0}; merge_pattern(o); }
static void p3_merge_both(void) { static const int o[4] = {1,3,0,2}; merge_pattern(o); }

static void p3_collapse(void)
{
	/* SLACK is how much of the arena this case is willing to lose to
	 * per-block metadata. The reference design spends one 16-byte header;
	 * a boundary tag spends 32; the case must not care which, so it asks
	 * for the arena minus a kilobyte and calls that "nearly all of it".
	 * A kilobyte is 1.5% of this 64 KiB arena, and it only has to cover ONE
	 * block's metadata -- by the time demand_all is asked for, everything
	 * has been freed and the heap holds a single free block. Do not tune
	 * SLACK down towards the true per-block cost: it is deliberately loose
	 * so that no design is measured against the reference's overhead. */
	enum { N = 100, DEMAND = 90 * 256, SLACK = 1024 };
	void *p[N], *filler, *big, *whole, *lowest;
	mym_stats_t s;
	size_t demand_all;
	int i, j;
	void *t;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	for (i = 0; i < N; i++) {
		p[i] = mymalloc(256);
		REQUIRE(p[i] != NULL, "mymalloc(256) failed at block %d", i);
	}
	mym_get_stats(&s);
	CHECK(s.bytes_in_use >= N * 256,
	      "100 blocks of 256 bytes are live but the arena reports only %zu "
	      "bytes in use", s.bytes_in_use);

	/* Take the rest of the arena too, and hold on to it. Without this the
	 * request at the end of the case could be served out of the untouched
	 * tail, and would say nothing about whether the hundred holes joined. */
	filler = alloc_largest(NULL);
	REQUIRE(filler != NULL, "could not fill the rest of the arena");
	mym_get_stats(&s);
	REQUIRE(s.free_bytes == 0,
	        "the arena should be full but %zu free bytes remain",
	        s.free_bytes);

	/* p[0] is the first block ever handed out of a fresh arena, so its
	 * payload sits at the very bottom of the arena, one header in. Keep it
	 * before the shuffle: it is where a fully coalesced run of these
	 * hundred holes begins, and where a fully coalesced empty arena begins
	 * too. Both allocations at the end of this case must land on it, and
	 * that is what makes them evidence -- an address inside this arena is
	 * an address that came OUT of this arena, and no amount of borrowing
	 * memory from somewhere else can produce it. */
	lowest = p[0];

	/* Free them in a shuffled order so that merges happen in every
	 * direction and every combination. */
	for (i = 0; i < N; i++) {
		j = (i * 37 + 11) % N;
		t = p[i]; p[i] = p[j]; p[j] = t;
	}
	for (i = 0; i < N; i++)
		myfree(p[i]);
	check_invariant("after freeing the hundred blocks");

	/* The statistics have not been consulted yet, and this does not consult
	 * them: 23040 contiguous bytes exist only where a hundred 256-byte
	 * holes really did join up. The filler is still live, so they are the
	 * only free space there is. */
	big = mymalloc(DEMAND);
	CHECK(big != NULL,
	      "a %d-byte request failed after all 100 blocks of 256 bytes were "
	      "freed. Those hundred holes are the only free space in the arena, "
	      "and that much of it is contiguous only if the frees merged.",
	      (int)DEMAND);
	if (big)
		CHECK(big == lowest,
		      "the %d-byte request was served from %p; the hundred holes "
		      "begin at %p and first fit takes the lowest-addressed block "
		      "that fits, so a merged arena serves it from there. A "
		      "pointer from anywhere else did not come out of those "
		      "holes.",
		      (int)DEMAND, big, lowest);
	myfree(big);
	myfree(filler);

	mym_get_stats(&s);
	demand_all = s.arena_bytes - SLACK;
	CHECK(s.free_blocks == 1,
	      "after freeing everything the heap holds %zu free blocks; "
	      "fully coalesced it holds exactly one", s.free_blocks);
	CHECK(s.bytes_in_use == 0,
	      "%zu bytes are still recorded as in use after freeing everything",
	      s.bytes_in_use);
	/* How much metadata a block carries is the student's business -- one
	 * 16-byte header, or a header and a boundary tag, or more -- so this
	 * case never says what largest_free_block ought to be. It says only
	 * what no design can escape: the biggest free block cannot be larger
	 * than the arena minus one header, and after an empty heap has fully
	 * coalesced it cannot be much smaller either. */
	CHECK(s.largest_free_block <= s.arena_bytes - MYM_HEADER_BYTES,
	      "the largest free block is %zu, which is more than the arena "
	      "(%zu) minus the one header that block must itself carry",
	      s.largest_free_block, s.arena_bytes);
	CHECK(s.largest_free_block >= demand_all,
	      "after freeing everything the largest free block is %zu, less "
	      "than the whole arena (%zu) minus a kilobyte of slack for "
	      "whatever metadata your blocks carry",
	      s.largest_free_block, s.arena_bytes);
	check_invariant("after freeing everything");

	/* And the assertion that does not take the allocator's word for any of
	 * it. Every block has been freed, so an allocator that really merged
	 * can serve nearly the whole arena in one piece; one that did not is
	 * holding a hundred 256-byte holes and cannot serve a tenth of it, no
	 * matter what its statistics say. Nothing here assumes a per-block
	 * overhead of 16 bytes, or of anything else. */
	whole = mymalloc(demand_all);
	CHECK(whole != NULL,
	      "a %zu-byte request failed on a completely empty arena of %zu "
	      "bytes. Everything has been freed, so the whole arena is one "
	      "free block if the merges were done -- and a kilobyte is left "
	      "over for your per-block metadata.",
	      demand_all, s.arena_bytes);
	if (whole)
		CHECK(whole == lowest,
		      "the whole-arena request came back at %p. Everything has "
		      "been freed, so the arena is one free block starting at its "
		      "very bottom, and the first allocation out of it is at %p -- "
		      "exactly where the first allocation out of the fresh arena "
		      "was. An address that is not that one is not an address in "
		      "this arena, and this case is about serving a request FROM "
		      "the arena, not about producing a pointer.",
		      whole, lowest);
	myfree(whole);
	check_invariant("after the whole-arena allocation");
}

/* ------------------------------------------------------------------ Part 4 */

/* Lay the arena out as
 *
 *  [live][A:512][live][B:128][live][C:1024][live][D:128][live][E:1024][live]
 *
 * then free A..E. They are not adjacent, so they stay five separate free
 * blocks, and filling the tail means they are the ONLY free blocks. A request
 * for 100 bytes now fits in all five, and which one it lands in is entirely
 * the fit policy's decision:
 *
 *    first fit -> A, the lowest address that fits
 *    best fit  -> B, the smallest that fits -- and B and D are the SAME size,
 *                 so the rule "ties go to the lowest-address candidate" is
 *                 what picks B rather than D
 *    worst fit -> C, the largest there is -- and C and E are the same size,
 *                 so the same tie-break picks C rather than E
 *
 * The duplicated sizes are the point. Without them every candidate is unique,
 * no tie ever arises, and the tie-break rule stated in the contract and in
 * mymalloc.h would be a rule no test could fail on.
 */
#define NCAND 5

static void fit_pattern(mym_fit_t policy, int want)
{
	void *live[NCAND + 1], *cand[NCAND], *p;
	static const size_t csize[NCAND] = { 512, 128, 1024, 128, 1024 };
	static const char *cname[NCAND] = {
		"A (512)", "B (128)", "C (1024)", "D (128, same size as B)",
		"E (1024, same size as C)"
	};
	mym_stats_t s;
	int i;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	for (i = 0; i < NCAND; i++) {
		live[i] = mymalloc(64);
		cand[i] = mymalloc(csize[i]);
		REQUIRE(live[i] && cand[i], "the layout allocations failed");
	}
	live[NCAND] = mymalloc(64);
	REQUIRE(live[NCAND] != NULL, "the layout allocations failed");

	REQUIRE(fill_arena(), "could not fill the tail of the arena");

	for (i = 0; i < NCAND; i++)
		myfree(cand[i]);

	mym_get_stats(&s);
	REQUIRE(s.free_blocks == NCAND,
	        "the layout should leave exactly %d free blocks, not %zu; they "
	        "are separated by live blocks and must not have merged",
	        NCAND, s.free_blocks);

	mym_set_fit(policy);
	REQUIRE(mym_get_fit() == policy,
	        "mym_set_fit() did not stick: mym_get_fit() returns %d",
	        (int)mym_get_fit());

	p = mymalloc(100);
	REQUIRE(p != NULL, "the 100-byte request failed");
	for (i = 0; i < NCAND; i++) {
		if (p == cand[i]) {
			CHECK(i == want,
			      "the 100-byte request was served from block %s; this "
			      "policy must choose %s. Best and worst fit break ties "
			      "by taking the lowest-address candidate.",
			      cname[i], cname[want]);
			return;
		}
	}
	fail("the 100-byte request was served from %p, which is none of the "
	     "%d free blocks (%p, %p, %p, %p, %p)",
	     p, NCAND, cand[0], cand[1], cand[2], cand[3], cand[4]);
}

static void p4_first(void) { fit_pattern(MYM_FIT_FIRST, 0); }
static void p4_best(void)  { fit_pattern(MYM_FIT_BEST,  1); }
static void p4_worst(void) { fit_pattern(MYM_FIT_WORST, 2); }

/* ------------------------------------------------------------------ Part 5 */

static void p5_double_free(void)
{
	void *a, *b;

	use_arena("65536");
	a = mymalloc(64);
	REQUIRE(a != NULL, "mymalloc(64) returned NULL");
	myfree(a);
	mym_clear_error();
	myfree(a);
	CHECK(mym_last_error() == MYM_ERR_DOUBLE_FREE,
	      "freeing the same pointer twice reported error %d, expected "
	      "MYM_ERR_DOUBLE_FREE (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_DOUBLE_FREE);
	CHECK(mym_check_heap() == 0,
	      "the heap is inconsistent after a double free was rejected");

	/* The order of the checks, which mymalloc.h states and nothing else
	 * here exercises. `a + 8` is misaligned AND sits inside a block whose
	 * in-use bit is now clear, so it is simultaneously "not a block" and
	 * "already free". The contract says bounds first, then alignment and
	 * header validity, then the in-use bit -- so the verdict is
	 * NOT_A_BLOCK: a pointer that never passed the validity test never
	 * reaches the question of whether the block it does not name is free.
	 * An implementation that asks "is it already free?" first answers
	 * DOUBLE_FREE, and is reading a flags word out of the middle of
	 * somebody else's block to do it. */
	mym_clear_error();
	myfree((char *)a + 8);
	CHECK(mym_last_error() == MYM_ERR_NOT_A_BLOCK,
	      "freeing a misaligned pointer into an already-freed block "
	      "reported error %d, expected MYM_ERR_NOT_A_BLOCK (%d). It is "
	      "both invalid and already free; the check order in mymalloc.h "
	      "decides which answer is right, and validity comes first.",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_A_BLOCK);
	CHECK(mym_check_heap() == 0,
	      "the heap is inconsistent after that free was rejected");

	b = mymalloc(64);
	CHECK(b != NULL, "the allocator stopped working after a double free");
	if (b) {
		paint(b, 64, 0x33);
		CHECK(painted(b, 64, 0x33), "the block handed out after a double "
		                            "free is not usable");
	}
	check_invariant("after a rejected double free");
}

static void p5_interior(void)
{
	size_t plausible[2];
	void *a;

	use_arena("65536");
	a = mymalloc(128);
	REQUIRE(a != NULL, "mymalloc(128) returned NULL");
	paint(a, 128, 0x55);

	mym_clear_error();
	myfree((char *)a + 32);
	CHECK(mym_last_error() == MYM_ERR_NOT_A_BLOCK,
	      "freeing an interior pointer (block + 32) reported error %d, "
	      "expected MYM_ERR_NOT_A_BLOCK (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_A_BLOCK);

	mym_clear_error();
	myfree((char *)a + 8);
	CHECK(mym_last_error() == MYM_ERR_NOT_A_BLOCK,
	      "freeing a misaligned interior pointer (block + 8) reported error "
	      "%d, expected MYM_ERR_NOT_A_BLOCK (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_A_BLOCK);

	CHECK(painted(a, 128, 0x55),
	      "a rejected interior free damaged the block's contents");
	CHECK(mym_check_heap() == 0,
	      "the heap is inconsistent after two interior frees were rejected");

	/* Both of those land on painted bytes, and painted bytes read as an
	 * absurd size -- so a plausible-size check alone rejects them and the
	 * magic number never has to do any work.
	 *
	 * This one is the real test. A caller who keeps a length and a flag
	 * word at the front of its own buffer, and then hands out a pointer to
	 * the bytes after them, has written something that looks exactly like a
	 * header: a legal multiple-of-16 size, in range, with the in-use bit
	 * set. Everything about it is plausible. The only thing wrong with it
	 * is that it has no magic number in it, and nothing but the magic
	 * number can tell. */
	plausible[0] = 64;               /* a legal payload size */
	plausible[1] = 1;                /* in use -- but no magic */
	memcpy(a, plausible, sizeof(plausible));

	mym_clear_error();
	myfree((char *)a + MYM_HEADER_BYTES);
	CHECK(mym_last_error() == MYM_ERR_NOT_A_BLOCK,
	      "freeing an interior pointer whose preceding 16 bytes are a "
	      "plausible-looking header reported error %d, expected "
	      "MYM_ERR_NOT_A_BLOCK (%d). Nothing about those bytes is out of "
	      "range; what they lack is the magic number.",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_A_BLOCK);
	CHECK(mym_check_heap() == 0,
	      "the heap is damaged after that free: it was accepted, and the "
	      "block it 'freed' does not exist");

	mym_clear_error();
	myfree(a);
	CHECK(mym_last_error() == MYM_OK,
	      "the real pointer was then rejected too, with error %d",
	      (int)mym_last_error());
	check_invariant("after the interior frees");
}

static void p5_outside(void)
{
	static char elsewhere[256];
	char onstack[256];
	mym_stats_t s;
	void *a;

	use_arena("65536");

	/* Before anything is allocated there is no arena, so a non-NULL
	 * pointer cannot be inside one. That is NOT_OURS, and answering it must
	 * not bring an arena into existence: myfree() is not an allocation. */
	mym_clear_error();
	myfree(elsewhere + 32);
	CHECK(mym_last_error() == MYM_ERR_NOT_OURS,
	      "freeing a foreign pointer before the arena exists reported error "
	      "%d, expected MYM_ERR_NOT_OURS (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_OURS);
	mym_get_stats(&s);
	CHECK(s.arena_bytes == 0,
	      "a rejected myfree() created an arena of %zu bytes; nothing but "
	      "mymalloc() may do that", s.arena_bytes);

	a = mymalloc(64);
	REQUIRE(a != NULL, "mymalloc(64) returned NULL");

	mym_clear_error();
	myfree(elsewhere + 64);
	CHECK(mym_last_error() == MYM_ERR_NOT_OURS,
	      "freeing a static buffer reported error %d, expected "
	      "MYM_ERR_NOT_OURS (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_OURS);

	mym_clear_error();
	myfree(onstack + 64);
	CHECK(mym_last_error() == MYM_ERR_NOT_OURS,
	      "freeing a stack address reported error %d, expected "
	      "MYM_ERR_NOT_OURS (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_OURS);

	CHECK(mym_check_heap() == 0,
	      "the heap is inconsistent after two foreign frees were rejected");
	/* A clean heap is not news, so the walker leaves last_error alone: the
	 * rejection recorded a moment ago is still readable. Only
	 * mym_clear_error() clears it. */
	CHECK(mym_last_error() == MYM_ERR_NOT_OURS,
	      "mym_check_heap() found a clean heap and changed last_error to "
	      "%d; it must leave it at the MYM_ERR_NOT_OURS (%d) the rejected "
	      "free recorded, because only mym_clear_error() clears it",
	      (int)mym_last_error(), (int)MYM_ERR_NOT_OURS);
	myfree(a);
	check_invariant("after the foreign frees");
}

static void p5_overflow(void)
{
	void *a, *b, *c;
	size_t plausible[2];

	use_arena("65536");
	a = mymalloc(64);
	b = mymalloc(64);
	REQUIRE(a != NULL && b != NULL, "the two allocations failed");
	REQUIRE((char *)b >= (char *)a + 64 + MYM_HEADER_BYTES,
	        "the second block is at %p, only %ld bytes after the first; two "
	        "consecutive 64-byte allocations cannot overlap and each needs "
	        "at least a %d-byte header",
	        b, (long)((char *)b - (char *)a), MYM_HEADER_BYTES);
	REQUIRE(mym_check_heap() == 0,
	        "mym_check_heap() already reports %d problem(s) on an "
	        "undamaged heap", mym_check_heap());

	/* The classic off-by-16: writing past the end of a block lands squarely
	 * in the next block's header, which the contract puts exactly
	 * MYM_HEADER_BYTES in front of the next payload. Nothing else in the
	 * heap changes, so only a walker that validates headers can notice. */
	memset(header_of(b), 0xff, MYM_HEADER_BYTES);

	CHECK(mym_check_heap() != 0,
	      "mym_check_heap() reports a clean heap after the next block's "
	      "header was overwritten with 0xff");
	CHECK(mym_last_error() == MYM_ERR_CORRUPT,
	      "after detecting the damage the error is %d, expected "
	      "MYM_ERR_CORRUPT (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_CORRUPT);

	/* That one is caught by any sanity check on the size field, because
	 * 0xffff... is neither a multiple of 16 nor smaller than the arena. The
	 * harder case is an overflow that happens to write PLAUSIBLE bytes: a
	 * size that is a legal multiple of 16, the in-use bit set, the blocks
	 * still tiling the arena, the free list still correct. Nothing is out
	 * of range and nothing is inconsistent -- the only thing wrong with
	 * this header is that it is not one. That is what the magic number is
	 * for, and this is the only check that requires it. */
	mym_reset();
	mym_clear_error();
	a = mymalloc(64);
	b = mymalloc(64);
	c = mymalloc(64);
	REQUIRE(a != NULL && b != NULL && c != NULL,
	        "the three allocations failed after mym_reset()");
	REQUIRE(mym_check_heap() == 0,
	        "mym_check_heap() reports %d problem(s) on a fresh heap",
	        mym_check_heap());

	plausible[0] = 64;    /* a legal payload size */
	plausible[1] = 1;     /* in use, but with no magic number in it */
	memcpy(header_of(b), plausible, sizeof(plausible));

	CHECK(mym_check_heap() != 0,
	      "mym_check_heap() reports a clean heap after the next block's "
	      "header was overwritten with a plausible-looking one. Everything "
	      "about it is in range; what it lacks is the magic number.");
	/* The heap is deliberately wrecked; this case ends here. */
}

/* The four cases below damage a heap in ways the physical walk alone cannot
 * see, or cannot survive. They are the only places in the suite that write to
 * a header directly, and they do it through the one thing the contract fixes
 * about a header: sixteen bytes immediately before the payload, the payload
 * size in the first word, the flags -- magic in the top 32 bits, in-use in
 * bit 0 -- in the second. Nothing here knows how your free list is stored.
 *
 * Note the shape of each: the damage is invisible to a walker that checks only
 * that every header carries the magic, because every header still does. */

/* The free-list half of the walker, on its own.
 *
 * Set the in-use bit on a block that is on the free list, and nothing about
 * the physical walk changes: every header is valid, the blocks still tile the
 * arena, and no two free blocks are adjacent -- the physical walk simply finds
 * one fewer free block than the list holds. Only the free-list half of the
 * walker can see it, and this is the bug it catches at the allocating end: a
 * block left on the free list after being handed out, which the next
 * allocation will hand out a second time.
 *
 * This one is visible to the per-entry test alone ("every entry the list holds
 * is a valid block that is actually free"). p5_walker_leak, below, is the same
 * check's other half -- the counts -- and needs the reconciliation. */
static void p5_walker_list(void)
{
	void *x, *a, *y;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	x = mymalloc(64);
	a = mymalloc(64);
	y = mymalloc(64);
	REQUIRE(x && a && y, "the layout allocations failed");
	REQUIRE(fill_arena(), "could not fill the rest of the arena");

	myfree(a);                      /* a is free, both neighbours in use */
	REQUIRE(mym_check_heap() == 0,
	        "mym_check_heap() reports %d problem(s) on an undamaged heap",
	        mym_check_heap());

	header_of(a)[1] |= (size_t)1;   /* now it claims to be in use */

	CHECK(mym_check_heap() != 0,
	      "mym_check_heap() reports a clean heap although the free list "
	      "holds a block the physical walk sees as in use. Every header is "
	      "still valid and the arena is still tiled, so the physical walk "
	      "cannot notice: this is what the free-list half of the walker is "
	      "for.");
	CHECK(mym_last_error() == MYM_ERR_CORRUPT,
	      "after detecting the damage the error is %d, expected "
	      "MYM_ERR_CORRUPT (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_CORRUPT);
	/* The heap is deliberately wrecked; this case ends here. */
}

/* The other half of the same check: the count, on its own.
 *
 * p5_walker_list above catches a block that is ON the list and should not be.
 * A per-entry test ("every entry the list holds is a valid, free block") sees
 * that one by itself, so it says nothing about whether the two walks are
 * reconciled by NUMBER. This case is the leak: a block that is genuinely free
 * and is NOT on the list at all -- the bug a real allocator actually commits,
 * because dropping a link is easy and nothing ever notices until the heap has
 * quietly shrunk.
 *
 * Lay out eight live blocks, fill the arena, free one of them, then clear the
 * in-use bit on a different one whose two PHYSICAL neighbours are both still
 * live. After that edit:
 *
 *   - every header still carries the magic and a legal size, and the blocks
 *     still tile the arena, so the physical walk is happy;
 *   - no two free blocks are physically adjacent, so the adjacency check is
 *     happy;
 *   - the one entry the free list holds is a real, in-arena, genuinely free
 *     block, so every per-entry test is happy.
 *
 * The physical walk finds two free blocks and the list holds one. Only
 * comparing the two counts can say so. */
static void p5_walker_leak(void)
{
	enum { NB = 8 };
	void *b[NB];
	int i;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	for (i = 0; i < NB; i++) {
		b[i] = mymalloc(64);
		REQUIRE(b[i] != NULL, "the layout allocation %d failed", i);
	}
	for (i = 1; i < NB; i++)
		REQUIRE((char *)b[i - 1] < (char *)b[i],
		        "eight consecutive allocations came back out of address "
		        "order");
	REQUIRE(fill_arena(), "could not fill the rest of the arena");

	myfree(b[1]);                   /* b[0] and b[2] are still live */
	REQUIRE(mym_check_heap() == 0,
	        "mym_check_heap() reports %d problem(s) on an undamaged heap",
	        mym_check_heap());

	/* b[5] now reads as free to the physical walk, but it was never freed,
	 * so it is on nobody's free list. b[4] and b[6] are live, so it has no
	 * free physical neighbour either. */
	header_of(b[5])[1] &= ~(size_t)1;

	CHECK(mym_check_heap() != 0,
	      "mym_check_heap() reports a clean heap although the physical walk "
	      "finds two free blocks and the free list holds one. Every header "
	      "is valid, the arena is still tiled, no two free blocks are "
	      "adjacent, and the single list entry is a genuine free block -- so "
	      "nothing but counting the two walks and comparing them can see "
	      "this. A free block that has fallen off the list is a leak, and it "
	      "is the bug this half of the check exists to catch.");
	CHECK(mym_last_error() == MYM_ERR_CORRUPT,
	      "after detecting the damage the error is %d, expected "
	      "MYM_ERR_CORRUPT (%d)",
	      (int)mym_last_error(), (int)MYM_ERR_CORRUPT);
	/* The heap is deliberately wrecked; this case ends here. */
}

/* The adjacency half of the walker, on its own.
 *
 * Lay out five live blocks Z A B C D, free A and C -- they are not adjacent,
 * so the heap is clean and the list holds two entries. Then two edits:
 *
 *   - clear B's in-use bit, so the physical walk sees B and C as two free
 *     blocks side by side. That is a missed merge and nothing else;
 *   - give Z's size field the whole of A as well, so the physical walk steps
 *     straight from Z to B and never sees A at all.
 *
 * A's header is untouched and still says "free", so the free list still holds
 * two valid, in-arena, ascending, genuinely free entries -- and the physical
 * walk now finds exactly two free blocks too. The counts agree. Every header
 * carries the magic. The blocks still tile the arena. The single thing wrong
 * with this heap is that two free blocks are physically adjacent, and the
 * adjacency check is the only thing that can say so. */
static void p5_walker_adjacent(void)
{
	void *z, *a, *b, *c, *d;
	mym_stats_t s;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	z = mymalloc(64);
	a = mymalloc(64);
	b = mymalloc(64);
	c = mymalloc(64);
	d = mymalloc(64);
	REQUIRE(z && a && b && c && d, "the layout allocations failed");
	REQUIRE((char *)z < (char *)a && (char *)a < (char *)b &&
	        (char *)b < (char *)c && (char *)c < (char *)d,
	        "five consecutive allocations came back out of address order");
	REQUIRE(fill_arena(), "could not fill the rest of the arena");

	myfree(a);
	myfree(c);
	mym_get_stats(&s);
	REQUIRE(s.free_blocks == 2,
	        "freeing two blocks separated by a live one should leave 2 free "
	        "blocks, not %zu", s.free_blocks);
	REQUIRE(mym_check_heap() == 0,
	        "mym_check_heap() reports %d problem(s) on an undamaged heap",
	        mym_check_heap());

	header_of(b)[1] &= ~(size_t)1;                 /* B reads as free */
	header_of(z)[0] += MYM_HEADER_BYTES + header_of(a)[0];   /* Z eats A */

	CHECK(mym_check_heap() != 0,
	      "mym_check_heap() reports a clean heap although two physically "
	      "adjacent blocks are both marked free. Every header is valid, the "
	      "blocks tile the arena, and the free list has exactly as many "
	      "entries as the walk found free blocks -- all of which is true "
	      "here. With coalescing on, adjacent free blocks are a merge that "
	      "did not happen, and that is the only check that can see it.");
	/* The heap is deliberately wrecked; this case ends here. */
}

/* And the other side of the same rule. With MYM_COALESCE=0 two adjacent free
 * blocks are exactly what was asked for, so the walker must NOT complain --
 * "no two free blocks are physically adjacent" is a rule about coalescing
 * allocators. A walker that checks it unconditionally fails here. */
static void p5_walker_coalesce_off(void)
{
	void *a, *b, *c;
	mym_stats_t s;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	a = mymalloc(64);
	b = mymalloc(64);
	c = mymalloc(64);
	REQUIRE(a && b && c, "the layout allocations failed");
	REQUIRE(fill_arena(), "could not fill the rest of the arena");

	myfree(a);
	myfree(b);
	mym_get_stats(&s);
	REQUIRE(s.free_blocks == 2,
	        "with MYM_COALESCE=0 two adjacent frees must stay two blocks, "
	        "not %zu. This case is run with coalescing off on purpose.",
	        s.free_blocks);
	CHECK(mym_check_heap() == 0,
	      "mym_check_heap() reports %d problem(s) on a heap that is exactly "
	      "what MYM_COALESCE=0 asks for: two physically adjacent free "
	      "blocks. Adjacent free blocks are a missed merge only when "
	      "coalescing is on.", mym_check_heap());
	myfree(c);
	check_invariant("with coalescing off");
}

/* The bound on the walk.
 *
 * Overwrite one block's size field so that its physical successor is computed
 * as an EARLIER block. Every header the walk then visits carries the magic and
 * looks fine, so a walker that validates nothing but the magic and trusts the
 * size it reads walks the same two blocks for ever -- and the harness reports
 * it as a timeout, with no clue as to why.
 *
 * mym_check_heap() must come back. What it says is secondary; that it returns
 * at all is the requirement, and the two ways to guarantee it are to bound the
 * number of steps and to refuse a size the arena cannot contain. The reference
 * does both. */
static void p5_walker_bound(void)
{
	void *a, *b;
	int rc;

	use_arena("65536");
	mym_set_fit(MYM_FIT_FIRST);
	a = mymalloc(64);
	b = mymalloc(64);
	REQUIRE(a && b, "the two allocations failed");
	REQUIRE((char *)b > (char *)a, "the blocks came back out of order");
	REQUIRE(mym_check_heap() == 0,
	        "mym_check_heap() reports %d problem(s) on an undamaged heap",
	        mym_check_heap());

	/* header + this size lands exactly back on the first block's header. */
	header_of(b)[0] = (size_t)((char *)header_of(a) - (char *)header_of(b))
	                  - MYM_HEADER_BYTES;

	fprintf(stderr, "    (calling mym_check_heap() on a heap whose physical "
	                "walk is now a two-block cycle;\n"
	                "     if this case times out, that walk is unbounded)\n");
	fflush(stderr);

	rc = mym_check_heap();
	CHECK(rc != 0,
	      "mym_check_heap() returned 0 for a heap whose physical walk is a "
	      "cycle: one block's size field says its successor is the block "
	      "before it");
	/* The heap is deliberately wrecked; this case ends here. */
}

/* A fixed-seed randomised workload: allocate, paint, verify, free, in an
 * unpredictable order, with every live block verified every time it is
 * touched. This is the case that catches the bugs the structured cases do not
 * have the imagination for. */
static unsigned long lcg_state = 12345;

static unsigned long lcg(void)
{
	lcg_state = lcg_state * 6364136223846793005UL + 1442695040888963407UL;
	return lcg_state >> 33;
}

static void p5_stress(void)
{
	enum { SLOTS = 128 };
	void *p[SLOTS];
	size_t sz[SLOTS];
	unsigned char tag[SLOTS];
	mym_stats_t s;
	long ops, i;
	int slot;
	const char *e;

	use_arena("1048576");
	mym_set_fit(MYM_FIT_FIRST);
	memset(p, 0, sizeof(p));

	e = getenv("MYM_STRESS_OPS");
	ops = e ? atol(e) : 200000;
	if (ops < 1000)
		ops = 1000;

	for (i = 0; i < ops; i++) {
		slot = (int)(lcg() % SLOTS);
		if (p[slot]) {
			if (!painted(p[slot], sz[slot], tag[slot])) {
				fail("op %ld: the %zu-byte block in slot %d was "
				     "corrupted while it was live", i, sz[slot], slot);
				return;
			}
			myfree(p[slot]);
			p[slot] = NULL;
			if (mym_last_error() != MYM_OK) {
				fail("op %ld: freeing a live block reported error %d",
				     i, (int)mym_last_error());
				return;
			}
			continue;
		}
		sz[slot] = 1 + lcg() % 2048;
		tag[slot] = (unsigned char)(lcg() & 0xff);
		p[slot] = mymalloc(sz[slot]);
		if (!p[slot]) {
			mym_get_stats(&s);
			fail("op %ld: mymalloc(%zu) failed. At most 256 KiB is ever "
			     "live in a 1 MiB arena. (in use %zu, free %zu in %zu "
			     "blocks, largest %zu)",
			     i, sz[slot], s.bytes_in_use, s.free_bytes,
			     s.free_blocks, s.largest_free_block);
			return;
		}
		paint(p[slot], sz[slot], tag[slot]);
	}

	mym_get_stats(&s);
	CHECK(s.bytes_in_use > 0,
	      "after %ld operations nothing is recorded as in use: the blocks "
	      "being handed out are not coming from the arena", ops);

	for (slot = 0; slot < SLOTS; slot++) {
		if (!p[slot])
			continue;
		CHECK(painted(p[slot], sz[slot], tag[slot]),
		      "the %zu-byte block in slot %d did not survive to the end",
		      sz[slot], slot);
		myfree(p[slot]);
	}

	CHECK(mym_check_heap() == 0,
	      "mym_check_heap() reports %d problem(s) after %ld random "
	      "operations", mym_check_heap(), ops);
	check_invariant("after the stress run");

	mym_get_stats(&s);
	CHECK(s.bytes_in_use == 0,
	      "%zu bytes are still in use after every block was freed",
	      s.bytes_in_use);
	CHECK(s.free_blocks == 1,
	      "the heap holds %zu free blocks after everything was freed; "
	      "fully coalesced it holds one", s.free_blocks);

	/* And the same claim again, without taking the allocator's word for
	 * it: three quarters of a 1 MiB arena is contiguous only if the whole
	 * run really did merge back down to one block. */
	CHECK(mymalloc(768 * 1024) != NULL,
	      "a %d-byte request failed after %ld random operations were undone; "
	      "the statistics above say the arena is one free block.",
	      768 * 1024, ops);
}

/* ---------------------------------------------------------------- dispatch */

static const struct {
	const char *name;
	void (*fn)(void);
} CASES[] = {
	{ "p1_align",        p1_align        },
	{ "p1_no_overlap",   p1_no_overlap   },
	{ "p1_survives",     p1_survives     },
	{ "p1_zero",         p1_zero         },
	{ "p1_free_null",    p1_free_null    },
	{ "p1_stats",        p1_stats_account},
	{ "p1_too_big",      p1_too_big      },
	{ "p2_reuse",        p2_reuse        },
	{ "p2_churn",        p2_churn        },
	{ "p2_split",        p2_split        },
	{ "p2_tiny_remainder", p2_tiny_remainder },
	{ "p3_merge_up",     p3_merge_up     },
	{ "p3_merge_down",   p3_merge_down   },
	{ "p3_merge_both",   p3_merge_both   },
	{ "p3_collapse",     p3_collapse     },
	{ "p4_first",        p4_first        },
	{ "p4_best",         p4_best         },
	{ "p4_worst",        p4_worst        },
	{ "p5_double_free",  p5_double_free  },
	{ "p5_interior",     p5_interior     },
	{ "p5_outside",      p5_outside      },
	{ "p5_overflow",     p5_overflow     },
	{ "p5_walker_list",  p5_walker_list  },
	{ "p5_walker_leak",  p5_walker_leak  },
	{ "p5_walker_adjacent", p5_walker_adjacent },
	{ "p5_walker_coalesce_off", p5_walker_coalesce_off },
	{ "p5_walker_bound", p5_walker_bound },
	{ "p5_stress",       p5_stress       },
};

int main(int argc, char **argv)
{
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "usage: cases <case-name>\n");
		return 2;
	}
	for (i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
		if (strcmp(CASES[i].name, argv[1]) == 0) {
			CASES[i].fn();
			return failures ? 1 : 0;
		}
	}
	fprintf(stderr, "cases: no such case '%s'\n", argv[1]);
	return 2;
}
