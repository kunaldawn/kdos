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

/* How many modes one connector may publish. A monitor lists a few dozen; a
 * list longer than this is truncated, which is a shorter picker and not a
 * broken one. */
#define KKMS_MAX_MODES 64

/*
 * ONE SCREEN. Everything here is per-connector and nothing is shared: a second
 * monitor is a second dumb buffer, a second CRTC and a second row diff, and the
 * one thing that would break if they were shared is the diff — two screens
 * comparing against one `prev` would each see the other's paint as already
 * done and neither would redraw.
 */
struct kkms_out {
	uint32_t connector, crtc;
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

	/*
	 * WHAT THIS SCREEN PUBLISHED, gathered at the probe and HELD. The
	 * connector is already open there; re-opening one asks the kernel to
	 * probe the monitor again, so a picker that fetched its list on demand
	 * would pay a modeset-shaped stall every time somebody opened it.
	 */
	drmModeModeInfo modes[KKMS_MAX_MODES];
	int nmodes, cur_mode;
	/* What a PERSON calls this screen — `HDMI-A-1`, `eDP-1`. A connector
	 * id is a kernel object number and means nothing to somebody choosing
	 * between two monitors. */
	char name[32];

	/*
	 * TWO SCANOUT BUFFERS AND THE PAINTER'S OWN.
	 *
	 * `shadow` is system memory and is where every glyph is composited:
	 * OP_OVER reads the destination back, and a dumb buffer is mapped
	 * write-combined, so compositing into one costs an uncached read per
	 * pixel of every glyph on the screen.
	 *
	 * `pixels[]` are the two dumb buffers. A frame is painted into the
	 * shadow, the rows that changed are copied into the buffer NOT being
	 * scanned out, and that buffer is flipped to at the next vblank — so
	 * no pixel is ever rewritten while the raster is inside it. `back` is
	 * the one being painted and `flip_pending` is a flip the kernel has
	 * not yet reported.
	 *
	 * `owed[]` is per buffer because the two are a frame apart: a row
	 * copied into one is still the previous frame's in the other, and a
	 * flip that forgot that would show a screen half of two frames.
	 * `pad_owed[]` is the same debt for the strip below the last whole
	 * row, which no row of `owed[]` covers: it is written only on a full
	 * paint, so a buffer that missed that frame keeps whatever it held and
	 * the strip blinks at half the flip rate.
	 *
	 * `nbuf` is 1 where the driver cannot flip or must not be asked to,
	 * and the buffer is then painted and marked dirty in place.
	 *
	 * `flip_gen` names the buffers a flip was issued against, and it is the
	 * ONLY thing a completion is matched against — never the output count,
	 * which says how many screens are lit and not which slots hold
	 * buffers. A completion arrives after the framebuffer it named may
	 * have been freed and the slot given to another connector, and one
	 * taken for a later flip clears `flip_pending` a frame early — the
	 * next paint then writes the buffer the raster is inside.
	 */
	void *pixels[2];
	uint32_t handle[2], fb[2];
	int nbuf, back, flip_pending;
	unsigned flip_gen;
	unsigned char *owed[2];
	unsigned char pad_owed[2];

	void *shadow_bits;
	pixman_image_t *image;		/* over shadow_bits */
	/*
	 * WHICH SCREEN AND WHICH MODE THE BUFFERS ABOVE WERE MADE FOR.
	 *
	 * A re-probe rewrites this array from the connectors, so slot 2 may be
	 * a different monitor than it was; the buffers are only still this
	 * output's if all three agree. Tearing down a framebuffer that is
	 * being scanned out disables its CRTC, so remaking one that did not
	 * need it is every OTHER screen going black because one changed.
	 *
	 * `buf_mode` is the TIMING the CRTC was last programmed with, which
	 * the pixel size does not determine: two modes of the same size at
	 * different refresh rates keep the same buffers and still need the
	 * modeset, or a chosen mode is reported in force and is not.
	 */
	uint32_t buf_conn;
	int buf_w, buf_h;
	drmModeModeInfo buf_mode;

	/* This output's own slice of the shared grid, and the frame it last
	 * painted. Its own, because the diff is what decides which rows are
	 * repainted and a shared one would be right for at most one screen. */
	KtuiCell *cur, *prev;
	int ncell;
	int force_full;
	/* One byte per row, filled by the painter with the rows it drew. */
	unsigned char *painted;
};

/*
 * How many raw input events are held between drains. Two hundred and fifty-six
 * is four seconds of a hand at human speed and a tenth of a second of a
 * thousand-hertz mouse whose motion did not coalesce; a queue that fills is a
 * view that has stopped draining, and the OLDEST entry goes then, because the
 * newest carries the release a key held down depends on.
 */
#define KKMS_RAWQ 256

/* How many screens one session lights. Eight is the number of cards the device
 * sweep already tries; a machine with more connectors than this lights the
 * first eight, which is a shorter desktop and not a broken one. */
#define KKMS_MAX_OUT 8

struct kkms {
	struct libseat *seat;
	int active;
	/* The screen is powered down: no painting, no page flip and no dirty
	 * rectangle until kkms_blank(0), which owes every output a full
	 * repaint. */
	int blanked;

