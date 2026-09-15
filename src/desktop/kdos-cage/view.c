/*
 * Cage: A Wayland kiosk.
 *
 * Copyright (C) 2018-2021 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

#include "embed.h"
#include "idle_inhibit_v1.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#if CAGE_HAS_XWAYLAND
#include "xwayland.h"
#endif

char *
view_get_title(struct cg_view *view)
{
	const char *title = view->impl->get_title(view);
	if (!title) {
		return NULL;
	}
	return strndup(title, strlen(title));
}

bool
view_is_primary(struct cg_view *view)
{
	return view->impl->is_primary(view);
}

struct cg_view *
view_get_parent(struct cg_view *view)
{
	return view->impl->get_parent(view);
}

bool
view_is_window(struct cg_view *view)
{
#if CAGE_HAS_XWAYLAND
	if (view->type == CAGE_XWAYLAND_VIEW) {
		return xwayland_view_should_manage(view);
	}
#endif
	return true;
}

bool
view_is_transient_for(struct cg_view *child, struct cg_view *parent)
{
	return child->impl->is_transient_for(child, parent);
}

void
view_activate(struct cg_view *view, bool activate)
{
	view->impl->activate(view, activate);
	wlr_foreign_toplevel_handle_v1_set_activated(view->foreign_toplevel_handle, activate);
}

static bool
view_extends_output_layout(struct cg_view *view, struct wlr_box *layout_box)
{
	int width, height;
	view->impl->get_geometry(view, &width, &height);

	return (layout_box->height < height || layout_box->width < width);
}

static void
view_maximize(struct cg_view *view, struct wlr_box *layout_box)
{
	view->lx = layout_box->x;
	view->ly = layout_box->y;

	if (view->scene_tree) {
		wlr_scene_node_set_position(&view->scene_tree->node, view->lx, view->ly);
	}

	/*
	 * A TOPLEVEL THAT NAMES AN OWNER IS SIZED AND NOT MAXIMISED, when it is
	 * a window on somebody's desktop. Maximised is what strips a toolkit's
	 * shadow and rounded corners from a window that fills its frame; on a
	 * dialog it adds a restore button to something no desktop shows one on.
	 * A kiosk on a terminal of its own has no frame either way, so the shape
	 * it draws on a terminal is the one it keeps.
	 */
	if (view->server->embed.embedded && view_get_parent(view)) {
		view->impl->set_size(view, layout_box->width, layout_box->height);
	} else {
		view->impl->maximize(view, layout_box->width, layout_box->height);
	}
}

static void
view_center(struct cg_view *view, struct wlr_box *layout_box)
{
	int width, height;
	view->impl->get_geometry(view, &width, &height);

	view->lx = (layout_box->width - width) / 2;
	view->ly = (layout_box->height - height) / 2;

	if (view->scene_tree) {
		wlr_scene_node_set_position(&view->scene_tree->node, view->lx, view->ly);
	}
}

void
view_position(struct cg_view *view)
{
	struct wlr_box layout_box;

	/*
	 * EVERY TOPLEVEL FILLS ITS OWN OUTPUT, because that output IS its
	 * window on the parent's desktop. The union box maximises every
	 * parentless toplevel onto one rectangle and centres every child on it
	 * — five GIMP windows in one place, pixel for pixel — and with one
	 * output per window it would maximise each of them across all of them.
	 * Where the parent decides a window sits, how large it is and what is
	 * above what, this end decides nothing.
	 *
	 * A TOPLEVEL WITH NO OUTPUT YET IS NOT PLACED. It is configured 0x0 —
	 * "you choose" — so the size it picks is the natural size KEMBED_OPEN
	 * reports, and the parent has something to place a dialog at.
	 */
	if (view->server->embed.embedded) {
		if (!view->win.out) {
			return;
		}
		wlr_output_layout_get_box(view->server->output_layout, view->win.out->wlr_output,
					  &layout_box);
		view_maximize(view, &layout_box);
		return;
	}

	wlr_output_layout_get_box(view->server->output_layout, NULL, &layout_box);

	if (view_is_primary(view) || view_extends_output_layout(view, &layout_box)) {
		view_maximize(view, &layout_box);
	} else {
		view_center(view, &layout_box);
	}
}

