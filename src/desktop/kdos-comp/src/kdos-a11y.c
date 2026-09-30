// SPDX-License-Identifier: GPL-2.0-only
/*
 * ACCESSIBILITY — the keyboard aids, dwell click, the pointer size and the
 * on-screen keyboard. The keyboard monitor a screen reader talks to is
 * kdos-a11ymon.c; every key it sees has been through the aids here first,
 * so a reader hears what the person's keyboard actually typed.
 *
 * THE AIDS WORK ON NON-MODIFIER KEYS OF PHYSICAL KEYBOARDS. Two reasons,
 * both structural:
 *
 * - The key signal fires before wlroots updates the keyboard's xkb state,
 *   and that update happens whatever this file decides. Dropping a Shift
 *   press here would leave Shift set in xkb and sent to the client as a
 *   modifier anyway, so slow and bounce keys leave modifiers and lock keys
 *   alone and sticky keys works WITH xkb, through the latched and locked
 *   masks.
 * - A virtual keyboard (wvkbd, the input method's re-emit, wayvnc) is not a
 *   hand on a key. Filtering it would delay or drop keystrokes a program
 *   generated deliberately.
 *
 * Hooks in upstream files: handle_key() calls kdos_a11y_key() and, through
 * it, keyboard_key_deliver(); both pointer motion handlers and the button
 * handler call in for dwell; update_active_text_input() reports whether a
 * text field is active, for the on-screen keyboard.
 */
#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "input/cursor.h"
#include "input/keyboard.h"
#include "kdos.h"
#include "labwc.h"

/* KEY_MAX + 1: every evdev code a keyboard can send. */
#define A11Y_KEYCODES		768

/* The modifiers sticky keys latches and locks. Caps and Num Lock are not
 * among them: they are locks already, and xkb owns them. */
#define STICKY_MODS (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL \
	| WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO | WLR_MODIFIER_MOD5)

/*
 * Dwell geometry, in layout pixels. A resting hand still moves a mouse a
 * pixel or two, so motion inside DWELL_JITTER_PX of where the count started
 * does not restart it; after a click the pointer has to travel
 * DWELL_REARM_PX before another can fire, or a pointer left alone would
 * click the same spot for ever.
 */
#define DWELL_JITTER_PX		4.0
#define DWELL_REARM_PX		16.0

/*
 * The on-screen keyboard is hidden this long after the last text field
 * lets go. Moving the focus from one field to another disables the first
 * text-input before the second enables, and hiding at once would drop the
 * keyboard and raise it again between two fields.
 */
#define OSK_HIDE_DELAY_MS	300

#define OSK_CMD "wvkbd-deskintl"

/* The session's switches: comp.conf at start, flipped by the Toggle*
 * actions, and set again by a reconfigure whose comp.conf changed them. */
static struct {
	bool sticky, slow, bounce, dwell, large_cursor;
} live;

/* A press a filter took; its release is taken with it, or the client would
 * see a release for a key it never saw go down. */
static uint8_t filtered[A11Y_KEYCODES / 8];

static struct {
	bool armed;
	struct keyboard *keyboard;
	struct wlr_keyboard_key_event event;
} slow;
static struct wl_event_source *slow_timer;

static uint32_t bounce_keycode = UINT32_MAX;
static uint32_t bounce_released_at;

static uint32_t sticky_latched, sticky_locked;
/* modifiers pressed with no other key since: a release of one of these is
 * a tap, which is what latches */
static uint32_t sticky_tapping;
/* the sticky bits in xkb are ours to rewrite: set while sticky keys is on,
 * and for the one apply that clears them when it goes off */
static bool sticky_owns;
static struct {
	bool pending;
	uint32_t mask, value;
} lock_fix;
static struct wl_event_source *mods_idle;

static struct wl_event_source *dwell_timer;
static bool dwell_armed = true;
static bool dwell_counting;
static double dwell_x, dwell_y;
static int buttons_down;

static int base_cursor_size = 24;
static int applied_cursor_size;

static struct wl_event_source *osk_hide_timer;
static bool osk_shown;
static bool text_active;

static bool
bit_get(const uint8_t *set, uint32_t n)
{
	return set[n / 8] & (1u << (n % 8));
}

static void
bit_put(uint8_t *set, uint32_t n, bool on)
{
	if (on) {
		set[n / 8] |= (uint8_t)(1u << (n % 8));
	} else {
		set[n / 8] &= (uint8_t)~(1u << (n % 8));
	}
}

static uint32_t
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ── the keyboard aids ─────────────────────────────────────────────── */

