#ifndef CG_EMBED_H
#define CG_EMBED_H

#include <stdbool.h>

#include "server.h"
#include "view.h"

/*
 * The embedded mode's half of the private channel to kdos-con. See kembed.h
 * for the protocol and cage.c's header for what --embed is.
 *
 * THE CHANNEL IS THE PROCESS'S AND EVERY WINDOW OP IS A TOPLEVEL'S. One cage
 * is one socket and one seat; each mapped toplevel is a window on the parent's
 * desktop with an output, a mapping and an id of its own, so the calls that
 * carry a window take the view and the two that carry the channel take the
 * server.
 */
bool embed_init(struct cg_server *server, int fd);
void embed_finish(struct cg_server *server);

/* Called after a frame has been rendered into `buffer` for this window's own
 * output: copies it into that window's mapping and tells the parent what
 * changed in it. */
void embed_publish(struct cg_view *view, struct wlr_buffer *buffer,
		   const pixman_region32_t *damage);

/*
 * THE ALLOCATOR AN EMBEDDED CAGE NEEDS, which is not always the one wlroots
 * would choose. Every frame has to be readable by this process — the parent
 * gets bytes and nothing else — and `wlr_allocator_autocreate()` picks gbm the
 * moment the renderer has a DRM descriptor, whose buffers offer neither a
 * pointer nor an shm handle. Answers NULL only when nothing can allocate.
 */
struct wlr_allocator *embed_allocator(struct wlr_backend *backend,
				      struct wlr_renderer *renderer);

/*
 * THE SIZE THIS TOPLEVEL CHOSE FOR ITSELF, in pixels, or 0 for neither. Its own
 * geometry first, the surface's committed size where the client set no window
 * geometry, and nothing where it has neither — the protocol says 0 means "none"
 * and the parent has a rule for it. Read AT THE MAP: one configure later the
 * size is the one this end told it.
 */
void embed_natural_size(struct cg_view *view, int *width, int *height);

/*
 * THIS TOPLEVEL IS A WINDOW ON THE PARENT'S DESKTOP, from now until
 * embed_close_window(). Allocates its id, which is never reused, and sends
 * KEMBED_OPEN with the natural size, the owner's id, the role bits and the
 * title. Nothing about a window may cross the channel before it, and a toplevel
 * the grid had no output left for gets none at all: a window the parent was
 * told about that can never carry a frame is a taskbar row with nothing behind
 * it.
 */
void embed_open_window(struct cg_view *view);

/*
 * THAT WINDOW IS GONE. KEMBED_CLOSE_WIN is the last op that may name its id,
 * and the mapping goes with it: the parent unmaps its own copy on the message
 * and a mapping kept here is a frame's worth of memory per dialog opened.
 */
void embed_close_window(struct cg_view *view);

/*
 * AN ORDINARY TOPLEVEL — no owner and no role bit at all. The parent hands its
 * remembered rectangle to the first such window and to no other, so this end
 * has to pick the same one for the output that carries that rectangle: a splash
 * that maps first would otherwise be framed at the application's size while the
 * image window behind it is framed at the splash's, with neither end telling
 * the other.
 */
bool embed_win_is_ordinary(struct cg_view *view);

/*
 * THE WINDOW `win` NAMES, or NULL. Zero names the front window, which is what a
 * close asked of the process reaches and what a channel that has no window yet
 * is answered on. A window that does not exist is dropped and never guessed at:
 * one can go while a message about it is still in flight.
 */
struct cg_view *embed_view_from_win(struct cg_server *server, uint32_t win);

/*
 * THE OUTPUT UNDER THIS WINDOW, AT THE SIZE THE PARENT ASKED FOR. A window
 * resize IS an output resize, so the guest reconfigures exactly the way it
 * would on any compositor — there is no second notion of "the window is smaller
 * than the output".
 */
void embed_set_size(struct cg_view *view, int w, int h);

/*
 * THE GUEST RENAMED THIS WINDOW. The frame and the taskbar entry a person reads
 * are the parent's, so a name kept only here is a window called whatever the
 * launcher was called for as long as it is open. The toplevel that spoke names
 * itself, so an export dialog cannot rename the image window it opened over.
 */
void embed_set_title(struct cg_view *view, const char *title);

/*
 * THE GUEST ASKED FOR THE SCREEN, or gave it back. This window's output is the
 * parent's window, so a fullscreen honoured only here resizes the window to
 * itself and the video stays the size it was.
 */
void embed_set_fullscreen(struct cg_view *view, bool fullscreen);

/*
 * SOMETHING IN THIS WINDOW IS PLAYING — do not blank the screen, or release it.
 * The saver, the lock and the DPMS clock belong to the parent, so a guest's
 * idle inhibitor is bookkeeping in this process until it is forwarded.
 */
void embed_set_inhibit(struct cg_view *view, bool inhibited);

/*
 * THE GUEST HOLDS THE POINTER, or has given it back. `grab` is KEMBED_GRAB_*;
 * the hint is where the guest asked the pointer to be left, in THIS WINDOW's
 * pixels. While a grab is held the parent stops moving its own arrow, stops
 * changing which window is hovered, and sends relative motion alone.
 */
void embed_set_grab(struct cg_view *view, int grab, bool have_hint,
		    int hint_x, int hint_y);

/* Is this compositor embedded at all? */
bool embed_active(struct cg_server *server);
/* Nobody can see this window, so nothing is rendered for it. */
bool embed_asleep(const struct cg_view *view);

#endif
