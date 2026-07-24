/* msg.h -- Lab 10, GIVEN CODE (complete, do not modify).
 *
 * Bounds-checked marshalling for a small fixed set of types.  Byte packing
 * is tedious and not the objective of this lab; protocol logic is.
 *
 * A message buffer (mbuf) wraps caller-owned storage.  Writers append with
 * mb_put_*; readers consume with mb_get_*.  Integers travel big-endian.
 * Any overflow or underflow sets the sticky error flag; check mb_ok() once
 * at the end instead of testing every call.
 */
#ifndef MSG_H
#define MSG_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint8_t *p;
	size_t cap;     /* storage size */
	size_t len;     /* bytes written so far */
	size_t off;     /* read cursor */
	int err;        /* sticky: any over/underflow */
} mbuf;

/* Writer: start an empty message in storage[0..cap). */
void mb_winit(mbuf *m, uint8_t *storage, size_t cap);

/* Reader: parse an existing message of len bytes. */
void mb_rinit(mbuf *m, uint8_t *storage, size_t len);

void mb_put_u32(mbuf *m, uint32_t v);
void mb_put_u64(mbuf *m, uint64_t v);
/* Raw bytes, no length prefix. */
void mb_put_bytes(mbuf *m, const void *b, size_t n);
/* u32 length prefix, then the bytes. */
void mb_put_blob(mbuf *m, const void *b, size_t n);

uint32_t mb_get_u32(mbuf *m);
uint64_t mb_get_u64(mbuf *m);
/* Copy exactly n raw bytes out. */
void mb_get_bytes(mbuf *m, void *out, size_t n);
/* Read a u32-prefixed blob into out (capacity max); returns its length,
 * or sets the error flag and returns 0 if it does not fit. */
size_t mb_get_blob(mbuf *m, void *out, size_t max);

/* Bytes remaining unread (reader). */
size_t mb_remain(const mbuf *m);

/* 1 if no operation so far has over- or under-run. */
int mb_ok(const mbuf *m);

#endif
