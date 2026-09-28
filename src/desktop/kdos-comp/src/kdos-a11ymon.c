// SPDX-License-Identifier: GPL-2.0-only
/*
 * THE KEYBOARD MONITOR — org.freedesktop.a11y.KeyboardMonitor, owned as
 * org.freedesktop.a11y.Manager at /org/freedesktop/a11y/Manager on the
 * session bus. It is the interface at-spi2-core's AtspiDeviceA11yManager
 * speaks, and through it the one a screen reader such as Orca uses on
 * Wayland to hear keys and to take its own shortcuts: a Wayland client
 * sees keys only while it has the focus, so without this a reader can
 * neither echo typing nor answer Insert+H.
 *
 * The interface, as the client library uses it:
 *
 *   WatchKeyboard / UnwatchKeyboard   every key reaches the caller as a
 *                                     KeyEvent AND the focused client
 *   GrabKeyboard / UngrabKeyboard     every key reaches the caller only
 *   SetKeyGrabs(au modifiers, a(uu) keystrokes)
 *       modifiers: keysyms the caller uses as its own modifier (the Orca
 *       key). Such a key is grabbed, and so is every key pressed while one
 *       is held — except the second press of a double tap, which goes
 *       through as an ordinary key: pressed twice within the key-repeat
 *       delay with nothing between, Caps Lock as the Orca key still
 *       toggles Caps Lock. keystrokes: (keysym, modifier state) pairs
 *       grabbed exactly — the state is compared whole, which is why the
 *       library sends each shortcut four times, with and without Caps Lock
 *       and Num Lock.
 *   signal KeyEvent(b released, u state, u keysym, u unichar, q keycode)
 *       sent to each interested caller alone, never broadcast. state is
 *       the modifier mask before the key (X11 bit order, which is also
 *       wlroots'), keysym the first translated one, keycode the xkb code
 *       (evdev + 8).
 *
 * A grabbed key goes nowhere else, and that includes the lock it would
 * toggle: xkb has already flipped Caps or Num Lock by the time the key is
 * grabbed, so the flip is put back (kdos_a11y_restore_locks).
 *
 * WHO MAY CALL. A caller must own one of ALLOWED's well-known names, the
 * name the client library requests for itself as `<app id>.KeyboardMonitor`
 * before it calls anything. A box shares the session bus and could request
 * the same name while no reader holds it, so this keeps an ordinary program
 * from subscribing to keystrokes by accident, not a hostile one by design.
 * While the session is locked nothing is sent and nothing is grabbed:
 * a password is typed there.
 *
 * Keys reach this file from physical keyboards only (kdos-a11y.c): the
 * input method re-emits the keys it does not consume through a virtual
 * keyboard, and passing those on as well would give a reader every key
 * twice.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kdos.h"
#include "labwc.h"
#include "session-lock.h"

#define MON_NAME	"org.freedesktop.a11y.Manager"
#define MON_PATH	"/org/freedesktop/a11y/Manager"
#define MON_IFACE	"org.freedesktop.a11y.KeyboardMonitor"

static const char *const ALLOWED[] = {
	"org.gnome.Orca.KeyboardMonitor",
};
#define NALLOWED ((int)(sizeof(ALLOWED) / sizeof(ALLOWED[0])))

/*
 * Bounds on what one caller may hand us. Orca asks for a few hundred
 * shortcuts, each four times over; the ceilings are an order of magnitude
 * past that and exist so a caller cannot make every keystroke walk an
 * unbounded list.
 */
#define MON_MAX_CLIENTS		8
#define MON_MAX_MODIFIERS	32
#define MON_MAX_KEYSTROKES	8192
#define MON_KEYCODES		768

struct mon_keystroke {
	uint32_t keysym;
	uint32_t state;
};