/* The modifier bits this key sets, from its keysyms in the CURRENT xkb
 * state; 0 for everything that is not a modifier. */
static uint32_t
key_modifier_mask(struct wlr_keyboard *kb, uint32_t evdev_keycode)
{
	if (!kb->xkb_state) {
		return 0;
	}
	const xkb_keysym_t *syms = NULL;
	int n = xkb_state_key_get_syms(kb->xkb_state, evdev_keycode + 8, &syms);
	uint32_t mask = 0;
	for (int i = 0; i < n; i++) {
		switch (syms[i]) {
		case XKB_KEY_Shift_L:   case XKB_KEY_Shift_R:
			mask |= WLR_MODIFIER_SHIFT;
			break;
		case XKB_KEY_Control_L: case XKB_KEY_Control_R:
			mask |= WLR_MODIFIER_CTRL;
			break;
		case XKB_KEY_Alt_L:     case XKB_KEY_Alt_R:
		case XKB_KEY_Meta_L:    case XKB_KEY_Meta_R:
			mask |= WLR_MODIFIER_ALT;
			break;
		case XKB_KEY_Super_L:   case XKB_KEY_Super_R:
		case XKB_KEY_Hyper_L:   case XKB_KEY_Hyper_R:
			mask |= WLR_MODIFIER_LOGO;
			break;
		case XKB_KEY_ISO_Level3_Shift:
		case XKB_KEY_Mode_switch:
			mask |= WLR_MODIFIER_MOD5;
			break;
		default:
			break;
		}
	}
	return mask;
}

/*
 * A lock key. xkb flips its lock whatever this file does with the key, so
 * slow and bounce keys pass it through as they pass a modifier: a filtered
 * Caps Lock would still turn Caps Lock on, and a dropped bounce of it would
 * leave the lock the other way round from what the person pressed.
 */
static bool
key_is_lock(struct wlr_keyboard *kb, uint32_t evdev_keycode)
{
	if (!kb->xkb_state) {
		return false;
	}
	const xkb_keysym_t *syms = NULL;
	int n = xkb_state_key_get_syms(kb->xkb_state, evdev_keycode + 8, &syms);
	for (int i = 0; i < n; i++) {
		switch (syms[i]) {
		case XKB_KEY_Caps_Lock:
		case XKB_KEY_Shift_Lock:
		case XKB_KEY_Num_Lock:
			return true;
		default:
			break;
		}
	}
	return false;
}

/*
 * A member of the keyboard group. Changing one member's modifiers is how
 * the group, and every other member, changes too (wlr_keyboard_group syncs
 * them) — the same route keyboard_update_layout() takes. Looked up at the
 * moment of use rather than kept, because a keyboard can be unplugged
 * between a key and the idle that follows it.
 */
static struct wlr_keyboard *
group_member(void)
{
	struct input *input;
	wl_list_for_each(input, &server.seat.inputs, link) {
		if (input->wlr_input_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}
		struct keyboard *kb = (struct keyboard *)input;
		if (!kb->is_virtual) {
			return kb->wlr_keyboard;
		}
	}
	return NULL;
}

static bool
keyboard_alive(struct keyboard *keyboard)
{
	struct input *input;
	wl_list_for_each(input, &server.seat.inputs, link) {
		if (input == &keyboard->base) {
			return true;
		}
	}
	return false;
}

/*
 * The latched/locked rewrite runs from an idle, not from inside the key
 * signal: wlroots updates the xkb state AFTER the listeners return, and a
 * mask written from inside one would be overwritten by the key it came with.
 */
static void
mods_apply(void *data)
{
	mods_idle = NULL;
	struct wlr_keyboard *kb = group_member();
	if (!kb) {
		lock_fix.pending = false;
		return;
	}
	struct wlr_keyboard_modifiers m = kb->modifiers;
	uint32_t latched = m.latched;
	uint32_t locked = m.locked;
	if (sticky_owns) {
		latched = (latched & ~STICKY_MODS) | sticky_latched;
		locked = (locked & ~STICKY_MODS) | sticky_locked;
		sticky_owns = live.sticky;
	}
	if (lock_fix.pending) {
		locked = (locked & ~lock_fix.mask) | (lock_fix.value & lock_fix.mask);
		lock_fix.pending = false;
	}
	if (latched != m.latched || locked != m.locked) {
		wlr_keyboard_notify_modifiers(kb, m.depressed, latched, locked,
			m.group);
	}
}

static void
mods_schedule(void)
{
	if (!mods_idle) {
		mods_idle = wl_event_loop_add_idle(server.wl_event_loop,
			mods_apply, NULL);
	}
}

