#ifndef CG_OUTPUT_H
#define CG_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>

#include "server.h"
#include "view.h"

struct cg_output {
	struct cg_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;

	/*
	 * THE TOPLEVEL WHOSE WINDOW THIS OUTPUT IS, or NULL while the anchor is
	 * unclaimed. The frame handler has an output and needs the window, so
	 * the back-pointer is what says which mapping a frame is copied into
	 * and which `win` its damage carries.
	 */
	struct cg_view *view;

	/*
	 * WHERE IT SITS ON THE LAYOUT, counted in spans of CG_EMBED_SPAN. The
	 * boxes must never overlap: a window rendering inside another window's
	 * box is the single-framebuffer pile this mode exists to undo. The
	 * lowest free slot is taken, so the count of spans in use is the count
	 * of windows open and an origin already handed out never moves.
	 */
	int slot;

	struct wl_listener commit;
	struct wl_listener request_state;
	struct wl_listener destroy;
	struct wl_listener frame;

	struct wl_list link; // cg_server::outputs
};

void handle_output_manager_apply(struct wl_listener *listener, void *data);
void handle_output_manager_test(struct wl_listener *listener, void *data);
void handle_output_layout_change(struct wl_listener *listener, void *data);
void handle_new_output(struct wl_listener *listener, void *data);
void output_set_window_title(struct cg_output *output, const char *title);

/*
 * AN OUTPUT OF THIS TOPLEVEL'S OWN, and it is what makes it a window. The
 * anchor is taken where it is free — a guest that shows one window allocates
 * nothing and behaves as a cage with a single output does — and a headless
 * output at `w` x `h` is added where it is not. Answers false when
 * this cage is not embedded, when the view already holds one, when every span
 * of the grid is taken, or when the backend refuses.
 */
bool output_claim(struct cg_view *view, int w, int h);

/*
 * THE ANCHOR AND NOTHING ELSE, which is what an initial commit may take. An
 * ORDINARY toplevel — no owner, no role — that finds it free is configured into
 * it and comes up at the size the parent forked this cage with; anything else
 * is configured 0x0 and picks its own size, because at that point there is
 * nothing to fill and a size named here would be a size the client takes for
 * the parent's. Ordinary, because the parent gives its remembered rectangle to
 * the first such window too, and the two ends must name the same one.
 */
bool output_claim_anchor(struct cg_view *view);

/*
 * THE OUTPUT GOES WITH THE TOPLEVEL, except the anchor, which is released back
 * to the next window instead. Called from the unmap and the destroy and never
 * from a frame handler: an output freed under the handler rendering it is a
 * use-after-free in wlroots' own call.
 */
void output_release(struct cg_view *view);

#endif
