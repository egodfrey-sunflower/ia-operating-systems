/* msg.c -- Lab 10, GIVEN CODE (complete, do not modify).  See msg.h. */

#include "msg.h"

#include <string.h>

void mb_winit(mbuf *m, uint8_t *storage, size_t cap)
{
	m->p = storage;
	m->cap = cap;
	m->len = 0;
	m->off = 0;
	m->err = 0;
}

void mb_rinit(mbuf *m, uint8_t *storage, size_t len)
{
	m->p = storage;
	m->cap = len;
	m->len = len;
	m->off = 0;
	m->err = 0;
}

static int wroom(mbuf *m, size_t n)
{
	if (m->len + n > m->cap) {
		m->err = 1;
		return 0;
	}
	return 1;
}

static int rroom(mbuf *m, size_t n)
{
	if (m->off + n > m->len) {
		m->err = 1;
		return 0;
	}
	return 1;
}

void mb_put_u32(mbuf *m, uint32_t v)
{
	if (!wroom(m, 4))
		return;
	m->p[m->len++] = (uint8_t)(v >> 24);
	m->p[m->len++] = (uint8_t)(v >> 16);
	m->p[m->len++] = (uint8_t)(v >> 8);
	m->p[m->len++] = (uint8_t)v;
}

void mb_put_u64(mbuf *m, uint64_t v)
{
	mb_put_u32(m, (uint32_t)(v >> 32));
	mb_put_u32(m, (uint32_t)v);
}

void mb_put_bytes(mbuf *m, const void *b, size_t n)
{
	if (!wroom(m, n))
		return;
	memcpy(m->p + m->len, b, n);
	m->len += n;
}

void mb_put_blob(mbuf *m, const void *b, size_t n)
{
	mb_put_u32(m, (uint32_t)n);
	mb_put_bytes(m, b, n);
}

uint32_t mb_get_u32(mbuf *m)
{
	if (!rroom(m, 4))
		return 0;
	uint32_t v = ((uint32_t)m->p[m->off] << 24) |
	             ((uint32_t)m->p[m->off + 1] << 16) |
	             ((uint32_t)m->p[m->off + 2] << 8) |
	             (uint32_t)m->p[m->off + 3];
	m->off += 4;
	return v;
}

uint64_t mb_get_u64(mbuf *m)
{
	uint64_t hi = mb_get_u32(m);
	uint64_t lo = mb_get_u32(m);
	return (hi << 32) | lo;
}

void mb_get_bytes(mbuf *m, void *out, size_t n)
{
	if (!rroom(m, n)) {
		memset(out, 0, n);
		return;
	}
	memcpy(out, m->p + m->off, n);
	m->off += n;
}

size_t mb_get_blob(mbuf *m, void *out, size_t max)
{
	uint32_t n = mb_get_u32(m);
	if (m->err || n > max || !rroom(m, n)) {
		m->err = 1;
		return 0;
	}
	memcpy(out, m->p + m->off, n);
	m->off += n;
	return n;
}

size_t mb_remain(const mbuf *m)
{
	return m->len - m->off;
}

int mb_ok(const mbuf *m)
{
	return !m->err;
}
