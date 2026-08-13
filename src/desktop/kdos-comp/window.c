/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   window.c — window state: maximise, fullscreen, shade, close
 *
 * kdos-comp had NO request_maximize and NO request_fullscreen handler at all,
 * which meant a video player or a game asking to fill the screen was ignored and
 * F11 did nothing anywhere on this desktop. It was on the TODO list as its own
 * item. The frames are where it gets fixed, because both answers are geometry
 * the frame already has to compute.
 *
 * THE TWO BOXES ARE NOT THE SAME BOX, and that is the whole content of this
 * file:
 *
 *   maximise   -> the USABLE box, inset by the frame, so the FRAME fills the
 *                 space the panels left and the titlebar stays reachable
 *   fullscreen -> the OUTPUT box exactly, frame and shadow hidden, panels
 *                 covered — an application asking for the whole screen means
 *                 the whole screen
 *
 * Using the output box for maximise is how a maximised window ends up painted
 * over the panel, which is the bug kc_usable_at() was introduced to stop.
 * ---------------------------------
 */

#include <stdlib.h>

#include "kdos-comp.h"

/*
 * Remember where the window was, once.
 *
 * Once, and not on every call: maximising a window that is already maximised
 * would otherwise save the maximised box as the box to go back to, and the
 * window could never be restored. Every desktop has shipped that bug at least
 * once.
 */
static void save_box(struct kc_toplevel *t)
{
	if (t->maximized || t->fullscreen)
		return;
	struct wlr_box g = t->xdg_toplevel->base->geometry;
	/*
	 * A toplevel that is initialized but has not committed a buffer has a
	 * 0x0 geometry, and an application is entitled to ask for fullscreen in
	 * that state — a video player started with a file does exactly that.
	 * Recording 0x0 makes restore_box() a silent no-op, so the window can
	 * never come back out. Refusing to record it leaves the previous saved
	 * box, and if there is none the restore is skipped and the window keeps
	 * the size it has, which is recoverable by hand.
	 */
	if (g.width <= 0 || g.height <= 0)
		return;
	t->saved.x = t->x;
	t->saved.y = t->y;
	t->saved.width = g.width;
	t->saved.height = g.height;
}

static void restore_box(struct kc_toplevel *t)
{
	if (t->saved.width <= 0 || t->saved.height <= 0)
		return;
	t->x = t->saved.x;
	t->y = t->saved.y;
	wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
	wlr_xdg_toplevel_set_size(t->xdg_toplevel, t->saved.width,
				  t->saved.height);
}

void kc_window_maximize(struct kc_toplevel *t, bool on)
{
	struct kc_server *s = t->server;

	if (t->fullscreen)		/* fullscreen outranks it */
		return;
	if (on == t->maximized) {
		wlr_xdg_toplevel_set_maximized(t->xdg_toplevel, on);
		return;
	}

	if (on) {
		struct wlr_box u;
		/* The output the window is on, not the one the pointer is on: a
		 * maximise can arrive from a keybinding while the pointer is
		 * somewhere else entirely. */
		if (!kc_usable_at(s, t->x, t->y, &u))
			return;
		save_box(t);

		/* Inset by the frame so the FRAME fills the usable box. Without
		 * this the window fills it and the titlebar is pushed under the
		 * panel — which is the same failure kc_usable_at() exists to
		 * prevent, arriving one level further in. */
		int bw = t->deco ? kc_deco_border_w(t) : 0;
		int bh = t->deco ? kc_deco_border_h(t) : 0;

		t->x = u.x + bw;
		t->y = u.y + bh;
		wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
		wlr_xdg_toplevel_set_size(t->xdg_toplevel,
					  u.width - 2 * bw, u.height - 2 * bh);
		t->maximized = true;
	} else {
		t->maximized = false;
		restore_box(t);
	}

	wlr_xdg_toplevel_set_maximized(t->xdg_toplevel, on);
	kc_deco_arrange(t);
}

