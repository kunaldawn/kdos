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

static void push(const KtuiEvent *e)
{
	int next = (K.qtail + 1) % (int)(sizeof(K.q) / sizeof(K.q[0]));

	/* Full means the session is behind. The OLDEST goes, because the
	 * newest is the one describing where the hand is now. */
	if (next == K.qhead)
		K.qhead = (K.qhead + 1) % (int)(sizeof(K.q) / sizeof(K.q[0]));
	K.q[K.qtail] = *e;
	K.qtail = next;
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
	default:
		break;
	}

	/* Anything else is whatever character it produces, which is what makes
	 * a non-US layout type its own letters rather than the ones printed on
	 * an American keyboard. */
	uint32_t cp = xkb_keysym_to_utf32(sym);

	return cp ? (int)cp : 0;
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
		}
	}

	xkb_state_update_key(K.state, code,
			     down ? XKB_KEY_DOWN : XKB_KEY_UP);
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
	if (K.ptr_px > K.width - 1)
		K.ptr_px = K.width - 1;
	if (K.ptr_py > K.height - 1)
		K.ptr_py = K.height - 1;

	int x = (int)K.ptr_px / cw;
	int y = (int)K.ptr_py / ch;

	/* Reported in CELLS, and only when the cell changes: a mouse moving
	 * inside one cell has not moved as far as anything above here is
	 * concerned. */
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

static void on_motion(struct libinput_event *ev, int absolute)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);

	if (absolute) {
		K.ptr_px = libinput_event_pointer_get_absolute_x_transformed(
			p, K.width);
		K.ptr_py = libinput_event_pointer_get_absolute_y_transformed(
			p, K.height);
	} else {
		K.ptr_px += libinput_event_pointer_get_dx(p);
		K.ptr_py += libinput_event_pointer_get_dy(p);
	}

	moved();
}

static void on_button(struct libinput_event *ev)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	uint32_t b = libinput_event_pointer_get_button(p);
	int down = libinput_event_pointer_get_button_state(p) ==
		   LIBINPUT_BUTTON_STATE_PRESSED;
	KtuiEvent e;

	memset(&e, 0, sizeof(e));
	e.type = KT_EVT_MOUSE;
	e.mx = K.ptr_x;
	e.my = K.ptr_y;
	sub_of(&e);
	e.press = down ? KT_MP_PRESS : KT_MP_RELEASE;

	switch (b) {
	case BTN_LEFT: e.btn = KT_MB_LEFT; break;
	case BTN_RIGHT: e.btn = KT_MB_RIGHT; break;
	case BTN_MIDDLE: e.btn = KT_MB_MIDDLE; break;
	default: return;
	}

	push(&e);
}

static void on_axis(struct libinput_event *ev)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);

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
		v = libinput_event_pointer_get_axis_value(
			p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
		ticks = (int)((v < 0 ? -v : v) / 10.0);
		if (ticks > 5)
			ticks = 5;
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
		e.mx = (int)libinput_event_touch_get_x_transformed(t, K.width) / cw;
		e.my = (int)libinput_event_touch_get_y_transformed(t, K.height) / ch;
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

	K.ptr_px = K.width / 2.0;
	K.ptr_py = K.height / 2.0;
	K.ptr_x = K.ptr_y = -1;
	return 0;
}

void kkms_input_shutdown(void)
{
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
		 * NOTHING IS ACTED ON WHILE THE SESSION IS SWITCHED AWAY. The
		 * devices are suspended, but events queued before the switch
		 * would otherwise arrive as if they had just happened.
		 */
		if (!K.active) {
			libinput_event_destroy(ev);
			continue;
		}

		switch (libinput_event_get_type(ev)) {
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
