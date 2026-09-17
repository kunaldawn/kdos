/*
 * Cage: A Wayland kiosk.
 *
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "embed.h"
#include "output.h"
#include "server.h"
#include "view.h"
#include "xdg_shell.h"

static void
xdg_decoration_set_mode(struct cg_xdg_decoration *xdg_decoration)
{
	enum wlr_xdg_toplevel_decoration_v1_mode mode;
	if (xdg_decoration->server->xdg_decoration) {
		mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
	} else {
		mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
	}
	wlr_xdg_toplevel_decoration_v1_set_mode(xdg_decoration->wlr_decoration, mode);
}

static void
xdg_decoration_handle_destroy(struct wl_listener *listener, void *data)
{
	struct cg_xdg_decoration *xdg_decoration = wl_container_of(listener, xdg_decoration, destroy);

	wl_list_remove(&xdg_decoration->destroy.link);
	wl_list_remove(&xdg_decoration->commit.link);
	wl_list_remove(&xdg_decoration->request_mode.link);
	free(xdg_decoration);
}

static void
xdg_decoration_handle_commit(struct wl_listener *listener, void *data)
{
	struct cg_xdg_decoration *xdg_decoration = wl_container_of(listener, xdg_decoration, commit);

	if (xdg_decoration->wlr_decoration->toplevel->base->initial_commit) {
		xdg_decoration_set_mode(xdg_decoration);
	}
}

static void
xdg_decoration_handle_request_mode(struct wl_listener *listener, void *data)
{
	struct cg_xdg_decoration *xdg_decoration = wl_container_of(listener, xdg_decoration, request_mode);

	if (xdg_decoration->wlr_decoration->toplevel->base->initialized) {
		xdg_decoration_set_mode(xdg_decoration);
	}
}

static struct cg_view *
popup_get_view(struct wlr_xdg_popup *popup)
{
	while (true) {
		if (popup->parent == NULL) {
			return NULL;
		}
		struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(popup->parent);
		if (xdg_surface == NULL) {
			return NULL;
		}
		switch (xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			return xdg_surface->data;
		case WLR_XDG_SURFACE_ROLE_POPUP:
			popup = xdg_surface->popup;
			break;
		case WLR_XDG_SURFACE_ROLE_NONE:
			return NULL;
		}
	}
}

/*
 * THE RESIZE PASS CLIPS, AND A CLIPPED MENU IS A BAND WITH NOTHING IN IT.
 * wlr_xdg_positioner_rules_unconstrain_box() tries flip, then slide, then
 * resize, and the resize pass keeps any rectangle that is not empty: a menu
 * taller than the console window it is anchored in comes back as the gap
 * between its anchor and the window's edge — tens of pixels, no items, and the
 * client cannot tell that it was not what it asked for.
 *
 * Take the resize back once it has removed more than half of the asked-for
 * extent. A client that means to shrink asks for a size close to the one it is
 * given, so a cut past half is this window being smaller than the menu, and
 * what survives such a cut is whichever end the anchor happened to fall near
 * rather than the first item. Restore the asked-for extent, pull the popup back
 * to the near edge of the box, and let the scene clip the tail at the output
 * edge: a menu whose head is on screen is usable, a band is not. A smaller cut
 * is left alone — undoing it would move a menu that genuinely fits off the
 * anchor it was placed against.
 */
static void
popup_keep_extent(int *pos, int *extent, int requested, int box_pos, int box_extent)
{
	/* Halve the request rather than double the result: a client is free to
	 * ask for a size near INT32_MAX, and the doubling would overflow. */
	if (requested <= 0 || *extent >= requested / 2) {
		return;
	}

	*extent = requested;

	if (*pos < box_pos || requested >= box_extent) {
		*pos = box_pos;
	} else if (*pos + requested > box_pos + box_extent) {
		*pos = box_pos + box_extent - requested;
	}
}

