/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-notifyd — org.freedesktop.Notifications, as character cells
 *
 *   ╔══════════════════════════════╗
 *   ║ Battery low                  ║
 *   ║ 9% remaining                 ║
 *   ╚══════════════════════════════╝
 *
 * The session's notification daemon. Every alien app expects one to exist:
 * without a bus name owning org.freedesktop.Notifications, a GTK app's
 * `notify_notification_show` fails and — worse — anything using gdbus waits out
 * its default 25-SECOND reply timeout first. kdos-appbox already carries a
 * comment about that timeout for the same reason.
 *
 * TWO EVENT SOURCES, ONE POLL. A notification daemon is idle almost always, so
 * it waits on the Wayland fd and the bus fd together rather than polling either
 * on a timer. The third deadline is the next toast's expiry, which is what the
 * poll timeout is computed from — so an idle session wakes this process exactly
 * never, and a session with a 5-second toast wakes it once.
 */

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * KDOS ships basu, which is sd-bus extracted from systemd; a development host
 * usually has libsystemd, whose sd-bus is the same API. Selecting between them
 * here rather than in the build is what lets this daemon be exercised on a
 * machine that is not KDOS — and every defect found in this rewrite was found
 * by running something, not by reading it.
 */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kwl.h"
#include "shell.h"

#define MAX_TOASTS 4
#define TOAST_COLS 40

struct toast {
	uint32_t id;
	char summary[128];
	char body[256];
	char app[64];
	int64_t expires_ms;	/* monotonic; 0 = never */
	int urgent;
};

static struct toast toasts[MAX_TOASTS];
static int ntoasts;
static uint32_t next_id = 1;
static sd_bus *bus;

static int64_t now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ── the stack ─────────────────────────────────────────────────────────── */

static void emit_closed(uint32_t id, uint32_t reason)
{
	if (bus)
		sd_bus_emit_signal(bus, "/org/freedesktop/Notifications",
				   "org.freedesktop.Notifications",
				   "NotificationClosed", "uu", id, reason);
}

/* reason: 1 expired, 2 dismissed by the user, 3 closed by a CloseNotification
 * call. The spec requires the signal either way, and a client that is waiting
 * for it before it will show another notification hangs without it. */
static void drop_at(int i, uint32_t reason)
{
	if (i < 0 || i >= ntoasts)
		return;
	emit_closed(toasts[i].id, reason);
	memmove(&toasts[i], &toasts[i + 1],
		(size_t)(ntoasts - i - 1) * sizeof(toasts[0]));
	ntoasts--;
}

static void expire_due(void)
{
	int64_t t = now_ms();
	for (int i = ntoasts - 1; i >= 0; i--)
		if (toasts[i].expires_ms && toasts[i].expires_ms <= t)
			drop_at(i, 1);
}

/* How long until something needs doing, or -1 to wait forever. */
static int next_timeout(void)
{
	int64_t best = -1, t = now_ms();
	for (int i = 0; i < ntoasts; i++) {
		if (!toasts[i].expires_ms)
			continue;
		int64_t d = toasts[i].expires_ms - t;
		if (d < 0)
			d = 0;
		if (best < 0 || d < best)
			best = d;
	}
	return best < 0 ? -1 : (int)best;
}

/* ── the bus interface ─────────────────────────────────────────────────── */

static int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	const char *app, *icon, *summary, *body;
	uint32_t replaces;
	int32_t timeout;
	int r;

	(void)userdata;
	(void)err;

	r = sd_bus_message_read(m, "susss", &app, &replaces, &icon, &summary,
				&body);
	if (r < 0)
		return r;
	/* actions (as) and hints (a{sv}) are read past rather than parsed: this
	 * daemon draws text and has no buttons, so advertising `actions` in
	 * GetCapabilities would be a promise it cannot keep. */
	r = sd_bus_message_skip(m, "as");
	if (r < 0)
		return r;
	r = sd_bus_message_skip(m, "a{sv}");
	if (r < 0)
		return r;
	r = sd_bus_message_read(m, "i", &timeout);
	if (r < 0)
		return r;

	struct toast *t = NULL;
	if (replaces) {
		for (int i = 0; i < ntoasts; i++)
			if (toasts[i].id == replaces)
				t = &toasts[i];
	}
	if (!t) {
		/*
		 * Full: drop the OLDEST, not the newest. A stack that refuses
		 * new notifications when it is full shows you stale ones while
		 * hiding the thing that just happened.
		 */
		if (ntoasts == MAX_TOASTS)
			drop_at(0, 1);
		t = &toasts[ntoasts++];
		memset(t, 0, sizeof(*t));
		t->id = replaces ? replaces : next_id++;
	}

	snprintf(t->app, sizeof(t->app), "%s", app ? app : "");
	snprintf(t->summary, sizeof(t->summary), "%s", summary ? summary : "");
	snprintf(t->body, sizeof(t->body), "%s", body ? body : "");
	/*
	 * -1 means "the server decides"; 0 means "never expire", and that is
	 * honoured rather than clamped — a "battery critical" notification is
	 * supposed to stay up.
	 */
	t->expires_ms = timeout == 0	? 0
			: timeout < 0	? now_ms() + 5000
					: now_ms() + timeout;

	return sd_bus_reply_method_return(m, "u", t->id);
}

