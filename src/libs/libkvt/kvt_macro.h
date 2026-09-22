/*
 * SHL - Macros
 *
 * Copyright (c) 2011-2014 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

/*
 * Scope-exit cleanup, for kvt_pty.c's fork paths.
 *
 * A variable declared _shl_close_ closes its descriptor on every exit from
 * the block, which is what makes the error paths of pty_open() a plain
 * `return` rather than a ladder of gotos. Keep the initialiser at -1: the
 * handler runs even when the block is left before the descriptor is opened.
 */

#ifndef KVT_SHL_MACRO_H
#define KVT_SHL_MACRO_H

#include <stddef.h>
#include <unistd.h>

/* Align to the next higher power of two; 0 and an overflowing value both
 * give 0. The ring buffer masks its indices, so a size that is not a power
 * of two silently wraps to the wrong offset. */
static inline size_t KVT_SHL_ALIGN_POWER2(size_t u)
{
	return 1ULL << ((sizeof(u) * 8ULL) - __builtin_clzll(u - 1ULL));
}

#define _shl_cleanup_(_val) __attribute__((__cleanup__(_val)))

static inline void kvt_shl_closep(int *p)
{
	if (*p >= 0)
		close(*p);
}

#define _shl_close_ _shl_cleanup_(kvt_shl_closep)

#endif /* KVT_SHL_MACRO_H */
