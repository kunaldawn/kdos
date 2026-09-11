/* libkkms — shared between the seat/mode half and the input half. */

#ifndef KKMS_PRIV_H
#define KKMS_PRIV_H

#include <stdint.h>

/*
 * The real headers, not hand-written forward declarations. pixman_image_t is a
 * UNION and drmModeRes is a typedef of an anonymous struct; declaring either by
 * hand compiles until it meets the real one and then contradicts it.
 */
#include <libinput.h>
#include <libseat.h>
#include <pixman.h>
#include <xf86drmMode.h>
#include <xkbcommon/xkbcommon.h>

#include <libudev.h>

#include "ktui.h"

/*
 * ONE SCREEN. Everything here is per-connector and nothing is shared: a second
 * monitor is a second dumb buffer, a second CRTC and a second row diff, and the
 * one thing that would break if they were shared is the diff — two screens
 * comparing against one `prev` would each see the other's paint as already
 * done and neither would redraw.
 */
struct kkms_out {
	uint32_t connector, crtc, fb, handle;
	uint32_t stride;
	uint64_t size;
	drmModeModeInfo mode;
	int width, height;	/* this output's mode, in pixels            */

	/*
	 * WHERE THIS OUTPUT SITS IN THE GRID, in cells, and how many it shows.
	 * Screens are placed edge to edge from the left in connector order —
	 * an ORDER and not a geometry, which is what window-model.md already
	 * says the desktop means by a screen layout.
	 */
	int col, cols, rows;
	/* Where the slice is drawn inside this output's pixels. A screen
	 * showing fewer rows than the grid has is CENTRED, never scaled: a
	 * character grid stretched to fit is a grid of the wrong shape. */
	int px, py;

	void *pixels;
	pixman_image_t *image;
	/* This output's own slice of the shared grid, and the frame it last
	 * painted. Its own, because the diff is what decides which rows are
	 * repainted and a shared one would be right for at most one screen. */
	KtuiCell *cur, *prev;
	int ncell;
	int force_full;
};

/* How many screens one session lights. Eight is the number of cards the device
 * sweep already tries; a machine with more connectors than this lights the
 * first eight, which is a shorter desktop and not a broken one. */
#define KKMS_MAX_OUT 8

struct kkms {
	struct libseat *seat;
	int active;

	int drm_fd, drm_dev;
	drmModeRes *res;

	struct kkms_out out[KKMS_MAX_OUT];
	int nout;
	/* The whole desktop, in pixels: the widest row of outputs by the
	 * tallest. `kkms_size()` divides this by the cell, so a window dragged
	 * past the right edge of one screen is on the next. */
	int vw, vh;

	/* The DRM hotplug monitor. libinput's udev context watches `input`
	 * and nothing else, so a monitor of our own is what makes a screen
	 * plugged in after login appear — there was none, and the plan's
	 * "the udev monitor it already holds" named one that did not exist. */
	struct udev *hotplug_udev;
	struct udev_monitor *hotplug;
	int hotplug_fd;

	/* The font this screen is drawing with, kept so kkms_set_font() can
	 * put it back when a new one will not load — a chord that made the
	 * screen unreadable and could not be undone is worse than one that
	 * did nothing. Empty means the built-in default. */
	char font[192];

	struct libinput *li;
	struct xkb_context *xkb;
	struct xkb_keymap *keymap;
	struct xkb_state *state;

	/* The pointer, in cells, and the queue the backend drains. */
	int ptr_x, ptr_y;
	double ptr_px, ptr_py;
	/* Set by the first motion. Nothing is drawn before it: a machine with no
	 * pointing device would otherwise wear an arrow in its top-left corner
	 * for the life of the session. */
	int ptr_seen;
	KtuiEvent q[64];
	int qhead, qtail;
};

extern struct kkms K;

int kkms_input_init(void);
void kkms_input_shutdown(void);
void kkms_input_pump(void);
int kkms_input_fd(void);
int kkms_poll_event(KtuiEvent *ev, int timeout_ms);

#endif /* KKMS_PRIV_H */
