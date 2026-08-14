/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the surface, the buffers, the input, and the backend vtable
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE		/* memfd_create */
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "ext-session-lock-v1-client-protocol.h"
#include "kwl_priv.h"
#include "cursor-shape-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/*
 * Every output gets a lock surface, because the protocol will not report the
 * session locked until they all have one. Eight is a ceiling on physical
 * monitors, not on ambition.
 */
#define KWL_MAX_OUTPUTS 8
#define KWL_OUTPUT_NAME_MAX 64

/* ── state ─────────────────────────────────────────────────────────────── */

static struct {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_pointer *pointer;
	struct xdg_wm_base *wm_base;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct ext_session_lock_manager_v1 *lock_mgr;
	/* cursor-shape-v1. Without it the pointer VANISHES over every libkwl
	 * surface: on Wayland the focused client owns the cursor image, this
	 * library never set one, and once kdos-desk covered the whole screen
	 * "no cursor over chrome" became "no cursor anywhere". The protocol is
	 * the cheap route — a shape name instead of a theme, a surface and a
	 * buffer of our own. */
	struct wp_cursor_shape_manager_v1 *shape_mgr;
	struct wp_cursor_shape_device_v1 *shape_dev;

	/* Lock role. `engaged` is the compositor's confirmation, `finished` its
	 * refusal; a lock screen must not take a password before the first or
	 * after the second. */
	struct ext_session_lock_v1 *lock;
	int lock_engaged, lock_finished;
	struct wl_output *outputs[KWL_MAX_OUTPUTS];
	/* The compositor's own name for each — `eDP-1`, `HDMI-A-1`. wl_output
	 * version 4's `name` event, which is the only handle on an output that
	 * survives being written on a command line. */
	char output_name[KWL_MAX_OUTPUTS][KWL_OUTPUT_NAME_MAX];
	int noutputs;
	/* The extra outputs: one lock surface each, filled with the theme
	 * background. No cell grid — libktui has one buffer, and it is on the
	 * first output. */
	struct {
		struct wl_surface *surface;
		struct ext_session_lock_surface_v1 *lock_surface;
		KwlBuffer buf;
		int w, h;
	} extra[KWL_MAX_OUTPUTS];
	int nextra;

	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct zwlr_layer_surface_v1 *layer_surface;

	struct xkb_context *xkb_ctx;
	struct xkb_keymap *keymap;
	struct xkb_state *xkb_state;

	KwlBuffer buf[2];
	int cur_buf;

	KwlConfig cfg;
	int px_w, px_h;		/* surface size in pixels                  */
	int cols, rows;		/* and in cells                            */
	int configured;
	int closed;
	int kb_entered;		/* the keyboard has been here at least once */

	/* One pending event, filled by the wl listeners and drained by
	 * poll_event. A cell UI consumes one event per frame, so a queue would
	 * be machinery for a case that does not arise. */
	KtuiEvent pending;
	int has_pending;
	int ptr_cx, ptr_cy;
} K;

/* The gap a corner-anchored overlay keeps from the screen edges, in pixels. */
#define KWL_OVERLAY_MARGIN 8

int kwl_should_close(void) { return K.closed; }
int kwl_lock_engaged(void) { return K.lock_engaged; }
int kwl_lock_finished(void) { return K.lock_finished; }
void *kwl_display(void) { return K.display; }
void *kwl_seat(void) { return K.seat; }

int kwl_fd(void)
{
	return K.display ? wl_display_get_fd(K.display) : -1;
}

void kwl_pump(void)
{
	if (!K.display)
		return;
	/*
	 * READ the socket, not just the queue. dispatch_pending() alone never
	 * reads the fd, so an event the compositor sent sat in the socket
	 * forever: poll() reported readable, nothing consumed it, and the
	 * caller span at 100% CPU — measured on kdos-notifyd the moment its
	 * surface was destroyed and the compositor answered with anything at
	 * all. This is also why a consumer never noticed a DEAD compositor:
	 * the EOF was never read either. prepare_read/read_events is
	 * libwayland's non-blocking read protocol; a failed read (EOF, EPIPE)
	 * is the compositor genuinely gone.
	 */
	while (wl_display_prepare_read(K.display) != 0) {
		if (wl_display_dispatch_pending(K.display) < 0) {
			K.closed = 1;
			return;
		}
	}
	/*
	 * read_events() BLOCKS on a socket with nothing on it — the caller
	 * is expected to have polled. kdos-notifyd does; a next caller that
	 * forgets would hang its whole loop, so the poll is here instead of
	 * in the contract. Zero timeout, the same prepare/cancel discipline
	 * kwl_event() below uses.
	 */
	struct pollfd pfd = {
		.fd = wl_display_get_fd(K.display),
		.events = POLLIN,
	};
	int n = poll(&pfd, 1, 0);
	if (n <= 0) {
		wl_display_cancel_read(K.display);
		if (n < 0 && errno != EINTR) {
			K.closed = 1;
			return;
		}
		/* nothing to read is not a reason to leave requests queued */
		goto flush;
	}
	if (wl_display_read_events(K.display) < 0) {
		K.closed = 1;
		return;
	}
	if (wl_display_dispatch_pending(K.display) < 0) {
		K.closed = 1;
		return;
	}
flush:
	/*
	 * EAGAIN from flush means the socket is full, not that the compositor is
	 * gone — libwayland has buffered the requests and will send them when
	 * the fd drains. Treating it as fatal is how a client that got briefly
	 * ahead of the compositor exited 0 with no message at all.
	 */
	if (wl_display_flush(K.display) < 0 && errno != EAGAIN)
		K.closed = 1;
}
int kwl_cell_w(void) { return kcell_w(); }
int kwl_cell_h(void) { return kcell_h(); }

static void push_event(const KtuiEvent *ev)
{
	/* Last one wins when a frame produced several. Losing an intermediate
	 * pointer-motion is invisible; losing the newest would make the cursor
	 * lag, which is not. */
	K.pending = *ev;
	K.has_pending = 1;
}