static void
popup_unconstrain(struct wlr_xdg_popup *popup)
{
	struct cg_view *view = popup_get_view(popup);
	if (view == NULL) {
		return;
	}

	struct cg_server *server = view->server;
	struct wlr_box *popup_box = &popup->current.geometry;

	struct wlr_output_layout *output_layout = server->output_layout;
	/*
	 * THE OUTPUT ITS TOPLEVEL IS ON, AND NEVER THE ONE UNDER A POINT. A
	 * popup whose origin falls outside every output resolves to NULL, and
	 * wlr_output_layout_get_box(NULL) is the WHOLE layout — so a menu would
	 * be unconstrained across every window this guest has and composited
	 * into the framebuffer of the one next to it.
	 */
	struct wlr_output *wlr_output =
		view->win.out ? view->win.out->wlr_output
			      : wlr_output_layout_output_at(output_layout, view->lx + popup_box->x,
							    view->ly + popup_box->y);
	struct wlr_box output_box;

	if (!wlr_output && server->embed.anchor) {
		wlr_output = server->embed.anchor->wlr_output;
	}
	wlr_output_layout_get_box(output_layout, wlr_output, &output_box);

	/*
	 * THE BOX IS IN THE TOPLEVEL'S SURFACE SPACE, and the scene node sits
	 * at the layout position less the window-geometry origin — so the
	 * output's corner is `origin` pixels into the surface, not at its
	 * corner. wlr_xdg_popup_unconstrain_from_box() then takes the
	 * toplevel's own geometry origin back off again to reach the space a
	 * positioner is written in. Drop the term and every menu is placed one
	 * shadow margin from where the client asked for it.
	 */
	int gx, gy;

	view_origin(view, &gx, &gy);

	struct wlr_box output_toplevel_box = {
		.x = output_box.x - view->lx + gx,
		.y = output_box.y - view->ly + gy,
		.width = output_box.width,
		.height = output_box.height,
	};

	wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);

	/*
	 * THE SAME CONSTRAINT THAT PASS USED, and not the window box itself:
	 * scheduled.geometry is written relative to the popup's own parent, so
	 * unconstrain_from_box() takes the popup's offset from its root toplevel
	 * off the box first. A submenu clamped against anything else lands a
	 * whole parent-menu's distance out of its window's span, in the
	 * framebuffer of the window beside it.
	 */
	int tx, ty;

	wlr_xdg_popup_get_toplevel_coords(popup, 0, 0, &tx, &ty);

	struct wlr_box constraint = {
		.x = output_toplevel_box.x - tx,
		.y = output_toplevel_box.y - ty,
		.width = output_toplevel_box.width,
		.height = output_toplevel_box.height,
	};
	struct wlr_box *geometry = &popup->scheduled.geometry;

	popup_keep_extent(&geometry->x, &geometry->width, popup->scheduled.rules.size.width, constraint.x,
			  constraint.width);
	popup_keep_extent(&geometry->y, &geometry->height, popup->scheduled.rules.size.height, constraint.y,
			  constraint.height);
}

static struct cg_xdg_shell_view *
xdg_shell_view_from_view(struct cg_view *view)
{
	return (struct cg_xdg_shell_view *) view;
}

static char *
get_title(struct cg_view *view)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	return xdg_shell_view->xdg_toplevel->title;
}

static void
get_geometry(struct cg_view *view, int *width_out, int *height_out)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	struct wlr_xdg_surface *xdg_surface = xdg_shell_view->xdg_toplevel->base;

	*width_out = xdg_surface->geometry.width;
	*height_out = xdg_surface->geometry.height;
}

/*
 * A CLIENT DRAWING ITS OWN DECORATIONS PUTS ITS WINDOW INSIDE A LARGER BUFFER,
 * and this is where. See cg_view_impl::get_origin.
 */
static void
get_origin(struct cg_view *view, int *x_out, int *y_out)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	struct wlr_xdg_surface *xdg_surface = xdg_shell_view->xdg_toplevel->base;

	*x_out = xdg_surface->geometry.x;
	*y_out = xdg_surface->geometry.y;
}

static bool
is_primary(struct cg_view *view)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	struct wlr_xdg_toplevel *parent = xdg_shell_view->xdg_toplevel->parent;

	return parent == NULL;
}

static struct cg_view *
get_parent(struct cg_view *view)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	struct wlr_xdg_toplevel *parent = xdg_shell_view->xdg_toplevel->parent;

	/* `base->data` is the cg_xdg_shell_view, whose first member IS the
	 * cg_view — the same road popup_get_view() takes. */
	return parent ? parent->base->data : NULL;
}