void
kdos_a11y_restore_locks(unsigned int mask, unsigned int value)
{
	lock_fix.pending = true;
	lock_fix.mask = mask;
	lock_fix.value = value;
	mods_schedule();
}

/* A modifier went down or up, or another key went down (mask 0). */
static void
sticky_key(bool pressed, uint32_t mask)
{
	if (!mask) {
		if (pressed) {
			sticky_tapping = 0;
		}
		return;
	}
	if (pressed) {
		sticky_tapping |= mask;
		return;
	}
	if (!(sticky_tapping & mask)) {
		return;	/* it was held for a chord, not tapped */
	}
	sticky_tapping &= ~mask;
	if (sticky_locked & mask) {
		sticky_locked &= ~mask;
	} else if (sticky_latched & mask) {
		sticky_latched &= ~mask;
		sticky_locked |= mask;
	} else {
		sticky_latched |= mask;
	}
	mods_schedule();
}

/* The one key after a latch has been delivered WITH it; now it lets go. */
static void
sticky_consumed(bool pressed, uint32_t mask)
{
	if (pressed && !mask && sticky_latched) {
		sticky_latched = 0;
		mods_schedule();
	}
}

static void
deliver(struct keyboard *keyboard, struct wlr_keyboard_key_event *event,
		uint32_t modmask)
{
	uint32_t kc = event->keycode;
	bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
	bool physical = !keyboard->is_virtual;

	if (physical && live.sticky) {
		sticky_key(pressed, modmask);
	}
	/* the monitor hears physical keys only; see kdos-a11ymon.c */
	if (!physical || !kdos_a11ymon_key(keyboard->wlr_keyboard, kc,
			pressed)) {
		keyboard_key_deliver(keyboard, event);
	}
	if (physical && !pressed && !modmask) {
		bounce_keycode = kc;
		bounce_released_at = event->time_msec;
	}
	if (physical && live.sticky) {
		sticky_consumed(pressed, modmask);
	}
}

static void
slow_cancel(void)
{
	slow.armed = false;
	if (slow_timer) {
		wl_event_source_timer_update(slow_timer, 0);
	}
}

/* Held long enough: the press goes through now, stamped with now. */
static int
slow_fire(void *data)
{
	if (!slow.armed) {
		return 0;
	}
	slow.armed = false;
	uint32_t kc = slow.event.keycode;
	if (!bit_get(filtered, kc) || !keyboard_alive(slow.keyboard)) {
		return 0;
	}
	bit_put(filtered, kc, false);
	slow.event.time_msec = now_ms();
	deliver(slow.keyboard, &slow.event, 0);
	return 0;
}

/*
 * One pending key at a time. A second press before the first has been held
 * long enough replaces it, and the first stays filtered to its release — two
 * keys held together is not the steady press slow keys is waiting for.
 */
static void
slow_arm(struct keyboard *keyboard, struct wlr_keyboard_key_event *event)
{
	if (!slow_timer) {
		slow_timer = wl_event_loop_add_timer(server.wl_event_loop,
			slow_fire, NULL);
		if (!slow_timer) {
			return;
		}
	}
	slow.armed = true;
	slow.keyboard = keyboard;
	slow.event = *event;
	wl_event_source_timer_update(slow_timer, kdos_conf.slow_keys_ms);
}

bool
kdos_a11y_key(struct keyboard *keyboard, struct wlr_keyboard_key_event *event)
{
	uint32_t kc = event->keycode;
	if (kc >= A11Y_KEYCODES) {
		return false;
	}
	bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;

	if (!pressed && bit_get(filtered, kc)) {
		bit_put(filtered, kc, false);
		if (slow.armed && slow.event.keycode == kc) {
			slow_cancel();
		}
		return true;
	}
	if (pressed) {
		/* a mark left by a keyboard unplugged mid-press */
		bit_put(filtered, kc, false);
	}

	uint32_t modmask = key_modifier_mask(keyboard->wlr_keyboard, kc);
	if (pressed && !keyboard->is_virtual && !modmask
			&& !key_is_lock(keyboard->wlr_keyboard, kc)) {
		if (live.bounce && kc == bounce_keycode
				&& event->time_msec - bounce_released_at
					< (uint32_t)kdos_conf.bounce_keys_ms) {
			bit_put(filtered, kc, true);
			return true;
		}
		if (live.slow) {
			bit_put(filtered, kc, true);
			slow_arm(keyboard, event);
			return true;
		}
	}
	deliver(keyboard, event, modmask);
	return true;
}

/* ── dwell click ───────────────────────────────────────────────────── */