/* ── buffers ───────────────────────────────────────────────────────────── */

static void buffer_release(void *data, struct wl_buffer *wl)
{
	(void)wl;
	((KwlBuffer *)data)->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static void buffer_free(KwlBuffer *b)
{
	if (b->img)
		pixman_image_unref(b->img);
	if (b->wl)
		wl_buffer_destroy(b->wl);
	if (b->data)
		munmap(b->data, b->size);
	memset(b, 0, sizeof(*b));
}

static int buffer_alloc(KwlBuffer *b, int w, int h)
{
	buffer_free(b);

	size_t stride = (size_t)w * 4;
	size_t size = stride * (size_t)h;
	int fd = memfd_create("kwl", MFD_CLOEXEC);
	if (fd < 0)
		return -1;
	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return -1;
	}
	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return -1;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(K.shm, fd, (int32_t)size);
	close(fd);
	if (!pool) {
		munmap(data, size);
		return -1;
	}
	/*
	 * ARGB for the background role, XRGB for everything else. XRGB has no
	 * alpha at all, so a desktop surface in it paints an opaque rectangle
	 * over the compositor's wallpaper no matter what is in the cells — the
	 * wallpaper vanished the moment kdos-desk started.
	 */
	bool argb = K.cfg.role == KWL_ROLE_BACKGROUND;
	b->wl = wl_shm_pool_create_buffer(pool, 0, w, h, (int32_t)stride,
					  argb ? WL_SHM_FORMAT_ARGB8888
					       : WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	if (!b->wl) {
		munmap(data, size);
		return -1;
	}
	wl_buffer_add_listener(b->wl, &buffer_listener, b);

	b->img = pixman_image_create_bits(argb ? PIXMAN_a8r8g8b8
					      : PIXMAN_x8r8g8b8, w, h, data,
					  (int)stride);
	if (!b->img) {
		wl_buffer_destroy(b->wl);
		munmap(data, size);
		memset(b, 0, sizeof(*b));
		return -1;
	}
	b->data = data;
	b->size = size;
	b->w = w;
	b->h = h;
	return 0;
}

/* ── the backend ───────────────────────────────────────────────────────── */

static void kwl_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int full)
{
	/* !K.surface: an overlay hidden via kwl_overlay_hide() — drawing
	 * calls while hidden are no-ops, not errors. */
	if (!K.configured || !K.surface)
		return;

	/*
	 * Two buffers, and the flip is not an optimisation. Painting into a
	 * buffer the compositor is still scanning out tears; blocking until it
	 * is released stalls the frame. With two, the common case never waits.
	 */
	KwlBuffer *b = &K.buf[K.cur_buf];
	if (b->busy) {
		K.cur_buf ^= 1;
		b = &K.buf[K.cur_buf];
		/*
		 * Both busy: the compositor is behind. Repaint the one we are
		 * about to reuse in full, because `prev` describes what is in
		 * the OTHER buffer and a partial paint over stale contents is
		 * how a cell grid grows a corrupted row that never repairs.
		 */
		full = 1;
	}
	if (!b->img || b->w != K.px_w || b->h != K.px_h) {
		if (buffer_alloc(b, K.px_w, K.px_h) < 0)
			return;
		full = 1;
	}

	/*
	 * Scale 1: a client's surface is sized in the compositor's pixels and
	 * libkwl does not do HiDPI — wl_surface.set_buffer_scale would need the
	 * cell buffer resized to match, and every layer-shell consumer here
	 * asks for a size in CELLS already. The compositor-side consumer is the
	 * one that scales, because it is the one that knows which output a
	 * frame is on.
	 *
	 * b->w/b->h rather than w*cell_w: the shm buffer is what the compositor
	 * will read, and any of it the grid does not reach must be KT_BG rather
	 * than whatever the last frame left.
	 */
	/*
	 * THE FLICKER FIX, in two halves, and it belongs here because every
	 * chrome client inherits it at once.
	 *
	 * First: an unchanged frame is NOT COMMITTED. The panel redraws on a
	 * one-second tick, the desk on its rescan, the bottom panel on its own
	 * — and every one of those used to attach a fresh buffer and damage its
	 * FULL surface even when not a cell had moved. On a virtio guest with
	 * no GL every commit is a full framebuffer upload, so the desktop
	 * pulsed at the union of everyone's timers. The diff below is against
	 * `prev` — what was last flushed — so "nothing changed" is measured,
	 * not assumed.
	 *
	 * Second: when something DID change, the damage is the dirty row span,
	 * not the whole surface. The buffer is still painted in full when the
	 * swap flipped (the compositor holds the other one), but damage
	 * describes what changed ON SCREEN, and that is the diff against the
	 * last flush regardless of which buffer carries it.
	 */
	int dirty_y0 = -1, dirty_y1 = -1;
	if (prev) {
		for (int y = 0; y < h; y++)
			if (memcmp(cur + (size_t)y * w, prev + (size_t)y * w,
				   (size_t)w * sizeof(*cur))) {
				if (dirty_y0 < 0)
					dirty_y0 = y;
				dirty_y1 = y;
			}
	}
	if (!full && prev && dirty_y0 < 0)
		return;			/* nothing changed: no commit at all */

	kcell_paint(b->img, cur, prev, w, h, full, 1, b->w, b->h);

	wl_surface_attach(K.surface, b->wl, 0, 0);
	if (!full && dirty_y0 >= 0) {
		int ch = kcell_h();
		int y0 = dirty_y0 * ch;
		int y1 = (dirty_y1 + 1) * ch;
		if (y1 > K.px_h)
			y1 = K.px_h;
		wl_surface_damage_buffer(K.surface, 0, y0, K.px_w, y1 - y0);
	} else {
		wl_surface_damage_buffer(K.surface, 0, 0, K.px_w, K.px_h);
	}
	wl_surface_commit(K.surface);
	b->busy = true;
	K.cur_buf ^= 1;
	wl_display_flush(K.display);
}

