// SPDX-License-Identifier: GPL-2.0-only
/*
 * THE D-BUS IDLE INHIBITOR — org.freedesktop.ScreenSaver on the session bus,
 * at /ScreenSaver and /org/freedesktop/ScreenSaver.
 *
 * The Wayland idle-inhibit protocol (idle.c) reaches only native Wayland
 * clients. An X11 client under Xwayland has no way onto it — Xwayland turns
 * its own screensaver extension off and forwards nothing — so a player that
 * runs under Xwayland (VLC 3's Qt 5 interface) inhibits through this name or
 * not at all, and without it a film dims at idle_dim and locks at idle_lock.
 *
 *   Inhibit(s application, s reason) -> u cookie
 *   UnInhibit(u cookie)               only the caller that took the cookie
 *                                     can return it; an unknown one is a
 *                                     no-op, as the interface specifies
 *   GetActive() -> b                  whether the session is locked
 *
 * Every cookie is one kdos_idle_inhibit(true), exactly as one Wayland
 * inhibitor is, so the policy cannot tell the two routes apart. A caller
 * that leaves the bus without UnInhibit — a crash, a kill — has every cookie
 * it holds returned on NameOwnerChanged; without that, one crashed player
 * would stop the idle policy until the session ended.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wlr/util/log.h>

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

#define SS_NAME		"org.freedesktop.ScreenSaver"
#define SS_IFACE	"org.freedesktop.ScreenSaver"

/* Both paths are in use: VLC and xdg-screensaver call /ScreenSaver, Qt and
 * Chromium /org/freedesktop/ScreenSaver. */
static const char *const SS_PATHS[] = {
	"/ScreenSaver",
	"/org/freedesktop/ScreenSaver",
};
#define SS_NPATHS ((int)(sizeof(SS_PATHS) / sizeof(SS_PATHS[0])))

/* A ceiling so a looping caller cannot grow the table without bound; a
 * desktop holds a handful at most. Past it Inhibit fails and says so. */
#define SS_MAX_COOKIES 64

struct ss_cookie {
	bool live;
	uint32_t cookie;
	char owner[64];		/* the caller's unique bus name */
};

static sd_bus *bus;
static struct wl_event_source *bus_source;
static struct wl_event_source *bus_timer;
static struct ss_cookie cookies[SS_MAX_COOKIES];
static uint32_t next_cookie = 1;

static void ss_teardown(void);

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
		wlr_log(WLR_ERROR, "screensaver: session bus: %s — D-Bus idle "
			"inhibitors off for this session", strerror(-r));
		ss_teardown();
		return;
	}
	bus_arm();
}

