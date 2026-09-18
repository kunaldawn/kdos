/* libkkms — input: libinput for the devices, xkb for what a key means.
 * See kkms.h.
 *
 * TOUCH IS NOT DISAMBIGUATED HERE. It is fed to ktui_gesture_feed, the same
 * recogniser libkwl feeds from wl_touch, because a disambiguator written
 * inside a backend is written twice and disagrees twice — and then a long
 * press means one thing on the console and another on the graphical desktop.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libinput.h>
#include <libseat.h>
#include <libudev.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "kcell.h"
#include "kkms.h"
#include "kkms_priv.h"

/* libseat hands out a device id per open; libinput hands back only the
 * descriptor when it closes one, so the pairing is kept here. */
#define MAX_DEV 64
static struct {
	int fd, id;
} devs[MAX_DEV];

static struct udev *udev;

static int li_open(const char *path, int flags, void *user)
{
	(void)flags;
	(void)user;

	int fd = -1;
	int id = libseat_open_device(K.seat, path, &fd);

	if (id < 0 || fd < 0)
		return -1;

	for (int i = 0; i < MAX_DEV; i++)
		if (!devs[i].id) {
			devs[i].fd = fd;
			devs[i].id = id;
			return fd;
		}

	libseat_close_device(K.seat, id);
	return -1;
}

static void li_close(int fd, void *user)
{
	(void)user;
	for (int i = 0; i < MAX_DEV; i++)
		if (devs[i].id && devs[i].fd == fd) {
			libseat_close_device(K.seat, devs[i].id);
			devs[i].id = 0;
			return;
		}
}

static const struct libinput_interface li_iface = {
	.open_restricted = li_open,
	.close_restricted = li_close,
};

/*
 * THE SEAT'S POINTING POLICY, and every device that will accept a field gets
 * it. A device that will not — a mouse asked about tap-to-click, a touchpad
 * asked for a button it has not got — is skipped, because the policy is one
 * answer for a seat made of different devices and a refusal there is not an
 * error anybody can act on.
 *
 * APPLIED AT DEVICE_ADDED AND NOT ONLY AT STARTUP. libinput announces every
 * device it already has through the event queue, so the first pump configures
 * the ones that were there and a USB mouse plugged in later is configured on
 * arrival by the same line.
 */
static KkmsInput in_pol = {
	KKMS_IN_KEEP, KKMS_IN_KEEP, KKMS_IN_KEEP, KKMS_IN_KEEP,
	KKMS_IN_KEEP, KKMS_IN_KEEP, KKMS_IN_KEEP
};

static void dev_config(struct libinput_device *d)
{
	if (!d)
		return;

	if (in_pol.speed != KKMS_IN_KEEP &&
	    libinput_device_config_accel_is_available(d)) {
		double v = in_pol.speed / 10.0;

		if (v < -1.0)
			v = -1.0;
		if (v > 1.0)
			v = 1.0;
		libinput_device_config_accel_set_speed(d, v);
	}
	if (in_pol.natural != KKMS_IN_KEEP &&
	    libinput_device_config_scroll_has_natural_scroll(d))
		libinput_device_config_scroll_set_natural_scroll_enabled(
			d, in_pol.natural ? 1 : 0);
	if (in_pol.tap != KKMS_IN_KEEP &&
	    libinput_device_config_tap_get_finger_count(d) > 0)
		libinput_device_config_tap_set_enabled(
			d, in_pol.tap ? LIBINPUT_CONFIG_TAP_ENABLED
				      : LIBINPUT_CONFIG_TAP_DISABLED);
	if (in_pol.tap_drag != KKMS_IN_KEEP &&
	    libinput_device_config_tap_get_finger_count(d) > 0)
		libinput_device_config_tap_set_drag_enabled(
			d, in_pol.tap_drag ? LIBINPUT_CONFIG_DRAG_ENABLED
					   : LIBINPUT_CONFIG_DRAG_DISABLED);
	if (in_pol.dwt != KKMS_IN_KEEP &&
	    libinput_device_config_dwt_is_available(d))
		libinput_device_config_dwt_set_enabled(
			d, in_pol.dwt ? LIBINPUT_CONFIG_DWT_ENABLED
				      : LIBINPUT_CONFIG_DWT_DISABLED);
	if (in_pol.left_handed != KKMS_IN_KEEP &&
	    libinput_device_config_left_handed_is_available(d))
		libinput_device_config_left_handed_set(d,
						       in_pol.left_handed ? 1 : 0);
	if (in_pol.middle_emulate != KKMS_IN_KEEP &&
	    libinput_device_config_middle_emulation_is_available(d))
		libinput_device_config_middle_emulation_set_enabled(
			d, in_pol.middle_emulate
				   ? LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED
				   : LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);
}

/*
 * THE OPEN DEVICES, REFERENCED. libinput owns the objects and frees one when
 * it is removed, so a bare pointer kept here would outlive the device; a
 * reference of our own is what makes re-applying the policy to everything
 * already open safe. Dropped at DEVICE_REMOVED and at shutdown.
 */
static struct libinput_device *in_devs[MAX_DEV];

static void dev_track(struct libinput_device *d, int added)
{
	if (!d)
		return;
	if (added) {
		for (int i = 0; i < MAX_DEV; i++)
			if (!in_devs[i]) {
				in_devs[i] = libinput_device_ref(d);
				break;
			}
		dev_config(d);
		return;
	}
	for (int i = 0; i < MAX_DEV; i++)
		if (in_devs[i] == d) {
			libinput_device_unref(in_devs[i]);
			in_devs[i] = NULL;
			return;
		}
}