static bool
is_transient_for(struct cg_view *child, struct cg_view *parent)
{
	if (parent->type != CAGE_XDG_SHELL_VIEW) {
		return false;
	}
	struct cg_xdg_shell_view *_child = xdg_shell_view_from_view(child);
	struct wlr_xdg_toplevel *xdg_toplevel = _child->xdg_toplevel;
	struct cg_xdg_shell_view *_parent = xdg_shell_view_from_view(parent);
	while (xdg_toplevel) {
		if (xdg_toplevel->parent == _parent->xdg_toplevel) {
			return true;
		}
		xdg_toplevel = xdg_toplevel->parent;
	}
	return false;
}

static void
activate(struct cg_view *view, bool activate)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_activated(xdg_shell_view->xdg_toplevel, activate);
}

static void
maximize(struct cg_view *view, int output_width, int output_height)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_size(xdg_shell_view->xdg_toplevel, output_width, output_height);
	wlr_xdg_toplevel_set_maximized(xdg_shell_view->xdg_toplevel, true);
}

static void
set_size(struct cg_view *view, int width, int height)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_size(xdg_shell_view->xdg_toplevel, width, height);
}

static void
destroy(struct cg_view *view)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	free(xdg_shell_view);
}

static void
close(struct cg_view *view)
{
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_send_close(xdg_shell_view->xdg_toplevel);
}

static void
set_fullscreen(struct cg_xdg_shell_view *xdg_shell_view, bool fullscreen)
{
	/**
	 * Certain clients do not like figuring out their own window geometry if they
	 * display in fullscreen mode, so we set it here.
	 *
	 * ITS OWN OUTPUT AND NOT THE UNION. The union spans every window this
	 * guest has open, so a fullscreen sized from it hands the client a
	 * surface several screens wide.
	 */
	struct cg_view *view = &xdg_shell_view->view;
	struct cg_output *output = view->win.out ? view->win.out : view->server->embed.anchor;
	struct wlr_box layout_box;

	wlr_output_layout_get_box(view->server->output_layout, output ? output->wlr_output : NULL,
				  &layout_box);
	wlr_xdg_toplevel_set_size(xdg_shell_view->xdg_toplevel, layout_box.width, layout_box.height);
	wlr_xdg_toplevel_set_fullscreen(xdg_shell_view->xdg_toplevel, fullscreen);
}

/*
 * THE REQUEST GOES OUT BEFORE IT IS ANSWERED, when this cage is a window.
 * set_fullscreen() below sizes the toplevel to the output layout — which in
 * embedded mode is the parent's window and not the screen — so a request
 * answered only here gives a video player the same rectangle it already had.
 * The parent resizes the window, the output follows it, and the client is
 * configured again at the size it asked for.
 */
static void
handle_xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct cg_xdg_shell_view *xdg_shell_view = wl_container_of(listener, xdg_shell_view, request_fullscreen);
	bool fullscreen = xdg_shell_view->xdg_toplevel->requested.fullscreen;

	if (!xdg_shell_view->xdg_toplevel->base->initialized) {
		return;
	}

	embed_set_fullscreen(&xdg_shell_view->view, fullscreen);
	set_fullscreen(xdg_shell_view, fullscreen);
}

/*
 * The name a person reads is drawn by whoever owns the frame, and in embedded
 * mode that is the parent: a title kept only in this process is a window
 * called whatever its launcher was called for as long as it is open.
 */
static void
handle_xdg_toplevel_set_title(struct wl_listener *listener, void *data)
{
	struct cg_xdg_shell_view *xdg_shell_view = wl_container_of(listener, xdg_shell_view, set_title);
	const char *title = xdg_shell_view->xdg_toplevel->title;

	if (!title) {
		return;
	}
	if (xdg_shell_view->view.foreign_toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_set_title(xdg_shell_view->view.foreign_toplevel_handle, title);
	}
	embed_set_title(&xdg_shell_view->view, title);
}

static void
handle_xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct cg_xdg_shell_view *xdg_shell_view = wl_container_of(listener, xdg_shell_view, unmap);
	struct cg_view *view = &xdg_shell_view->view;

	view_unmap(view);
}

