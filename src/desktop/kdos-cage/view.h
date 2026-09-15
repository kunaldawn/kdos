#ifndef CG_VIEW_H
#define CG_VIEW_H

#include "config.h"

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#if CAGE_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "server.h"

enum cg_view_type {
	CAGE_XDG_SHELL_VIEW,
#if CAGE_HAS_XWAYLAND
	CAGE_XWAYLAND_VIEW,
#endif
};

/*
 * ONE TOPLEVEL'S OWN OUTPUT AND ITS OWN MAPPING, which is the whole of how a
 * guest gets more than one window on the parent's desktop.
 *
 * A SCENE OUTPUT RENDERS ONLY WHAT IS INSIDE ITS LAYOUT BOX, so two toplevels
 * placed in two disjoint boxes land in two framebuffers and nothing composites
 * one over the other. That is what `win` buys: one output composites the five
 * toplevels of a GIMP into one picture before the parent sees a byte, and no
 * work downstream can separate them again.
 *
 * ZERO IN `win` MEANS THIS TOPLEVEL HAS NOT BEEN ANNOUNCED. KEMBED_OPEN comes
 * at the map, where the client has committed a buffer and its geometry is real,
 * and nothing about a window may cross before it.
 */
struct cg_win {
	uint32_t win;			/* its id on the wire, 0 until OPEN  */
	uint32_t owner;			/* the owner's win, 0 for none       */
	struct cg_output *out;		/* the output this toplevel fills    */

	void *map;			/* KEMBED_SLOTS frames               */
	size_t map_len;
	size_t slot_len;
	size_t stride;
	int width, height;
	int slot;			/* the one the parent is reading     */

	/*
	 * NOTHING WHOLE IS IN THE MAPPING THE PARENT IS HOLDING. True from
	 * every remap until a frame has been copied and the slot flipped. A
	 * mapping in that state is zero-filled and the parent is showing none
	 * of it, which makes "keep the last whole frame" a lie there.
	 */
	bool map_blank;

	/*
	 * NOBODY CAN SEE THIS WINDOW, so it is not rendered. Per window and not
	 * per process: a guest whose docks are on another workspace stops
	 * drawing them and goes on drawing the one in front of the person.
	 */
	bool asleep;

	/*
	 * THIS TOPLEVEL ASKED FOR THE SCREEN BEFORE IT WAS ANNOUNCED. A client
	 * may request fullscreen at its initial commit, which is before the map
	 * and so before there is a `win` to name — and an op with no window is
	 * one the parent has nowhere to put. Sent immediately behind
	 * KEMBED_OPEN, or a video player launched fullscreen comes up windowed.
	 */
	bool want_fullscreen;
};

struct cg_view {
	struct cg_server *server;
	struct wl_list link; // server::views
	struct wlr_surface *wlr_surface;
	struct wlr_scene_tree *scene_tree;

	/* The view has a position in layout coordinates. */
	int lx, ly;

	/* Its window on the parent's desktop. Unused unless --embed. */
	struct cg_win win;

	enum cg_view_type type;
	const struct cg_view_impl *impl;

	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wl_listener request_activate;
	struct wl_listener request_close;
};

struct cg_view_impl {
	char *(*get_title)(struct cg_view *view);
	void (*get_geometry)(struct cg_view *view, int *width_out, int *height_out);
	bool (*is_primary)(struct cg_view *view);
	/*
	 * THE TOPLEVEL THIS ONE BELONGS TO, or NULL for a window of its own.
	 * The view and not the shell object, because the owner's `win` is what
	 * KEMBED_OPEN carries — a parent that cannot name the owner cannot keep
	 * a dialog above it, close it with it, or leave it out of the taskbar.
	 * A parent that is not mapped is no owner: it has no window to sit on.
	 */
	struct cg_view *(*get_parent)(struct cg_view *view);
	bool (*is_transient_for)(struct cg_view *child, struct cg_view *parent);
	void (*activate)(struct cg_view *view, bool activate);
	void (*maximize)(struct cg_view *view, int output_width, int output_height);
	/*
	 * THE SAME RECTANGLE WITHOUT THE MAXIMISED STATE, for a toplevel that
	 * names an owner. Maximised is what strips a toolkit's shadow and
	 * rounded corners from a window that fills its frame; on a dialog it
	 * adds a restore button to something no desktop shows one on.
	 */
	void (*set_size)(struct cg_view *view, int width, int height);
	void (*close)(struct cg_view *view);
	void (*destroy)(struct cg_view *view);
};

char *view_get_title(struct cg_view *view);
bool view_is_primary(struct cg_view *view);

/*
 * A WINDOW ON THE PARENT'S DESKTOP, or a surface that places itself. An X11
 * override-redirect surface — a menu, a tooltip, a splash, a drag icon — names
 * its own position in root coordinates, which IS the output layout, so it
 * renders inside the output its owner already has. Giving one an output of its
 * own would make every tooltip a taskbar entry.
 */
bool view_is_window(struct cg_view *view);

/*
 * THE TOPLEVEL THIS ONE BELONGS TO, or NULL. See cg_view_impl::get_parent.
 */
struct cg_view *view_get_parent(struct cg_view *view);
bool view_is_transient_for(struct cg_view *child, struct cg_view *parent);
void view_activate(struct cg_view *view, bool activate);
void view_position(struct cg_view *view);
void view_position_all(struct cg_server *server);
void view_unmap(struct cg_view *view);
void view_map(struct cg_view *view, struct wlr_surface *surface);
void view_destroy(struct cg_view *view);
void view_init(struct cg_view *view, struct cg_server *server, enum cg_view_type type, const struct cg_view_impl *impl);

struct cg_view *view_from_wlr_surface(struct wlr_surface *surface);

#endif
