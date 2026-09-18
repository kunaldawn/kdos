#ifndef CG_SEAT_H
#define CG_SEAT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "server.h"
#include "view.h"

#define DEFAULT_XCURSOR "left_ptr"
#define XCURSOR_SIZE 24

struct cg_seat {
	struct wlr_seat *seat;
	struct cg_server *server;
	struct wl_listener destroy;

	struct wl_list keyboards;
	struct wl_list keyboard_groups;
	struct wl_list pointers;
	struct wl_list touch;
	struct wl_listener new_input;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct wl_listener cursor_motion_relative;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	int32_t touch_id;
	double touch_lx;
	double touch_ly;
	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;
	struct wl_listener touch_frame;

	struct wl_list drag_icons;
	struct wl_listener request_start_drag;
	struct wl_listener start_drag;

	/*
	 * The keyboard an EMBEDDED guest is typed into. A headless backend has
	 * no keyboard device, and without a keyboard object the seat has no
	 * keymap to send — and a client with no keymap ignores every key. NULL
	 * unless --embed.
	 */
	struct wlr_keyboard_group *embed_keys;

	/*
	 * THE POINTER THE GUEST TOOK. A game and a three-dimensional editor
	 * lock or confine the pointer and read motion as a delta, and while one
	 * is held where the pointer is belongs to this seat and not to the
	 * parent: a position from outside fights a lock, and under a confine it
	 * undoes the moves the region allowed. NULL unless --embed: a
	 * constraint announced where it is not honoured is a client told its
	 * grab took when the cursor keeps moving.
	 */
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wl_listener new_constraint;
	struct wlr_pointer_constraint_v1 *constraint;

	/*
	 * THE WINDOW THE GRAB WAS REPORTED AGAINST. The release has to name the
	 * same one the grab did, and by the time a grab ends the keyboard has
	 * usually moved — a release told against the window that took the
	 * keyboard leaves the parent holding a grab for a window that never had
	 * one, with its own arrow stopped for ever.
	 */
	struct cg_view *grab_view;

	/*
	 * HOW FAR THE DEVICE MOVED, waiting for the motion it belongs to. Two
	 * positions differ by the pointer's speed after acceleration, clamping
	 * and the desktop's own cell grid, which is not what a guest reading
	 * relative motion asked for — so the parent sends the delta first and
	 * the motion after it, and the motion spends this.
	 */
	double rel_dx, rel_dy, rel_dx_un, rel_dy_un;
	bool rel_pending;

	/*
	 * WHETHER THE ARROW IS ON THIS GUEST'S PIXELS AT ALL.
	 *
	 * The guest's cursor is composited into the buffer the parent shows, so
	 * a cursor left drawn after the pointer has gone elsewhere is a second
	 * arrow on the desktop, parked wherever the pointer last was inside the
	 * window. It comes off with the pointer focus and goes back on with the
	 * next position the parent sends.
	 */
	bool ptr_hidden;

	struct wl_listener request_set_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;
};

struct cg_keyboard_group {
	struct wlr_keyboard_group *wlr_group;
	struct cg_seat *seat;
	struct wl_listener key;
	struct wl_listener modifiers;
	struct wl_list link; // cg_seat::keyboard_groups
	bool is_virtual;
};

struct cg_pointer {
	struct wl_list link; // seat::pointers
	struct cg_seat *seat;
	struct wlr_pointer *pointer;

	struct wl_listener destroy;
};

struct cg_touch {
	struct wl_list link; // seat::touch
	struct cg_seat *seat;
	struct wlr_touch *touch;

	struct wl_listener destroy;
};

struct cg_drag_icon {
	struct wl_list link; // seat::drag_icons
	struct cg_seat *seat;
	struct wlr_drag_icon *wlr_drag_icon;
	struct wlr_scene_tree *scene_tree;

	/* The drag icon has a position in layout coordinates. */
	double lx, ly;

	struct wl_listener destroy;
};