	int drm_fd, drm_dev;
	drmModeRes *res;
	/* This driver shows the guest's buffer by copying it to the host at
	 * the dirty rectangle it is given, rather than scanning it out. One
	 * buffer is then tear-free and a page flip is a whole-plane upload,
	 * so the pair is not made. Decided once: the card cannot change. */
	int drm_transfers;

	struct kkms_out out[KKMS_MAX_OUT];
	int nout;
	/* The whole desktop, in pixels, and always a WHOLE NUMBER OF CELLS:
	 * the outputs' cell widths laid end to end by the tallest one's cell
	 * height. `kkms_size()` divides this by the cell, so `pixel / cell` is
	 * the same column the paint cut — a box sized in raw mode pixels
	 * yields a column past the last slice whenever a mode is not a whole
	 * number of cells. */
	int vw, vh;

	/* The DRM hotplug monitor. libinput's udev context watches `input`
	 * and nothing else, so a monitor of our own is what makes a screen
	 * plugged in after login appear — there was none, and the plan's
	 * "the udev monitor it already holds" named one that did not exist. */
	struct udev *hotplug_udev;
	struct udev_monitor *hotplug;
	int hotplug_fd;
	/* A hotplug that arrived while the session was switched away. The
	 * device could not be re-probed then and the event does not repeat, so
	 * it is acted on at the next pump after the seat comes back. */
	int hotplug_pending;

	/* The font this screen is drawing with, kept so kkms_set_font() can
	 * put it back when a new one will not load — a chord that made the
	 * screen unreadable and could not be undone is worse than one that
	 * did nothing. Empty means the built-in default. */
	char font[192];

	struct libinput *li;
	struct xkb_context *xkb;
	struct xkb_keymap *keymap;
	struct xkb_state *state;

	/*
	 * THE KEY BEING HELD, and when it next repeats.
	 *
	 * libinput delivers one press and one release and no repeats at all —
	 * autorepeat is the compositor's job, and this backend IS the
	 * compositor. Without it a held Backspace on the console deleted one
	 * character and an arrow key moved one row, which reads as a dropped
	 * keyboard rather than as a missing feature. The figures are the
	 * kernel's own defaults: 500 ms to the first repeat, then 25 a second.
	 *
	 * The KEY is latched and the modifiers are not: a repeat carries the
	 * modifiers as they are held at the moment it fires, so taking Shift
	 * while an arrow is held extends a selection, and re-resolving the
	 * keysym as well would turn a repeating letter into its capital
	 * mid-stream.
	 */
	uint32_t rep_code;
	int rep_key;
	unsigned long long rep_due_ms;

	/* A touchpad's scroll, accumulated: one event carries a few units and
	 * a tick is ten, so a tick taken per event is always zero. */
	double scroll_acc;

	/* The pointer, in cells, and the queue the backend drains. */
	int ptr_x, ptr_y;
	double ptr_px, ptr_py;
	/* Set by the first motion. Nothing is drawn before it: a machine with no
	 * pointing device would otherwise wear an arrow in its top-left corner
	 * for the life of the session. */
	int ptr_seen;
	KtuiEvent q[64];
	int qhead, qtail;
	/*
	 * HOW MANY COOKED EVENTS A CALLER WILL HAVE TAKEN by the time both
	 * queues are empty — what was queued, less whatever a full queue
	 * dropped. Every raw event carries it as KtuiRaw.after, which is the
	 * only ordering between the two queues.
	 */
	unsigned long long cooked_n;

	/*
	 * AND THE SAME INPUT UNRESOLVED, in a queue of its own. See
	 * KtuiBackend.poll_raw: a pixel guest wants the switch and the pixel,
	 * and nothing drawn in cells reads either.
	 *
	 * IT CANNOT SHARE THE QUEUE ABOVE. That one drops its OLDEST entry when
	 * it fills, which is right for a hand that has moved on and wrong here:
	 * a mouse reporting a thousand positions a second would evict the click
	 * and the keystroke that came before them. Consecutive motions merge
	 * into one here instead, and nothing else merges at all — a dropped
	 * press is a letter that never arrives and a dropped release is a key
	 * held down for ever.
	 */
	KtuiRaw rq[KKMS_RAWQ];
	int rqhead, rqtail;
	/* How many entries at the tail were queued since the last cooked event
	 * and so may still belong to it — see raw_bump() in kkms_input.c. */
	int rq_pend;

	/*
	 * THE COMPILED KEYMAP AS TEXT, made on demand and held. xkbcommon
	 * allocates a fresh copy per call and a guest is handed the layout
	 * whenever it takes the focus, so asking each time is tens of
	 * kilobytes of malloc on the focus path. `keymap_gen` is bumped
	 * whenever the text changes, which is what lets a caller forward it
	 * again without comparing it.
	 */
	char *keymap_text;
	unsigned keymap_gen;
};

extern struct kkms K;

int kkms_input_init(void);
void kkms_input_shutdown(void);
void kkms_input_pump(void);
int kkms_input_fd(void);
int kkms_poll_event(KtuiEvent *ev, int timeout_ms);
int kkms_poll_raw(KtuiRaw *ev);
const char *kkms_keymap_text(unsigned *gen);

#endif /* KKMS_PRIV_H */
