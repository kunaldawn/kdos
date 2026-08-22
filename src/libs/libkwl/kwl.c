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
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
/* Dead keys and Compose sequences. A separate header in xkbcommon, and
 * without it every xkb_compose_* call below is an implicit declaration —
 * which under this tree's -Werror is a kdos-shell that does not build. */
#include <xkbcommon/xkbcommon-compose.h>

#include "ext-session-lock-v1-client-protocol.h"
#include "kwl_priv.h"
#include "cursor-shape-v1-client-protocol.h"
#include "primary-selection-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

/*
 * Every output gets a lock surface, because the protocol will not report the
 * session locked until they all have one. Eight is a ceiling on physical
 * monitors, not on ambition.
 */
#define KWL_MAX_OUTPUTS 8
#define KWL_OUTPUT_NAME_MAX 64

/*
 * The event QUEUE, and why one slot was not enough.
 *
 * A single `pending` overwritten by the newest event is right for motion and
 * wrong for everything else: libwayland delivers a whole batch of listener
 * callbacks from one read, so a button PRESS followed in the same batch by the
 * motion that inevitably accompanies it was dropped before any consumer saw
 * it. Clicks went missing under a moving hand, which is every click.
 *
 * Sixteen is a batch's worth. Consecutive motion still collapses — losing an
 * intermediate position is invisible, and coalescing is what keeps a fast drag
 * from filling the ring with stale coordinates.
 */
#define KWL_EVQ 16

/* Key repeat, when the compositor sends no repeat_info (or an older one). */
#define KWL_REPEAT_DELAY_MS 400
#define KWL_REPEAT_RATE_HZ  25

/*
 * Paste is capped, and the cap is a policy, not a buffer size that happened:
 * these are single-line text fields (a filename, a command, a password) and a
 * 64K paste into one of them is a mistake, not a use case.
 */
#define KWL_PASTE_MAX     65536
#define KWL_PASTE_TIMEOUT_MS 2000	/* a source that never finishes */

/*
 * A frame callback the compositor never answers — the surface is occluded, or
 * the output is off — must not freeze the surface forever: past this, commit
 * anyway and let the compositor throttle us its own way.
 */
#define KWL_FRAME_STALL_MS 100

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
	int cursor;		/* enum kwl_cursor, re-sent on every enter */
	uint32_t ptr_serial;	/* the pointer-enter serial set_shape needs */

	/*
	 * Paste, and only paste. The selection offers are tracked so Ctrl+V and
	 * middle-click can receive text/plain; setting the selection and
	 * drag-and-drop are future work — a source needs data to keep serving
	 * after the widget that owned it is gone, which is a lifetime this
	 * library does not hold yet.
	 */
	struct wl_data_device_manager *data_mgr;
	struct wl_data_device *data_dev;
	struct zwp_primary_selection_device_manager_v1 *primary_mgr;
	struct zwp_primary_selection_device_v1 *primary_dev;
	struct wl_data_offer *sel_offer;	/* the clipboard, or NULL   */
	struct zwp_primary_selection_offer_v1 *prim_offer;
	struct wl_data_offer *drag_offer;	/* held only to destroy it  */
	int paste_fd;				/* in-flight receive, or -1 */
	char *paste_buf;
	size_t paste_len;
	int64_t paste_deadline;

	/* The last serial an input event carried: what wl_data_device
	 * set_selection (and start_drag, one day) must present. */
	uint32_t input_serial;

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
	int output_scale[KWL_MAX_OUTPUTS];
	/* The current mode, in physical pixels — divided by the scale it is
	 * the logical box an overlay has to fit inside. See place_clamp(). */
	int output_w[KWL_MAX_OUTPUTS];
	int output_h[KWL_MAX_OUTPUTS];
	/* The registry id each slot was bound from: without it a global_remove
	 * cannot be matched back to an output, and an unplugged screen is left
	 * in the table for named_output() and make_lock() to hand a destroyed
	 * proxy to — an "invalid object" error, which kills the client. A
	 * removed slot is emptied rather than compacted: the output listener
	 * carries its index as user data, and the next bind reuses it. */
	uint32_t output_id[KWL_MAX_OUTPUTS];
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
	/*
	 * SERVER-SIDE DECORATION, asked for explicitly. A toplevel that never
	 * binds this protocol has not said which side draws its frame, and it
	 * gets whatever the compositor guesses — which here was no frame at
	 * all: no titlebar to drag, no close button, and the window manager's
	 * own Close/Maximize unreachable by pointer. Every other surface in
	 * this library is a layer or a lock surface and has no decoration to
	 * negotiate, so this is the toplevel path's alone.
	 */
	struct zxdg_decoration_manager_v1 *deco_mgr;
	struct zxdg_toplevel_decoration_v1 *deco;
	struct zwlr_layer_surface_v1 *layer_surface;
	/*
	 * What the compositor offered for zwlr_layer_shell_v1. It matters:
	 * KEYBOARD_INTERACTIVITY_ON_DEMAND arrived in version 4, and wlroots
	 * answers the request on an older resource with `!!interactive` — so a
	 * client bound at version 1 asking for ON_DEMAND gets EXCLUSIVE. That
	 * is not a cosmetic downgrade: labwc then parks the seat's keyboard on
	 * that layer surface and `seat_focus()` refuses every later window
	 * focus, so with desktop icons on (the default) NOTHING TYPED REACHED
	 * ANY WINDOW until an overlay happened to take the focus and give it
	 * back.
	 */
	int layer_version;
	/* Panel autohide: non-zero while the panel is collapsed to its edge
	 * strip with no exclusive zone. Kept here rather than in the caller so
	 * a repeated request is free. */
	int autohidden;

	struct xkb_context *xkb_ctx;
	struct xkb_keymap *keymap;
	/* Dead keys. NULL on a machine with no Compose file, which is the old
	 * behaviour exactly. */
	struct xkb_compose_table *compose_table;
	struct xkb_compose_state *compose_state;
	struct xkb_state *xkb_state;

	KwlBuffer buf[2];
	int cur_buf;
	/*
	 * What the compositor is SHOWING, cell for cell — the damage diff's
	 * baseline. It cannot be either buffer's shadow (the front buffer
	 * changes hands every commit) and it cannot be libktui's own front
	 * buffer (a throttled commit lands after the flush that carried it,
	 * when that pointer may be gone).
	 */
	KtuiCell *screen;
	int screen_w, screen_h;

	/*
	 * Frame-callback throttling. While `frame_cb` is outstanding, a flush
	 * snapshots its cells into `pend` instead of committing; the callback
	 * commits the newest snapshot. Sustained change — a drag-hover, the
	 * OSD's volume ramp — then commits at display cadence rather than at
	 * event cadence.
	 */
	struct wl_callback *frame_cb;
	int64_t frame_at_ms;
	KtuiCell *pend;
	int pend_w, pend_h, pend_full, pend_valid;

	/*
	 * HiDPI: the integer scale of the output the surface is on, clamped to
	 * KCELL_MAX_SCALE. Layer-surface sizes stay in logical pixels; the shm
	 * buffer is px * scale and the glyphs are rendered at the scale rather
	 * than stretched. `scale_sent` trails it so set_buffer_scale is always
	 * committed together with a buffer of the matching size.
	 */
	int scale, scale_sent;
	int on_output;		/* index of the output last entered, or -1 */

	KwlConfig cfg;
	int px_w, px_h;		/* surface size in LOGICAL pixels          */
	/*
	 * The frame rule, in logical pixels, 0 for none. It sits on the edge
	 * the panel does NOT touch — under a top-anchored bar and above a
	 * bottom-anchored one — because the other three edges are the screen's
	 * and a line drawn against one of those is a line nobody sees.
	 */
	int rule;
	int rule_bottom;
	int cols, rows;		/* and in cells                            */
	int configured;
	int closed;
	int kb_entered;		/* the keyboard has been here at least once */

	/* Filled by the wl listeners, drained by poll_event. See KWL_EVQ. */
	KtuiEvent q[KWL_EVQ];
	int qhead, qtail;
	int ptr_cx, ptr_cy;
	/* The last cell a MOTION was reported for — see pt_motion. Seeded off
	 * the grid so an enter and the first motion after one always report. */
	int move_cx, move_cy;

	/*
	 * The wheel, accumulated rather than forwarded event for event.
	 * wl_pointer.axis carries a CONTINUOUS value: a mouse notch is one
	 * large one, a touchpad gesture is a stream of small ones, and treating
	 * each as a tick made two fingers on a touchpad scroll a list by
	 * hundreds of rows. `discrete` is used when the compositor sends it,
	 * because that is the notch count measured at the source.
	 */
	double axis_acc;
	int axis_disc;
	/* wl_pointer.axis_source, which decides what the accumulator below is
	 * FOR — see pt_frame. */
	uint32_t axis_src;
	int axis_src_seen;
	/* The last tick this client emitted, for the duplicate gate — see
	 * wheel_gate(). */
	int64_t wheel_last_ms;
	int wheel_last_up;

	/*
	 * Key repeat. Wayland has no repeat of its own — the compositor sends
	 * repeat_info and every client is expected to do the repeating, which
	 * is why holding an arrow key did nothing in the launcher, the menu,
	 * the file chooser, the run box and the desktop.
	 */
	int32_t rep_rate, rep_delay;
	uint32_t rep_code;		/* the key held, or 0 for none */
	KtuiEvent rep_ev;
	int64_t rep_at_ms;
} K;

static int64_t now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

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

static void paste_pump(void);
static void send_pump(void);

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
	/* An in-flight paste rides this loop too: the read is non-blocking and
	 * kwl_pump's callers call it on every turn. So does an in-flight COPY —
	 * a selection somebody is reading out of us. */
	paste_pump();
	send_pump();
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

/*
 * The SURFACE's own height in logical pixels — the cells plus the rule.
 *
 * A panel that anchors a popup just above itself has to pass its own
 * thickness as the margin, and `rows * cell_h` stopped being that the moment
 * the bar grew a rule outside the grid: every popup would have sat three
 * pixels low and covered the line it was meant to clear.
 */
/*
 * IS SOMEBODY ELSE DRAWING THIS WINDOW'S FRAME?
 *
 * A toplevel gets the compositor's server-side decoration — the same
 * `════ Title ════[_][=][X]` every alien app wears — so a program that also
 * drew its own box would be wearing two, one inside the other, with its title
 * written twice. A popup, a panel and a terminal have no such frame and must
 * draw their own or they have none at all.
 *
 * The question is asked of the SURFACE rather than answered from a flag the
 * caller keeps, because the caller does not always know: the same program is a
 * popup when the panel opens it and a window when it is typed by name.
 */
int kwl_decorated(void)
{
	return K.cfg.role == KWL_ROLE_TOPLEVEL && K.deco != NULL;
}

int kwl_px_h(void)
{
	return K.px_h > 0 ? K.px_h : K.rows * kcell_h() + K.rule;
}
/* The integer output scale this surface is being rendered at. A consumer that
 * rasterises anything of its own — libkicon is the one — has to do it at
 * cell * scale, or a HiDPI panel gets a picture upscaled from half its size. */
int kwl_scale(void) { return K.scale > 0 ? K.scale : 1; }

static int ev_is_motion(const KtuiEvent *ev)
{
	return ev->type == KT_EVT_MOUSE && ev->press == KT_MP_DRAG;
}