struct cg_seat *seat_create(struct cg_server *server, struct wlr_backend *backend);
void seat_destroy(struct cg_seat *seat);
struct cg_view *seat_get_focus(struct cg_seat *seat);
void seat_set_focus(struct cg_seat *seat, struct cg_view *view);

void seat_center_cursor(struct cg_seat *seat);

/* Input with no input device — see seat.c. Only --embed calls these. */
void seat_embed_enable(struct cg_seat *seat);

/*
 * THE LAYOUT THE PERSON IS TYPING ON, in xkb's text format, `len` counting the
 * terminator. A keycode means nothing without one: with no keymap of the
 * person's own a guest reads US positions and a French keyboard types the
 * wrong letters. A text that does not compile leaves the layout already set.
 */
void seat_embed_keymap(struct cg_seat *seat, const char *text, size_t len);

/*
 * THE LOCKS AND THE LAYOUT GROUP, which no key stream can establish: Caps Lock
 * was pressed before this process existed. A RESYNC AND NOT A PER-KEY EVENT —
 * xkb's own rule is that a state driven by keys must not also be set by mask,
 * because the two then disagree about which keys are down and the next key
 * resolves under the wrong one — so the parent sends it at focus-in, on a
 * keymap change, and BEHIND a key that left the locks or the layout group
 * somewhere other than where that window was last told they were. Behind such
 * a key and never ahead of one, and none at all while the session holds no
 * mask of its own.
 */
void seat_embed_mods(struct cg_seat *seat, uint32_t depressed,
		     uint32_t latched, uint32_t locked, uint32_t group);

/* A key went down, or came up: an evdev code, NOT +8. */
void seat_embed_key(struct cg_seat *seat, uint32_t keycode, bool pressed,
		    uint32_t time_msec);
/*
 * THE POINTER, IN `view`'s OWN PIXELS. Every coordinate on the channel is
 * window-relative — the parent has one arrow over many windows and each one has
 * a top-left of its own — so the window's origin on the layout is added here,
 * once, on receipt.
 */
void seat_embed_motion(struct cg_seat *seat, struct cg_view *view, double x,
		       double y, uint32_t time_msec);

/* How far the device moved, in pixels. While the guest holds the pointer this
 * is the whole event, because the parent sends no position behind it: a lock
 * gets the delta alone, a confine gets it and the cursor walked by it inside
 * the region. Otherwise it is spent by the motion that follows. */
void seat_embed_rel(struct cg_seat *seat, double dx, double dy,
		    double dx_unaccel, double dy_unaccel, uint32_t time_msec);
/* The same aim with no arrow drawn — the parent is carrying a drag and draws
 * its own pointer. See seat.c. */
void seat_embed_drag_motion(struct cg_seat *seat, struct cg_view *view, double x,
			    double y, uint32_t time_msec);
void seat_embed_button(struct cg_seat *seat, uint32_t button, bool pressed,
		       uint32_t time_msec);

/* A scroll: `value` in pixels, `value120` in the high-resolution count a
 * modern toolkit steps by, `axis` 0 vertical and 1 horizontal, `source`
 * KEMBED_AXIS_*. Both numbers, because a client reads one or the other and
 * never both; zero on both ends the gesture. */
void seat_embed_axis(struct cg_seat *seat, double value, int32_t value120,
		     int axis, int source, bool inverted, uint32_t time_msec);

/* The pointer is no longer over this window. */
void seat_embed_leave(struct cg_seat *seat);

/*
 * THE KEYBOARD IS `view`'s, or NULL and it is nobody's. THE PARENT NAMES THE
 * WINDOW AND THIS END NEVER CHOOSES ONE: a focus moved here is keys going to a
 * window the person is not looking at, while the parent goes on releasing them
 * against another. Losing it releases every key the guest still holds — a press
 * whose release went to another window is a key held for ever — and drops any
 * pointer grab, which is what makes moving the focus the one escape from a
 * pointer the guest has taken.
 */
void seat_embed_focus(struct cg_seat *seat, struct cg_view *view);

#endif