struct mon_client {
	bool live;
	char name[64];			/* the caller's unique bus name */
	bool watch;
	bool grab_all;
	uint32_t *modifiers;
	size_t nmodifiers;
	struct mon_keystroke *keystrokes;
	size_t nkeystrokes;
};

static sd_bus *bus;
static struct wl_event_source *bus_source;
static struct wl_event_source *bus_timer;
static struct mon_client clients[MON_MAX_CLIENTS];
static char allowed_owner[NALLOWED][64];
/*
 * The keysym of every press that was grabbed and is still down, by evdev
 * code; 0 for a key not grabbed. The release of a grabbed press is grabbed
 * with it, and "a custom modifier is held" is read from here rather than
 * counted per caller, so a SetKeyGrabs that replaces the modifier list while
 * one is down cannot leave every later key grabbed.
 */
static uint32_t grabbed[MON_KEYCODES];

/*
 * The double tap: the custom modifier last pressed and released with no
 * other key between, and when it was let go. Its next press within the
 * key-repeat delay goes through ungrabbed.
 */
static struct {
	uint32_t keysym;	/* a custom modifier, down with nothing else */
	bool tapped;		/* ... and released: the tap is complete */
	uint32_t released_at;
} tap;

static void mon_teardown(void);

/* ── the bus on the compositor's event loop ────────────────────────── */

static void
bus_arm(void)
{
	if (!bus) {
		return;
	}
	int ev = sd_bus_get_events(bus);
	uint32_t mask = WL_EVENT_READABLE;
	if (ev > 0 && (ev & POLLOUT)) {
		mask |= WL_EVENT_WRITABLE;
	}
	wl_event_source_fd_update(bus_source, mask);

	uint64_t until = UINT64_MAX;
	int ms = 0;
	if (sd_bus_get_timeout(bus, &until) >= 0 && until != UINT64_MAX) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		uint64_t now = (uint64_t)ts.tv_sec * 1000000u
			+ (uint64_t)ts.tv_nsec / 1000u;
		/* 0 from the timer means disarmed, so "now" is 1 ms */
		ms = until > now ? (int)((until - now + 999) / 1000) : 1;
		if (ms < 1) {
			ms = 1;
		}
	}
	wl_event_source_timer_update(bus_timer, ms);
}

static void
bus_pump(void)
{
	int r;
	do {
		r = sd_bus_process(bus, NULL);
	} while (r > 0);
	if (r < 0) {
		wlr_log(WLR_ERROR, "a11y monitor: session bus: %s — keyboard "
			"monitor off for this session", strerror(-r));
		mon_teardown();
		return;
	}
	bus_arm();
}

static int
bus_ready(int fd, uint32_t mask, void *data)
{
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		wlr_log(WLR_ERROR, "a11y monitor: session bus hung up — "
			"keyboard monitor off for this session");
		mon_teardown();
		return 0;
	}
	bus_pump();
	return 0;
}

static int
bus_timeout(void *data)
{
	if (bus) {
		bus_pump();
	}
	return 0;
}

/* ── callers ───────────────────────────────────────────────────────── */

static bool
sender_allowed(const char *sender)
{
	for (int i = 0; i < NALLOWED; i++) {
		if (allowed_owner[i][0] && !strcmp(allowed_owner[i], sender)) {
			return true;
		}
	}
	return false;
}

static void
client_drop(struct mon_client *c)
{
	free(c->modifiers);
	free(c->keystrokes);
	memset(c, 0, sizeof(*c));
}

static struct mon_client *
client_find(const char *name)
{
	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		if (clients[i].live && !strcmp(clients[i].name, name)) {
			return &clients[i];
		}
	}
	return NULL;
}

/* A caller that asks for nothing any more is forgotten. */
static void
client_settle(struct mon_client *c)
{
	if (!c->watch && !c->grab_all && !c->nmodifiers && !c->nkeystrokes) {
		client_drop(c);
	}
}

static bool
any_client(void)
{
	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		if (clients[i].live) {
			return true;
		}
	}
	return false;
}