void
view_position_all(struct cg_server *server)
{
	struct cg_view *view;
	wl_list_for_each (view, &server->views, link) {
		/* A surface that sets its own root coordinates is placed by
		 * the client that owns it and by nothing here. */
		if (!view_is_window(view)) {
			continue;
		}
		view_position(view);
	}
}

void
view_unmap(struct cg_view *view)
{
	/*
	 * THE VIEW LEAVES THE LIST BEFORE ITS OUTPUT GOES. Destroying an output
	 * changes the layout, whose change handler walks the view list and asks
	 * every view where it belongs — and this one is reached from a shell
	 * destroy that has already cleared the object those answers come out of.
	 *
	 * AND THE WINDOW IS RETIRED BEFORE THE OUTPUT TOO: KEMBED_CLOSE_WIN is
	 * the last op that may name this `win`, and a frame reached in between
	 * would copy through a mapping that has been unmapped.
	 */
	wl_list_remove(&view->link);

	embed_close_window(view);
	output_release(view);

	wl_list_remove(&view->request_activate.link);
	wl_list_remove(&view->request_close.link);
	wlr_foreign_toplevel_handle_v1_destroy(view->foreign_toplevel_handle);
	view->foreign_toplevel_handle = NULL;

	wlr_scene_node_destroy(&view->scene_tree->node);

	view->wlr_surface->data = NULL;
	view->wlr_surface = NULL;
}

void
handle_surface_request_activate(struct wl_listener *listener, void *data)
{
	struct cg_view *view = wl_container_of(listener, view, request_activate);

	wlr_scene_node_raise_to_top(&view->scene_tree->node);

	/*
	 * AND THE PARENT IS THE ONLY SOURCE OF KEYBOARD FOCUS WHEN THIS CAGE IS
	 * A WINDOW. A guest asking its own foreign-toplevel handle to activate
	 * would take the keyboard from a terminal the person is typing in,
	 * while the parent goes on sending that terminal's keys and releasing
	 * them against a window that never got the presses. The raise above is
	 * harmless either way: the toplevels are on disjoint outputs.
	 */
	if (!view->server->embed.embedded) {
		seat_set_focus(view->server->seat, view);
	}
}

void
handle_surface_request_close(struct wl_listener *listener, void *data)
{
	struct cg_view *view = wl_container_of(listener, view, request_close);
	view->impl->close(view);
}