static void dev_forget_all(void)
{
	for (int i = 0; i < MAX_DEV; i++)
		if (in_devs[i]) {
			libinput_device_unref(in_devs[i]);
			in_devs[i] = NULL;
		}
}

void kkms_set_input(const KkmsInput *in)
{
	static const KkmsInput keep = { KKMS_IN_KEEP, KKMS_IN_KEEP,
					KKMS_IN_KEEP, KKMS_IN_KEEP,
					KKMS_IN_KEEP, KKMS_IN_KEEP,
					KKMS_IN_KEEP };

	in_pol = in ? *in : keep;
	if (!in)
		return;
	for (int i = 0; i < MAX_DEV; i++)
		if (in_devs[i])
			dev_config(in_devs[i]);
}

/*
 * THE RAW EVENTS QUEUED SINCE THE LAST COOKED ONE BELONG TO IT.
 *
 * A handler may queue the switch before the character it resolves to — a
 * button's evdev code travels whether or not the cells have a name for it — so
 * the count a raw event was stamped with is provisional until the input it
 * came from is finished with. Anything still pending when a cooked event is
 * queued takes that event's number, and a caller then sends the cooked event
 * first. Without it a guest receives the key and the desktop also acts on it.
 *
 * THE WINDOW IS CLOSED WHERE ONE INPUT ENDS, in kkms_input_pump(), and nowhere
 * else: a raw event still pending when the NEXT input's cooked event is queued
 * would be paired with the verdict on the key after it.
 *
 * ONLY EVER RAISED. A raw event held one cooked event longer than it had to be
 * is delivered in the same turn; one delivered early is a chord the session
 * swallowed and the guest saw.
 */
static void raw_bump(void)
{
	int i = K.rqtail;

	while (K.rq_pend > 0 && i != K.rqhead) {
		i = (i + KKMS_RAWQ - 1) % KKMS_RAWQ;
		K.rq[i].after = K.cooked_n;
		K.rq_pend--;
	}
	K.rq_pend = 0;
}

static void push(const KtuiEvent *e)
{
	int next = (K.qtail + 1) % (int)(sizeof(K.q) / sizeof(K.q[0]));

	/* Full means the session is behind. The OLDEST goes, because the
	 * newest is the one describing where the hand is now. A dropped event
	 * is one no caller will ever pop, so it leaves the count as well —
	 * a count that included it would hold every later raw event back for
	 * a cooked partner that is gone. */
	if (next == K.qhead) {
		K.qhead = (K.qhead + 1) % (int)(sizeof(K.q) / sizeof(K.q[0]));
		K.cooked_n--;
	}
	K.q[K.qtail] = *e;
	K.qtail = next;
	K.cooked_n++;
	raw_bump();
}

/*
 * A RAW EVENT INTO THE QUEUE A PIXEL GUEST IS DRIVEN FROM. See
 * KtuiBackend.poll_raw and `rq` in kkms_priv.h.
 *
 * A BARE MOTION MERGES INTO THE PENDING NEWEST ONE and its deltas are SUMMED,
 * not replaced: only the newest position is true, but a delta is a distance,
 * and dropping one shortens the movement a guest that grabbed the pointer
 * sees.
 *
 * NOTHING ELSE MERGES, and a message carrying a button, a key or an axis is
 * the boundary. A click coalesced away is a click that never happened, and a
 * click merged into a later position is a click on the wrong thing.
 *
 * AND EVERY ENTRY TAKES THE COOKED COUNT, stamped here and raised by
 * raw_bump() above, which is the whole of the ordering between the two queues.
 * A merge takes the later count with the later position, because it is also
 * the later event.
 */
static void rpush(const KtuiRaw *e)
{
	int last = (K.rqtail + KKMS_RAWQ - 1) % KKMS_RAWQ;

	if (e->type == KT_RAW_PTR && !e->code && K.rqhead != K.rqtail &&
	    K.rq[last].type == KT_RAW_PTR && !K.rq[last].code) {
		int dx = K.rq[last].dx + e->dx;
		int dy = K.rq[last].dy + e->dy;
		int ux = K.rq[last].dx_un + e->dx_un;
		int uy = K.rq[last].dy_un + e->dy_un;

		K.rq[last] = *e;
		K.rq[last].dx = dx;
		K.rq[last].dy = dy;
		K.rq[last].dx_un = ux;
		K.rq[last].dy_un = uy;
		K.rq[last].after = K.cooked_n;
		if (!K.rq_pend)
			K.rq_pend = 1;
		return;
	}

	int next = (K.rqtail + 1) % KKMS_RAWQ;

	if (next == K.rqhead)
		K.rqhead = (K.rqhead + 1) % KKMS_RAWQ;
	K.rq[K.rqtail] = *e;
	K.rq[K.rqtail].after = K.cooked_n;
	K.rqtail = next;
	if (K.rq_pend < KKMS_RAWQ)
		K.rq_pend++;
}

/* Pixels to the 1/256ths every raw distance is carried in, rounded away from
 * zero: a slow drag whose every step truncated to nothing is a pointer that
 * does not move at all. */
static int fx(double px)
{
	return (int)(px * 256.0 + (px < 0.0 ? -0.5 : 0.5));
}

/*
 * THE XKB STATE AS IT STANDS, for a guest to resync against. It is not what
 * drives the guest's modifiers — the key stream is, because xkb's own rule is
 * that a state driven by keys must not also be set by mask — so this carries
 * only what no key can establish: a lock, and the layout group.
 */