/* The caller of this method call, created on first contact; NULL with
 * `err` set when it may not call at all. */
static struct mon_client *
caller(sd_bus_message *m, sd_bus_error *err, bool create)
{
	const char *sender = sd_bus_message_get_sender(m);
	if (!sender || !sender_allowed(sender)) {
		sd_bus_error_set(err, SD_BUS_ERROR_ACCESS_DENIED,
			"the keyboard monitor answers an assistive technology "
			"that owns its KeyboardMonitor name");
		return NULL;
	}
	struct mon_client *c = client_find(sender);
	if (c || !create) {
		return c;
	}
	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		if (!clients[i].live) {
			c = &clients[i];
			c->live = true;
			snprintf(c->name, sizeof(c->name), "%s", sender);
			return c;
		}
	}
	sd_bus_error_set(err, SD_BUS_ERROR_LIMITS_EXCEEDED,
		"too many keyboard monitor clients");
	return NULL;
}

static int
m_watch(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct mon_client *c = caller(m, err, true);
	if (!c) {
		return -EACCES;
	}
	c->watch = true;
	return sd_bus_reply_method_return(m, "");
}

static int
m_unwatch(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct mon_client *c = caller(m, err, false);
	if (sd_bus_error_is_set(err)) {
		return -EACCES;
	}
	if (c) {
		c->watch = false;
		client_settle(c);
	}
	return sd_bus_reply_method_return(m, "");
}

static int
m_grab(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct mon_client *c = caller(m, err, true);
	if (!c) {
		return -EACCES;
	}
	c->grab_all = true;
	return sd_bus_reply_method_return(m, "");
}

static int
m_ungrab(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct mon_client *c = caller(m, err, false);
	if (sd_bus_error_is_set(err)) {
		return -EACCES;
	}
	if (c) {
		c->grab_all = false;
		client_settle(c);
	}
	return sd_bus_reply_method_return(m, "");
}

