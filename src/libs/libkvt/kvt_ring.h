/*
 * SHL - Ring buffer
 *
 * Copyright (c) 2011-2014 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

/*
 * Ring buffer
 */

#ifndef KVT_SHL_RING_H
#define KVT_SHL_RING_H

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

struct kvt_shl_ring {
	uint8_t *buf;		/* buffer or NULL */
	size_t size;		/* actual size of @buf */
	size_t start;		/* start position of ring */
	size_t used;		/* number of actually used bytes */
};

/* flush buffer so it is empty again */
void kvt_shl_ring_flush(struct kvt_shl_ring *r);

/* flush buffer, free allocated data and reset to initial state */
void kvt_shl_ring_clear(struct kvt_shl_ring *r);

/* get pointers to buffer data and their length */
size_t kvt_shl_ring_peek(struct kvt_shl_ring *r, struct iovec *vec);

/* copy data into external linear buffer */
size_t kvt_shl_ring_copy(struct kvt_shl_ring *r, void *buf, size_t size);

/* push data to the end of the buffer */
int kvt_shl_ring_push(struct kvt_shl_ring *r, const void *u8, size_t size);

/* pull data from the front of the buffer */
void kvt_shl_ring_pull(struct kvt_shl_ring *r, size_t size);

/* return size of occupied buffer in bytes */
static inline size_t kvt_shl_ring_get_size(struct kvt_shl_ring *r)
{
	return r->used;
}

#endif  /* KVT_SHL_RING_H */