static int kwl_poll_event(KtuiEvent *ev, int timeout_ms)
{
	/* The documented libwayland read sequence. Anything simpler races: two
	 * threads or a re-entrant dispatch can consume the socket between the
	 * poll and the read, and prepare_read/cancel_read is what makes that
	 * safe rather than occasionally hanging. */
	while (wl_display_prepare_read(K.display) != 0)
		wl_display_dispatch_pending(K.display);
	wl_display_flush(K.display);

	if (K.has_pending) {
		wl_display_cancel_read(K.display);
		*ev = K.pending;
		K.has_pending = 0;
		return 1;
	}

	struct pollfd pfd = {
		.fd = wl_display_get_fd(K.display),
		.events = POLLIN,
	};
	int n = poll(&pfd, 1, timeout_ms);
	if (n <= 0) {
		wl_display_cancel_read(K.display);
		if (n < 0 && errno != EINTR)
			K.closed = 1;
		return 0;
	}
	if (wl_display_read_events(K.display) < 0) {
		K.closed = 1;
		return 0;
	}
	if (wl_display_dispatch_pending(K.display) < 0) {
		K.closed = 1;
		return 0;
	}

	if (K.has_pending) {
		*ev = K.pending;
		K.has_pending = 0;
		return 1;
	}
	return 0;
}

static void kwl_size(int *w, int *h)
{
	*w = K.cols;
	*h = K.rows;
}

static int kwl_caps(void)
{
	/*
	 * TRUECOLOR and UTF8 unconditionally: this is our own renderer, so the
	 * question "does the terminal support it" has no meaning. Not LINUXVT,
	 * which is what keeps A_BOLD usable and evdev out of the input path.
	 * The rich glyph tier follows from UTF8 — see ktui_ramp_init().
	 */
	return KT_CAP_TRUECOLOR | KT_CAP_UTF8 | KT_CAP_MOUSE;
}

static const KtuiBackend kwl_backend = {
	.name = "wayland",
	.flush = kwl_flush,
	.poll_event = kwl_poll_event,
	.size = kwl_size,
	.caps = kwl_caps,
};

/* ── input ─────────────────────────────────────────────────────────────── */

static void kb_keymap(void *d, struct wl_keyboard *k, uint32_t fmt, int fd,
		      uint32_t size)
{
	(void)d;
	(void)k;
	if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}
	char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED)
		return;

	struct xkb_keymap *km = xkb_keymap_new_from_string(
		K.xkb_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1,
		XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map, size);
	if (!km)
		return;

	if (K.xkb_state)
		xkb_state_unref(K.xkb_state);
	if (K.keymap)
		xkb_keymap_unref(K.keymap);
	K.keymap = km;
	K.xkb_state = xkb_state_new(km);
}