static int method_close(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	uint32_t id;
	int r;
	(void)userdata;
	(void)err;

	r = sd_bus_message_read(m, "u", &id);
	if (r < 0)
		return r;
	for (int i = 0; i < ntoasts; i++)
		if (toasts[i].id == id) {
			drop_at(i, 3);
			break;
		}
	return sd_bus_reply_method_return(m, "");
}

static int method_caps(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	(void)userdata;
	(void)err;
	/*
	 * `body` and nothing else. No `actions` (there are no buttons), no
	 * `body-markup` (the cell grid has no bold), no `icon-static` (there is
	 * nowhere to put a pixmap in a text row). A daemon that overstates its
	 * capabilities gets sent markup it then draws as literal `<b>` tags.
	 */
	return sd_bus_reply_method_return(m, "as", 1, "body");
}

static int method_info(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	(void)userdata;
	(void)err;
	return sd_bus_reply_method_return(m, "ssss", "kdos-notifyd", "KDOS",
					  "0.1.0", "1.2");
}

static const sd_bus_vtable notify_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", method_notify,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CloseNotification", "u", "", method_close,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetCapabilities", "", "as", method_caps,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetServerInformation", "", "ssss", method_info,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
	SD_BUS_SIGNAL("ActionInvoked", "us", 0),
	SD_BUS_VTABLE_END
};

/* ── drawing ───────────────────────────────────────────────────────────── */

static void draw_toasts(void)
{
	int w = ktui_w, h = ktui_h;
	if (w < 4 || h < 2)
		return;

	ktui_draw_clear();
	int y = 0;
	for (int i = 0; i < ntoasts && y + 3 <= h; i++) {
		const struct toast *t = &toasts[i];
		int rows = t->body[0] ? 4 : 3;
		if (y + rows > h)
			break;
		KRect r = krect(0, y, w, rows);
		int accent = t->urgent ? KT_ERR : KT_ACCENT;

		ktui_draw_fill(r, KT_SURFACE);
		ktui_draw_box(r, NULL, accent, KT_SURFACE, 1);
		ktui_draw_text(2, y + 1, w - 4, t->summary, KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
		if (t->body[0])
			ktui_draw_text(2, y + 2, w - 4, t->body, KT_DIM,
				       KT_SURFACE, KT_A_NONE);
		y += rows;
	}
	ktui_draw_flush();
}

/* ── main ──────────────────────────────────────────────────────────────── */

int notifyd_main(int argc, char **argv)
{
	const char *font = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-notifyd [--font NAME]\n");
			return 2;
		}
	}

	int r = sd_bus_open_user(&bus);
	if (r < 0) {
		fprintf(stderr, "kdos-notifyd: no session bus: %s\n",
			strerror(-r));
		return 1;
	}
	r = sd_bus_add_object_vtable(bus, NULL, "/org/freedesktop/Notifications",
				     "org.freedesktop.Notifications",
				     notify_vtable, NULL);
	if (r < 0) {
		fprintf(stderr, "kdos-notifyd: cannot export the interface: %s\n",
			strerror(-r));
		return 1;
	}
	/*
	 * No REPLACE_EXISTING: if something already owns the name it is doing
	 * this job, and two daemons answering Notify means every notification
	 * appears twice or once at random.
	 */
	r = sd_bus_request_name(bus, "org.freedesktop.Notifications", 0);
	if (r < 0) {
		fprintf(stderr, "kdos-notifyd: another notification daemon is "
				"already running\n");
		return 1;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = TOAST_COLS,
		.rows = MAX_TOASTS * 4,
		.app_id = "kdos-notifyd",
		.font = font,
	};
	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-notifyd: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	int wl_fd = kwl_fd();
	int bus_fd = sd_bus_get_fd(bus);

	while (!kwl_should_close()) {
		expire_due();
		draw_toasts();

		/* Drain everything already queued before sleeping — sd_bus can
		 * hold several messages, and poll would not report the fd
		 * readable for the ones already read out of it. */
		while (sd_bus_process(bus, NULL) > 0)
			;

		struct pollfd fds[2] = {
			{ .fd = wl_fd, .events = POLLIN },
			{ .fd = bus_fd, .events = POLLIN },
		};
		int r2 = poll(fds, 2, next_timeout());
		if (r2 < 0 && errno != EINTR)
			break;
		if (fds[0].revents)
			kwl_pump();
	}

	sd_bus_unref(bus);
	kwl_shutdown();
	return 0;
}