static void
handle_xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	struct cg_xdg_shell_view *xdg_shell_view = wl_container_of(listener, xdg_shell_view, map);
	struct cg_view *view = &xdg_shell_view->view;

	/* THE NAME TRAVELS WITH KEMBED_OPEN, which view_map() sends: a title set
	 * before the window exists is one the parent has nowhere to put. */
	view_map(view, xdg_shell_view->xdg_toplevel->base->surface);

	if (xdg_shell_view->xdg_toplevel->title) {
		wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel_handle,
							 xdg_shell_view->xdg_toplevel->title);
	}
	if (xdg_shell_view->xdg_toplevel->app_id)
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel_handle,
							  xdg_shell_view->xdg_toplevel->app_id);
	/* Activation state will be set by seat_set_focus */
}

static void
handle_xdg_toplevel_commit(struct wl_listener *listener, void *data)
{
	struct cg_xdg_shell_view *xdg_shell_view = wl_container_of(listener, xdg_shell_view, commit);

	if (xdg_shell_view->xdg_toplevel->base->surface->mapped) {
		wlr_foreign_toplevel_handle_v1_set_fullscreen(xdg_shell_view->view.foreign_toplevel_handle,
							      xdg_shell_view->xdg_toplevel->current.fullscreen);
		/*
		 * AND THE WINDOW'S PLACE INSIDE ITS BUFFER IS RE-READ HERE. A
		 * client drops its shadow margins when it is maximised and
		 * takes them back when it is not, so the origin moves with the
		 * state; a node left where the previous state put it shows
		 * margin on two sides and clips the window on the other two.
		 * wlr_scene_node_set_position() returns on an unchanged
		 * position, so a commit that moved nothing costs a compare.
		 */
		view_place_node(&xdg_shell_view->view);
	}

	if (!xdg_shell_view->xdg_toplevel->base->initial_commit) {
		return;
	}

	wlr_xdg_toplevel_set_wm_capabilities(xdg_shell_view->xdg_toplevel, XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

	struct cg_view *view = &xdg_shell_view->view;

	/*
	 * THE FIRST ORDINARY TOPLEVEL TAKES THE OUTPUT THIS CAGE STARTED WITH,
	 * at the initial commit and not at the map: a guest that shows one
	 * window is then configured at the size the parent forked it with,
	 * allocates no output and comes up exactly as a single-window cage does.
	 */
	if (view->server->embed.embedded) {
		output_claim_anchor(view);
	}

	if (xdg_shell_view->xdg_toplevel->requested.fullscreen) {
		embed_set_fullscreen(view, true);
		set_fullscreen(xdg_shell_view, true);
	} else if (view->server->embed.embedded && !view->win.out) {
		/*
		 * A SECOND TOPLEVEL IS CONFIGURED 0x0 — "you choose". It has no
		 * output yet, and a size named here is a size the client would
		 * take for the parent's; the size it picks instead is the
		 * natural size KEMBED_OPEN reports at the map, which is what
		 * lets the console place a dialog at dialog size.
		 */
		wlr_xdg_surface_schedule_configure(xdg_shell_view->xdg_toplevel->base);
	} else {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface. */
		view_position(view);
	}
}

static void
handle_xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct cg_xdg_shell_view *xdg_shell_view = wl_container_of(listener, xdg_shell_view, destroy);
	struct cg_view *view = &xdg_shell_view->view;

	wl_list_remove(&xdg_shell_view->commit.link);
	wl_list_remove(&xdg_shell_view->map.link);
	wl_list_remove(&xdg_shell_view->unmap.link);
	wl_list_remove(&xdg_shell_view->destroy.link);
	wl_list_remove(&xdg_shell_view->set_title.link);
	wl_list_remove(&xdg_shell_view->request_fullscreen.link);
	xdg_shell_view->xdg_toplevel = NULL;

	view_destroy(view);
}

static const struct cg_view_impl xdg_shell_view_impl = {
	.get_title = get_title,
	.get_geometry = get_geometry,
	.get_origin = get_origin,
	.is_primary = is_primary,
	.get_parent = get_parent,
	.is_transient_for = is_transient_for,
	.activate = activate,
	.maximize = maximize,
	.set_size = set_size,
	.destroy = destroy,
	.close = close,
};