static void raw_xkb(KtuiRaw *e)
{
	if (!K.state)
		return;
	e->depressed = xkb_state_serialize_mods(K.state,
					       XKB_STATE_MODS_DEPRESSED);
	e->latched = xkb_state_serialize_mods(K.state, XKB_STATE_MODS_LATCHED);
	e->locked = xkb_state_serialize_mods(K.state, XKB_STATE_MODS_LOCKED);
	e->group = xkb_state_serialize_layout(K.state,
					     XKB_STATE_LAYOUT_EFFECTIVE);
}

/*
 * A KEY AS THE SWITCH IT IS, beside the character above. The code is EVDEV'S
 * and not xkb's: the eight xkb adds is added again by whoever compiles a
 * keymap at the far end, and adding it twice is a keyboard one row out.
 */
static void raw_key(uint32_t evcode, int down, unsigned ms)
{
	KtuiRaw r;

	memset(&r, 0, sizeof(r));
	r.type = KT_RAW_KEY;
	r.code = (int)evcode;
	r.state = down;
	r.ms = ms;
	raw_xkb(&r);
	rpush(&r);
}

static int mods_now(void)
{
	int m = 0;

	if (!K.state)
		return 0;
	if (xkb_state_mod_name_is_active(K.state, XKB_MOD_NAME_SHIFT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_SHIFT;
	if (xkb_state_mod_name_is_active(K.state, XKB_MOD_NAME_CTRL,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_CTRL;
	if (xkb_state_mod_name_is_active(K.state, XKB_MOD_NAME_ALT,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_ALT;
	/* The desktop's own modifier. Every window-management chord is on it,
	 * so none of them can collide with what a program in a window wants. */
	if (xkb_state_mod_name_is_active(K.state, XKB_MOD_NAME_LOGO,
					 XKB_STATE_MODS_EFFECTIVE) > 0)
		m |= KT_MOD_SUPER;
	return m;
}

static int key_of(xkb_keysym_t sym)
{
	switch (sym) {
	case XKB_KEY_Up: return KT_K_UP;
	case XKB_KEY_Down: return KT_K_DOWN;
	case XKB_KEY_Left: return KT_K_LEFT;
	case XKB_KEY_Right: return KT_K_RIGHT;
	case XKB_KEY_Home: return KT_K_HOME;
	case XKB_KEY_End: return KT_K_END;
	case XKB_KEY_Page_Up: return KT_K_PGUP;
	case XKB_KEY_Page_Down: return KT_K_PGDN;
	case XKB_KEY_Insert: return KT_K_INS;
	case XKB_KEY_Delete: return KT_K_DEL;
	case XKB_KEY_ISO_Left_Tab: return KT_K_BTAB;
	case XKB_KEY_Tab: return KT_K_TAB;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter: return KT_K_ENTER;
	case XKB_KEY_BackSpace: return KT_K_BACKSPACE;
	case XKB_KEY_Escape: return KT_K_ESC;
	case XKB_KEY_F1: return KT_K_F1;
	case XKB_KEY_F2: return KT_K_F2;
	case XKB_KEY_F3: return KT_K_F3;
	case XKB_KEY_F4: return KT_K_F4;
	case XKB_KEY_F5: return KT_K_F5;
	case XKB_KEY_F6: return KT_K_F6;
	case XKB_KEY_F7: return KT_K_F7;
	case XKB_KEY_F8: return KT_K_F8;
	case XKB_KEY_F9: return KT_K_F9;
	case XKB_KEY_F10: return KT_K_F10;
	case XKB_KEY_F11: return KT_K_F11;
	case XKB_KEY_F12: return KT_K_F12;
	/*
	 * The media keys, which produce no character at all — the fall-through
	 * below asks the layout for one and is answered zero, so without these
	 * a volume key is a keypress the session never hears about.
	 */
	case XKB_KEY_XF86AudioRaiseVolume: return KT_K_VOLUP;
	case XKB_KEY_XF86AudioLowerVolume: return KT_K_VOLDOWN;
	case XKB_KEY_XF86AudioMute: return KT_K_MUTE;
	case XKB_KEY_XF86AudioPlay:
	case XKB_KEY_XF86AudioPause: return KT_K_PLAY;
	case XKB_KEY_XF86AudioStop: return KT_K_STOP;
	case XKB_KEY_XF86AudioNext: return KT_K_NEXT;
	case XKB_KEY_XF86AudioPrev: return KT_K_PREV;
	case XKB_KEY_Print: return KT_K_PRINT;
	default:
		break;
	}

	/* Anything else is whatever character it produces, which is what makes
	 * a non-US layout type its own letters rather than the ones printed on
	 * an American keyboard. */
	uint32_t cp = xkb_keysym_to_utf32(sym);

	return cp ? (int)cp : 0;
}

#define KKMS_REP_DELAY_MS 500
#define KKMS_REP_RATE_MS 40

static unsigned long long rep_now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000 +
	       (unsigned long long)ts.tv_nsec / 1000000;
}

static void on_key(struct libinput_event *ev)
{
	struct libinput_event_keyboard *k =
		libinput_event_get_keyboard_event(ev);
	uint32_t code = libinput_event_keyboard_get_key(k) + 8;	/* evdev -> xkb */
	int down = libinput_event_keyboard_get_key_state(k) ==
		   LIBINPUT_KEY_STATE_PRESSED;

	if (!K.state)
		return;

	xkb_keysym_t sym = xkb_state_key_get_one_sym(K.state, code);

	/*
	 * THE STATE IS UPDATED AFTER READING THE SYMBOL on press and before on
	 * release, which is what xkb expects: a modifier's own press must not
	 * already be in the state that resolves it.
	 */
	if (down) {
		int key = key_of(sym);
		KtuiEvent e;

		/*
		 * Ctrl+Alt+F<n> IS A KEYSYM, NOT A CHORD.
		 *
		 * xkb resolves it to XF86Switch_VT_<n> before any modifier
		 * reaches a caller, so a desktop that looked for F<n> plus two
		 * modifiers would find neither and the switch would silently
		 * do nothing. It is answered here because this is where the
		 * keysym is, and taken rather than forwarded because
		 * `libseat` putting this VT into graphics mode is what stops
		 * the kernel answering it — without this the tty2 recovery
		 * console `/etc/inittab` guarantees cannot be reached while
		 * the desktop holds the screen.
		 *
		 * It is deliberately not rebindable and not a session chord:
		 * a chord that could be rebound away is a machine that can be
		 * locked out of its own console.
		 */
		if (sym >= XKB_KEY_XF86Switch_VT_1 &&
		    sym <= XKB_KEY_XF86Switch_VT_12) {
			kkms_switch_vt((int)(sym - XKB_KEY_XF86Switch_VT_1) + 1);
			xkb_state_update_key(K.state, code, XKB_KEY_DOWN);
			return;
		}

		if (key) {
			memset(&e, 0, sizeof(e));
			e.type = KT_EVT_KEY;
			e.key = key;
			e.mods = mods_now();
			push(&e);

			/* A key the keymap says does not repeat — a modifier,
			 * a lock — is held without repeating. */
			if (K.keymap &&
			    xkb_keymap_key_repeats(K.keymap, code)) {
				K.rep_code = code;
				K.rep_key = key;
				K.rep_due_ms = rep_now_ms() +
					       KKMS_REP_DELAY_MS;
			} else {
				K.rep_code = 0;
			}
		}
	} else if (K.rep_code == code) {
		/* Only the key that is repeating stops it: releasing a
		 * modifier while a letter is held must not. */
		K.rep_code = 0;
	}

	xkb_state_update_key(K.state, code,
			     down ? XKB_KEY_DOWN : XKB_KEY_UP);

	/*
	 * AND THE SWITCH, AFTER THE STATE HAS MOVED, so the mask a guest
	 * resyncs from describes the keyboard including this key. Every key
	 * travels here, the ones that produce no character included: a guest
	 * that never saw Ctrl go down cannot read Ctrl+click, and one that
	 * never saw a release holds the key for ever.
	 *
	 * The VT switch above returns before this. It is TAKEN and not
	 * forwarded — the keysym exists nowhere else — so no guest is handed a
	 * chord that moved the machine to another terminal.
	 */
	raw_key(code - 8, down,
		(unsigned)libinput_event_keyboard_get_time(k));
}

/*
 * Where the pointer is INSIDE the cell it is on, as an offset from the cell's
 * centre in 1/256ths. Only an embedded pixel guest reads it; everything drawn
 * in cells is pointed at a cell at a time.
 */
static void sub_of(KtuiEvent *e)
{
	int cw = kcell_w(), ch = kcell_h();

	if (cw <= 0 || ch <= 0)
		return;
	e->subx = ((int)K.ptr_px % cw) * 256 / cw - 128;
	e->suby = ((int)K.ptr_py % ch) * 256 / ch - 128;
}

static void moved(void)
{
	int cw = kcell_w(), ch = kcell_h();

	if (cw <= 0 || ch <= 0)
		return;

	K.ptr_seen = 1;

	if (K.ptr_px < 0)
		K.ptr_px = 0;
	if (K.ptr_py < 0)
		K.ptr_py = 0;
	/* THE WHOLE DESKTOP, not one screen. The pointer crosses the seam
	 * because there is no seam in the virtual box — the cut into screens
	 * happens at the paint, below anything that knows where the arrow is.
	 * Clamped against K.vw and K.vh for that reason and not against a
	 * mode, and the clamp lands on the last cell because that box is a
	 * whole number of cells. */
	if (K.ptr_px > K.vw - 1)
		K.ptr_px = K.vw - 1;
	if (K.ptr_py > K.vh - 1)
		K.ptr_py = K.vh - 1;

	int x = (int)K.ptr_px / cw;
	int y = (int)K.ptr_py / ch;

	/*
	 * REPORTED IN CELLS, AND ONLY WHEN THE CELL CHANGES: a mouse moving
	 * inside one cell has not moved as far as anything above here is
	 * concerned, and the cell pointer the view draws is the cell under it
	 * reversed — it has nowhere finer to be drawn.
	 *
	 * THE SUB-CELL MOTION IS NOT LOST HERE, AND MUST NOT BE SENT HERE
	 * EITHER. raw_motion() runs after this function on EVERY device event,
	 * carrying the pixel and the delta an embedded guest is aimed with, so
	 * a motion this test drops has already been forwarded whole. Emitting a
	 * cooked event for it as well would deliver the same movement twice to
	 * every guest — once as a pixel and once as the middle of a cell it is
	 * already inside — and make the pointer inside an application jump
	 * backwards on every sample.
	 */
	if (x == K.ptr_x && y == K.ptr_y)
		return;

	K.ptr_x = x;
	K.ptr_y = y;

	KtuiEvent e;

	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_MOUSE;
	e.mx = x;
	e.my = y;
	sub_of(&e);
	e.btn = KT_MB_MOVE;
	e.press = KT_MP_DRAG;
	push(&e);
}

/*
 * WHERE THE POINTER IS IN PIXELS, AND HOW FAR THE DEVICE MOVED. Emitted for
 * EVERY motion, unlike the cell above it: a scrollbar two pixels wide and a
 * gizmo in a three-dimensional editor are aimed at inside one character cell,
 * and a stream sampled at cell crossings cannot reach either.
 *
 * Called after moved(), which is what clamps the position into the desktop —
 * a raw event carrying a position outside it would put a guest's pointer where
 * the arrow on the screen is not.
 */
static void raw_motion(double dx, double dy, double ux, double uy, unsigned ms)
{
	int cw = kcell_w(), ch = kcell_h();
	KtuiRaw r;

	/* THE CELL IS A DIVISOR at whatever derives a cell from this, so a
	 * backend that has none yet reports nothing — and the position would
	 * be an unclamped one in any case, because moved() left early too. */
	if (cw <= 0 || ch <= 0)
		return;

	memset(&r, 0, sizeof(r));
	r.type = KT_RAW_PTR;
	r.x = (int)K.ptr_px;
	r.y = (int)K.ptr_py;
	r.cell_w = cw;
	r.cell_h = ch;
	r.dx = fx(dx);
	r.dy = fx(dy);
	r.dx_un = fx(ux);
	r.dy_un = fx(uy);
	r.mods = mods_now();
	r.ms = ms;
	rpush(&r);
}

static void on_motion(struct libinput_event *ev, int absolute)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	double dx, dy, ux, uy;

	if (absolute) {
		double ox = K.ptr_px, oy = K.ptr_py;
		/* READ BEFORE moved() SETS IT: the first sample from an
		 * absolute device is a place and not a step, because the
		 * pointer was never where the seed put it. Reporting the seed
		 * as a distance moves a guest that reads deltas half the
		 * desktop on the first touch of a tablet. */
		int seen = K.ptr_seen;

		K.ptr_px = libinput_event_pointer_get_absolute_x_transformed(
			p, K.vw);
		K.ptr_py = libinput_event_pointer_get_absolute_y_transformed(
			p, K.vh);
		/* AN ABSOLUTE DEVICE REPORTS A PLACE AND NOT A DISTANCE. The
		 * step between two places is the only delta there is, and it
		 * is the same number accelerated and not, because nothing
		 * accelerated it. */
		dx = ux = seen ? K.ptr_px - ox : 0.0;
		dy = uy = seen ? K.ptr_py - oy : 0.0;
	} else {
		dx = libinput_event_pointer_get_dx(p);
		dy = libinput_event_pointer_get_dy(p);
		ux = libinput_event_pointer_get_dx_unaccelerated(p);
		uy = libinput_event_pointer_get_dy_unaccelerated(p);
		K.ptr_px += dx;
		K.ptr_py += dy;
	}

	moved();
	raw_motion(dx, dy, ux, uy,
		   (unsigned)libinput_event_pointer_get_time(p));
}

static void on_button(struct libinput_event *ev)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	uint32_t b = libinput_event_pointer_get_button(p);
	int down = libinput_event_pointer_get_button_state(p) ==
		   LIBINPUT_BUTTON_STATE_PRESSED;
	int cw = kcell_w(), ch = kcell_h();
	KtuiEvent e;

	/*
	 * EVERY BUTTON THE DEVICE HAS, and the evdev code it sent. The switch
	 * below names three; a mouse with side buttons drives Back and Forward
	 * in a browser, and narrowing here is where those stop existing.
	 */
	if (cw > 0 && ch > 0) {
		KtuiRaw r;

		memset(&r, 0, sizeof(r));
		r.type = KT_RAW_PTR;
		r.code = (int)b;
		r.state = down;
		r.x = (int)K.ptr_px;
		r.y = (int)K.ptr_py;
		r.cell_w = cw;
		r.cell_h = ch;
		r.mods = mods_now();
		r.ms = (unsigned)libinput_event_pointer_get_time(p);
		rpush(&r);
	}

	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_MOUSE;
	e.mx = K.ptr_x;
	e.my = K.ptr_y;
	sub_of(&e);
	/* The modifiers held with the click, which the other two backends
	 * already carry — a chord on a pointer is Super and a drag, and a
	 * backend that reports the button without them cannot express one. */
	e.mods = mods_now();
	e.press = down ? KT_MP_PRESS : KT_MP_RELEASE;

	switch (b) {
	case BTN_LEFT: e.btn = KT_MB_LEFT; break;
	case BTN_RIGHT: e.btn = KT_MB_RIGHT; break;
	case BTN_MIDDLE: e.btn = KT_MB_MIDDLE; break;
	default: return;
	}

	push(&e);
}

/*
 * WHAT ONE DETENT IS WORTH IN PIXELS, on the raw arm. The cell path spends no
 * pixels at all — it counts one detent per frame and names it KT_MB_WHEEL_UP
 * or _DOWN — so this is the one place a notch is turned into a distance.
 */
#define KKMS_WHEEL_STEP_PX 10.0

static int axis_src_of(struct libinput_event_pointer *p)
{
	switch (libinput_event_pointer_get_axis_source(p)) {
	case LIBINPUT_POINTER_AXIS_SOURCE_FINGER:
		return KT_RAW_SRC_FINGER;
	case LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS:
		return KT_RAW_SRC_CONTINUOUS;
	case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL_TILT:
		return KT_RAW_SRC_WHEEL_TILT;
	default:
		return KT_RAW_SRC_WHEEL;
	}
}

/*
 * A SCROLL ALONG ONE AXIS, WITH ITS REAL VALUE AND WHAT MADE IT.
 *
 * BOTH AXES REACH HERE and only the vertical one reaches the cells: the cell
 * path's whole scroll vocabulary is KT_MB_WHEEL_UP and _DOWN, so a horizontal
 * scroll has nowhere to go there and reaches nothing drawn in cells.
 *
 * NOT QUANTISED TO A TICK EITHER. The accumulator below exists to make a whole
 * row out of a finger's stream, which is what a grid moves by; a pixel guest
 * wants the stream, and a page that jumps a screen per detent is the tick
 * arriving somewhere that could have used the distance.
 */
static void raw_axis(struct libinput_event_pointer *p,
		     enum libinput_pointer_axis li, int axis, unsigned ms)
{
	KtuiRaw r;

	if (!libinput_event_pointer_has_axis(p, li))
		return;

	memset(&r, 0, sizeof(r));
	r.type = KT_RAW_AXIS;
	r.axis = axis;
	r.source = axis_src_of(p);
	r.mods = mods_now();
	r.ms = ms;

	if (r.source == KT_RAW_SRC_WHEEL ||
	    r.source == KT_RAW_SRC_WHEEL_TILT) {
		/*
		 * A DETENT IS COUNTED, NOT MEASURED. libinput reports a
		 * wheel's value in DEGREES, which nothing scrolls by; the
		 * count is what the device measured, 120 to a detent is what a
		 * high-resolution client steps by, and the distance is that
		 * count times the step above.
		 */
		double d = libinput_event_pointer_get_axis_value_discrete(p,
									 li);

		r.value120 = (int)(d * 120.0);
		r.value = fx(d * KKMS_WHEEL_STEP_PX);
	} else {
		/* A finger's value IS pixels, and a zero of it is libinput
		 * saying the finger left the pad — which is the end of the
		 * gesture, and is why a zero is a message rather than
		 * nothing. */
		r.value = fx(libinput_event_pointer_get_axis_value(p, li));
	}
	rpush(&r);
}

static void on_axis(struct libinput_event *ev)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	unsigned ms = (unsigned)libinput_event_pointer_get_time(p);

	raw_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL, KT_RAW_VERT, ms);
	raw_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL, KT_RAW_HORIZ, ms);

	if (!libinput_event_pointer_has_axis(
		    p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
		return;

	/*
	 * A WHEEL IS ALREADY QUANTISED AND A FINGER IS NOT — the same
	 * distinction libkwl draws. libinput says which this is, so a notch is
	 * one tick and a touchpad's continuous stream is accumulated.
	 */
	double v;
	int ticks;

	if (libinput_event_pointer_get_axis_source(p) ==
	    LIBINPUT_POINTER_AXIS_SOURCE_WHEEL) {
		v = libinput_event_pointer_get_axis_value_discrete(
			p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
		ticks = (int)(v < 0 ? -v : v);
		if (ticks > 1)
			ticks = 1;	/* one frame is one detent */
	} else {
		/*
		 * ACCUMULATED, which is what makes a slow drag scroll at all.
		 * A finger's deltas are a few units per event, so a tick taken
		 * from one event alone is zero every time and the remainder
		 * was thrown away — a touchpad that moved the page only when
		 * somebody flicked it.
		 */
		v = libinput_event_pointer_get_axis_value(
			p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
		/*
		 * THE REMAINDER BELONGS TO THE GESTURE THAT MADE IT. libinput
		 * ends a finger scroll with a zero-value event; carrying the
		 * leftover fraction past it spends it on the next gesture, so
		 * a flick down after a slow drag up moves one detent fewer
		 * than the finger asked for — and a fraction left by a
		 * gesture in one direction can fire a tick in the other.
		 */
		if (v == 0.0) {
			K.scroll_acc = 0.0;
			return;
		}
		/* A change of direction is a new gesture too: the two
		 * fractions are not the same quantity. */
		if ((v < 0.0) != (K.scroll_acc < 0.0))
			K.scroll_acc = 0.0;
		K.scroll_acc += v;
		ticks = (int)((K.scroll_acc < 0 ? -K.scroll_acc
						: K.scroll_acc) / 10.0);
		if (ticks > 5) {
			/* The cap discards the surplus rather than banking
			 * it: a fling is one gesture, not a queue of them. */
			ticks = 5;
			v = K.scroll_acc;
			K.scroll_acc = 0.0;
		} else if (ticks) {
			v = K.scroll_acc;
			K.scroll_acc -= (K.scroll_acc < 0 ? -ticks : ticks) *
					10.0;
		}
	}

	for (int i = 0; i < ticks; i++) {
		KtuiEvent e;

		memset(&e, 0, sizeof(e));
		e.type = KT_EVT_MOUSE;
		e.mx = K.ptr_x;
		e.my = K.ptr_y;
		e.btn = v < 0 ? KT_MB_WHEEL_UP : KT_MB_WHEEL_DOWN;
		e.press = KT_MP_PRESS;
		push(&e);
	}
}

static void on_touch(struct libinput_event *ev, int phase)
{
	struct libinput_event_touch *t = libinput_event_get_touch_event(ev);
	int cw = kcell_w(), ch = kcell_h();
	KtuiEvent e;

	if (cw <= 0 || ch <= 0)
		return;

	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_TOUCH;
	e.phase = phase;
	e.slot = libinput_event_touch_get_seat_slot(t);
	e.ms = (unsigned)(libinput_event_touch_get_time(t));

	if (phase != KT_TOUCH_UP && phase != KT_TOUCH_CANCEL) {
		/* The transform's range is INCLUSIVE of the box, so a finger
		 * on the far edge lands on K.vw itself — one cell past the
		 * grid, which every hit test above here would miss. */
		int gw = K.vw / cw, gh = K.vh / ch;

		e.mx = (int)libinput_event_touch_get_x_transformed(t, K.vw) / cw;
		e.my = (int)libinput_event_touch_get_y_transformed(t, K.vh) / ch;
		if (e.mx > gw - 1)
			e.mx = gw > 0 ? gw - 1 : 0;
		if (e.my > gh - 1)
			e.my = gh > 0 ? gh - 1 : 0;
		if (e.mx < 0)
			e.mx = 0;
		if (e.my < 0)
			e.my = 0;
	}

	KtuiGesture g;
	KtuiEvent mouse;
	int have = 0;

	if (ktui_gesture_feed(&e, &g, &mouse, &have))
		e.gesture = g.type;

	if (e.gesture || phase != KT_TOUCH_MOVE)
		push(&e);
	if (have)
		push(&mouse);
}

int kkms_input_init(void)
{
	udev = udev_new();
	if (!udev)
		return -1;

	K.li = libinput_udev_create_context(&li_iface, NULL, udev);
	if (!K.li)
		return -1;

	const char *seat = getenv("XDG_SEAT");

	if (libinput_udev_assign_seat(K.li, seat && *seat ? seat : "seat0") != 0)
		return -1;

	K.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!K.xkb)
		return -1;

	/*
	 * The layout comes from the environment, which the session sets from
	 * /etc/keymap — the same variables the Wayland session exports. A
	 * console that ignored them would be the one place on the machine
	 * insisting on a US keyboard.
	 */
	struct xkb_rule_names names = { 0 };

	K.keymap = xkb_keymap_new_from_names(K.xkb, &names,
					     XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!K.keymap)
		return -1;

	K.state = xkb_state_new(K.keymap);
	if (!K.state)
		return -1;

	K.ptr_px = K.vw / 2.0;
	K.ptr_py = K.vh / 2.0;
	K.ptr_x = K.ptr_y = -1;
	return 0;
}

void kkms_input_shutdown(void)
{
	dev_forget_all();
	if (K.keymap_text) {
		free(K.keymap_text);
		K.keymap_text = NULL;
	}
	K.rqhead = K.rqtail = 0;
	if (K.state) {
		xkb_state_unref(K.state);
		K.state = NULL;
	}
	if (K.keymap) {
		xkb_keymap_unref(K.keymap);
		K.keymap = NULL;
	}
	if (K.xkb) {
		xkb_context_unref(K.xkb);
		K.xkb = NULL;
	}
	if (K.li) {
		libinput_unref(K.li);
		K.li = NULL;
	}
	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}
	ktui_gesture_reset();
}

int kkms_input_fd(void)
{
	return K.li ? libinput_get_fd(K.li) : -1;
}

void kkms_input_pump(void)
{
	if (!K.li)
		return;

	libinput_dispatch(K.li);

	struct libinput_event *ev;

	while ((ev = libinput_get_event(K.li))) {
		/*
		 * A NEW PHYSICAL INPUT CLOSES THE LAST ONE'S WINDOW. Anything
		 * raw_bump() could still raise belongs to the event just
		 * handled, and raising it for THIS event's cooked message
		 * would pair a key's switch with the verdict on the key after
		 * it. See raw_bump().
		 */
		K.rq_pend = 0;

		/*
		 * NOTHING IS ACTED ON WHILE THE SESSION IS SWITCHED AWAY. The
		 * devices are suspended, but events queued before the switch
		 * would otherwise arrive as if they had just happened.
		 */
		if (!K.active) {
			/*
			 * A DEVICE STILL ARRIVES AND STILL GOES. The seat is
			 * switched away, not unplugged: a mouse added while
			 * another session has the terminal is one this
			 * library must know about, or it comes back
			 * unconfigured and untracked for the rest of the
			 * session.
			 */
			if (libinput_event_get_type(ev) ==
			    LIBINPUT_EVENT_DEVICE_ADDED)
				dev_track(libinput_event_get_device(ev), 1);
			else if (libinput_event_get_type(ev) ==
				 LIBINPUT_EVENT_DEVICE_REMOVED)
				dev_track(libinput_event_get_device(ev), 0);
			/*
			 * A KEY STILL MOVES THE XKB STATE. Ctrl+Alt+F<n> is
			 * acted on at the press and the three releases arrive
			 * after the seat has gone, so a state that never saw
			 * them comes back with Ctrl and Alt held — and the
			 * first key typed on return is a chord nobody pressed.
			 */
			if (libinput_event_get_type(ev) ==
			    LIBINPUT_EVENT_KEYBOARD_KEY && K.state) {
				struct libinput_event_keyboard *k =
					libinput_event_get_keyboard_event(ev);
				uint32_t code =
					libinput_event_keyboard_get_key(k) + 8;
				int down =
					libinput_event_keyboard_get_key_state(k) ==
					LIBINPUT_KEY_STATE_PRESSED;

				xkb_state_update_key(K.state, code,
						     down ? XKB_KEY_DOWN
							  : XKB_KEY_UP);
				/*
				 * AND A RELEASE STILL TRAVELS RAW, though a
				 * press does not. The switch away is acted on
				 * at the press and the releases arrive after
				 * the seat has gone, so a guest that was
				 * handed the presses holds those keys for the
				 * rest of its life. A release for a key the
				 * guest never saw pressed is the harmless
				 * direction; the other one is a keyboard that
				 * types nothing but a chord.
				 */
				if (!down)
					raw_key(code - 8, 0,
						(unsigned)libinput_event_keyboard_get_time(k));
			}
			libinput_event_destroy(ev);
			continue;
		}

		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_DEVICE_ADDED:
			dev_track(libinput_event_get_device(ev), 1);
			break;
		case LIBINPUT_EVENT_DEVICE_REMOVED:
			dev_track(libinput_event_get_device(ev), 0);
			break;
		case LIBINPUT_EVENT_KEYBOARD_KEY:
			on_key(ev);
			break;
		case LIBINPUT_EVENT_POINTER_MOTION:
			on_motion(ev, 0);
			break;
		case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
			on_motion(ev, 1);
			break;
		case LIBINPUT_EVENT_POINTER_BUTTON:
			on_button(ev);
			break;
		case LIBINPUT_EVENT_POINTER_AXIS:
			on_axis(ev);
			break;
		case LIBINPUT_EVENT_TOUCH_DOWN:
			on_touch(ev, KT_TOUCH_DOWN);
			break;
		case LIBINPUT_EVENT_TOUCH_MOTION:
			on_touch(ev, KT_TOUCH_MOVE);
			break;
		case LIBINPUT_EVENT_TOUCH_UP:
			on_touch(ev, KT_TOUCH_UP);
			break;
		case LIBINPUT_EVENT_TOUCH_CANCEL:
			on_touch(ev, KT_TOUCH_CANCEL);
			break;
		default:
			break;
		}

		libinput_event_destroy(ev);
	}

	/*
	 * LONG PRESS HAS NO EVENT TO ARRIVE ON. The finger is down and nothing
	 * is moving, so the deadline is checked from the idle wait instead —
	 * the same shape the Wayland backend uses, because there is one
	 * recogniser and it must be polled the same way from both.
	 *
	 * libinput's timestamps are CLOCK_MONOTONIC milliseconds and that is
	 * what the recogniser was fed, so that is what it is asked with: a
	 * clock of a different base makes a deadline that never expires.
	 */
	/*
	 * A REPEAT HAS NO EVENT TO ARRIVE ON either: the key is down and
	 * libinput has said everything it is going to say. The deadline is
	 * checked from the same idle wait the long press uses.
	 *
	 * THE KEY IS THE ONE THAT WAS PRESSED AND THE MODIFIERS ARE THE ONES
	 * HELD NOW. Taking Shift while an arrow repeats extends a selection,
	 * and letting go of Ctrl stops the chord — which is what every other
	 * keyboard on the machine does, libkwl included. The KEYSYM is not
	 * re-resolved with them: that would turn a repeating letter into its
	 * capital mid-stream, and a function key into a VT switch.
	 */
	/* AND THE LAST DISPATCHED EVENT'S WINDOW IS CLOSED TOO, so that a
	 * repeat or a long press below does not pull a raw event that already
	 * has its answer along behind it. */
	K.rq_pend = 0;

	if (K.active && K.rep_code) {
		unsigned long long now = rep_now_ms();

		while (K.rep_code && now >= K.rep_due_ms) {
			KtuiEvent e;

			memset(&e, 0, sizeof(e));
			e.type = KT_EVT_KEY;
			e.key = K.rep_key;
			e.mods = mods_now();
			push(&e);
			K.rep_due_ms += KKMS_REP_RATE_MS;
			/* A wait longer than the interval must not deliver the
			 * whole backlog: a key is held, not queued. */
			if (now >= K.rep_due_ms)
				K.rep_due_ms = now + KKMS_REP_RATE_MS;
		}
	}
	if (!K.active)
		K.rep_code = 0;

	if (K.active) {
		KtuiGesture g;
		struct timespec ts;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		if (ktui_gesture_tick((unsigned)(ts.tv_sec * 1000 +
						 ts.tv_nsec / 1000000), &g)) {
			KtuiEvent e;

			memset(&e, 0, sizeof(e));
			e.type = KT_EVT_TOUCH;
			e.phase = KT_TOUCH_MOVE;
			e.mx = g.x;
			e.my = g.y;
			e.gesture = g.type;
			push(&e);
		}
	}
}

/*
 * THE LAYOUT THIS KEYBOARD IS RUNNING, as xkb's text. Made once and held: a
 * guest is handed it whenever it takes the focus, and xkbcommon allocates a
 * fresh copy of tens of kilobytes per call.
 *
 * `gen` is what a caller compares instead of the text. It is bumped when the
 * text is made, so a caller holding zero always forwards the first one.
 */
const char *kkms_keymap_text(unsigned *gen)
{
	if (!K.keymap)
		return NULL;
	if (!K.keymap_text) {
		K.keymap_text = xkb_keymap_get_as_string(
			K.keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
		if (!K.keymap_text)
			return NULL;
		K.keymap_gen++;
	}
	if (gen)
		*gen = K.keymap_gen;
	return K.keymap_text;
}

int kkms_poll_raw(KtuiRaw *ev)
{
	if (K.rqhead == K.rqtail)
		return 0;
	*ev = K.rq[K.rqhead];
	K.rqhead = (K.rqhead + 1) % KKMS_RAWQ;
	return 1;
}

int kkms_poll_event(KtuiEvent *ev, int timeout_ms)
{
	if (K.qhead != K.qtail) {
		*ev = K.q[K.qhead];
		K.qhead = (K.qhead + 1) % (int)(sizeof(K.q) / sizeof(K.q[0]));
		return 1;
	}

	(void)timeout_ms;	/* the caller owns the wait; see kkms_pump */
	memset(ev, 0, sizeof(*ev));
	ev->type = KT_EVT_TICK;
	return 0;
}