static int
dwell_fire(void *data)
{
	struct seat *seat = &server.seat;
	dwell_counting = false;
	if (!live.dwell || !dwell_armed || buttons_down > 0) {
		return 0;
	}
	/* a click ends a move or resize the pointer is in the middle of */
	if (server.input_mode == LAB_INPUT_STATE_MOVE
			|| server.input_mode == LAB_INPUT_STATE_RESIZE) {
		return 0;
	}
	uint32_t t = now_ms();
	cursor_emulate_button(seat, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED, t);
	cursor_emulate_button(seat, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED, t);
	dwell_armed = false;
	dwell_x = seat->cursor->x;
	dwell_y = seat->cursor->y;
	return 0;
}

static void
dwell_stop(void)
{
	dwell_counting = false;
	if (dwell_timer) {
		wl_event_source_timer_update(dwell_timer, 0);
	}
}

void
kdos_a11y_pointer_motion(struct seat *seat)
{
	if (!live.dwell) {
		return;
	}
	double x = seat->cursor->x;
	double y = seat->cursor->y;
	double moved = hypot(x - dwell_x, y - dwell_y);

	if (!dwell_armed) {
		if (moved < DWELL_REARM_PX) {
			return;
		}
		dwell_armed = true;
	} else if (dwell_counting && moved < DWELL_JITTER_PX) {
		return;
	}
	if (!dwell_timer) {
		dwell_timer = wl_event_loop_add_timer(server.wl_event_loop,
			dwell_fire, NULL);
		if (!dwell_timer) {
			return;
		}
	}
	dwell_x = x;
	dwell_y = y;
	dwell_counting = true;
	wl_event_source_timer_update(dwell_timer, kdos_conf.dwell_click_ms);
}

void
kdos_a11y_pointer_button(struct seat *seat, bool pressed)
{
	buttons_down += pressed ? 1 : -1;
	if (buttons_down < 0) {
		buttons_down = 0;
	}
	if (live.dwell && pressed) {
		dwell_stop();
	}
}

/* ── the pointer size ──────────────────────────────────────────────── */

/*
 * The size goes into XCURSOR_SIZE as well as into the compositor's own
 * cursor manager: cursor_load() reads it from there, and every program
 * started after the switch inherits it, which is what makes a client that
 * draws its own pointer draw the same size.
 */
static void
cursor_apply(void)
{
	int size = live.large_cursor ? kdos_conf.large_cursor_size
		: kdos_conf.cursor_size > 0 ? kdos_conf.cursor_size
		: base_cursor_size;
	if (size == applied_cursor_size) {
		return;
	}
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", size);
	setenv("XCURSOR_SIZE", buf, 1);
	applied_cursor_size = size;
	cursor_reload(&server.seat);
}

/*
 * The size the session was started with, which cursor_size = 0 returns to.
 * A value that differs from the one this file last wrote came from
 * somewhere else — ~/.config/kdos-comp/environment, re-read on SIGHUP — and
 * becomes the new base.
 */
static void
cursor_read_base(void)
{
	const char *e = getenv("XCURSOR_SIZE");
	int n = e ? atoi(e) : 0;
	if (n > 0 && n != applied_cursor_size) {
		base_cursor_size = n;
	}
	/* cursor_load() has just run with whatever the environment says */
	applied_cursor_size = n > 0 ? n : 24;
}

/* ── the on-screen keyboard ────────────────────────────────────────── */

static void
osk_signal(int sig)
{
	pid_t pid = kdos_child_pid(OSK_CMD);
	if (pid > 0) {
		kill(pid, sig);
	}
	osk_shown = sig == SIGUSR2;
}

static int
osk_hide_fire(void *data)
{
	if (!text_active) {
		osk_signal(SIGUSR1);
	}
	return 0;
}

void
kdos_a11y_text_input(bool active)
{
	if (active == text_active) {
		return;
	}
	text_active = active;
	if (kdos_conf.osk != KDOS_OSK_AUTO) {
		return;
	}
	if (active) {
		if (osk_hide_timer) {
			wl_event_source_timer_update(osk_hide_timer, 0);
		}
		osk_signal(SIGUSR2);
		return;
	}
	if (!osk_hide_timer) {
		osk_hide_timer = wl_event_loop_add_timer(server.wl_event_loop,
			osk_hide_fire, NULL);
	}
	if (osk_hide_timer) {
		wl_event_source_timer_update(osk_hide_timer, OSK_HIDE_DELAY_MS);
	}
}

/* ── the switches ──────────────────────────────────────────────────── */

