/*
 * SHL - PTY Helpers
 *
 * Copyright (c) 2011-2014 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

/*
 * PTY Helpers
 */

#ifndef KVT_SHL_PTY_H
#define KVT_SHL_PTY_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "kvt_macro.h"

/* pty */

struct kvt_shl_pty;

typedef void (*kvt_shl_pty_input_fn) (struct kvt_shl_pty *pty,
				  void *data,
				  char *u8,
				  size_t len);

pid_t kvt_shl_pty_open(struct kvt_shl_pty **out,
		   kvt_shl_pty_input_fn fn_input,
		   void *fn_input_data,
		   unsigned short term_width,
		   unsigned short term_height);
void kvt_shl_pty_unref(struct kvt_shl_pty *pty);
void kvt_shl_pty_close(struct kvt_shl_pty *pty);

static inline void kvt_shl_pty_unref_p(struct kvt_shl_pty **pty)
{
	kvt_shl_pty_unref(*pty);
}

#define _shl_pty_unref_ _shl_cleanup_(kvt_shl_pty_unref_p)

bool kvt_shl_pty_is_open(struct kvt_shl_pty *pty);
int kvt_shl_pty_get_fd(struct kvt_shl_pty *pty);

int kvt_shl_pty_dispatch(struct kvt_shl_pty *pty);
/* Bytes written to the child that the pty has not accepted. Poll the
 * descriptor for POLLOUT while this is non-zero, or they move only when the
 * child next writes something of its own. */
size_t kvt_shl_pty_pending(struct kvt_shl_pty *pty);
int kvt_shl_pty_write(struct kvt_shl_pty *pty, const char *u8, size_t len);
int kvt_shl_pty_signal(struct kvt_shl_pty *pty, int sig);
int kvt_shl_pty_resize(struct kvt_shl_pty *pty,
		   unsigned short term_width,
		   unsigned short term_height);

#endif  /* KVT_SHL_PTY_H */