void
view_map(struct cg_view *view, struct wlr_surface *surface)
{
	view->scene_tree = wlr_scene_subsurface_tree_create(&view->server->scene->tree, surface);
	if (!view->scene_tree)
		goto fail;
	view->scene_tree->node.data = view;

	view->wlr_surface = surface;
	surface->data = view;

	/* We shouldn't position override-redirect windows. They set
	   their own (x,y) coordinates in handle_wayland_surface_map. */
	if (view_is_window(view)) {
		/*
		 * THE NATURAL SIZE IS READ BEFORE ANYTHING IS CONFIGURED. At
		 * the map the client has committed a buffer, so its geometry is
		 * the size it chose for itself — and that is what the parent
		 * places a dialog at. One configure later it is the size we
		 * told it, and the answer is gone.
		 */
		int w = 0, h = 0;

		embed_natural_size(view, &w, &h);
		output_claim(view, w, h);

		/*
		 * AND A TOPLEVEL THE GRID HAS NO SPAN FOR RENDERS NOWHERE,
		 * which is what refusing it means. It gets no KEMBED_OPEN and
		 * so is no window on the parent's desktop; a scene node left
		 * enabled sits at the layout origin, inside the first window's
		 * box, and is composited into that window's framebuffer.
		 */
		if (view->server->embed.embedded && !view->win.out) {
			wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		}
		view_position(view);
	} else {
		/*
		 * AND AN OVERRIDE-REDIRECT SURFACE IS PUT WHERE IT ASKED TO BE.
		 * Its X root coordinates ARE the output layout, so honouring
		 * them lands a menu inside the box of the window that raised
		 * it; a scene node left at the layout origin renders in the
		 * first window's frames whichever window opened the menu.
		 */
		wlr_scene_node_set_position(&view->scene_tree->node, view->lx, view->ly);
	}

	wl_list_insert(&view->server->views, &view->link);

	/*
	 * A WINDOW OPENS BEFORE ITS PIXELS DO. KEMBED_OPEN precedes every op
	 * naming this window, so it goes out once the output exists and the
	 * geometry is real, and before the first frame can be published into
	 * it. A surface that places itself is not a window and gets none.
	 */
	if (view_is_window(view)) {
		embed_open_window(view);
	}

	view->foreign_toplevel_handle = wlr_foreign_toplevel_handle_v1_create(view->server->foreign_toplevel_manager);
	if (!view->foreign_toplevel_handle)
		goto fail;

	view->request_activate.notify = handle_surface_request_activate;
	wl_signal_add(&view->foreign_toplevel_handle->events.request_activate, &view->request_activate);
	view->request_close.notify = handle_surface_request_close;
	wl_signal_add(&view->foreign_toplevel_handle->events.request_close, &view->request_close);

	/*
	 * THE PARENT IS THE ONLY SOURCE OF KEYBOARD FOCUS WHEN THIS CAGE IS A
	 * WINDOW. A dialog that took the keyboard here would take it from a
	 * terminal the person is typing in, while the parent goes on sending
	 * that terminal's keys and releasing them against a window that never
	 * got the presses.
	 */
	if (!view->server->embed.embedded) {
		seat_set_focus(view->server->seat, view);
	}
	return;

fail:
	wl_resource_post_no_memory(surface->resource);
}

void
view_destroy(struct cg_view *view)
{
	struct cg_server *server = view->server;

	if (view->wlr_surface != NULL) {
		view_unmap(view);
	}

	/*
	 * A TOPLEVEL THAT CLAIMED AN OUTPUT AND NEVER MAPPED STILL HOLDS IT.
	 * The anchor is taken at the initial commit, which a client may follow
	 * with a destroy instead of a map; an anchor left pointing at freed
	 * memory is a frame copied into it.
	 */
	output_release(view);

	/*
	 * AND EVERY OTHER POINTER AT THIS VIEW GOES WITH IT. The keyboard, the
	 * window a pointer grab was reported against and any idle inhibitor
	 * hung on it all outlive the toplevel — an inhibitor's own destroy
	 * arrives when the client gets round to it, and a constraint is
	 * destroyed with the surface rather than with the window. Each one left
	 * standing is this view dereferenced after the free.
	 */
	if (server->embed.kbd == view) {
		server->embed.kbd = NULL;
	}
	if (server->seat && server->seat->grab_view == view) {
		server->seat->grab_view = NULL;
	}
	idle_inhibit_forget_view(server, view);

	view->impl->destroy(view);

	/* If there is a previous view in the list, focus that. The parent owns
	 * the keyboard when this cage is a window, and a focus moved here on a
	 * destroy is one it did not ask for and will not know about. */
	bool empty = wl_list_empty(&server->views);
	if (!empty && !server->embed.embedded) {
		struct cg_view *prev = wl_container_of(server->views.next, prev, link);
		seat_set_focus(server->seat, prev);
	}
}

void
view_init(struct cg_view *view, struct cg_server *server, enum cg_view_type type, const struct cg_view_impl *impl)
{
	view->server = server;
	view->type = type;
	view->impl = impl;
}

struct cg_view *
view_from_wlr_surface(struct wlr_surface *surface)
{
	assert(surface);
	return surface->data;
}