static void push_event(const KtuiEvent *ev)
{
	int next = (K.qtail + 1) % KWL_EVQ;

	/*
	 * Motion collapses onto motion: a drag produces one event per pointer
	 * sample and only the newest position means anything. A button or a key
	 * NEVER overwrites anything — that was the bug this queue exists for.
	 */
	if (ev_is_motion(ev) && K.qtail != K.qhead) {
		int last = (K.qtail + KWL_EVQ - 1) % KWL_EVQ;
		if (ev_is_motion(&K.q[last])) {
			K.q[last] = *ev;
			return;
		}
	}
	if (next == K.qhead) {
		/* Full: drop the OLDEST. A consumer that has fallen this far
		 * behind wants the newest click, not a click from a frame ago. */
		K.qhead = (K.qhead + 1) % KWL_EVQ;
	}
	K.q[K.qtail] = *ev;
	K.qtail = next;
}

static int pop_event(KtuiEvent *ev)
{
	if (K.qhead == K.qtail)
		return 0;
	*ev = K.q[K.qhead];
	K.qhead = (K.qhead + 1) % KWL_EVQ;
	return 1;
}

/*
 * The modifiers as xkb holds them NOW. One reading, shared by the key and the
 * pointer handlers: the tty backend's SGR decoding fills ev.mods for mouse
 * events, and the two backends must not disagree about the event vocabulary —
 * shift-click was invisible to every chrome surface here.
 */