static void
sticky_set(bool on)
{
	live.sticky = on;
	sticky_tapping = 0;
	if (!on) {
		sticky_latched = 0;
		sticky_locked = 0;
		if (sticky_owns) {
			mods_schedule();
		}
	} else {
		sticky_owns = true;
	}
}

static void
slow_set(bool on)
{
	live.slow = on;
	if (!on) {
		slow_cancel();
	}
}

static void
dwell_set(bool on)
{
	live.dwell = on;
	dwell_armed = true;
	if (!on) {
		dwell_stop();
	}
}

void
kdos_a11y_toggle(enum kdos_a11y_switch which)
{
	const char *what = NULL;
	bool on = false;

	switch (which) {
	case KDOS_A11Y_STICKY_KEYS:
		sticky_set(!live.sticky);
		what = "Sticky keys";
		on = live.sticky;
		break;
	case KDOS_A11Y_SLOW_KEYS:
		slow_set(!live.slow);
		what = "Slow keys";
		on = live.slow;
		break;
	case KDOS_A11Y_BOUNCE_KEYS:
		live.bounce = !live.bounce;
		what = "Bounce keys";
		on = live.bounce;
		break;
	case KDOS_A11Y_DWELL_CLICK:
		dwell_set(!live.dwell);
		what = "Dwell click";
		on = live.dwell;
		break;
	case KDOS_A11Y_LARGE_CURSOR:
		live.large_cursor = !live.large_cursor;
		cursor_apply();
		what = "Large pointer";
		on = live.large_cursor;
		break;
	case KDOS_A11Y_OSK:
		if (kdos_conf.osk == KDOS_OSK_OFF) {
			kdos_a11ymon_notify("On-screen keyboard: off in comp.conf "
				"(osk = manual or auto)");
			return;
		}
		osk_signal(osk_shown ? SIGUSR1 : SIGUSR2);
		return;
	}

	/* A switch that changes how every key or click behaves says so: the
	 * person who pressed it by accident otherwise has a keyboard that has
	 * started misbehaving and nothing to read about why. */
	char msg[64];
	snprintf(msg, sizeof(msg), "%s %s", what, on ? "on" : "off");
	wlr_log(WLR_INFO, "a11y: %s", msg);
	kdos_a11ymon_notify(msg);
}

/*
 * A reload takes a switch from comp.conf only when the file's value for it
 * changed. Every `kdos theme` is a reload, and an accent switch that turned
 * off the sticky keys somebody had just turned on with a key would be a
 * keyboard that changed under them for no reason they could see.
 */
void
kdos_a11y_reconfigure(void)
{
	static struct {
		bool valid, sticky, slow, bounce, dwell, large_cursor;
	} seen;
	const struct kdos_conf *c = &kdos_conf;

	if (!seen.valid || c->sticky_keys != seen.sticky) {
		sticky_set(c->sticky_keys);
	}
	if (!seen.valid || c->slow_keys != seen.slow) {
		slow_set(c->slow_keys);
	}
	if (!seen.valid || c->bounce_keys != seen.bounce) {
		live.bounce = c->bounce_keys;
	}
	if (!seen.valid || c->dwell_click != seen.dwell) {
		dwell_set(c->dwell_click);
	}
	if (!seen.valid || c->large_cursor != seen.large_cursor) {
		live.large_cursor = c->large_cursor;
	}
	seen.valid = true;
	seen.sticky = c->sticky_keys;
	seen.slow = c->slow_keys;
	seen.bounce = c->bounce_keys;
	seen.dwell = c->dwell_click;
	seen.large_cursor = c->large_cursor;
	/* seat_reconfigure() reloaded the cursor from the environment just
	 * before this ran */
	cursor_read_base();
	cursor_apply();
	if (kdos_conf.osk != KDOS_OSK_AUTO && osk_hide_timer) {
		wl_event_source_timer_update(osk_hide_timer, 0);
	}
}

void
kdos_a11y_init(void)
{
	applied_cursor_size = 0;
	kdos_a11y_reconfigure();
}

void
kdos_a11y_finish(void)
{
	if (mods_idle) {
		wl_event_source_remove(mods_idle);
		mods_idle = NULL;
	}
	if (slow_timer) {
		wl_event_source_remove(slow_timer);
		slow_timer = NULL;
	}
	if (dwell_timer) {
		wl_event_source_remove(dwell_timer);
		dwell_timer = NULL;
	}
	if (osk_hide_timer) {
		wl_event_source_remove(osk_hide_timer);
		osk_hide_timer = NULL;
	}
	slow.armed = false;
}
