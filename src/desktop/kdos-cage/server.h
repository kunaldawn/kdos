#ifndef CG_SERVER_H
#define CG_SERVER_H

#include "config.h"

#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <stddef.h>
#include <stdint.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/util/log.h>

#if CAGE_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

struct cg_output;
struct cg_view;

/*
 * THE WIDEST AND TALLEST WINDOW THIS MODE WILL MAKE, in pixels, and the span
 * every window's output is placed at a whole multiple of.
 *
 * The spans must never overlap: a window rendering inside another window's
 * layout box is the single-framebuffer pile that per-window outputs exist to
 * undo, so a window is refused a size that would reach past its own span.
 * Comfortably above any display a console runs on — an 8K screen is 7680 across
 * — and a window cannot be larger than the screen it is on.
 *
 * FOUR SPANS ACROSS AND FOUR DOWN, so the grid holds SIXTEEN WINDOWS and a
 * seventeenth toplevel is refused an output rather than placed outside it. An
 * X11 client is placed by its ORIGIN in SIGNED 16-BIT root coordinates: the
 * sixteenth span starts at 24576 and the seventeenth would start past 32767,
 * where no Xwayland guest can be told where its window is. The scene's
 * background is sized to the whole grid once and never resized, so a span past
 * it would render against nothing for a Wayland guest too. The parent's own
 * ceiling on windows per channel is this number, and the two must agree: one
 * end allocating past the other is a window that renders and publishes for the
 * life of the process with nobody able to see it or close it.
 */
#define CG_EMBED_SPAN 8192
#define CG_EMBED_COLS 4
#define CG_EMBED_WINS (CG_EMBED_COLS * CG_EMBED_COLS)

enum cg_multi_output_mode {
	CAGE_MULTI_OUTPUT_MODE_EXTEND,
	CAGE_MULTI_OUTPUT_MODE_LAST,
};

/*
 * --embed: the guest's pixels leave through a shared mapping instead of a
 * screen, and its input arrives from the parent instead of a device. See
 * embed.c and kembed.h.
 *
 * THIS IS THE CHANNEL AND NOT A WINDOW. One cage is one socket, one seat and
 * one keyboard focus; the buffer, the output and the size belong to each
 * toplevel and live in its `struct cg_win`. A guest is as many windows on the
 * parent's desktop as it maps toplevels, so anything held once per process here
 * is something every one of those windows would have to share.
 */
struct cg_embed {
	/*
	 * `embedded` IS TRUE FROM THE COMMAND LINE AND `active` ONLY ONCE THE
	 * CHANNEL IS UP. Outputs are laid out and the first one is claimed
	 * before the socket is armed, and a send before HELLO would reach the
	 * parent out of order.
	 */
	bool embedded;
	bool active;

	int fd;				/* the socketpair to kdos-con        */
	struct wl_event_source *source;

	/*
	 * THE SIZE --embed NAMED, which is the anchor output's and the fallback
	 * for a toplevel that reports no size of its own. Not "the size of the
	 * window": that is a property of a window and lives on it.
	 */
	int first_w, first_h;

	/*
	 * THE OUTPUT THIS PROCESS STARTED WITH, and it is never destroyed. A
	 * cage with no output at all stalls every client that waits for a
	 * wl_output before mapping, and leaves Xwayland's root screen at 0x0
	 * where no X client can map at all. The first toplevel claims it, so a
	 * guest that shows one window allocates nothing.
	 */
	struct cg_output *anchor;

	/*
	 * THE ID COUNTER, first 1 and never reused. The parent keys a window on
	 * (channel, win); an id handed out twice is a frame drawn into the
	 * window that used to hold it.
	 */
	uint32_t next_win;

	/*
	 * THE VIEW THE PARENT GAVE THE KEYBOARD TO, or NULL. The parent is the
	 * only source of keyboard focus in this mode: a focus this end moved by
	 * itself is keys going to a window the person is not looking at, while
	 * the parent goes on releasing them against another.
	 */
	struct cg_view *kbd;
};

struct cg_server {
	struct wl_display *wl_display;
	struct wl_list views;
	struct wlr_backend *backend;
	/*
	 * THE HEADLESS BACKEND INSIDE WHATEVER AUTOCREATE BUILT, so a toplevel
	 * that maps can be given an output of its own. autocreate wraps a
	 * single backend in a MULTI one and the wrapper asserts rather than
	 * forwarding an output add, so the real thing is found once and kept.
	 * NULL unless --embed.
	 */
	struct wlr_backend *headless;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_session *session;
	struct wl_listener display_destroy;

	struct cg_seat *seat;
	struct wlr_idle_notifier_v1 *idle;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_v1;
	struct wl_listener new_idle_inhibitor_v1;
	struct wl_list inhibitors;

	enum cg_multi_output_mode output_mode;
	struct wlr_output_layout *output_layout;
	struct wlr_scene_output_layout *scene_output_layout;

	struct wlr_scene *scene;
	/*
	 * The palette's deep colour, behind everything. Resized on every layout
	 * change: a rectangle sized once stops covering the screen the moment a
	 * monitor is plugged in.
	 */
	struct wlr_scene_rect *background;
	/* Includes disabled outputs; depending on the output_mode
	 * some outputs may be disabled. */
	struct wl_list outputs; // cg_output::link
	struct wl_listener new_output;
	struct wl_listener output_layout_change;

	struct wl_listener xdg_toplevel_decoration;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

	struct wl_listener new_virtual_keyboard;
	struct wl_listener new_virtual_pointer;
#if CAGE_HAS_XWAYLAND
	struct wl_listener new_xwayland_surface;
#endif
	struct wlr_output_manager_v1 *output_manager_v1;
	struct wl_listener output_manager_apply;
	struct wl_listener output_manager_test;

#if WLR_HAS_DRM_BACKEND
	struct wlr_drm_lease_v1_manager *drm_lease_v1;
	struct wl_listener drm_lease_request;
#endif

	struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;

	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

	struct cg_embed embed;

	bool xdg_decoration;
	bool allow_vt_switch;
	bool return_app_code;
	bool terminated;
	enum wlr_log_importance log_level;
};

void server_terminate(struct cg_server *server);

#endif