static void kb_key(void *d, struct wl_keyboard *k, uint32_t serial,
		   uint32_t time, uint32_t key, uint32_t state)
{
	(void)d;
	(void)k;
	(void)serial;
	(void)time;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !K.xkb_state)
		return;

	xkb_keycode_t kc = key + 8;	/* evdev -> xkb */
	xkb_keysym_t sym = xkb_state_key_get_one_sym(K.xkb_state, kc);

	KtuiEvent ev = { .type = KT_EVT_KEY };
	ev.mods = 0;
	if (xkb_state_mod_name_is_active(K.xkb_state, XKB_MOD_NAME_CTRL,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		ev.mods |= KT_MOD_CTRL;
	if (xkb_state_mod_name_is_active(K.xkb_state, XKB_MOD_NAME_ALT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		ev.mods |= KT_MOD_ALT;
	if (xkb_state_mod_name_is_active(K.xkb_state, XKB_MOD_NAME_SHIFT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		ev.mods |= KT_MOD_SHIFT;

	ev.key = kwl_keysym_to_ktui(sym, K.xkb_state, kc);
	if (ev.key)
		push_event(&ev);
}

static void kb_modifiers(void *d, struct wl_keyboard *k, uint32_t serial,
			 uint32_t dep, uint32_t lat, uint32_t lock,
			 uint32_t group)
{
	(void)d;
	(void)k;
	(void)serial;
	if (K.xkb_state)
		xkb_state_update_mask(K.xkb_state, dep, lat, lock, 0, 0, group);
}

/*
 * CLICK AWAY CLOSES IT — for the callers that asked.
 *
 * The menu, the launcher and the run box are transient and have no business
 * surviving the moment the user's attention goes elsewhere: before this,
 * clicking on a window while a menu was open left the menu floating over that
 * window until somebody found the Escape key, and there is no useful
 * "unfocused menu" state.
 *
 * It is `dismiss_on_unfocus` rather than "every keyboard overlay" because a
 * DIALOG is not a menu — see the flag's comment in kwl.h for the file chooser,
 * which is the case that decides it.
 *
 * Gated on having seen an ENTER first. A leave that arrives before any enter
 * would otherwise close the surface during its own map — and the compositor
 * decides when an ON_DEMAND layer surface gets the keyboard, so that ordering
 * is not ours to assume.
 */
static void kb_enter(void *d, struct wl_keyboard *k, uint32_t s,
		     struct wl_surface *sf, struct wl_array *keys)
{
	(void)d; (void)k; (void)s; (void)sf; (void)keys;
	K.kb_entered = 1;
}
static void kb_leave(void *d, struct wl_keyboard *k, uint32_t s,
		     struct wl_surface *sf)
{
	(void)d; (void)k; (void)s; (void)sf;
	if (K.kb_entered && K.cfg.role == KWL_ROLE_OVERLAY &&
	    K.cfg.dismiss_on_unfocus)
		K.closed = 1;
}
static void kb_repeat(void *d, struct wl_keyboard *k, int32_t rate,
		      int32_t delay)
{ (void)d; (void)k; (void)rate; (void)delay; }

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = kb_keymap,
	.enter = kb_enter,
	.leave = kb_leave,
	.key = kb_key,
	.modifiers = kb_modifiers,
	.repeat_info = kb_repeat,
};

static void pt_motion(void *d, struct wl_pointer *p, uint32_t time,
		      wl_fixed_t sx, wl_fixed_t sy)
{
	(void)d;
	(void)p;
	(void)time;
	int cw = kcell_w(), ch = kcell_h();
	if (cw <= 0 || ch <= 0)
		return;
	K.ptr_cx = wl_fixed_to_int(sx) / cw;
	K.ptr_cy = wl_fixed_to_int(sy) / ch;

	KtuiEvent ev = { .type = KT_EVT_MOUSE, .btn = KT_MB_MOVE,
			 .mx = K.ptr_cx, .my = K.ptr_cy, .press = KT_MP_DRAG };
	push_event(&ev);
}

static void pt_button(void *d, struct wl_pointer *p, uint32_t serial,
		      uint32_t time, uint32_t button, uint32_t state)
{
	(void)d;
	(void)p;
	(void)serial;
	(void)time;
	KtuiEvent ev = { .type = KT_EVT_MOUSE, .mx = K.ptr_cx, .my = K.ptr_cy };
	/* linux/input-event-codes.h, not repeated as an include: libkwl is a
	 * Wayland client and these three are the whole vocabulary. */
	switch (button) {
	case 0x110: ev.btn = KT_MB_LEFT; break;		/* BTN_LEFT   */
	case 0x111: ev.btn = KT_MB_RIGHT; break;	/* BTN_RIGHT  */
	case 0x112: ev.btn = KT_MB_MIDDLE; break;	/* BTN_MIDDLE */
	default: return;
	}
	ev.press = state == WL_POINTER_BUTTON_STATE_PRESSED ? KT_MP_PRESS
							    : KT_MP_RELEASE;
	push_event(&ev);
}

static void pt_axis(void *d, struct wl_pointer *p, uint32_t time,
		    uint32_t axis, wl_fixed_t value)
{
	(void)d;
	(void)p;
	(void)time;
	if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
		return;
	KtuiEvent ev = { .type = KT_EVT_MOUSE, .mx = K.ptr_cx, .my = K.ptr_cy,
			 .press = KT_MP_PRESS };
	ev.btn = wl_fixed_to_double(value) < 0 ? KT_MB_WHEEL_UP
					       : KT_MB_WHEEL_DOWN;
	push_event(&ev);
}

static void pt_enter(void *d, struct wl_pointer *p, uint32_t serial,
		     struct wl_surface *sf, wl_fixed_t x, wl_fixed_t y)
{
	(void)d;
	(void)sf;
	(void)x;
	(void)y;
	/*
	 * The enter serial is the ONLY serial a set-cursor request may carry,
	 * which is why this lives here and not somewhere more convenient. A
	 * client that never answers the enter has no cursor at all — which is
	 * exactly what every libkwl surface did until the desktop grew a
	 * full-screen one and the pointer disappeared over the whole session.
	 */
	if (K.shape_mgr) {
		if (!K.shape_dev)
			K.shape_dev = wp_cursor_shape_manager_v1_get_pointer(
				K.shape_mgr, p);
		if (K.shape_dev)
			wp_cursor_shape_device_v1_set_shape(K.shape_dev, serial,
				WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
	}
}
static void pt_leave(void *d, struct wl_pointer *p, uint32_t s,
		     struct wl_surface *sf)
{ (void)d; (void)p; (void)s; (void)sf; }
static void pt_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void pt_axis_src(void *d, struct wl_pointer *p, uint32_t s)
{ (void)d; (void)p; (void)s; }
static void pt_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a)
{ (void)d; (void)p; (void)t; (void)a; }
static void pt_axis_disc(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{ (void)d; (void)p; (void)a; (void)v; }

static const struct wl_pointer_listener pointer_listener = {
	.enter = pt_enter,
	.leave = pt_leave,
	.motion = pt_motion,
	.button = pt_button,
	.axis = pt_axis,
	.frame = pt_frame,
	.axis_source = pt_axis_src,
	.axis_stop = pt_axis_stop,
	.axis_discrete = pt_axis_disc,
};

static void seat_caps(void *d, struct wl_seat *seat, uint32_t caps)
{
	(void)d;
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !K.keyboard) {
		K.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(K.keyboard, &keyboard_listener, NULL);
	}
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !K.pointer) {
		K.pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(K.pointer, &pointer_listener, NULL);
	}
}

static void seat_name(void *d, struct wl_seat *s, const char *n)
{ (void)d; (void)s; (void)n; }

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_caps,
	.name = seat_name,
};

/* ── surface roles ─────────────────────────────────────────────────────── */

static void resize_cells(int px_w, int px_h)
{
	/*
	 * A configure is not a resize. The compositor sends one on every commit
	 * that changes anything, so raising `ktui_resized` unconditionally puts
	 * a consumer that acts on it into a feedback loop: invalidate → full
	 * repaint → attach/commit → configure → invalidate. Measured on
	 * kdos-notifyd at ~100 commits a second, which ends when
	 * `wl_display_flush` finally answers EAGAIN and the client decides the
	 * compositor is gone — a clean exit 0 with nothing in any log saying
	 * why. Only a real change in size is a resize.
	 */
	if (px_w == K.px_w && px_h == K.px_h)
		return;
	K.px_w = px_w;
	K.px_h = px_h;
	int cw = kcell_w(), ch = kcell_h();
	K.cols = cw > 0 ? px_w / cw : 0;
	K.rows = ch > 0 ? px_h / ch : 0;
	if (K.cols < 1)
		K.cols = 1;
	if (K.rows < 1)
		K.rows = 1;
	ktui_resized = 1;
}

static void layer_configure(void *d, struct zwlr_layer_surface_v1 *ls,
			    uint32_t serial, uint32_t w, uint32_t h)
{
	(void)d;
	zwlr_layer_surface_v1_ack_configure(ls, serial);
	if (w && h)
		resize_cells((int)w, (int)h);
	K.configured = 1;
}

static void layer_closed(void *d, struct zwlr_layer_surface_v1 *ls)
{
	(void)d;
	(void)ls;
	K.closed = 1;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
	.configure = layer_configure,
	.closed = layer_closed,
};

static void xdg_surf_configure(void *d, struct xdg_surface *s, uint32_t serial)
{
	(void)d;
	xdg_surface_ack_configure(s, serial);
	K.configured = 1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surf_configure,
};

static void xdg_top_configure(void *d, struct xdg_toplevel *t, int32_t w,
			      int32_t h, struct wl_array *states)
{
	(void)d;
	(void)t;
	(void)states;
	if (w > 0 && h > 0)
		resize_cells(w, h);
}

static void xdg_top_close(void *d, struct xdg_toplevel *t)
{
	(void)d;
	(void)t;
	K.closed = 1;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_top_configure,
	.close = xdg_top_close,
};

static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{
	(void)d;
	xdg_wm_base_pong(b, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	.ping = wm_ping,
};

/* ── outputs ───────────────────────────────────────────────────────────── */

/*
 * All six events, because a listener with a NULL function pointer is a crash
 * the first time the compositor sends that event, not a compile error. Only
 * `name` is wanted: it is the string the compositor uses for the output
 * everywhere else — `eDP-1`, `HDMI-A-1` — and therefore the only thing that
 * can be passed on a command line.
 */
static void out_geometry(void *d, struct wl_output *o, int32_t x, int32_t y,
			 int32_t pw, int32_t ph, int32_t sub, const char *make,
			 const char *model, int32_t transform)
{ (void)d; (void)o; (void)x; (void)y; (void)pw; (void)ph; (void)sub;
  (void)make; (void)model; (void)transform; }
static void out_mode(void *d, struct wl_output *o, uint32_t flags, int32_t w,
		     int32_t h, int32_t refresh)
{ (void)d; (void)o; (void)flags; (void)w; (void)h; (void)refresh; }
static void out_done(void *d, struct wl_output *o) { (void)d; (void)o; }
static void out_scale(void *d, struct wl_output *o, int32_t f)
{ (void)d; (void)o; (void)f; }
static void out_name(void *data, struct wl_output *o, const char *name)
{
	(void)o;
	if (data && name)
		snprintf((char *)data, KWL_OUTPUT_NAME_MAX, "%s", name);
}
static void out_description(void *d, struct wl_output *o, const char *desc)
{ (void)d; (void)o; (void)desc; }

static const struct wl_output_listener output_listener = {
	.geometry = out_geometry,
	.mode = out_mode,
	.done = out_done,
	.scale = out_scale,
	.name = out_name,
	.description = out_description,
};

/*
 * The output `cfg.output` names, or NULL for "the compositor decides".
 *
 * NULL is not a failure and must not be turned into one: a panel asked for a
 * screen that has just been unplugged is better placed somewhere than not
 * placed at all, and the compositor's supervisor will notice the output is
 * gone and stop it a moment later.
 */
static struct wl_output *named_output(void)
{
	if (!K.cfg.output || !*K.cfg.output)
		return NULL;
	for (int i = 0; i < K.noutputs; i++)
		if (!strcmp(K.output_name[i], K.cfg.output))
			return K.outputs[i];
	return NULL;
}

/* ── registry ──────────────────────────────────────────────────────────── */

static void reg_global(void *d, struct wl_registry *r, uint32_t name,
		       const char *iface, uint32_t version)
{
	(void)d;
	(void)version;
	if (!strcmp(iface, wl_compositor_interface.name))
		K.compositor = wl_registry_bind(r, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, wl_shm_interface.name))
		K.shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, wl_seat_interface.name)) {
		K.seat = wl_registry_bind(r, name, &wl_seat_interface, 5);
		wl_seat_add_listener(K.seat, &seat_listener, NULL);
	} else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		K.wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(K.wm_base, &wm_base_listener, NULL);
	} else if (!strcmp(iface, zwlr_layer_shell_v1_interface.name))
		K.layer_shell = wl_registry_bind(r, name,
						 &zwlr_layer_shell_v1_interface, 1);
	else if (!strcmp(iface, wl_output_interface.name)) {
		/*
		 * Bound for the lock role, which needs a surface per output —
		 * and for `cfg.output`, which is how a panel says WHICH screen
		 * it belongs to. Version 4 for the `name` event: without it the
		 * only handle on an output is a registry id no other process
		 * can name, so a second monitor could be given a panel but not
		 * the RIGHT one.
		 */
		if (K.noutputs < KWL_MAX_OUTPUTS) {
			uint32_t v = version < 4 ? version : 4;
			K.outputs[K.noutputs] = wl_registry_bind(
				r, name, &wl_output_interface, v);
			if (v >= 4)
				wl_output_add_listener(K.outputs[K.noutputs],
						       &output_listener,
						       K.output_name[K.noutputs]);
			K.noutputs++;
		}
	} else if (!strcmp(iface, wp_cursor_shape_manager_v1_interface.name))
		K.shape_mgr = wl_registry_bind(
			r, name, &wp_cursor_shape_manager_v1_interface, 1);
	else if (!strcmp(iface, ext_session_lock_manager_v1_interface.name))
		K.lock_mgr = wl_registry_bind(
			r, name, &ext_session_lock_manager_v1_interface, 1);
}