static int mods_now(void)
{
	int m = 0;
	if (!K.xkb_state)
		return 0;
	if (xkb_state_mod_name_is_active(K.xkb_state, XKB_MOD_NAME_CTRL,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_CTRL;
	if (xkb_state_mod_name_is_active(K.xkb_state, XKB_MOD_NAME_ALT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_ALT;
	if (xkb_state_mod_name_is_active(K.xkb_state, XKB_MOD_NAME_SHIFT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_SHIFT;
	return m;
}

/* The held key's next repeat, if it is due. */
static int repeat_due(KtuiEvent *ev)
{
	if (!K.rep_code || now_ms() < K.rep_at_ms)
		return 0;
	int hz = K.rep_rate > 0 ? K.rep_rate : KWL_REPEAT_RATE_HZ;
	K.rep_at_ms = now_ms() + 1000 / hz;
	*ev = K.rep_ev;
	/* The modifiers as held NOW, not as they were at the press: a Shift
	 * released mid-repeat must stop extending the selection. */
	ev->mods = mods_now();
	return 1;
}

/* ── paste ─────────────────────────────────────────────────────────────── */

/*
 * The mimes worth receiving, best first. The rank survives on the offer's
 * user data (index + 1, 0 for "nothing textual"), because the mime events
 * arrive long before anybody presses Ctrl+V and the strings themselves are
 * gone by then.
 */
static const char *const paste_mime[] = {
	"text/plain;charset=utf-8",
	"UTF8_STRING",
	"text/plain",
};

static void offer_rank(struct wl_proxy *o, const char *mime)
{
	intptr_t cur = (intptr_t)wl_proxy_get_user_data(o);
	for (int i = 0; i < (int)(sizeof(paste_mime) / sizeof(*paste_mime)); i++)
		if (!strcmp(mime, paste_mime[i])) {
			if (!cur || (intptr_t)i + 1 < cur)
				wl_proxy_set_user_data(o, (void *)(intptr_t)(i + 1));
			return;
		}
}

/*
 * Drain the receive pipe; on EOF hand the text to libktui. Non-blocking, and
 * pumped from BOTH event loops (kwl_pump and the backend's poll_event),
 * because a paste started in the file chooser arrives through poll_event and
 * one started while kdos-notifyd is idle arrives through kwl_pump.
 */
static void paste_pump(void)
{
	if (K.paste_fd < 0)
		return;
	for (;;) {
		ssize_t r = read(K.paste_fd, K.paste_buf + K.paste_len,
				 KWL_PASTE_MAX - K.paste_len);
		if (r > 0) {
			K.paste_len += (size_t)r;
			if (K.paste_len >= KWL_PASTE_MAX)
				break;	/* the cap: keep what fits */
			continue;
		}
		if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			if (now_ms() < K.paste_deadline)
				return;	/* still coming; poll again */
			break;		/* a source that never finishes */
		}
		break;			/* EOF, or an error: done */
	}
	close(K.paste_fd);
	K.paste_fd = -1;
	if (K.paste_len)
		ktui_paste_push(K.paste_buf, K.paste_len);
	K.paste_len = 0;
}

static void paste_start(int primary)
{
	int rank;

	if (K.paste_fd >= 0)
		return;			/* one at a time */
	if (primary) {
		if (!K.prim_offer)
			return;
		rank = (int)(intptr_t)wl_proxy_get_user_data(
			(struct wl_proxy *)K.prim_offer);
	} else {
		if (!K.sel_offer)
			return;
		rank = (int)(intptr_t)wl_proxy_get_user_data(
			(struct wl_proxy *)K.sel_offer);
	}
	if (!rank)
		return;			/* nothing textual on offer */
	if (!K.paste_buf) {
		K.paste_buf = malloc(KWL_PASTE_MAX);
		if (!K.paste_buf)
			return;
	}

	int fds[2];
	if (pipe2(fds, O_CLOEXEC) < 0)
		return;
	if (primary)
		zwp_primary_selection_offer_v1_receive(
			K.prim_offer, paste_mime[rank - 1], fds[1]);
	else
		wl_data_offer_receive(K.sel_offer, paste_mime[rank - 1], fds[1]);
	/* The write end is the SOURCE's now; holding our copy open turns its
	 * close-at-EOF into a pipe that never reports EOF here. */
	close(fds[1]);
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	K.paste_fd = fds[0];
	K.paste_len = 0;
	K.paste_deadline = now_ms() + KWL_PASTE_TIMEOUT_MS;
	wl_display_flush(K.display);
}

/* ── copy ────────────────────────────────────────────────────────────────
 *
 * The half libkwl did not have. Every surface here could PASTE and nothing on
 * this desktop could put anything on the clipboard — not a file name from the
 * chooser, not a line from kdos-doc, not an SSID from the network manager, not
 * an error from a toast. Every one of those is a thing a person copies.
 *
 * THE SEND MUST NOT BLOCK THE FRAME. A data source is handed a pipe and the
 * receiving client may not read it for a while; a blocking write of a large
 * selection into a full pipe stops the panel. So the fd goes non-blocking, one
 * write is attempted immediately (which finishes it for anything that fits a
 * pipe buffer — every clipboard payload this desktop produces), and a partial
 * write is parked and drained from the pump. A send that never drains is
 * dropped on a deadline rather than held forever.
 *
 * BOTH SELECTIONS, because this desktop has both: wl_data_device is Ctrl+C and
 * the primary selection is the middle-click paste foot and mc expect.
 */
#define KWL_COPY_SENDS 4
#define KWL_COPY_TIMEOUT_MS 4000

struct kwl_send {
	int fd;
	size_t off;
	int64_t deadline;
};

static char *copy_text;
static size_t copy_len;
static struct wl_data_source *copy_src;
static struct zwp_primary_selection_source_v1 *copy_prim;
static struct kwl_send copy_send[KWL_COPY_SENDS];

static void send_pump(void)
{
	for (int i = 0; i < KWL_COPY_SENDS; i++) {
		struct kwl_send *t = &copy_send[i];
		if (t->fd < 0)
			continue;
		while (t->off < copy_len) {
			ssize_t w = write(t->fd, copy_text + t->off,
					  copy_len - t->off);
			if (w > 0) {
				t->off += (size_t)w;
				continue;
			}
			if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				if (now_ms() < t->deadline)
					goto next;	/* still draining */
			}
			break;			/* EPIPE, or gave up */
		}
		close(t->fd);
		t->fd = -1;
next:
		;
	}
}

static void send_start(int fd)
{
	if (!copy_text || !copy_len) {
		close(fd);
		return;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);
	for (int i = 0; i < KWL_COPY_SENDS; i++) {
		if (copy_send[i].fd >= 0)
			continue;
		copy_send[i].fd = fd;
		copy_send[i].off = 0;
		copy_send[i].deadline = now_ms() + KWL_COPY_TIMEOUT_MS;
		send_pump();
		return;
	}
	/* Four consumers reading one selection at once is not a thing that
	 * happens; refusing the fifth beats growing an unbounded table. */
	close(fd);
}

static void src_target(void *d, struct wl_data_source *src, const char *mime)
{
	(void)d;
	(void)src;
	(void)mime;
}

static void src_send(void *d, struct wl_data_source *src, const char *mime,
		     int32_t fd)
{
	(void)d;
	(void)src;
	(void)mime;
	send_start(fd);
}

/*
 * Somebody else took the selection. The source is destroyed here and NOT in
 * kwl_copy: destroying it at set time would cancel the selection we just made.
 */
static void src_cancelled(void *d, struct wl_data_source *src)
{
	(void)d;
	if (src == copy_src) {
		wl_data_source_destroy(copy_src);
		copy_src = NULL;
	}
}

static const struct wl_data_source_listener data_source_listener = {
	.target = src_target,
	.send = src_send,
	.cancelled = src_cancelled,
};

static void psrc_send(void *d, struct zwp_primary_selection_source_v1 *src,
		      const char *mime, int32_t fd)
{
	(void)d;
	(void)src;
	(void)mime;
	send_start(fd);
}

static void psrc_cancelled(void *d,
			   struct zwp_primary_selection_source_v1 *src)
{
	(void)d;
	if (src == copy_prim) {
		zwp_primary_selection_source_v1_destroy(copy_prim);
		copy_prim = NULL;
	}
}

static const struct zwp_primary_selection_source_v1_listener
		primary_source_listener = {
	.send = psrc_send,
	.cancelled = psrc_cancelled,
};

int kwl_copy(const char *text, size_t len, int primary)
{
	static const char *const MIMES[] = {
		"text/plain;charset=utf-8", "text/plain", "TEXT", "STRING",
		"UTF8_STRING",
	};

	if (!text || !len)
		return -1;
	/*
	 * A serial is REQUIRED: set_selection presents the serial of the input
	 * event that justified it, and a compositor refuses one it has never
	 * seen. A surface that has not been clicked or typed into cannot copy,
	 * which is the protocol saying that a background client may not take
	 * the clipboard.
	 */
	if (!K.input_serial)
		return -1;

	char *copy = malloc(len);
	if (!copy)
		return -1;
	memcpy(copy, text, len);
	free(copy_text);
	copy_text = copy;
	copy_len = len;

	if (primary) {
		if (!K.primary_mgr || !K.primary_dev)
			return -1;
		if (copy_prim)
			zwp_primary_selection_source_v1_destroy(copy_prim);
		copy_prim = zwp_primary_selection_device_manager_v1_create_source(
			K.primary_mgr);
		if (!copy_prim)
			return -1;
		zwp_primary_selection_source_v1_add_listener(
			copy_prim, &primary_source_listener, NULL);
		for (size_t i = 0; i < sizeof(MIMES) / sizeof(MIMES[0]); i++)
			zwp_primary_selection_source_v1_offer(copy_prim,
							      MIMES[i]);
		zwp_primary_selection_device_v1_set_selection(K.primary_dev,
							      copy_prim,
							      K.input_serial);
	} else {
		if (!K.data_mgr || !K.data_dev)
			return -1;
		if (copy_src)
			wl_data_source_destroy(copy_src);
		copy_src = wl_data_device_manager_create_data_source(K.data_mgr);
		if (!copy_src)
			return -1;
		wl_data_source_add_listener(copy_src, &data_source_listener,
					    NULL);
		for (size_t i = 0; i < sizeof(MIMES) / sizeof(MIMES[0]); i++)
			wl_data_source_offer(copy_src, MIMES[i]);
		wl_data_device_set_selection(K.data_dev, copy_src,
					     K.input_serial);
	}
	wl_display_flush(K.display);
	return 0;
}

static void doff_offer(void *d, struct wl_data_offer *o, const char *mime)
{
	(void)d;
	offer_rank((struct wl_proxy *)o, mime);
}

static const struct wl_data_offer_listener data_offer_listener = {
	.offer = doff_offer,
};

static void dd_data_offer(void *d, struct wl_data_device *dev,
			  struct wl_data_offer *o)
{
	(void)d;
	(void)dev;
	wl_data_offer_add_listener(o, &data_offer_listener, NULL);
}

/* No drag-and-drop: the offer a drag announces is held only so it can be
 * destroyed when the drag ends, as the protocol requires. */
static void dd_enter(void *d, struct wl_data_device *dev, uint32_t serial,
		     struct wl_surface *sf, wl_fixed_t x, wl_fixed_t y,
		     struct wl_data_offer *o)
{
	(void)d; (void)dev; (void)serial; (void)sf; (void)x; (void)y;
	K.drag_offer = o;
}
static void dd_leave(void *d, struct wl_data_device *dev)
{
	(void)d;
	(void)dev;
	if (K.drag_offer) {
		wl_data_offer_destroy(K.drag_offer);
		K.drag_offer = NULL;
	}
}
static void dd_motion(void *d, struct wl_data_device *dev, uint32_t time,
		      wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)dev; (void)time; (void)x; (void)y; }
static void dd_drop(void *d, struct wl_data_device *dev)
{
	(void)d;
	(void)dev;
	if (K.drag_offer) {
		wl_data_offer_destroy(K.drag_offer);
		K.drag_offer = NULL;
	}
}

static void dd_selection(void *d, struct wl_data_device *dev,
			 struct wl_data_offer *o)
{
	(void)d;
	(void)dev;
	/* The previous offer is invalid the moment this event arrives; `o` is
	 * NULL when the selection was cleared. */
	if (K.sel_offer && K.sel_offer != o)
		wl_data_offer_destroy(K.sel_offer);
	K.sel_offer = o;
}

static const struct wl_data_device_listener data_device_listener = {
	.data_offer = dd_data_offer,
	.enter = dd_enter,
	.leave = dd_leave,
	.motion = dd_motion,
	.drop = dd_drop,
	.selection = dd_selection,
};

static void poff_offer(void *d, struct zwp_primary_selection_offer_v1 *o,
		       const char *mime)
{
	(void)d;
	offer_rank((struct wl_proxy *)o, mime);
}

static const struct zwp_primary_selection_offer_v1_listener primary_offer_listener = {
	.offer = poff_offer,
};

static void pd_data_offer(void *d, struct zwp_primary_selection_device_v1 *dev,
			  struct zwp_primary_selection_offer_v1 *o)
{
	(void)d;
	(void)dev;
	zwp_primary_selection_offer_v1_add_listener(o, &primary_offer_listener,
						    NULL);
}

static void pd_selection(void *d, struct zwp_primary_selection_device_v1 *dev,
			 struct zwp_primary_selection_offer_v1 *o)
{
	(void)d;
	(void)dev;
	if (K.prim_offer && K.prim_offer != o)
		zwp_primary_selection_offer_v1_destroy(K.prim_offer);
	K.prim_offer = o;
}

static const struct zwp_primary_selection_device_v1_listener primary_device_listener = {
	.data_offer = pd_data_offer,
	.selection = pd_selection,
};

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
	if (b->grid)
		pixman_image_unref(b->grid);
	if (b->img)
		pixman_image_unref(b->img);
	if (b->wl)
		wl_buffer_destroy(b->wl);
	if (b->data)
		munmap(b->data, b->size);
	free(b->shadow);
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
	/*
	 * THE GRID STARTS BELOW THE RULE, and a second pixman image over the
	 * SAME memory a few rows down is the whole of how. kcell_paint has no
	 * y origin — it puts row 0 at pixel 0 of whatever it is handed — and
	 * giving it one would mean an argument on a call every consumer of
	 * libkcell makes, for a thing exactly one surface wants. Zero rule and
	 * the two images are the same picture.
	 */
	int rule_px = K.rule * (K.scale > 0 ? K.scale : 1);
	if (rule_px > 0 && rule_px < h && !K.rule_bottom) {
		b->grid = pixman_image_create_bits(
			argb ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8, w,
			h - rule_px, (uint32_t *)data + (size_t)rule_px * w,
			(int)stride);
	}
	if (!b->grid)
		b->grid = pixman_image_ref(b->img);
	b->data = data;
	b->size = size;
	b->w = w;
	b->h = h;
	return 0;
}

/* ── the backend ───────────────────────────────────────────────────────── */

static void frame_done(void *d, struct wl_callback *cb, uint32_t t);

static const struct wl_callback_listener frame_listener = {
	.done = frame_done,
};

/*
 * Paint `cur` into a buffer and commit it. Two different diffs, against two
 * different baselines, and keeping them apart is the whole of S1:
 *
 * The DAMAGE is the diff against `K.screen` — what the compositor is showing —
 * because damage describes what changed ON SCREEN. This is also the flicker
 * fix: an unchanged frame is NOT COMMITTED at all. The panel redraws on a
 * one-second tick, the desk on its rescan — and every one of those used to
 * attach a fresh buffer and damage its full surface even when not a cell had
 * moved; on a virtio guest with no GL every commit is a full framebuffer
 * upload, so the desktop pulsed at the union of everyone's timers.
 *
 * The PAINT diffs against the buffer's OWN shadow, because commits alternate
 * buffers: the buffer being painted holds the frame before last, and a partial
 * paint measured against the frame on screen is how a cell grid grows stale
 * rows that never repair.
 */
static void flush_commit(const KtuiCell *cur, int w, int h, int full)
{
	size_t n = (size_t)w * h;

	if (!K.screen || K.screen_w != w || K.screen_h != h) {
		free(K.screen);
		K.screen = malloc(n * sizeof(KtuiCell));
		if (!K.screen) {
			K.screen_w = K.screen_h = 0;
			return;
		}
		K.screen_w = w;
		K.screen_h = h;
		full = 1;
	}

	int dirty_y0 = -1, dirty_y1 = -1;
	if (!full) {
		for (int y = 0; y < h; y++)
			if (memcmp(cur + (size_t)y * w, K.screen + (size_t)y * w,
				   (size_t)w * sizeof(*cur))) {
				if (dirty_y0 < 0)
					dirty_y0 = y;
				dirty_y1 = y;
			}
		if (dirty_y0 < 0)
			return;		/* nothing changed: no commit at all */
	}

	/*
	 * Two buffers, and the flip is not an optimisation. Painting into a
	 * buffer the compositor is still scanning out tears; blocking until it
	 * is released stalls the frame. With two, the common case never waits.
	 * Both busy means the compositor is behind; painting the one we hold
	 * anyway is the least-bad option, and the per-buffer shadow keeps the
	 * partial paint correct wherever it lands.
	 */
	KwlBuffer *b = &K.buf[K.cur_buf];
	if (b->busy) {
		K.cur_buf ^= 1;
		b = &K.buf[K.cur_buf];
	}
	/*
	 * A full repaint is full for BOTH buffers. `full` normally means the
	 * cell→pixel mapping changed under cells that did not — `kdos theme`
	 * SIGHUPs the panel and every slot moves without a cell moving — and
	 * the shadow records CELLS, so the buffer not painted here would take
	 * its next turn believing rows that still wear the old accent.
	 */
	if (full)
		K.buf[K.cur_buf ^ 1].stale = true;

	/*
	 * b->w/b->h rather than w*cell_w: the shm buffer is what the compositor
	 * will read, and any of it the grid does not reach must be KT_BG rather
	 * than whatever the last frame left. Sized at scale: the surface stays
	 * logical, the pixels are the output's own.
	 */
	int scale = K.scale > 0 ? K.scale : 1;
	int bw = K.px_w * scale, bh = K.px_h * scale;
	int bfull = full || b->stale;
	if (!b->img || b->w != bw || b->h != bh) {
		if (buffer_alloc(b, bw, bh) < 0)
			return;
		bfull = 1;
	}
	if (!b->shadow || b->scols != w || b->srows != h) {
		free(b->shadow);
		b->shadow = malloc(n * sizeof(KtuiCell));
		b->scols = w;
		b->srows = h;
		bfull = 1;
	}

	/* kcell_paint updates `prev` (the shadow) with what it painted; a NULL
	 * shadow — allocation failure — degrades to a full paint every frame. */
	int rule_px = K.rule * scale;
	kcell_paint(b->grid, cur, bfull ? NULL : b->shadow, w, h, bfull, scale,
		    b->w, b->h - rule_px);
	/*
	 * The rule itself, over the WHOLE width and on every paint: it is
	 * three pixels and it is outside the grid, so nothing in the cell diff
	 * would ever restore it.
	 */
	if (rule_px > 0) {
		/*
		 * `═`, NOT A BAR. The rule is the panel's own window edge and
		 * the compositor draws every other window's with the same
		 * mark — two thin lines with a gap, which is the cross-section
		 * of the double box-drawing character this whole desktop is
		 * framed in. A single solid band is a different picture from
		 * the one round every window on the screen, and side by side
		 * that is exactly what "the panel does not match" looks like.
		 *
		 * Two lines and the gap between them: `(rule - 1) / 2` each,
		 * so a 5px rule is 2/1/2 and a 3px one is 1/1/1 — the same
		 * weight the titlebar's own rule is drawn at, which is the
		 * point of matching it at all.
		 */
		KRgb rgb = ktui_theme->slot[K.cfg.rule_slot & 7];
		pixman_color_t c = { .red = (uint16_t)(rgb.r * 257),
				     .green = (uint16_t)(rgb.g * 257),
				     .blue = (uint16_t)(rgb.b * 257),
				     .alpha = 0xffff };
		int t = (rule_px - 1) / 2;
		int y0 = K.rule_bottom ? b->h - rule_px : 0;

		if (t < 1)
			t = 1;
		pixman_image_fill_rectangles(
			PIXMAN_OP_SRC, b->img, &c, 2,
			(pixman_rectangle16_t[]){
				{ 0, (int16_t)y0, (uint16_t)b->w,
				  (uint16_t)t },
				{ 0, (int16_t)(y0 + rule_px - t),
				  (uint16_t)b->w, (uint16_t)t } });
	}
	if (b->shadow && bfull)
		memcpy(b->shadow, cur, n * sizeof(KtuiCell));
	if (bfull)
		b->stale = false;
	memcpy(K.screen, cur, n * sizeof(KtuiCell));

	if (scale != K.scale_sent) {
		/* Sent here, not when the output changed: the scale and a
		 * buffer of the matching size must land in the SAME commit. */
		wl_surface_set_buffer_scale(K.surface, scale);
		K.scale_sent = scale;
	}
	wl_surface_attach(K.surface, b->wl, 0, 0);
	if (!full && dirty_y0 >= 0) {
		int ch = kcell_h() * scale;
		int off = K.rule_bottom ? 0 : rule_px;
		int y0 = off + dirty_y0 * ch;
		int y1 = off + (dirty_y1 + 1) * ch;
		if (y1 > bh)
			y1 = bh;
		wl_surface_damage_buffer(K.surface, 0, y0, bw, y1 - y0);
	} else {
		wl_surface_damage_buffer(K.surface, 0, 0, bw, bh);
	}
	K.frame_cb = wl_surface_frame(K.surface);
	if (K.frame_cb) {
		wl_callback_add_listener(K.frame_cb, &frame_listener, NULL);
		K.frame_at_ms = now_ms();
	}
	wl_surface_commit(K.surface);
	b->busy = true;
	K.cur_buf ^= 1;
	wl_display_flush(K.display);
}

static void frame_done(void *d, struct wl_callback *cb, uint32_t t)
{
	(void)d;
	(void)t;
	if (K.frame_cb == cb)
		K.frame_cb = NULL;
	wl_callback_destroy(cb);
	if (K.pend_valid && K.configured && K.surface) {
		K.pend_valid = 0;
		int full = K.pend_full;
		K.pend_full = 0;
		flush_commit(K.pend, K.pend_w, K.pend_h, full);
	}
}

static void kwl_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int full)
{
	/* The diff baseline is K.screen, not libktui's front buffer: a
	 * throttled commit happens after this call returns, when `prev` may
	 * describe a frame that was never presented — or be gone entirely. */
	(void)prev;

	/* !K.surface: an overlay hidden via kwl_overlay_hide() — drawing
	 * calls while hidden are no-ops, not errors. */
	if (!K.configured || !K.surface)
		return;

	/*
	 * Frame-callback throttling: while the compositor has not yet shown
	 * the last commit, snapshot the newest cells and commit them when it
	 * says it is ready. Motion-driven redraws then cost one commit per
	 * display frame instead of one per pointer sample.
	 */
	if (K.frame_cb && now_ms() - K.frame_at_ms < KWL_FRAME_STALL_MS) {
		size_t n = (size_t)w * h;
		if (!K.pend || K.pend_w != w || K.pend_h != h) {
			free(K.pend);
			K.pend = malloc(n * sizeof(KtuiCell));
			if (!K.pend) {
				K.pend_w = K.pend_h = 0;
				K.pend_valid = 0;
				return;
			}
			K.pend_w = w;
			K.pend_h = h;
			K.pend_full = 1;
		}
		memcpy(K.pend, cur, n * sizeof(KtuiCell));
		K.pend_full |= full;
		K.pend_valid = 1;
		return;
	}
	if (K.frame_cb) {
		/* See KWL_FRAME_STALL_MS: an unanswered callback must not
		 * freeze the surface for good. */
		wl_callback_destroy(K.frame_cb);
		K.frame_cb = NULL;
	}
	/*
	 * The stash is superseded by `cur`, which is newer by construction, so
	 * it is joined into this commit rather than left behind: keeping it
	 * would let the next frame callback republish a frame older than the
	 * one on screen, and dropping its `full` would lose a repaint the
	 * consumer has already forgotten about — ktui_draw_flush() clears
	 * force_full after ANY flush, throttled ones included.
	 */
	if (K.pend_valid) {
		full |= K.pend_full;
		K.pend_valid = 0;
		K.pend_full = 0;
	}
	flush_commit(cur, w, h, full);
}

static int kwl_poll_event(KtuiEvent *ev, int timeout_ms)
{
	/* The documented libwayland read sequence. Anything simpler races: two
	 * threads or a re-entrant dispatch can consume the socket between the
	 * poll and the read, and prepare_read/cancel_read is what makes that
	 * safe rather than occasionally hanging.
	 *
	 * The return value is not optional: after a fatal protocol error
	 * libwayland refuses to dispatch and leaves the queue non-empty, so
	 * prepare_read goes on answering EAGAIN and this loop never ends —
	 * 100% of a core, in the wait every front end sits in. */
	while (wl_display_prepare_read(K.display) != 0) {
		if (wl_display_dispatch_pending(K.display) < 0) {
			K.closed = 1;
			return 0;
		}
	}
	wl_display_flush(K.display);

	if (pop_event(ev)) {
		wl_display_cancel_read(K.display);
		return 1;
	}

	/*
	 * A held key is a deadline the caller does not know about, so the wait
	 * is shortened to it. Without this the repeat would fire at whatever
	 * cadence the consumer happened to poll at — one second, in every one
	 * of these loops.
	 */
	int wait = timeout_ms;
	if (K.rep_code) {
		int64_t rem = K.rep_at_ms - now_ms();
		if (rem < 0)
			rem = 0;
		if (wait < 0 || rem < wait)
			wait = (int)rem;
	}
	if (K.paste_fd >= 0) {
		/* An in-flight paste has its own deadline — a source that
		 * wedged must be abandoned even when no event ever comes. */
		int64_t rem = K.paste_deadline - now_ms();
		if (rem < 0)
			rem = 0;
		if (wait < 0 || rem < wait)
			wait = (int)rem;
	}

	struct pollfd pfd[2] = {
		{ .fd = wl_display_get_fd(K.display), .events = POLLIN },
		{ .fd = K.paste_fd, .events = POLLIN },
	};
	int n = poll(pfd, K.paste_fd >= 0 ? 2 : 1, wait);
	if (K.paste_fd >= 0 && (n <= 0 || pfd[1].revents))
		paste_pump();
	send_pump();
	if (n <= 0 || !(pfd[0].revents & (POLLIN | POLLHUP | POLLERR))) {
		wl_display_cancel_read(K.display);
		if (n < 0 && errno != EINTR)
			K.closed = 1;
		return repeat_due(ev);
	}
	if (wl_display_read_events(K.display) < 0) {
		K.closed = 1;
		return 0;
	}
	if (wl_display_dispatch_pending(K.display) < 0) {
		K.closed = 1;
		return 0;
	}
	if (K.paste_fd >= 0)
		paste_pump();

	if (pop_event(ev))
		return 1;
	return repeat_due(ev);
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

/* Defined below, beside the rest of the compose machine, and called from the
 * keymap handler above it — a keymap change is what invalidates the table. */
static void compose_init(void);

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
	compose_init();
}


/*
 * DEAD KEYS. Without this a compose sequence produces nothing at all: xkb
 * hands out `dead_acute` as a keysym with no text, `xkb_state_key_get_utf32`
 * answers 0, and libkwl dropped the event — so `Compose e '` typed an `e` and
 * then swallowed the quote, and a French or Czech layout could not write half
 * its own alphabet.
 *
 * The table is the locale's, from $XKB_DEFAULT_LAYOUT's Compose file by way of
 * $LC_CTYPE. A machine with no table at all keeps the old behaviour exactly —
 * `compose_state` stays NULL and every branch below is skipped.
 */
static void compose_init(void)
{
	const char *locale = getenv("LC_ALL");

	if (!locale || !*locale)
		locale = getenv("LC_CTYPE");
	if (!locale || !*locale)
		locale = getenv("LANG");
	if (!locale || !*locale)
		locale = "C.UTF-8";

	if (K.compose_state) {
		xkb_compose_state_unref(K.compose_state);
		K.compose_state = NULL;
	}
	if (K.compose_table) {
		xkb_compose_table_unref(K.compose_table);
		K.compose_table = NULL;
	}
	K.compose_table = xkb_compose_table_new_from_locale(
		K.xkb_ctx, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (K.compose_table)
		K.compose_state = xkb_compose_state_new(
			K.compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
}

/*
 * Feed one keysym through the compose machine.
 *
 * Returns 0 when the caller should carry on with `sym`, and 1 when the key was
 * SWALLOWED — either because a sequence is in progress or because it was
 * cancelled. A composed result replaces `*sym` in place.
 */
static int compose_feed(xkb_keysym_t *sym)
{
	if (!K.compose_state)
		return 0;
	if (xkb_compose_state_feed(K.compose_state, *sym) !=
	    XKB_COMPOSE_FEED_ACCEPTED)
		return 0;

	switch (xkb_compose_state_get_status(K.compose_state)) {
	case XKB_COMPOSE_COMPOSING:
		return 1;		/* mid-sequence: nothing to deliver */
	case XKB_COMPOSE_COMPOSED: {
		xkb_keysym_t out =
			xkb_compose_state_get_one_sym(K.compose_state);
		xkb_compose_state_reset(K.compose_state);
		if (out == XKB_KEY_NoSymbol)
			return 1;
		*sym = out;
		return 0;
	}
	case XKB_COMPOSE_CANCELLED:
		/* `Compose a a` is not a sequence: the keys are eaten and
		 * nothing is produced, which is what every other toolkit does
		 * and is less surprising than delivering the last one. */
		xkb_compose_state_reset(K.compose_state);
		return 1;
	case XKB_COMPOSE_NOTHING:
	default:
		return 0;
	}
}

static void kb_key(void *d, struct wl_keyboard *k, uint32_t serial,
		   uint32_t time, uint32_t key, uint32_t state)
{
	(void)d;
	(void)k;
	(void)time;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* The held key was let go: stop repeating it. Any OTHER key's
		 * release is not ours to act on — a chord ends when the key
		 * that is repeating ends. */
		if (K.rep_code == key + 8)
			K.rep_code = 0;
		return;
	}
	if (!K.xkb_state)
		return;
	K.input_serial = serial;

	xkb_keycode_t kc = key + 8;	/* evdev -> xkb */
	xkb_keysym_t sym = xkb_state_key_get_one_sym(K.xkb_state, kc);

	KtuiEvent ev = { .type = KT_EVT_KEY };
	ev.mods = mods_now();

	/*
	 * Compose FIRST, and the composed result is translated by keysym
	 * rather than by keycode: `Compose e '` produces XKB_KEY_eacute, which
	 * belongs to no key on the keyboard, so asking xkb what text that
	 * KEYCODE produces would answer `'` again.
	 */
	if (compose_feed(&sym)) {
		K.rep_code = 0;		/* a swallowed key does not repeat */
		return;
	}
	ev.key = kwl_keysym_to_ktui(sym, K.xkb_state, kc);
	if (!ev.key)
		return;			/* a bare modifier: nothing to repeat */
	/* Ctrl+V arrives from xkb as U+0016 — start receiving the clipboard.
	 * The key is still delivered: with nothing on the clipboard the
	 * consumer sees exactly what it always saw. */
	if (ev.key == 0x16 && K.data_dev)
		paste_start(0);
	push_event(&ev);

	/*
	 * Arm the repeat. `rate == 0` is the compositor saying repeat is
	 * disabled, and it must be honoured rather than defaulted over — as
	 * must the keymap's own per-key flag: a key the keymap marks
	 * non-repeating must not repeat here either.
	 */
	if (K.rep_rate == 0 && K.rep_delay > 0)
		return;
	if (K.keymap && !xkb_keymap_key_repeats(K.keymap, kc))
		return;
	K.rep_code = kc;
	K.rep_ev = ev;
	K.rep_at_ms = now_ms()
		    + (K.rep_delay > 0 ? K.rep_delay : KWL_REPEAT_DELAY_MS);
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
	(void)d; (void)k; (void)sf; (void)keys;
	K.input_serial = s;
	K.kb_entered = 1;
}
static void kb_leave(void *d, struct wl_keyboard *k, uint32_t s,
		     struct wl_surface *sf)
{
	(void)d; (void)k; (void)s; (void)sf;
	/* A key held when the focus went away is a key this client will never
	 * see released — repeating it would run until something else stopped
	 * it. */
	K.rep_code = 0;
	if (K.kb_entered && K.cfg.role == KWL_ROLE_OVERLAY &&
	    K.cfg.dismiss_on_unfocus)
		K.closed = 1;
}
static void kb_repeat(void *d, struct wl_keyboard *k, int32_t rate,
		      int32_t delay)
{
	(void)d;
	(void)k;
	/* Keys per second and milliseconds; rate 0 means "do not repeat". */
	K.rep_rate = rate;
	K.rep_delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = kb_keymap,
	.enter = kb_enter,
	.leave = kb_leave,
	.key = kb_key,
	.modifiers = kb_modifiers,
	.repeat_info = kb_repeat,
};

/*
 * A MOTION THAT DID NOT MOVE IS NOT A MOTION, and reporting one is how a menu
 * undoes what the wheel just did.
 *
 * Every consumer of this backend maps a motion to "which row is under the
 * pointer" and selects it, which is the whole of hover. A wheel notch on a
 * real machine does NOT arrive alone: an absolute pointing device (a tablet,
 * which is what every VM presents, and every touchpad's absolute mode) sends
 * the position again with the axis event, and the front end that turns one
 * host scroll into two sends it twice. So the sequence the consumer saw was
 * `wheel, motion(same place)` — it stepped the cursor and then instantly put
 * it back on the row under a pointer that had not moved a pixel. Reported as
 * "the highlight jumps back", and invisible from inside any one of the four
 * links in that chain.
 *
 * Cell granularity is the right resolution for it: hover IS cell-granular,
 * nothing above this reads sub-cell positions, and the one consumer that
 * tracks a continuous value (the volume slider's drag) reads the cell too.
 * The position is still recorded for the next click either way — only the
 * EVENT is dropped.
 */
static void pt_motion(void *d, struct wl_pointer *p, uint32_t time,
		      wl_fixed_t sx, wl_fixed_t sy)
{
	(void)d;
	(void)p;
	(void)time;
	int cw = kcell_w(), ch = kcell_h();
	if (cw <= 0 || ch <= 0)
		return;
	int cx = wl_fixed_to_int(sx) / cw;
	/* Below the rule when the rule is on top: the grid starts there, so a
	 * pointer on the rule itself is row -1 and hits nothing, which is what
	 * a border is. */
	int cy = (wl_fixed_to_int(sy) - (K.rule_bottom ? 0 : K.rule)) / ch;

	K.ptr_cx = cx;
	K.ptr_cy = cy;
	if (cx == K.move_cx && cy == K.move_cy)
		return;
	K.move_cx = cx;
	K.move_cy = cy;

	KtuiEvent ev = { .type = KT_EVT_MOUSE, .btn = KT_MB_MOVE,
			 .mx = K.ptr_cx, .my = K.ptr_cy, .press = KT_MP_DRAG,
			 .mods = mods_now() };
	push_event(&ev);
}

static void pt_button(void *d, struct wl_pointer *p, uint32_t serial,
		      uint32_t time, uint32_t button, uint32_t state)
{
	(void)d;
	(void)p;
	(void)time;
	K.input_serial = serial;
	KtuiEvent ev = { .type = KT_EVT_MOUSE, .mx = K.ptr_cx, .my = K.ptr_cy,
			 .mods = mods_now() };
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
	/* Middle-click pastes the primary selection, the X11 tradition every
	 * Wayland terminal keeps. The click is still delivered — with no
	 * focused text field the paste lands nowhere, and a consumer that
	 * means something else by middle (the window list closes) is
	 * unaffected. */
	if (ev.btn == KT_MB_MIDDLE && ev.press == KT_MP_PRESS && K.primary_dev)
		paste_start(1);
	push_event(&ev);
}

/*
 * One wheel tick, emitted from the frame handler rather than from the axis
 * event — see the `axis_acc` comment on the state block.
 */
/*
 * `KDOS_WHEEL_DEBUG=1` traces the axis events one physical notch produces.
 *
 * "The calendar moves two months per scroll" is a report about a chain with
 * four links in it — the emulated mouse, libinput, the compositor, and this —
 * and the only way to say which one doubles is to watch what arrives. Off by
 * default and one getenv at startup, so an instrumented build is the shipped
 * build.
 */
static int wheel_dbg(void)
{
	static int on = -1;

	if (on < 0) {
		const char *e = getenv("KDOS_WHEEL_DEBUG");
		on = e && *e && *e != '0';
	}
	return on;
}

/*
 * ONE PHYSICAL NOTCH IS ONE TICK, whatever the chain in front of us does.
 *
 * "The calendar moves two months per scroll" survived a correct reading of the
 * protocol here, and the reason is that this client is the LAST link of four:
 * the emulator, libinput, the compositor and this. Two of the three in front
 * are known to double a notch — QEMU's GTK display receives a smooth scroll
 * event AND the discrete one GTK emulates from it for legacy handlers, and
 * queues a wheel button for each; a device that reports both REL_WHEEL and
 * REL_WHEEL_HI_RES can do the same one layer lower. Both arrive as two
 * genuine, well-formed notches, so there is nothing left to read differently:
 * by the time they reach a Wayland client they are indistinguishable from a
 * scroll except by the CLOCK.
 *
 * So the gate is a rate limit and nothing cleverer: a second tick in the same
 * direction within KWL_WHEEL_MIN_MS of the last is a duplicate and is dropped.
 * Twenty milliseconds is fifty notches a second — several times what a hand
 * does with a detented wheel, and far more than the gap a doubling front end
 * leaves between its two copies, which come from ONE host event and are
 * queued back to back. A direction change is never a duplicate and is always
 * let through, so a reversal is instant, and a genuine multi-notch flick
 * measured inside one pointer frame is passed in full (see wheel_emit).
 *
 * The cost is stated rather than hidden: a free-spinning wheel spun hard can
 * outrun fifty notches a second, and on this desktop it will scroll at fifty.
 * `KDOS_WHEEL_MIN_MS` moves it and 0 turns it off — which, with
 * KDOS_WHEEL_DEBUG=1, is also how the raw chain gets measured on a machine
 * that has one.
 */
#define KWL_WHEEL_MIN_MS 20

static int wheel_gate(int up)
{
	static int min_ms = -1;
	int64_t now = now_ms();

	if (min_ms < 0) {
		const char *e = getenv("KDOS_WHEEL_MIN_MS");
		min_ms = e && *e ? atoi(e) : KWL_WHEEL_MIN_MS;
		if (min_ms < 0)
			min_ms = 0;
	}
	if (min_ms && K.wheel_last_ms && K.wheel_last_up == up &&
	    now - K.wheel_last_ms < min_ms) {
		if (wheel_dbg())
			fprintf(stderr, "kwl: wheel %s DROPPED (%lld ms since "
					"the last one)\n",
				up ? "up" : "down",
				(long long)(now - K.wheel_last_ms));
		return 0;
	}
	K.wheel_last_ms = now;
	K.wheel_last_up = up;
	return 1;
}

static void push_wheel_raw(int up)
{
	if (wheel_dbg())
		fprintf(stderr, "kwl: wheel %s\n", up ? "up" : "down");
	KtuiEvent ev = { .type = KT_EVT_MOUSE, .mx = K.ptr_cx, .my = K.ptr_cy,
			 .press = KT_MP_PRESS, .mods = mods_now(),
			 .btn = up ? KT_MB_WHEEL_UP : KT_MB_WHEEL_DOWN };
	push_event(&ev);
}

/*
 * `n` ticks from ONE pointer frame, past the duplicate gate — which is asked
 * once, because a frame is one gesture. The wheel path always passes 1 (see
 * pt_frame); the counted path is the touchpad's, where several ticks' worth of
 * accumulated delta in one frame is a real flick and dropping all but the
 * first would make a two-finger scroll crawl.
 */
static void wheel_emit(int up, int n)
{
	if (n < 1 || !wheel_gate(up))
		return;
	for (int i = 0; i < n; i++)
		push_wheel_raw(up);
}

static void pt_axis(void *d, struct wl_pointer *p, uint32_t time,
		    uint32_t axis, wl_fixed_t value)
{
	(void)d;
	(void)p;
	(void)time;
	if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
		return;
	K.axis_acc += wl_fixed_to_double(value);
	if (wheel_dbg())
		fprintf(stderr, "kwl: axis %+.2f (acc %+.2f)\n",
			wl_fixed_to_double(value), K.axis_acc);
}

static uint32_t shape_of(int cursor)
{
	switch (cursor) {
	case KWL_CUR_TEXT:	return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
	case KWL_CUR_POINTER:	return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
	case KWL_CUR_PROGRESS:	return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS;
	default:		return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
	}
}

static void pt_enter(void *d, struct wl_pointer *p, uint32_t serial,
		     struct wl_surface *sf, wl_fixed_t x, wl_fixed_t y)
{
	(void)d;
	(void)sf;
	/*
	 * The entry position is a real position: hover must be right and the
	 * first click must land where the pointer is, not at the (-1,-1) the
	 * last leave recorded. Reported the way pt_leave reports its own — a
	 * synthetic motion — so consumers learn it without a new vocabulary.
	 */
	int cw = kcell_w(), ch = kcell_h();
	if (cw > 0 && ch > 0) {
		K.ptr_cx = wl_fixed_to_int(x) / cw;
		K.ptr_cy = (wl_fixed_to_int(y) -
			    (K.rule_bottom ? 0 : K.rule)) / ch;
	}
	/* This IS the motion for that cell, so the dedup starts from here — an
	 * enter followed by a real move to the same cell is not two moves. */
	K.move_cx = K.ptr_cx;
	K.move_cy = K.ptr_cy;
	KtuiEvent ev = { .type = KT_EVT_MOUSE, .btn = KT_MB_MOVE,
			 .mx = K.ptr_cx, .my = K.ptr_cy, .press = KT_MP_DRAG,
			 .mods = mods_now() };
	push_event(&ev);
	/*
	 * The enter serial is the ONLY serial a set-cursor request may carry,
	 * which is why this lives here and not somewhere more convenient. A
	 * client that never answers the enter has no cursor at all — which is
	 * exactly what every libkwl surface did until the desktop grew a
	 * full-screen one and the pointer disappeared over the whole session.
	 * The shape re-sent is whatever kwl_cursor_set last chose.
	 */
	K.ptr_serial = serial;
	if (K.shape_mgr) {
		if (!K.shape_dev)
			K.shape_dev = wp_cursor_shape_manager_v1_get_pointer(
				K.shape_mgr, p);
		if (K.shape_dev)
			wp_cursor_shape_device_v1_set_shape(K.shape_dev, serial,
							    shape_of(K.cursor));
	}
}

void kwl_cursor_set(enum kwl_cursor c)
{
	if ((int)c == K.cursor)
		return;
	K.cursor = (int)c;
	/* No serial yet means the pointer has never been here; the enter
	 * handler will send the shape when it is. */
	if (K.shape_dev && K.ptr_serial)
		wp_cursor_shape_device_v1_set_shape(K.shape_dev, K.ptr_serial,
						    shape_of(K.cursor));
}

/*
 * The pointer left this surface, and a consumer has to be TOLD.
 *
 * Hover state is what makes the panel's three words read as three buttons, and
 * with no leave event the last word the pointer crossed stayed lit for the rest
 * of the session. An off-grid position is the honest report: every consumer
 * already maps a coordinate to "which thing is this", and (-1,-1) is not any of
 * them.
 */
static void pt_leave(void *d, struct wl_pointer *p, uint32_t s,
		     struct wl_surface *sf)
{
	(void)d; (void)p; (void)s; (void)sf;
	K.ptr_cx = -1;
	K.ptr_cy = -1;
	K.move_cx = -1;
	K.move_cy = -1;
	K.axis_acc = 0;
	K.axis_disc = 0;
	KtuiEvent ev = { .type = KT_EVT_MOUSE, .btn = KT_MB_MOVE, .mx = -1,
			 .my = -1, .press = KT_MP_DRAG, .mods = mods_now() };
	push_event(&ev);
}

/*
 * End of one logical pointer event group, which is where the wheel is decided.
 *
 * `discrete` is the notch count when the compositor measured one; otherwise the
 * continuous value is spent in notch-sized units. Ten is libinput's own step
 * for a wheel detent, so a mouse still moves a list one row per click while a
 * touchpad's small deltas accumulate instead of each becoming a full tick.
 */
static void pt_frame(void *d, struct wl_pointer *p)
{
	(void)d;
	(void)p;
	if (wheel_dbg() && (K.axis_disc || K.axis_acc != 0))
		fprintf(stderr, "kwl: frame disc=%d acc=%+.2f\n", K.axis_disc,
			K.axis_acc);
	if (K.axis_disc) {
		/*
		 * ONE FRAME IS ONE DETENT, and the count in it is deliberately
		 * thrown away.
		 *
		 * `axis_discrete` is only ever sent for a WHEEL — libinput
		 * measures notches for nothing else — so this branch IS the
		 * wheel path, and a frame carrying two of them is far more
		 * often one physical notch counted twice than a hand that
		 * moved two detents inside a single pointer frame. Measured in
		 * the VM: two wheel clicks queued back to back (what a front
		 * end that turns one host scroll into two produces, and what
		 * "the calendar moves two months per scroll" is) arrive as
		 * ONE frame with discrete=2 — so the duplicate gate below
		 * never sees a second tick to drop, and honouring the count
		 * moved the calendar two months. August → September → October
		 * → November for three single notches, and March → May for one
		 * doubled one, in the same session.
		 *
		 * A wheel spun hard enough to put two real detents in one
		 * frame is then capped at one, which is the same ceiling the
		 * duplicate gate already imposes and is not a second loss.
		 * A TOUCHPAD is unaffected: it sends no discrete at all and
		 * keeps the counted path below.
		 */
		wheel_emit(K.axis_disc < 0, 1);
		K.axis_disc = 0;
		K.axis_acc = 0;
		return;
	}
	/*
	 * A WHEEL HAS ALREADY BEEN QUANTISED; A FINGER HAS NOT.
	 *
	 * The accumulator below exists for a touchpad, where wl_pointer.axis
	 * is a continuous stream of small values and a tick has to be
	 * synthesised from a threshold. A WHEEL is the opposite: the
	 * compositor emits one axis event per detent, and running that
	 * through a ten-unit accumulator turns one notch into an uneven
	 * cadence — a fifteen-unit notch leaves five behind, so the second
	 * notch crosses the threshold twice and the list jumps two rows.
	 * `axis_source` says which this is and the protocol has carried it
	 * since version 5, which is the version this binds; a compositor that
	 * sends no source at all keeps the old accumulator, because then
	 * there is genuinely nothing to go on.
	 *
	 * This is the correct reading of the protocol whether or not it is
	 * what somebody's mouse is doing: `KDOS_WHEEL_DEBUG=1` prints what
	 * actually arrives.
	 */
	if (K.axis_src_seen && K.axis_src == WL_POINTER_AXIS_SOURCE_WHEEL) {
		if (K.axis_acc != 0)
			wheel_emit(K.axis_acc < 0, 1);
		K.axis_acc = 0;
		return;
	}

	/* Counted, then emitted in one call: the ticks a finger's worth of
	 * accumulated delta is worth all belong to the same frame, and asking
	 * the duplicate gate once per tick would throw away every one after the
	 * first. */
	int n = 0, up = K.axis_acc < 0;
	while ((K.axis_acc >= 10.0 || K.axis_acc <= -10.0) && n < 5) {
		n++;
		K.axis_acc += K.axis_acc < 0 ? 10.0 : -10.0;
	}
	wheel_emit(up, n);
}

static void pt_axis_src(void *d, struct wl_pointer *p, uint32_t s)
{
	K.axis_src = s;
	K.axis_src_seen = 1; (void)d; (void)p; (void)s; }

/* A finger left the touchpad: the leftover fraction is not the start of the
 * next gesture. */
static void pt_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a)
{
	(void)d; (void)p; (void)t; (void)a;
	K.axis_acc = 0;
}

static void pt_axis_disc(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{
	(void)d;
	(void)p;
	if (a == WL_POINTER_AXIS_VERTICAL_SCROLL)
		K.axis_disc += v;
	if (wheel_dbg())
		fprintf(stderr, "kwl: discrete axis=%u v=%d (disc %d)\n", a, v,
			K.axis_disc);
}

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
	int grid_h = px_h - K.rule;
	if (grid_h < 0)
		grid_h = 0;
	/* The grid is the same size either way; only its ORIGIN moves. */
	K.cols = cw > 0 ? px_w / cw : 0;
	K.rows = ch > 0 ? grid_h / ch : 0;
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
{
	(void)o; (void)refresh;
	int i = (int)(intptr_t)d;
	if (i < 0 || i >= KWL_MAX_OUTPUTS || !(flags & WL_OUTPUT_MODE_CURRENT))
		return;
	K.output_w[i] = (int)w;
	K.output_h[i] = (int)h;
}
static void out_done(void *d, struct wl_output *o) { (void)d; (void)o; }

/*
 * The largest overlay this output can actually show, in cells. Uses the output
 * the surface is already on when there is one, and the first one that reported
 * a mode otherwise — before the first configure there is nothing better, and a
 * machine whose outputs have not reported a mode at all is left alone.
 */
/*
 * `reserve` is the caller's own margin — the bar it is anchored above. With
 * exclusive_zone -1 the surface is measured against the whole output, so the
 * room it actually has is the output minus that margin; a surface sized past
 * it hangs off the far edge, which is how kdos-net lost its title bar.
 */
static void overlay_clamp(int *cols, int *rows, int reserve)
{
	int w = 0, h = 0;
	int i = K.on_output;

	if (i < 0 || i >= KWL_MAX_OUTPUTS || !K.output_w[i] || !K.output_h[i]) {
		i = -1;
		for (int k = 0; k < KWL_MAX_OUTPUTS; k++)
			if (K.output_w[k] > 0 && K.output_h[k] > 0) {
				i = k;
				break;
			}
	}
	if (i < 0)
		return;
	w = K.output_w[i];
	h = K.output_h[i];
	if (kcell_w() <= 0 || kcell_h() <= 0)
		return;

	if (reserve > 0 && reserve < h)
		h -= reserve;
	int max_cols = w / kcell_w() - 2;
	/* One row of air, not four: the four were standing in for a panel
	 * thickness this could not see, and `reserve` is now that thickness
	 * measured rather than guessed. */
	int max_rows = h / kcell_h() - (reserve > 0 ? 1 : 4);
	if (max_cols > 4 && *cols > max_cols)
		*cols = max_cols;
	if (max_rows > 4 && *rows > max_rows)
		*rows = max_rows;
}

static void apply_scale(void);

/* The listener's data is the output's INDEX: two arrays hang off it — the
 * name, and the scale HiDPI needs. */
static void out_scale(void *d, struct wl_output *o, int32_t f)
{
	(void)o;
	int i = (int)(intptr_t)d;
	if (i < 0 || i >= KWL_MAX_OUTPUTS)
		return;
	K.output_scale[i] = (int)f;
	if (i == K.on_output)
		apply_scale();
}
static void out_name(void *d, struct wl_output *o, const char *name)
{
	(void)o;
	int i = (int)(intptr_t)d;
	if (i < 0 || i >= KWL_MAX_OUTPUTS || !name)
		return;
	snprintf(K.output_name[i], KWL_OUTPUT_NAME_MAX, "%s", name);
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
 * HiDPI: adopt the scale of the output the surface is on. The layer-surface
 * size stays in LOGICAL pixels — the compositor's configure already speaks
 * them — while the shm buffer grows to px * scale and the glyphs are rendered
 * at the scale, which is a sharper picture than the compositor stretching a
 * 1x buffer. Integer only, clamped to what the glyph cache will render (see
 * KCELL_MAX_SCALE); the buffer swap itself waits for the next flush, where
 * set_buffer_scale and the resized buffer land in one commit.
 */
static void apply_scale(void)
{
	int s = K.on_output >= 0 ? K.output_scale[K.on_output] : 1;
	if (s < 1)
		s = 1;
	if (s > KCELL_MAX_SCALE)
		s = KCELL_MAX_SCALE;
	if (s == K.scale)
		return;
	K.scale = s;
	/* The cell grid is unchanged — same cols, same rows — so this is not
	 * a resize; but every pixel must be repainted at the new scale even
	 * when no cell differs. */
	ktui_draw_invalidate();
}

static void surf_enter(void *d, struct wl_surface *sf, struct wl_output *o)
{
	(void)d;
	(void)sf;
	for (int i = 0; i < K.noutputs; i++)
		if (K.outputs[i] == o) {
			K.on_output = i;
			apply_scale();
			return;
		}
}

/* A surface spanning two outputs gets an enter per output and a leave when it
 * stops overlapping one; the scale followed the LAST enter, which is as good
 * an answer as a single-buffer surface has. */
static void surf_leave(void *d, struct wl_surface *sf, struct wl_output *o)
{ (void)d; (void)sf; (void)o; }

static const struct wl_surface_listener surface_listener = {
	.enter = surf_enter,
	.leave = surf_leave,
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
		if (K.outputs[i] && !strcmp(K.output_name[i], K.cfg.output))
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
		/* ONE. A compositor advertising a second seat would otherwise
		 * get a second wl_pointer and a second wl_keyboard on this
		 * client, and every event would arrive twice. */
		if (K.seat)
			return;
		K.seat = wl_registry_bind(r, name, &wl_seat_interface, 5);
		wl_seat_add_listener(K.seat, &seat_listener, NULL);
	} else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		K.wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(K.wm_base, &wm_base_listener, NULL);
	} else if (!strcmp(iface, zxdg_decoration_manager_v1_interface.name)) {
		K.deco_mgr = wl_registry_bind(
			r, name, &zxdg_decoration_manager_v1_interface, 1);
	} else if (!strcmp(iface, zwlr_layer_shell_v1_interface.name)) {
		/*
		 * FOUR, not one. See K.layer_version: ON_DEMAND keyboard
		 * interactivity is a version-4 request and wlroots turns it into
		 * EXCLUSIVE on an older resource — which is how the desktop-icon
		 * layer came to hold the seat's keyboard against every window on
		 * the screen. Capped at what the compositor offers, so an older
		 * one still gets a working panel.
		 */
		uint32_t v = version < 4 ? version : 4;
		K.layer_version = (int)v;
		K.layer_shell = wl_registry_bind(
			r, name, &zwlr_layer_shell_v1_interface, v);
	} else if (!strcmp(iface, wl_output_interface.name)) {
		/*
		 * Bound for the lock role, which needs a surface per output —
		 * and for `cfg.output`, which is how a panel says WHICH screen
		 * it belongs to. Version 4 for the `name` event: without it the
		 * only handle on an output is a registry id no other process
		 * can name, so a second monitor could be given a panel but not
		 * the RIGHT one.
		 */
		int slot = -1;
		for (int i = 0; i < K.noutputs; i++)
			if (!K.outputs[i]) {
				slot = i;
				break;
			}
		if (slot < 0 && K.noutputs < KWL_MAX_OUTPUTS)
			slot = K.noutputs++;
		if (slot >= 0) {
			uint32_t v = version < 4 ? version : 4;
			K.outputs[slot] = wl_registry_bind(
				r, name, &wl_output_interface, v);
			K.output_id[slot] = name;
			K.output_name[slot][0] = 0;
			K.output_scale[slot] = 0;
			/* v2 for `scale`; `name` only ever arrives at v4. The
			 * data is the index, keying both arrays. */
			if (v >= 2)
				wl_output_add_listener(
					K.outputs[slot], &output_listener,
					(void *)(intptr_t)slot);
		}
	} else if (!strcmp(iface, wp_cursor_shape_manager_v1_interface.name))
		K.shape_mgr = wl_registry_bind(
			r, name, &wp_cursor_shape_manager_v1_interface, 1);
	else if (!strcmp(iface, wl_data_device_manager_interface.name))
		/* Version 1: receive-only. The DnD action negotiation the
		 * later versions add belongs to a copy/DnD milestone this
		 * library has not reached. */
		K.data_mgr = wl_registry_bind(
			r, name, &wl_data_device_manager_interface, 1);
	else if (!strcmp(iface,
			 zwp_primary_selection_device_manager_v1_interface.name))
		K.primary_mgr = wl_registry_bind(
			r, name,
			&zwp_primary_selection_device_manager_v1_interface, 1);
	else if (!strcmp(iface, ext_session_lock_manager_v1_interface.name))
		K.lock_mgr = wl_registry_bind(
			r, name, &ext_session_lock_manager_v1_interface, 1);
}

/*
 * Only outputs are tracked here, and only because a stale one is fatal: the
 * proxy is destroyed the moment the compositor says the screen is gone, so
 * nothing can hand it to get_layer_surface or get_lock_surface afterwards.
 * The other globals go away with the compositor.
 */
static void reg_remove(void *d, struct wl_registry *r, uint32_t name)
{
	(void)d;
	(void)r;
	for (int i = 0; i < K.noutputs; i++) {
		if (!K.outputs[i] || K.output_id[i] != name)
			continue;
		if (wl_output_get_version(K.outputs[i]) >=
		    WL_OUTPUT_RELEASE_SINCE_VERSION)
			wl_output_release(K.outputs[i]);
		else
			wl_output_destroy(K.outputs[i]);
		K.outputs[i] = NULL;
		K.output_id[i] = 0;
		K.output_name[i][0] = 0;
		K.output_scale[i] = 0;
		if (K.on_output == i) {
			K.on_output = -1;
			apply_scale();
		}
		return;
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = reg_global,
	.global_remove = reg_remove,
};

/* ── init ──────────────────────────────────────────────────────────────── */

/*
 * Keep an overlay on the screen.
 *
 * Layer-shell has no coordinates: a corner is an anchor plus a margin, and
 * wlroots places the surface at exactly that margin — it does NOT pull one
 * back that would hang the surface off the output. So a dropdown whose
 * margin_x is "where the word I clicked starts" runs off the right edge as
 * soon as the word is near it. Measured: the calendar, opened from the clock
 * in the panel's right wing, drew four columns on screen and the rest past
 * the edge.
 *
 * The output is the named one when a name was given, else the first whose
 * mode has arrived — two roundtrips happen before this, so on a single-screen
 * machine it always has. An unknown size clamps nothing, which is exactly the
 * old behaviour and no worse.
 */
static void place_clamp(int surf_w, int surf_h, int *mx, int *my)
{
	int idx = -1;

	if (K.cfg.output && *K.cfg.output) {
		for (int i = 0; i < KWL_MAX_OUTPUTS; i++)
			if (!strcmp(K.output_name[i], K.cfg.output)) {
				idx = i;
				break;
			}
	}
	if (idx < 0) {
		for (int i = 0; i < KWL_MAX_OUTPUTS; i++)
			if (K.output_w[i] > 0 && K.output_h[i] > 0) {
				idx = i;
				break;
			}
	}
	if (idx < 0)
		return;

	/* The mode is physical; a layer-surface margin is logical. */
	int scale = K.output_scale[idx] > 0 ? K.output_scale[idx] : 1;
	int ow = K.output_w[idx] / scale;
	int oh = K.output_h[idx] / scale;

	if (ow > surf_w && *mx > ow - surf_w)
		*mx = ow - surf_w;
	if (oh > surf_h && *my > oh - surf_h)
		*my = oh - surf_h;
	if (*mx < 0)
		*mx = 0;
	if (*my < 0)
		*my = 0;
}

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
	/* The rule is on TOP of the cells, not out of them: a bar that gave up
	 * three pixels of its own grid would clip the glyphs it was drawn to
	 * frame. */
	int thickness = K.cfg.cells * kcell_h() + K.rule;
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
		/*
		 * ON_DEMAND, and only where the compositor can hear it. On a
		 * version-3-or-older layer shell this request is read as
		 * EXCLUSIVE (wlroots: `!!interactive`), and an EXCLUSIVE
		 * BACKGROUND layer holds the seat's keyboard against every
		 * window for the rest of the session. A desktop whose arrow
		 * keys do nothing is a small loss; a desktop where nothing else
		 * receives a keystroke is not.
		 */
		if (K.cfg.keyboard && K.layer_version >= 4)
			zwlr_layer_surface_v1_set_keyboard_interactivity(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
		else if (K.cfg.keyboard)
			fprintf(stderr, "kwl: layer-shell v%d has no on-demand "
					"keyboard; this surface takes none\n",
				K.layer_version);
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
		/*
		 * CLAMPED TO THE OUTPUT, and this is a live defect it fixes.
		 *
		 * layer-shell honours the size a client asks for; it does not
		 * shrink it. A surface taller than the USABLE area — the
		 * output minus the taskbar's exclusive zone — is then centred
		 * around a negative y, and the top of it is simply off the
		 * screen: kdos-net asked for 24 rows on a 25-row display with
		 * a 2-row panel and lost its title bar. Photographed.
		 *
		 * The headroom is the caller's own margin where there is one —
		 * that IS the bar's thickness, measured rather than the four
		 * rows this used to guess at because the panel's height is a
		 * setting and a popup cannot see it.
		 */
		overlay_clamp(&cols, &rows, K.cfg.margin_y);
		int mx = K.cfg.margin_x, my = K.cfg.margin_y;
		place_clamp(cols * kcell_w(), rows * kcell_h(), &mx, &my);
		/*
		 * AN EXPLICIT MARGIN IS MEASURED FROM THE OUTPUT, NOT FROM WHAT
		 * IS LEFT OF IT — and this is the whole of "the Start menu is
		 * detached from the taskbar".
		 *
		 * A layer surface with exclusive_zone 0 is arranged inside the
		 * compositor's USABLE AREA, which already has every other
		 * surface's exclusive zone taken out of it — the panel's own,
		 * here. The panel then passes its height as this surface's
		 * margin so the popup sits just above the bar, and the two are
		 * applied one after the other: the popup floats exactly ONE BAR
		 * HEIGHT above the bar it belongs to. Photographed on the Start
		 * menu, the calendar, the volume slider and kdos-net, which is
		 * every popup this desktop has.
		 *
		 * -1 is the protocol's "do not move me out of anyone's
		 * exclusive zone", so the anchor is the output edge and the
		 * margin is the only offset there is — which is the arithmetic
		 * the caller already did. Only when a margin was actually
		 * GIVEN: a surface that named no position (a centred dialog, a
		 * notification with the default corner margin, the volume
		 * bezel) is asking to be placed, and being kept off the panel
		 * is exactly right for it.
		 */
		if (mx || my)
			zwlr_layer_surface_v1_set_exclusive_zone(K.layer_surface,
								 -1);
		if (K.cfg.corner == KWL_CORNER_TOP_RIGHT) {
			zwlr_layer_surface_v1_set_anchor(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
			zwlr_layer_surface_v1_set_margin(
				K.layer_surface,
				my ? my : KWL_OVERLAY_MARGIN,
				mx ? mx : KWL_OVERLAY_MARGIN,
				0, 0);
		} else if (K.cfg.corner == KWL_CORNER_TOP_LEFT) {
			/*
			 * The dropdown case. margin_y is normally the panel's
			 * own height, so the menu hangs off the bar rather
			 * than covering it, and margin_x is where the word
			 * that was clicked starts — already clamped by
			 * place_clamp(), because the compositor does NOT do it
			 * (this comment used to claim it did; the calendar,
			 * opened from a clock at the far right of the panel,
			 * hung off the edge of the screen with most of it
			 * unreachable).
			 */
			zwlr_layer_surface_v1_set_anchor(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
			zwlr_layer_surface_v1_set_margin(K.layer_surface,
							 my, 0, 0, mx);
		} else if (K.cfg.corner == KWL_CORNER_BOTTOM_LEFT) {
			/*
			 * The Start menu's case, and it is the TOP_LEFT one
			 * measured from the other end — a menu belonging to a
			 * bar on the bottom edge has to grow upwards from it.
			 * Anchoring TOP and computing a top margin cannot do
			 * this: the client does not know the output's pixel
			 * height, so it cannot say where "just above the
			 * taskbar" is. The compositor does, and the protocol
			 * already has the word for it.
			 */
			zwlr_layer_surface_v1_set_anchor(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
			zwlr_layer_surface_v1_set_margin(K.layer_surface, 0, 0,
							 my, mx);
		} else if (K.cfg.corner == KWL_CORNER_BOTTOM_CENTER) {
			/*
			 * ONE edge, not two: anchoring left and right as well
			 * would stretch the bezel across the whole output.
			 * The unanchored axis is centred by the compositor,
			 * which is exactly what a volume overlay wants.
			 */
			zwlr_layer_surface_v1_set_anchor(
				K.layer_surface,
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
			zwlr_layer_surface_v1_set_margin(
				K.layer_surface, 0, 0,
				my ? my : KWL_OVERLAY_MARGIN, 0);
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

	/*
	 * ON_DEMAND, not EXCLUSIVE — the same lesson as the BACKGROUND branch
	 * above, arriving on a different surface and costing more.
	 *
	 * EXCLUSIVE means "hold the seat's keyboard here until I go away", and
	 * labwc obeys it precisely: seat_focus() REFUSES every view focus while
	 * an exclusive layer surface is focused. Every front end in this
	 * desktop that takes a keyboard is one of these overlays, so with a
	 * menu, the launcher, the run box or the file chooser on screen,
	 * NOTHING ELSE COULD BE FOCUSED. Measured on a booted ISO: with the
	 * Applications menu open, a foot window that mapped afterwards reported
	 * `"focused":false` in `kdos hey list` and its titlebar stayed
	 * inactive. And because focus never moved, the menu never received
	 * wl_keyboard.leave, so `dismiss_on_unfocus` — the whole click-away
	 * mechanism — could never fire. What the user sees is a menu that will
	 * not close, over a window that will not take a keystroke.
	 *
	 * ON_DEMAND still hands these surfaces the keyboard the moment they
	 * map: they are in the OVERLAY layer, and labwc focuses an on-demand
	 * layer surface that sits above the toplevels (is_above_toplevels()).
	 * What it stops doing is nailing the keyboard down.
	 *
	 * Below version 4 there is no ON_DEMAND to ask for and the request is
	 * read as EXCLUSIVE regardless, so that case keeps the old behaviour
	 * and says so — a menu that holds the keyboard is bad, a menu that
	 * never receives a keystroke is worse.
	 */
	if (K.cfg.keyboard && K.layer_version >= 4) {
		zwlr_layer_surface_v1_set_keyboard_interactivity(
			K.layer_surface,
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
	} else if (K.cfg.keyboard) {
		fprintf(stderr, "kwl: layer-shell v%d has no on-demand keyboard; "
				"this surface holds the seat's keyboard until it "
				"exits\n", K.layer_version);
		zwlr_layer_surface_v1_set_keyboard_interactivity(
			K.layer_surface,
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
	}

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
	/* Only when the size actually moved: buffer_alloc frees what it is
	 * handed, and there is no second buffer here to flip to, so a
	 * configure at the size we already have would destroy the wl_buffer
	 * the compositor is showing. */
	if (K.extra[i].buf.w != K.extra[i].w || K.extra[i].buf.h != K.extra[i].h)
		if (buffer_alloc(&K.extra[i].buf, K.extra[i].w,
				 K.extra[i].h) < 0)
			return;
	if (!K.extra[i].buf.img)
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
	/* An emptied slot is an output that has been unplugged; a lock surface
	 * on one is a protocol error, not a blank screen. */
	int first = -1;
	for (int i = 0; i < K.noutputs; i++)
		if (K.outputs[i]) {
			first = i;
			break;
		}
	if (!K.lock_mgr || first < 0)
		return -1;

	K.lock = ext_session_lock_manager_v1_lock(K.lock_mgr);
	if (!K.lock)
		return -1;
	ext_session_lock_v1_add_listener(K.lock, &lock_listener, NULL);

	/* The first output carries the prompt: K.surface is the one libktui
	 * paints into. */
	struct ext_session_lock_surface_v1 *ls =
		ext_session_lock_v1_get_lock_surface(K.lock, K.surface,
						     K.outputs[first]);
	if (!ls)
		return -1;
	ext_session_lock_surface_v1_add_listener(ls, &lock_surface_listener, NULL);

	/*
	 * Every remaining output gets a surface too. Skipping them would leave
	 * `locked` unsent forever — the compositor is waiting for them, and a
	 * lock screen that never learns it is locked cannot safely accept a
	 * password.
	 */
	for (int i = 0; i < K.noutputs; i++) {
		if (i == first || !K.outputs[i])
			continue;
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
	/*
	 * Ask for a SERVER frame. A compositor that does not offer the
	 * protocol simply has no manager to bind and the window is undecorated
	 * as before, which is the honest fallback rather than a failure.
	 */
	if (K.deco_mgr) {
		K.deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
			K.deco_mgr, K.xdg_toplevel);
		if (K.deco)
			zxdg_toplevel_decoration_v1_set_mode(
				K.deco,
				ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
	/*
	 * A DEFAULT, not a demand: the compositor's first configure carries
	 * the size it wants and xdg_top_configure() takes it. This is only
	 * what to commit if it declines to choose.
	 *
	 * The CALLER'S size when it gave one, because these are the same
	 * programs that open as popups and they were each sized for their own
	 * content; a network panel that came up at somebody else's 80x22 would
	 * be a window with its list in the corner. 80x22 remains the fallback,
	 * and it is deliberately short of a whole screen — a frame needs
	 * somewhere to go.
	 */
	int cols = K.cfg.cols > 0 ? K.cfg.cols : 80;
	int rows = K.cfg.rows > 0 ? K.cfg.rows : 22;

	resize_cells(cols * kcell_w(), rows * kcell_h());
	return 0;
}

int kwl_init(const KwlConfig *cfg)
{
	memset(&K, 0, sizeof(K));
	K.cfg = *cfg;
	K.paste_fd = -1;
	/* -1 is "no send in flight"; a zeroed table would look like four
	 * consumers all reading fd 0. */
	for (int i = 0; i < KWL_COPY_SENDS; i++)
		copy_send[i].fd = -1;
	K.scale = 1;
	K.scale_sent = 1;	/* the protocol's own default */
	K.on_output = -1;

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
	/*
	 * The rule, once there is a cell to measure it against. Only a
	 * horizontally-anchored panel has a top edge to rule, and a rule as
	 * tall as a cell is not a rule.
	 */
	K.rule = (cfg->role == KWL_ROLE_PANEL && cfg->rule > 0 &&
		  cfg->rule < kcell_h() &&
		  (cfg->edge == KWL_EDGE_TOP || cfg->edge == KWL_EDGE_BOTTOM))
			 ? cfg->rule
			 : 0;
	K.rule_bottom = cfg->edge == KWL_EDGE_TOP;

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

	/*
	 * Paste needs a device per selection protocol, and both are guarded:
	 * a registry without them just leaves Ctrl+V and middle-click doing
	 * what they always did.
	 */
	if (K.seat && K.data_mgr) {
		K.data_dev = wl_data_device_manager_get_data_device(K.data_mgr,
								    K.seat);
		if (K.data_dev)
			wl_data_device_add_listener(K.data_dev,
						    &data_device_listener, NULL);
	}
	if (K.seat && K.primary_mgr) {
		K.primary_dev = zwp_primary_selection_device_manager_v1_get_device(
			K.primary_mgr, K.seat);
		if (K.primary_dev)
			zwp_primary_selection_device_v1_add_listener(
				K.primary_dev, &primary_device_listener, NULL);
	}

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
	/* enter/leave: which output the surface is on, for its scale */
	wl_surface_add_listener(K.surface, &surface_listener, NULL);

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

void kwl_input_cells(const KRect *rects, int n)
{
	if (!K.surface || !K.compositor)
		return;
	if (n < 0) {
		/* NULL is layer-shell/wl_surface for "all of me", which is the
		 * default a surface starts with. */
		wl_surface_set_input_region(K.surface, NULL);
		wl_surface_commit(K.surface);
		wl_display_flush(K.display);
		return;
	}

	struct wl_region *reg = wl_compositor_create_region(K.compositor);
	if (!reg)
		return;
	int cw = kcell_w(), ch = kcell_h();
	for (int i = 0; i < n; i++)
		wl_region_add(reg, rects[i].x * cw, rects[i].y * ch,
			      rects[i].w * cw, rects[i].h * ch);
	wl_surface_set_input_region(K.surface, reg);
	wl_region_destroy(reg);
	/*
	 * Committed here rather than left for the next frame: a surface whose
	 * content did not change is deliberately NOT committed by kwl_flush
	 * (the flicker fix), so a region set on an idle desktop would never
	 * take effect.
	 */
	wl_surface_commit(K.surface);
	wl_display_flush(K.display);
}

void kwl_layer_autohide(bool hidden)
{
	if (K.cfg.role != KWL_ROLE_PANEL || !K.layer_surface || !K.surface)
		return;
	if (K.autohidden == (hidden ? 1 : 0))
		return;
	K.autohidden = hidden ? 1 : 0;

	int vertical = K.cfg.edge == KWL_EDGE_TOP ||
		       K.cfg.edge == KWL_EDGE_BOTTOM;
	int cell = vertical ? kcell_h() : kcell_w();
	int cells = hidden ? 1 : (K.cfg.cells > 0 ? K.cfg.cells : 1);
	/* The rule rides on top of the cells here too, or the hidden strip
	 * would be a cell tall while the grid inside it believed it had one. */
	int thickness = cells * cell + K.rule;

	zwlr_layer_surface_v1_set_size(K.layer_surface,
				       vertical ? 0 : (uint32_t)thickness,
				       vertical ? (uint32_t)thickness : 0);
	/*
	 * The zone is the whole point: a hidden panel that still reserved its
	 * strip would hide nothing — every window would stay exactly where it
	 * was and the screen would simply have a blank line across it.
	 */
	zwlr_layer_surface_v1_set_exclusive_zone(
		K.layer_surface,
		(!hidden && K.cfg.exclusive) ? thickness : 0);

	/*
	 * Staged content is content for the OLD size. Attaching it against the
	 * configure that answers this request is the race that leaves a blank
	 * strip: the compositor gets a buffer whose height is not the one it
	 * just asked for, and what is on screen afterwards is whichever the
	 * renderer preferred. Drop the pending frame and the staged cells; the
	 * caller redraws from ktui_resized below.
	 */
	if (K.frame_cb) {
		wl_callback_destroy(K.frame_cb);
		K.frame_cb = NULL;
	}
	K.pend_valid = 0;

	wl_surface_commit(K.surface);
	/*
	 * A roundtrip rather than a dispatch loop: it is bounded by
	 * construction, and the configure answering the resize is delivered
	 * inside it. Waiting for a specific size instead would hang on the
	 * ordinary case where the geometry did not change at all — this panel
	 * is one cell tall either way and only its zone moves.
	 */
	if (wl_display_roundtrip(K.display) < 0) {
		K.closed = 1;
		return;
	}
	/* The surface is a different surface as far as the compositor is
	 * concerned; nothing on it survived. */
	ktui_resized = 1;
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
	if (K.frame_cb) {
		wl_callback_destroy(K.frame_cb);
		K.frame_cb = NULL;
	}
	K.pend_valid = 0;
	zwlr_layer_surface_v1_destroy(K.layer_surface);
	K.layer_surface = NULL;
	wl_surface_destroy(K.surface);
	K.surface = NULL;
	buffer_free(&K.buf[0]);
	buffer_free(&K.buf[1]);
	/* The next surface starts at the protocol defaults and shows nothing:
	 * neither the sent scale nor the on-screen record survives it. */
	K.scale_sent = 1;
	free(K.screen);
	K.screen = NULL;
	K.screen_w = K.screen_h = 0;
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
	wl_surface_add_listener(K.surface, &surface_listener, NULL);
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
	if (K.paste_fd >= 0)
		close(K.paste_fd);
	free(K.paste_buf);
	free(K.pend);
	free(K.screen);
	if (K.frame_cb)
		wl_callback_destroy(K.frame_cb);
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