static int
m_set_grabs(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct mon_client *c = caller(m, err, true);
	if (!c) {
		return -EACCES;
	}

	uint32_t *mods = NULL;
	size_t nmods = 0;
	struct mon_keystroke *keys = NULL;
	size_t nkeys = 0, capkeys = 0;
	int r = sd_bus_message_enter_container(m, 'a', "u");
	uint32_t sym, state;
	while (r >= 0 && (r = sd_bus_message_read(m, "u", &sym)) > 0) {
		if (nmods == MON_MAX_MODIFIERS) {
			r = -E2BIG;
			break;
		}
		uint32_t *grown = realloc(mods, (nmods + 1) * sizeof(*mods));
		if (!grown) {
			r = -ENOMEM;
			break;
		}
		mods = grown;
		mods[nmods++] = sym;
	}
	if (r >= 0) {
		r = sd_bus_message_exit_container(m);
	}
	if (r >= 0) {
		r = sd_bus_message_enter_container(m, 'a', "(uu)");
	}
	while (r >= 0 && (r = sd_bus_message_read(m, "(uu)", &sym, &state)) > 0) {
		if (nkeys == MON_MAX_KEYSTROKES) {
			r = -E2BIG;
			break;
		}
		if (nkeys == capkeys) {
			size_t cap = capkeys ? capkeys * 2 : 64;
			struct mon_keystroke *grown =
				realloc(keys, cap * sizeof(*keys));
			if (!grown) {
				r = -ENOMEM;
				break;
			}
			keys = grown;
			capkeys = cap;
		}
		keys[nkeys].keysym = sym;
		keys[nkeys].state = state;
		nkeys++;
	}
	if (r >= 0) {
		r = sd_bus_message_exit_container(m);
	}
	if (r < 0) {
		free(mods);
		free(keys);
		client_settle(c);
		sd_bus_error_set(err, SD_BUS_ERROR_INVALID_ARGS,
			r == -E2BIG ? "too many key grabs" : "bad key grabs");
		return r;
	}

	free(c->modifiers);
	free(c->keystrokes);
	c->modifiers = mods;
	c->nmodifiers = nmods;
	c->keystrokes = keys;
	c->nkeystrokes = nkeys;
	client_settle(c);
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable mon_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("GrabKeyboard", "", "", m_grab,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("UngrabKeyboard", "", "", m_ungrab,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("WatchKeyboard", "", "", m_watch,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("UnwatchKeyboard", "", "", m_unwatch,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetKeyGrabs", "aua(uu)", "", m_set_grabs,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("KeyEvent", "buuuq", 0),
	SD_BUS_VTABLE_END
};

/*
 * Who owns an allowed name, and which callers have left the bus. One match
 * on NameOwnerChanged covers both: a caller that exits or crashes never
 * calls Unwatch, and its grabs must not outlive it.
 */
static int
on_owner_changed(sd_bus_message *m, void *data, sd_bus_error *err)
{
	const char *name, *old_owner, *new_owner;
	if (sd_bus_message_read(m, "sss", &name, &old_owner, &new_owner) < 0) {
		return 0;
	}
	for (int i = 0; i < NALLOWED; i++) {
		if (!strcmp(name, ALLOWED[i])) {
			snprintf(allowed_owner[i], sizeof(allowed_owner[i]), "%s",
				new_owner);
		}
	}
	if (name[0] == ':' && !new_owner[0]) {
		struct mon_client *c = client_find(name);
		if (c) {
			client_drop(c);
		}
	}
	return 0;
}

static int
on_initial_owner(sd_bus_message *m, void *data, sd_bus_error *err)
{
	int i = (int)(intptr_t)data;
	const char *owner = NULL;
	if (!sd_bus_message_is_method_error(m, NULL)
			&& sd_bus_message_read(m, "s", &owner) > 0 && owner) {
		snprintf(allowed_owner[i], sizeof(allowed_owner[i]), "%s", owner);
	}
	return 0;
}

static int
on_name_acquired(sd_bus_message *m, void *data, sd_bus_error *err)
{
	const sd_bus_error *e = sd_bus_message_get_error(m);
	if (e) {
		wlr_log(WLR_ERROR, "a11y monitor: cannot own %s: %s", MON_NAME,
			e->message ? e->message : e->name);
	} else {
		wlr_log(WLR_INFO, "a11y monitor: %s on the session bus",
			MON_NAME);
	}
	return 0;
}

/* ── keys ──────────────────────────────────────────────────────────── */

static uint32_t
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static bool
is_custom_modifier(const struct mon_client *c, xkb_keysym_t sym)
{
	for (size_t i = 0; i < c->nmodifiers; i++) {
		if (c->modifiers[i] == sym) {
			return true;
		}
	}
	return false;
}

static bool
any_custom_modifier(xkb_keysym_t sym)
{
	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		if (clients[i].live && is_custom_modifier(&clients[i], sym)) {
			return true;
		}
	}
	return false;
}

/* One of this caller's custom modifiers is down, and was grabbed. */
static bool
custom_modifier_held(const struct mon_client *c)
{
	for (int k = 0; k < MON_KEYCODES; k++) {
		if (grabbed[k] && is_custom_modifier(c, grabbed[k])) {
			return true;
		}
	}
	return false;
}

/* Whether this caller grabs this press. Keystrokes match on the translated
 * keysym, its lower case, or the unshifted one, with the state exact. */
static bool
client_grabs(const struct mon_client *c, xkb_keysym_t sym,
		const xkb_keysym_t *raw, int nraw, uint32_t state)
{
	if (c->grab_all) {
		return true;
	}
	if (is_custom_modifier(c, sym) || custom_modifier_held(c)) {
		return true;
	}
	xkb_keysym_t lower = xkb_keysym_to_lower(sym);
	for (size_t i = 0; i < c->nkeystrokes; i++) {
		const struct mon_keystroke *k = &c->keystrokes[i];
		if (k->state != state) {
			continue;
		}
		if (k->keysym == sym || k->keysym == lower) {
			return true;
		}
		for (int j = 0; j < nraw; j++) {
			if (k->keysym == raw[j]) {
				return true;
			}
		}
	}
	return false;
}

static void
emit(const struct mon_client *c, bool released, uint32_t state,
		uint32_t keysym, uint32_t unichar, uint32_t keycode)
{
	sd_bus_message *m = NULL;
	if (sd_bus_message_new_signal(bus, &m, MON_PATH, MON_IFACE,
			"KeyEvent") < 0) {
		return;
	}
	if (sd_bus_message_set_destination(m, c->name) >= 0
			&& sd_bus_message_append(m, "buuuq", (int)released,
				state, keysym, unichar,
				(int)(uint16_t)keycode) >= 0) {
		sd_bus_send(bus, m, NULL);
	}
	sd_bus_message_unref(m);
}

bool
kdos_a11ymon_key(struct wlr_keyboard *kb, unsigned int evdev_keycode,
		bool pressed)
{
	if (!bus || evdev_keycode >= MON_KEYCODES || !kb->xkb_state) {
		return false;
	}
	bool was_grabbed = grabbed[evdev_keycode] != 0;
	if (!pressed) {
		grabbed[evdev_keycode] = 0;
	}
	if (server.session_lock_manager->locked || !any_client()) {
		/* a release still pairs with the press it follows */
		tap.keysym = 0;
		return !pressed && was_grabbed;
	}

	uint32_t xkb_keycode = evdev_keycode + 8;
	const xkb_keysym_t *syms = NULL;
	int nsyms = xkb_state_key_get_syms(kb->xkb_state, xkb_keycode, &syms);
	xkb_keysym_t sym = nsyms > 0 ? syms[0] : XKB_KEY_NoSymbol;
	uint32_t unichar = xkb_state_key_get_utf32(kb->xkb_state, xkb_keycode);
	const xkb_keysym_t *raw = NULL;
	int nraw = xkb_keymap_key_get_syms_by_level(kb->keymap, xkb_keycode,
		xkb_state_key_get_layout(kb->xkb_state, xkb_keycode), 0, &raw);
	uint32_t state = wlr_keyboard_get_modifiers(kb);

	/*
	 * The double tap. A press of a custom modifier that completes one is
	 * an ordinary key, and so is its release; any other key in between
	 * breaks the tap.
	 */
	bool second_tap = false;
	if (pressed) {
		uint32_t t = now_ms();
		int delay = kb->repeat_info.delay > 0 ? kb->repeat_info.delay : 600;
		bool custom = any_custom_modifier(sym);
		second_tap = custom && tap.tapped && tap.keysym == sym
			&& t - tap.released_at <= (uint32_t)delay;
		tap.keysym = custom && !second_tap ? sym : 0;
		tap.tapped = false;
	} else if (tap.keysym == sym && was_grabbed) {
		tap.tapped = true;
		tap.released_at = now_ms();
	}

	bool grab = false;
	bool to[MON_MAX_CLIENTS] = { false };
	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		const struct mon_client *c = &clients[i];
		if (!c->live) {
			continue;
		}
		bool mine = pressed
			? !second_tap && client_grabs(c, sym, raw, nraw, state)
			: was_grabbed;
		grab |= mine;
		to[i] = mine || c->watch || c->grab_all
			|| is_custom_modifier(c, sym);
	}
	if (pressed && grab) {
		grabbed[evdev_keycode] = sym != XKB_KEY_NoSymbol ? sym : 1;
	}

	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		if (to[i]) {
			emit(&clients[i], !pressed, state, sym, unichar,
				xkb_keycode);
		}
	}
	bus_arm();

	if (grab) {
		/* the lock this key toggled in xkb goes back to what it was */
		if (sym == XKB_KEY_Caps_Lock || sym == XKB_KEY_Shift_Lock) {
			kdos_a11y_restore_locks(WLR_MODIFIER_CAPS,
				kb->modifiers.locked);
		} else if (sym == XKB_KEY_Num_Lock) {
			kdos_a11y_restore_locks(WLR_MODIFIER_MOD2,
				kb->modifiers.locked);
		}
	}
	return grab;
}

/* ── notifications ─────────────────────────────────────────────────── */

void
kdos_a11ymon_notify(const char *summary)
{
	if (!bus) {
		return;
	}
	sd_bus_message *m = NULL;
	if (sd_bus_message_new_method_call(bus, &m,
			"org.freedesktop.Notifications",
			"/org/freedesktop/Notifications",
			"org.freedesktop.Notifications", "Notify") < 0) {
		return;
	}
	if (sd_bus_message_append(m, "susssasa{sv}i", "kdos-comp", 0u, "",
			summary, "", 0, 0, 3000) >= 0
			&& sd_bus_message_set_expect_reply(m, 0) >= 0) {
		sd_bus_send(bus, m, NULL);
	}
	sd_bus_message_unref(m);
	bus_arm();
}

/* ── lifetime ──────────────────────────────────────────────────────── */

static void
mon_teardown(void)
{
	if (bus_source) {
		wl_event_source_remove(bus_source);
		bus_source = NULL;
	}
	if (bus_timer) {
		wl_event_source_remove(bus_timer);
		bus_timer = NULL;
	}
	for (int i = 0; i < MON_MAX_CLIENTS; i++) {
		if (clients[i].live) {
			client_drop(&clients[i]);
		}
	}
	memset(grabbed, 0, sizeof(grabbed));
	memset(&tap, 0, sizeof(tap));
	if (bus) {
		sd_bus_flush_close_unref(bus);
		bus = NULL;
	}
}

void
kdos_a11ymon_init(void)
{
	int r = sd_bus_open_user(&bus);
	if (r < 0) {
		wlr_log(WLR_INFO, "a11y monitor: no session bus (%s) — no "
			"keyboard monitor for a screen reader", strerror(-r));
		bus = NULL;
		return;
	}

	r = sd_bus_add_object_vtable(bus, NULL, MON_PATH, MON_IFACE,
		mon_vtable, NULL);
	if (r >= 0) {
		r = sd_bus_match_signal(bus, NULL, "org.freedesktop.DBus",
			"/org/freedesktop/DBus", "org.freedesktop.DBus",
			"NameOwnerChanged", on_owner_changed, NULL);
	}
	if (r >= 0) {
		r = sd_bus_request_name_async(bus, NULL, MON_NAME, 0,
			on_name_acquired, NULL);
	}
	for (int i = 0; r >= 0 && i < NALLOWED; i++) {
		r = sd_bus_call_method_async(bus, NULL, "org.freedesktop.DBus",
			"/org/freedesktop/DBus", "org.freedesktop.DBus",
			"GetNameOwner", on_initial_owner, (void *)(intptr_t)i,
			"s", ALLOWED[i]);
	}
	if (r < 0) {
		wlr_log(WLR_ERROR, "a11y monitor: %s — no keyboard monitor",
			strerror(-r));
		sd_bus_flush_close_unref(bus);
		bus = NULL;
		return;
	}

	bus_source = wl_event_loop_add_fd(server.wl_event_loop,
		sd_bus_get_fd(bus), WL_EVENT_READABLE, bus_ready, NULL);
	bus_timer = wl_event_loop_add_timer(server.wl_event_loop,
		bus_timeout, NULL);
	if (!bus_source || !bus_timer) {
		mon_teardown();
		return;
	}
	bus_arm();
}

void
kdos_a11ymon_finish(void)
{
	mon_teardown();
}