static void reg_remove(void *d, struct wl_registry *r, uint32_t name)
{ (void)d; (void)r; (void)name; }

static const struct wl_registry_listener registry_listener = {
	.global = reg_global,
	.global_remove = reg_remove,
};

/* ── init ──────────────────────────────────────────────────────────────── */

static int make_panel(void)
{
	static const uint32_t ANCHOR[] = {
		[KWL_EDGE_TOP] = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				 ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				 ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
		[KWL_EDGE_BOTTOM] = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
		[KWL_EDGE_LEFT] = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				  ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
		[KWL_EDGE_RIGHT] = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				   ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
	};

	if (!K.layer_shell)
		return -1;

	int vertical = K.cfg.edge == KWL_EDGE_TOP || K.cfg.edge == KWL_EDGE_BOTTOM;
	int thickness = K.cfg.cells * kcell_h();
	if (!vertical)
		thickness = K.cfg.cells * kcell_w();

	uint32_t layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	if (K.cfg.role == KWL_ROLE_OVERLAY)
		layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
	else if (K.cfg.role == KWL_ROLE_BACKGROUND)
		layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

	/*
	 * The output, when one was named. Passing NULL is layer-shell for
	 * "you choose", and what the compositor chooses is ONE screen — which
	 * is why a two-monitor KDOS had a panel and a desktop on one of them
	 * and nothing on the other. Naming it is how a supervisor can run one
	 * panel per output and know which is which.
	 */
	K.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		K.layer_shell, K.surface, named_output(), layer,
		K.cfg.app_id ? K.cfg.app_id : "kdos");
	if (!K.layer_surface)
		return -1;

	if (K.cfg.role == KWL_ROLE_BACKGROUND) {
		/*
		 * Anchored on ALL FOUR edges with a size of 0x0, which is how
		 * layer-shell spells "the whole output": a surface anchored on
		 * both ends of an axis is stretched to fill it, and 0 means
		 * "you decide" rather than "no pixels".
		 *
		 * NO EXCLUSIVE ZONE, and that is the point of the role. The
		 * desktop is what windows sit ON — reserving space for it would
		 * shrink the compositor's usable area and with it every
		 * maximised window, which is the opposite of what a background
		 * is for. (labwc honours exclusive zones in placement, snapping
		 * and maximise, so this is not advisory there either.)
		 */
		zwlr_layer_surface_v1_set_anchor(
			K.layer_surface,
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
		zwlr_layer_surface_v1_set_size(K.layer_surface, 0, 0);
		zwlr_layer_surface_v1_set_exclusive_zone(K.layer_surface, 0);
		if (K.cfg.keyboard)
			zwlr_layer_surface_v1_set_keyboard_interactivity(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
		zwlr_layer_surface_v1_add_listener(K.layer_surface,
						   &layer_listener, NULL);
		wl_surface_commit(K.surface);
		return 0;
	}

	if (K.cfg.role == KWL_ROLE_OVERLAY) {
		/*
		 * No anchor at all, which is how layer-shell says "centre me":
		 * a surface anchored to nothing is placed in the middle of the
		 * output. Sized in cells, because the content is a grid.
		 *
		 * A corner is an ANCHOR TO TWO EDGES with a size, not a
		 * position — layer-shell has no coordinates. Two edges rather
		 * than all four, or the compositor stretches the surface to
		 * fill the axis it is anchored on both sides of.
		 */
		int cols = K.cfg.cols > 0 ? K.cfg.cols : 64;
		int rows = K.cfg.rows > 0 ? K.cfg.rows : 16;
		if (K.cfg.corner == KWL_CORNER_TOP_RIGHT) {
			zwlr_layer_surface_v1_set_anchor(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
			zwlr_layer_surface_v1_set_margin(
				K.layer_surface,
				K.cfg.margin_y ? K.cfg.margin_y
					       : KWL_OVERLAY_MARGIN,
				K.cfg.margin_x ? K.cfg.margin_x
					       : KWL_OVERLAY_MARGIN,
				0, 0);
		} else if (K.cfg.corner == KWL_CORNER_TOP_LEFT) {
			/*
			 * The dropdown case. margin_y is normally the panel's
			 * own height, so the menu hangs off the bar rather
			 * than covering it, and margin_x is where the word
			 * that was clicked starts. The compositor clamps a
			 * margin that would push the surface off the output,
			 * so a menu opened from the far right does not have to
			 * be clamped here as well.
			 */
			zwlr_layer_surface_v1_set_anchor(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
			zwlr_layer_surface_v1_set_margin(K.layer_surface,
							 K.cfg.margin_y, 0, 0,
							 K.cfg.margin_x);
		}
		zwlr_layer_surface_v1_set_size(K.layer_surface,
					       (uint32_t)(cols * kcell_w()),
					       (uint32_t)(rows * kcell_h()));
	} else {
		zwlr_layer_surface_v1_set_anchor(K.layer_surface, ANCHOR[K.cfg.edge]);
		zwlr_layer_surface_v1_set_size(K.layer_surface,
					       vertical ? 0 : (uint32_t)thickness,
					       vertical ? (uint32_t)thickness : 0);
		/*
		 * The exclusive zone is what makes a panel a panel rather than
		 * something floating over the windows: it tells the compositor
		 * to keep that strip out of every other surface's area.
		 */
		if (K.cfg.exclusive)
			zwlr_layer_surface_v1_set_exclusive_zone(K.layer_surface,
								 thickness);
	}

	if (K.cfg.keyboard)
		zwlr_layer_surface_v1_set_keyboard_interactivity(
			K.layer_surface,
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);

	zwlr_layer_surface_v1_add_listener(K.layer_surface, &layer_listener, NULL);
	return 0;
}

/* ── session lock ──────────────────────────────────────────────────────── */

static void lock_locked(void *d, struct ext_session_lock_v1 *lock)
{
	(void)d;
	(void)lock;
	/* The compositor has confirmed it: the session is secured and nothing
	 * of it is on screen. Only now may a password be accepted. */
	K.lock_engaged = 1;
}

static void lock_finished(void *d, struct ext_session_lock_v1 *lock)
{
	(void)d;
	(void)lock;
	/*
	 * Refused — something else already holds the lock, or the request came
	 * too late. The client must exit WITHOUT unlocking: it never locked
	 * anything, and calling unlock_and_destroy here would be asking the
	 * compositor to release a lock somebody else is holding.
	 */
	K.lock_finished = 1;
	K.closed = 1;
}

static const struct ext_session_lock_v1_listener lock_listener = {
	.locked = lock_locked,
	.finished = lock_finished,
};

/* The cell-grid surface, on the first output. */
static void lock_surface_configure(void *d, struct ext_session_lock_surface_v1 *ls,
				   uint32_t serial, uint32_t w, uint32_t h)
{
	(void)d;
	ext_session_lock_surface_v1_ack_configure(ls, serial);
	if (w && h)
		resize_cells((int)w, (int)h);
	K.configured = 1;
}

static const struct ext_session_lock_surface_v1_listener lock_surface_listener = {
	.configure = lock_surface_configure,
};

/*
 * A covered-but-blank output. The whole content is one colour, so it is one
 * buffer painted once — the double buffering the cell grid needs is for a
 * surface that changes, and this one never does until it is resized.
 */
static void extra_paint(int i)
{
	if (K.extra[i].w <= 0 || K.extra[i].h <= 0)
		return;
	if (buffer_alloc(&K.extra[i].buf, K.extra[i].w, K.extra[i].h) < 0)
		return;

	pixman_color_t bg = kcell_slot_color(KT_BG);
	pixman_image_t *fill = pixman_image_create_solid_fill(&bg);
	if (fill) {
		pixman_image_composite32(PIXMAN_OP_SRC, fill, NULL,
					 K.extra[i].buf.img, 0, 0, 0, 0, 0, 0,
					 K.extra[i].w, K.extra[i].h);
		pixman_image_unref(fill);
	}
	wl_surface_attach(K.extra[i].surface, K.extra[i].buf.wl, 0, 0);
	wl_surface_damage_buffer(K.extra[i].surface, 0, 0, K.extra[i].w,
				 K.extra[i].h);
	wl_surface_commit(K.extra[i].surface);
}

static void extra_configure(void *d, struct ext_session_lock_surface_v1 *ls,
			    uint32_t serial, uint32_t w, uint32_t h)
{
	int i = (int)(intptr_t)d;
	ext_session_lock_surface_v1_ack_configure(ls, serial);
	K.extra[i].w = (int)w;
	K.extra[i].h = (int)h;
	extra_paint(i);
}

static const struct ext_session_lock_surface_v1_listener extra_listener = {
	.configure = extra_configure,
};

static int make_lock(void)
{
	if (!K.lock_mgr || K.noutputs < 1)
		return -1;

	K.lock = ext_session_lock_manager_v1_lock(K.lock_mgr);
	if (!K.lock)
		return -1;
	ext_session_lock_v1_add_listener(K.lock, &lock_listener, NULL);

	/* The first output carries the prompt: K.surface is the one libktui
	 * paints into. */
	struct ext_session_lock_surface_v1 *ls =
		ext_session_lock_v1_get_lock_surface(K.lock, K.surface,
						     K.outputs[0]);
	if (!ls)
		return -1;
	ext_session_lock_surface_v1_add_listener(ls, &lock_surface_listener, NULL);

	/*
	 * Every remaining output gets a surface too. Skipping them would leave
	 * `locked` unsent forever — the compositor is waiting for them, and a
	 * lock screen that never learns it is locked cannot safely accept a
	 * password.
	 */
	for (int i = 1; i < K.noutputs; i++) {
		int k = K.nextra;
		K.extra[k].surface = wl_compositor_create_surface(K.compositor);
		if (!K.extra[k].surface)
			continue;
		K.extra[k].lock_surface = ext_session_lock_v1_get_lock_surface(
			K.lock, K.extra[k].surface, K.outputs[i]);
		if (!K.extra[k].lock_surface) {
			wl_surface_destroy(K.extra[k].surface);
			K.extra[k].surface = NULL;
			continue;
		}
		ext_session_lock_surface_v1_add_listener(K.extra[k].lock_surface,
							&extra_listener,
							(void *)(intptr_t)k);
		K.nextra++;
	}
	return 0;
}

void kwl_unlock(void)
{
	if (!K.lock)
		return;
	/*
	 * unlock_and_destroy, not destroy. Destroying the lock object without
	 * unlocking is exactly what a crash looks like to the compositor, and
	 * the compositor's answer to that is to keep the screen locked — which
	 * is correct, and is why this is the only way out.
	 */
	if (K.lock_engaged)
		ext_session_lock_v1_unlock_and_destroy(K.lock);
	else
		ext_session_lock_v1_destroy(K.lock);
	K.lock = NULL;
	wl_display_flush(K.display);
}

static int make_toplevel(void)
{
	if (!K.wm_base)
		return -1;
	K.xdg_surface = xdg_wm_base_get_xdg_surface(K.wm_base, K.surface);
	if (!K.xdg_surface)
		return -1;
	xdg_surface_add_listener(K.xdg_surface, &xdg_surface_listener, NULL);
	K.xdg_toplevel = xdg_surface_get_toplevel(K.xdg_surface);
	if (!K.xdg_toplevel)
		return -1;
	xdg_toplevel_add_listener(K.xdg_toplevel, &xdg_toplevel_listener, NULL);
	if (K.cfg.title)
		xdg_toplevel_set_title(K.xdg_toplevel, K.cfg.title);
	/*
	 * The app_id must equal the .desktop file's id or the shell shows a
	 * second, unnamed icon beside the launcher — the exact bug `kdos appid`
	 * exists to catch. Ours is the one program that has no excuse.
	 */
	if (K.cfg.app_id)
		xdg_toplevel_set_app_id(K.xdg_toplevel, K.cfg.app_id);
	resize_cells(80 * kcell_w(), 24 * kcell_h());
	return 0;
}

int kwl_init(const KwlConfig *cfg)
{
	memset(&K, 0, sizeof(K));
	K.cfg = *cfg;

	kcell_set_transparent_bg(cfg->role == KWL_ROLE_BACKGROUND);
	/*
	 * THE CHROME FONT DEFAULT LIVES HERE, and it is Terminus at the
	 * console's own cell — the same default kdos-comp uses for the window
	 * frames. It used to fall through to libkcell's generic
	 * `monospace:size=11`, and the result was on the first live screenshot:
	 * 32px Turbo Vision frames around an 11px DejaVu panel, a bar nobody
	 * could read. One knob, all chrome — panel, menus, desk, pick, run,
	 * launcher, notifyd, osd, lock. foot is CONTENT, not chrome, and keeps
	 * its own 16px config.
	 */
	if (kcell_font_load(cfg->font && *cfg->font ? cfg->font
						    : "Terminus:pixelsize=32") != 0)
		return -1;

	K.display = wl_display_connect(NULL);
	if (!K.display)
		goto fail_font;

	K.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!K.xkb_ctx)
		goto fail_display;

	K.registry = wl_display_get_registry(K.display);
	wl_registry_add_listener(K.registry, &registry_listener, NULL);
	wl_display_roundtrip(K.display);	/* globals */
	wl_display_roundtrip(K.display);	/* and the seat's capabilities */

	if (!K.compositor || !K.shm)
		goto fail_xkb;

	if (cfg->role == KWL_ROLE_NONE) {
		/* No surface, no backend: the caller draws offscreen. The
		 * connection and the seat are still live, which is the whole
		 * point — a dump of a panel with no window list would be a
		 * picture of nothing. */
		return 0;
	}

	K.surface = wl_compositor_create_surface(K.compositor);
	if (!K.surface)
		goto fail_xkb;

	int rc;
	switch (cfg->role) {
	case KWL_ROLE_TOPLEVEL:
		rc = make_toplevel();
		break;
	case KWL_ROLE_LOCK:
		rc = make_lock();
		break;
	default:
		rc = make_panel();
		break;
	}
	if (rc != 0)
		goto fail_xkb;

	/*
	 * Layer-shell and xdg-shell need an empty commit to ask for the first
	 * configure. A SESSION LOCK surface must not get one: the protocol makes
	 * committing a null buffer a fatal error, and the compositor kills the
	 * client for it —
	 *
	 *   ext_session_lock_surface_v1#12: error 1: session lock surface is
	 *   committed with a null buffer
	 *
	 * which, because the compositor is right to keep the session locked when
	 * the lock client dies, leaves a machine locked with no prompt on it.
	 * The lock surface is configured the moment it is created, so there is
	 * nothing to ask for.
	 */
	if (cfg->role != KWL_ROLE_LOCK)
		wl_surface_commit(K.surface);
	/* Wait for the first configure: the surface has no size until the
	 * compositor gives it one, and painting before that is painting into a
	 * buffer whose dimensions are a guess. */
	while (!K.configured && !K.closed)
		if (wl_display_dispatch(K.display) < 0)
			goto fail_xkb;

	ktui_backend_set(&kwl_backend);
	return 0;

fail_xkb:
	if (K.xkb_ctx)
		xkb_context_unref(K.xkb_ctx);
fail_display:
	wl_display_disconnect(K.display);
	K.display = NULL;
fail_font:
	kcell_font_free();
	return -1;
}

void kwl_overlay_resize(int cols, int rows)
{
	if (K.cfg.role != KWL_ROLE_OVERLAY || !K.layer_surface)
		return;
	if (cols < 1)
		cols = 1;
	if (rows < 1)
		rows = 1;
	zwlr_layer_surface_v1_set_size(K.layer_surface,
				       (uint32_t)(cols * kcell_w()),
				       (uint32_t)(rows * kcell_h()));
	/* The commit is what makes the compositor send a configure back; without
	 * it the request sits in the queue and nothing on screen changes. */
	wl_surface_commit(K.surface);
}

/*
 * Overlay only: destroy the layer surface instead of shrinking to one cell.
 *
 * layer-shell's own "hide" is a commit with a NULL buffer, and wlroots
 * answers that by resetting the surface to uninitialised — the next paint
 * then waits for a configure that only an initial-commit handshake will
 * produce, and the first toast after an idle period is accepted, given an
 * id, and drawn nowhere. Measured, in kdos-notifyd, which shrank to one
 * dark cell as a workaround. Destroying and recreating the surface is the
 * real fix: the connection, the seat, the font cache and the backend all
 * stay; only the surface goes.
 */
void kwl_overlay_hide(void)
{
	if (K.cfg.role != KWL_ROLE_OVERLAY || !K.layer_surface)
		return;
	zwlr_layer_surface_v1_destroy(K.layer_surface);
	K.layer_surface = NULL;
	wl_surface_destroy(K.surface);
	K.surface = NULL;
	buffer_free(&K.buf[0]);
	buffer_free(&K.buf[1]);
	K.configured = 0;
	K.px_w = 0;
	K.px_h = 0;
	wl_display_flush(K.display);
}

int kwl_overlay_show(int cols, int rows)
{
	if (K.cfg.role != KWL_ROLE_OVERLAY)
		return -1;
	if (K.surface) {
		kwl_overlay_resize(cols, rows);
		return 0;
	}
	if (cols < 1)
		cols = 1;
	if (rows < 1)
		rows = 1;
	K.cfg.cols = cols;
	K.cfg.rows = rows;
	K.surface = wl_compositor_create_surface(K.compositor);
	if (!K.surface)
		return -1;
	if (make_panel() != 0) {
		wl_surface_destroy(K.surface);
		K.surface = NULL;
		return -1;
	}
	/* the initial-commit handshake, same as kwl_init's */
	wl_surface_commit(K.surface);
	while (!K.configured && !K.closed)
		if (wl_display_dispatch(K.display) < 0)
			return -1;
	/* the recreated surface starts blank: force the next draw through */
	ktui_resized = 1;
	return 0;
}

void kwl_shutdown(void)
{
	if (K.cfg.role != KWL_ROLE_NONE)
		ktui_backend_set(NULL);
	buffer_free(&K.buf[0]);
	buffer_free(&K.buf[1]);
	for (int i = 0; i < K.nextra; i++) {
		buffer_free(&K.extra[i].buf);
		if (K.extra[i].lock_surface)
			ext_session_lock_surface_v1_destroy(K.extra[i].lock_surface);
		if (K.extra[i].surface)
			wl_surface_destroy(K.extra[i].surface);
	}
	if (K.xkb_state)
		xkb_state_unref(K.xkb_state);
	if (K.keymap)
		xkb_keymap_unref(K.keymap);
	if (K.xkb_ctx)
		xkb_context_unref(K.xkb_ctx);
	if (K.display) {
		wl_display_flush(K.display);
		wl_display_disconnect(K.display);
	}
	kcell_font_free();
	memset(&K, 0, sizeof(K));
}
