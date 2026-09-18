#ifndef CG_DRAG_H
#define CG_DRAG_H

#include "kembed.h"
#include "server.h"

/*
 * The embedded mode's drag bridge. See drag.c's header for the ownership rule
 * and kembed.h for the five ops it speaks.
 *
 * A cage that is not embedded has no parent to bridge to, so every call is a
 * no-op there: the guest's own seat is the whole desktop and a drag inside it
 * never leaves.
 */
void drag_init(struct cg_server *server);
void drag_finish(struct cg_server *server);

/*
 * The four parent -> child ops, in the order they arrive. `x` and `y` are
 * LOGICAL units — the caller divides by the window's scale, the same
 * conversion KEMBED_MOTION gets — and the view is the window the parent named.
 *
 * `mime` on the enter is the tail of that datagram; it is the caller's and is
 * copied here. `drag_drop` takes the descriptor the message carried, which
 * stays the CALLER'S: the dispatch closes every descriptor it received, so
 * this maps and copies within the call.
 */
void drag_enter(struct cg_server *server, struct cg_view *view, double x,
		double y, const char *mime, uint32_t time_msec);
void drag_motion(struct cg_server *server, struct cg_view *view, double x,
		 double y, uint32_t time_msec);
void drag_leave(struct cg_server *server);
void drag_drop(struct cg_server *server, struct cg_view *view, double x,
	       double y, const KembedMsg *m, int fd, uint32_t time_msec);

#endif