static int
bus_ready(int fd, uint32_t mask, void *data)
{
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		wlr_log(WLR_ERROR, "screensaver: session bus hung up — D-Bus "
			"idle inhibitors off for this session");
		ss_teardown();
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

/* ── cookies ───────────────────────────────────────────────────────── */

static void
cookie_release(struct ss_cookie *c)
{
	memset(c, 0, sizeof(*c));
	kdos_idle_inhibit(false);
}

static int
m_inhibit(sd_bus_message *m, void *data, sd_bus_error *err)
{
	const char *app, *reason;
	int r = sd_bus_message_read(m, "ss", &app, &reason);
	if (r < 0) {
		return r;
	}
	const char *sender = sd_bus_message_get_sender(m);
	if (!sender) {
		return sd_bus_error_set(err, SD_BUS_ERROR_INVALID_ARGS,
			"no sender");
	}
	for (int i = 0; i < SS_MAX_COOKIES; i++) {
		struct ss_cookie *c = &cookies[i];
		if (c->live) {
			continue;
		}
		c->live = true;
		c->cookie = next_cookie++;
		if (!next_cookie) {
			next_cookie = 1;	/* 0 is never a cookie */
		}
		snprintf(c->owner, sizeof(c->owner), "%s", sender);
		kdos_idle_inhibit(true);
		wlr_log(WLR_DEBUG, "screensaver: %s inhibits idle (%s): %u",
			app, reason, c->cookie);
		return sd_bus_reply_method_return(m, "u", c->cookie);
	}
	return sd_bus_error_set(err, SD_BUS_ERROR_LIMITS_EXCEEDED,
		"too many idle inhibitors");
}

static int
m_uninhibit(sd_bus_message *m, void *data, sd_bus_error *err)
{
	uint32_t cookie;
	int r = sd_bus_message_read(m, "u", &cookie);
	if (r < 0) {
		return r;
	}
	const char *sender = sd_bus_message_get_sender(m);
	for (int i = 0; sender && i < SS_MAX_COOKIES; i++) {
		struct ss_cookie *c = &cookies[i];
		if (c->live && c->cookie == cookie
				&& !strcmp(c->owner, sender)) {
			cookie_release(c);
			break;
		}
	}
	return sd_bus_reply_method_return(m, "");
}

static int
m_get_active(sd_bus_message *m, void *data, sd_bus_error *err)
{
	return sd_bus_reply_method_return(m, "b",
		(int)(server.session_lock_manager
			&& server.session_lock_manager->locked));
}

static const sd_bus_vtable ss_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Inhibit", "ss", "u", m_inhibit,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("UnInhibit", "u", "", m_uninhibit,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetActive", "", "b", m_get_active,
		SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

/* A caller that left the bus returns every cookie it still held. */
static int
on_owner_changed(sd_bus_message *m, void *data, sd_bus_error *err)
{
	const char *name, *old_owner, *new_owner;
	if (sd_bus_message_read(m, "sss", &name, &old_owner, &new_owner) < 0) {
		return 0;
	}
	if (name[0] != ':' || new_owner[0]) {
		return 0;
	}
	for (int i = 0; i < SS_MAX_COOKIES; i++) {
		if (cookies[i].live && !strcmp(cookies[i].owner, name)) {
			cookie_release(&cookies[i]);
		}
	}
	return 0;
}

static int
on_name_acquired(sd_bus_message *m, void *data, sd_bus_error *err)
{
	const sd_bus_error *e = sd_bus_message_get_error(m);
	if (e) {
		wlr_log(WLR_ERROR, "screensaver: cannot own %s: %s", SS_NAME,
			e->message ? e->message : e->name);
	} else {
		wlr_log(WLR_INFO, "screensaver: %s on the session bus",
			SS_NAME);
	}
	return 0;
}

/* ── lifetime ──────────────────────────────────────────────────────── */

static void
ss_teardown(void)
{
	if (bus_source) {
		wl_event_source_remove(bus_source);
		bus_source = NULL;
	}
	if (bus_timer) {
		wl_event_source_remove(bus_timer);
		bus_timer = NULL;
	}
	/* The inhibitors die with the bus that carried them: a policy left
	 * stopped by a connection that is gone never resumes. */
	for (int i = 0; i < SS_MAX_COOKIES; i++) {
		if (cookies[i].live) {
			cookie_release(&cookies[i]);
		}
	}
	if (bus) {
		sd_bus_flush_close_unref(bus);
		bus = NULL;
	}
}

void
kdos_screensaver_init(void)
{
	int r = sd_bus_open_user(&bus);
	if (r < 0) {
		wlr_log(WLR_INFO, "screensaver: no session bus (%s) — only "
			"Wayland clients can inhibit idle", strerror(-r));
		bus = NULL;
		return;
	}

	for (int i = 0; r >= 0 && i < SS_NPATHS; i++) {
		r = sd_bus_add_object_vtable(bus, NULL, SS_PATHS[i], SS_IFACE,
			ss_vtable, NULL);
	}
	if (r >= 0) {
		r = sd_bus_match_signal(bus, NULL, "org.freedesktop.DBus",
			"/org/freedesktop/DBus", "org.freedesktop.DBus",
			"NameOwnerChanged", on_owner_changed, NULL);
	}
	if (r >= 0) {
		r = sd_bus_request_name_async(bus, NULL, SS_NAME, 0,
			on_name_acquired, NULL);
	}
	if (r < 0) {
		wlr_log(WLR_ERROR, "screensaver: %s — only Wayland clients "
			"can inhibit idle", strerror(-r));
		sd_bus_flush_close_unref(bus);
		bus = NULL;
		return;
	}

	bus_source = wl_event_loop_add_fd(server.wl_event_loop,
		sd_bus_get_fd(bus), WL_EVENT_READABLE, bus_ready, NULL);
	bus_timer = wl_event_loop_add_timer(server.wl_event_loop,
		bus_timeout, NULL);
	if (!bus_source || !bus_timer) {
		ss_teardown();
		return;
	}
	bus_arm();
}

void
kdos_screensaver_finish(void)
{
	ss_teardown();
}