void kc_window_fullscreen(struct kc_toplevel *t, bool on)
{
	struct kc_server *s = t->server;

	if (on == t->fullscreen) {
		wlr_xdg_toplevel_set_fullscreen(t->xdg_toplevel, on);
		return;
	}

	if (on) {
		struct wlr_output *o = wlr_output_layout_output_at(
			s->output_layout, t->x, t->y);
		if (!o)
			return;
		struct wlr_box ob;
		wlr_output_layout_get_box(s->output_layout, o, &ob);
		if (wlr_box_empty(&ob))
			return;

		save_box(t);
		/* A shaded window that goes fullscreen must come back first, or
		 * it is a fullscreen titlebar with nothing under it. */
		if (t->shaded)
			kc_window_shade(t, false);

		t->x = ob.x;
		t->y = ob.y;
		wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
		wlr_xdg_toplevel_set_size(t->xdg_toplevel, ob.width, ob.height);
		t->fullscreen = true;
		/* No frame and no shadow: the application asked for the screen,
		 * and a border around a fullscreen video is a border in a
		 * cinema. */
		kc_deco_hide(t, true);
		/*
		 * Above the panels, which live outside the workspace trees —
		 * and that is exactly why the workspace switch has to be told.
		 * kc_ws_switch() hides a workspace by disabling ws_tree[n], and
		 * a window parented to layer_above is not in any of them, so a
		 * fullscreen video stayed on screen over every other workspace.
		 * The node is disabled directly when its workspace is not the
		 * current one.
		 */
		wlr_scene_node_reparent(&t->scene_tree->node, s->layer_above);
		wlr_scene_node_set_enabled(&t->scene_tree->node,
					   t->ws == s->cur_ws);
	} else {
		t->fullscreen = false;
		kc_deco_hide(t, false);
		wlr_scene_node_reparent(&t->scene_tree->node, s->ws_tree[t->ws]);
		if (t->maximized) {
			/*
			 * It was maximised before it went fullscreen, so it
			 * goes back to maximised rather than to the saved box.
			 *
			 * `fullscreen` is cleared FIRST and `maximized` is left
			 * alone: kc_window_maximize() calls save_box(), which
			 * only declines to overwrite while one of the two flags
			 * is set. Clearing `maximized` here — as this did —
			 * removed that guard and let save_box() record the
			 * FULLSCREEN box as the box to restore to, so the
			 * window could never get small again.
			 */
			kc_window_maximize(t, true);
		} else {
			restore_box(t);
		}
	}

	wlr_xdg_toplevel_set_fullscreen(t->xdg_toplevel, on);
	kc_deco_arrange(t);
}

/*
 * Shade — roll the window up into its titlebar.
 *
 * The one GNOME 2 window behaviour that is MORE useful on a character grid than
 * it was on pixels: a shaded window here is exactly one cell tall, so a row of
 * them along the bottom of the screen is a tab bar that costs nothing to draw.
 *
 * The client is not told and not resized. A shade is a compositor decision
 * about what is on screen; asking a client to be zero pixels tall is a request
 * most of them answer badly, and a client that redrew itself for the shade
 * would flicker when it came back.
 */
void kc_window_shade(struct kc_toplevel *t, bool on)
{
	if (t->fullscreen || on == t->shaded)
		return;
	t->shaded = on;
	if (t->surface_tree)
		wlr_scene_node_set_enabled(&t->surface_tree->node, !on);
	kc_deco_arrange(t);
}

void kc_window_close(struct kc_toplevel *t)
{
	wlr_xdg_toplevel_send_close(t->xdg_toplevel);
}

/*
 * Minimise is "put it on no workspace".
 *
 * KDOS has no iconified state of its own and does not need one: the window list
 * in the panel is what a minimised window is FOR, and it already reports every
 * window on every workspace. Disabling the tree is what a workspace switch does
 * to a window, so a minimised window is in exactly the state the rest of the
 * system already understands.
 */
void kc_window_minimize(struct kc_toplevel *t, bool on)
{
	if (on == t->minimized)
		return;
	t->minimized = on;
	wlr_scene_node_set_enabled(&t->scene_tree->node, !on);
	/*
	 * The flag is set by kc_shellsvc_update() below, not here. Setting it
	 * twice is not redundant, it is wrong: shellsvc computes minimized as
	 * `t->ws != s->cur_ws || t->minimized`, so setting it first and then
	 * calling update just overwrote this value with the computed one — and
	 * the panel showed a minimised window as ordinary.
	 */
	if (on) {
		struct kc_server *s = t->server;
		/* Focus must not stay on a window nobody can see. */
		if (s->seat->keyboard_state.focused_surface ==
		    t->xdg_toplevel->base->surface)
			kc_refocus(s);
	} else {
		kc_focus(t, t->xdg_toplevel->base->surface);
	}
	kc_shellsvc_update(t->server, t);
}

/* ── the client-initiated half ─────────────────────────────────────────── */

void kc_window_request_maximize(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, request_maximize);
	(void)data;
	/*
	 * The protocol requires an ack even when the answer is "no change" —
	 * wlroots 0.20 documents that a compositor which ignores the request
	 * must still send a configure, and a client that never gets one waits
	 * forever with a half-drawn window.
	 */
	if (!t->xdg_toplevel->base->initialized)
		return;
	kc_window_maximize(t, t->xdg_toplevel->requested.maximized);
}

void kc_window_request_fullscreen(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, request_fullscreen);
	(void)data;
	if (!t->xdg_toplevel->base->initialized)
		return;
	kc_window_fullscreen(t, t->xdg_toplevel->requested.fullscreen);
}

void kc_window_request_minimize(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, request_minimize);
	(void)data;
	if (!t->xdg_toplevel->base->initialized)
		return;
	kc_window_minimize(t, t->xdg_toplevel->requested.minimized);
}
