/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   capture.c — screen capture and clipboard, for programs that are not ours
 *
 * Everything here exists for a CLIENT: grim (which kdos-shot runs), wl-clipboard,
 * and xdg-desktop-portal-wlr, which is how a boxed OBS ever sees a screen. None
 * of them is ours and none of them can be made to work by anything but the
 * compositor advertising the global it looks for.
 *
 * Both spellings of each are advertised, and that is not belt-and-braces:
 *
 *   - wlr-screencopy is the OLD one and is the only one RELEASED grim speaks
 *     (v1.4.1; only grim master speaks ext-image-copy-capture). Implement the
 *     ext- protocols alone and kdos-shot breaks on the shipped grim.
 *   - ext-image-copy-capture is the NEW one and is what xdg-desktop-portal-wlr
 *     prefers. Implement wlr-screencopy alone and the portal falls back, which
 *     works, but every future client arrives expecting the staging protocol.
 *
 * Same for the clipboard: `wlr-data-control` is what wl-clipboard 2.2 binds,
 * `ext-data-control` what 2.3 binds, and a machine where `wl-paste` prints
 * nothing is a machine where the user assumes the clipboard is broken.
 *
 * All of these are on the security-context deny list (security.c), which was
 * written by NAME before any of them existed here — a boxed app that can copy
 * the screen or read the clipboard unfocused is a screen recorder nobody asked
 * for. That table is why this file adds no policy of its own.
 * ---------------------------------
 */

#include <stdlib.h>

#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>

#include "kdos-comp.h"

/* ── per-window capture ────────────────────────────────────────────────── */

/*
 * The source destroys itself when the scene node it was built on goes away, so
 * this listener exists to forget the pointer rather than to free anything.
 * Without it, a window that closed and a new capture request that arrived
 * afterwards would meet through a dangling `capture_src`.
 */
static void capture_src_destroy(struct wl_listener *l, void *data)
{
	struct kc_toplevel *t = wl_container_of(l, t, capture_src_destroy);
	(void)data;
	wl_list_remove(&t->capture_src_destroy.link);
	t->capture_src = NULL;
}

/*
 * The window is going away.
 *
 * The source outlives it: it hangs off the SCENE NODE, which xdg-shell destroys
 * after our own destroy handler has already freed the toplevel. So the listener
 * above fired on a freed `t` — a use-after-free ASan caught on the first run
 * with capture in play, and one that would never have shown up as a crash on
 * a machine where the freed memory still read back plausibly.
 */
void kc_capture_toplevel_free(struct kc_toplevel *t)
{
	if (!t->capture_src)
		return;
	wl_list_remove(&t->capture_src_destroy.link);
	t->capture_src = NULL;
}

/*
 * A client asked to capture one window.
 *
 * The source is created lazily and then KEPT: it owns a wlr_scene_output and a
 * swapchain of its own, so one per window is a cost worth paying once and once
 * per request is not. A request we cannot serve is answered with an inert
 * source rather than ignored — dropping it on the floor leaves the client
 * holding an object id that never becomes anything, waiting forever.
 */
static void capture_new_request(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, ftl_capture_request);
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *req =
		data;
	struct kc_toplevel *t = req->toplevel_handle->data;

	if (!t || !t->scene_tree) {
		wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
			req, NULL);
		return;
	}

	if (!t->capture_src) {
		t->capture_src = wlr_ext_image_capture_source_v1_create_with_scene_node(
			&t->scene_tree->node,
			wl_display_get_event_loop(s->display), s->allocator,
			s->renderer);
		if (!t->capture_src) {
			wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
				req, NULL);
			return;
		}
		t->capture_src_destroy.notify = capture_src_destroy;
		wl_signal_add(&t->capture_src->events.destroy,
			      &t->capture_src_destroy);
	}

	wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
		req, t->capture_src);
}

/* ── the globals ───────────────────────────────────────────────────────── */

void kc_capture_init(struct kc_server *s)
{
	/*
	 * Output capture, both generations. The screencopy path copies the
	 * output's committed buffer, which under the CRT pass is the PROCESSED
	 * one — a screenshot of a phosphor desktop looks like the desktop the
	 * user is looking at, which is the honest answer even though it is not
	 * the one a pixel-peeper wants.
	 */
	wlr_screencopy_manager_v1_create(s->display);
	wlr_export_dmabuf_manager_v1_create(s->display);
	wlr_ext_image_copy_capture_manager_v1_create(s->display, 1);
	wlr_ext_output_image_capture_source_manager_v1_create(s->display, 1);

	/*
	 * Per-window capture needs a toplevel to name, and ext-foreign-toplevel-list
	 * is the protocol that names them (shellsvc.c creates the list and its
	 * handles). Without it a portal can offer "share a screen" and not
	 * "share a window", which is the half most people actually want.
	 */
	if (s->ext_ftl_list) {
		s->ftl_capture_mgr =
			wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(
				s->display, 1);
		if (s->ftl_capture_mgr) {
			s->ftl_capture_request.notify = capture_new_request;
			wl_signal_add(&s->ftl_capture_mgr->events.new_request,
				      &s->ftl_capture_request);
		}
	}

	/* The clipboard, for programs outside the focus chain. */
	wlr_data_control_manager_v1_create(s->display);
	wlr_ext_data_control_manager_v1_create(s->display, 1);
}

/*
 * wlroots asserts an empty listener list when it destroys this manager, exactly
 * as wlr_cursor and the security-context manager do — the same assert that used
 * to abort every logout.
 */
void kc_capture_free(struct kc_server *s)
{
	if (s->ftl_capture_mgr) {
		wl_list_remove(&s->ftl_capture_request.link);
		s->ftl_capture_mgr = NULL;
	}
}