void
handle_new_xdg_toplevel(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *toplevel = data;

	struct cg_xdg_shell_view *xdg_shell_view = calloc(1, sizeof(struct cg_xdg_shell_view));
	if (!xdg_shell_view) {
		wlr_log(WLR_ERROR, "Failed to allocate XDG Shell view");
		return;
	}

	view_init(&xdg_shell_view->view, server, CAGE_XDG_SHELL_VIEW, &xdg_shell_view_impl);
	xdg_shell_view->xdg_toplevel = toplevel;

	xdg_shell_view->commit.notify = handle_xdg_toplevel_commit;
	wl_signal_add(&toplevel->base->surface->events.commit, &xdg_shell_view->commit);
	xdg_shell_view->map.notify = handle_xdg_toplevel_map;
	wl_signal_add(&toplevel->base->surface->events.map, &xdg_shell_view->map);
	xdg_shell_view->unmap.notify = handle_xdg_toplevel_unmap;
	wl_signal_add(&toplevel->base->surface->events.unmap, &xdg_shell_view->unmap);
	xdg_shell_view->destroy.notify = handle_xdg_toplevel_destroy;
	wl_signal_add(&toplevel->events.destroy, &xdg_shell_view->destroy);
	xdg_shell_view->set_title.notify = handle_xdg_toplevel_set_title;
	wl_signal_add(&toplevel->events.set_title, &xdg_shell_view->set_title);
	xdg_shell_view->request_fullscreen.notify = handle_xdg_toplevel_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &xdg_shell_view->request_fullscreen);

	toplevel->base->data = xdg_shell_view;
}

static void
popup_handle_destroy(struct wl_listener *listener, void *data)
{
	struct cg_xdg_popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->reposition.link);
	free(popup);
}

static void
popup_handle_commit(struct wl_listener *listener, void *data)
{
	struct cg_xdg_popup *popup = wl_container_of(listener, popup, commit);

	if (popup->xdg_popup->base->initial_commit) {
		popup_unconstrain(popup->xdg_popup);
	}
}

static void
popup_handle_reposition(struct wl_listener *listener, void *data)
{
	struct cg_xdg_popup *popup = wl_container_of(listener, popup, reposition);

	popup_unconstrain(popup->xdg_popup);
}

void
handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, new_xdg_popup);
	struct wlr_xdg_popup *wlr_popup = data;

	struct cg_view *view = popup_get_view(wlr_popup);
	if (view == NULL) {
		return;
	}

	struct wlr_scene_tree *parent_scene_tree = NULL;
	struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(wlr_popup->parent);
	if (parent == NULL) {
		return;
	}
	switch (parent->role) {
	case WLR_XDG_SURFACE_ROLE_TOPLEVEL:;
		parent_scene_tree = view->scene_tree;
		break;
	case WLR_XDG_SURFACE_ROLE_POPUP:
		parent_scene_tree = parent->data;
		break;
	case WLR_XDG_SURFACE_ROLE_NONE:
		break;
	}
	if (parent_scene_tree == NULL) {
		return;
	}

	struct cg_xdg_popup *popup = calloc(1, sizeof(*popup));
	if (popup == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate popup");
		return;
	}

	popup->xdg_popup = wlr_popup;

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);

	popup->commit.notify = popup_handle_commit;
	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

	popup->reposition.notify = popup_handle_reposition;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);

	struct wlr_scene_tree *popup_scene_tree = wlr_scene_xdg_surface_create(parent_scene_tree, wlr_popup->base);
	if (popup_scene_tree == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate scene-graph node for XDG popup");
		free(popup);
		return;
	}

	wlr_popup->base->data = popup_scene_tree;
}

void
handle_xdg_toplevel_decoration(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, xdg_toplevel_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration = data;

	struct cg_xdg_decoration *xdg_decoration = calloc(1, sizeof(struct cg_xdg_decoration));
	if (!xdg_decoration) {
		return;
	}

	xdg_decoration->wlr_decoration = wlr_decoration;
	xdg_decoration->server = server;

	xdg_decoration->destroy.notify = xdg_decoration_handle_destroy;
	wl_signal_add(&wlr_decoration->events.destroy, &xdg_decoration->destroy);
	xdg_decoration->commit.notify = xdg_decoration_handle_commit;
	wl_signal_add(&wlr_decoration->toplevel->base->surface->events.commit, &xdg_decoration->commit);
	xdg_decoration->request_mode.notify = xdg_decoration_handle_request_mode;
	wl_signal_add(&wlr_decoration->events.request_mode, &xdg_decoration->request_mode);
}
