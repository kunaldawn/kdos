/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   fakebus — the session's other ends, for kdos-notice's bus tests
 *
 *   fakebus notify LOG   org.freedesktop.Notifications: each Notify is a
 *                        line `notify <id> <replaces> <timeout> <summary>`
 *                        and each CloseNotification `close <id>` in LOG.
 *                        io.kdos.Test.Invoke(u id, s action) emits
 *                        ActivationToken and ActionInvoked as the server
 *   fakebus forge ID ACTION
 *                        the same two signals from a connection that does
 *                        not own the name, then exits
 *   fakebus shell        holds org.kde.plasmashell; sd-bus answers Ping
 *   fakebus slots LOG    io.kdos.Slots1 at /io/kdos/Slots1: each Bless is
 *                        a line `bless <unix seconds>` in LOG
 *
 * Each runs until killed, on the bus $DBUS_SESSION_BUS_ADDRESS names.
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

static const char *g_log;
static uint32_t g_next = 1;

static void log_line(const char *fmt, ...)
{
	FILE *f = fopen(g_log, "a");
	va_list ap;

	if (!f)
		return;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fclose(f);
}

static int m_notify(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *app, *icon, *summary, *body;
	uint32_t replaces, id;
	int32_t timeout;
	int r;

	(void)ud;
	(void)err;
	r = sd_bus_message_read(m, "susss", &app, &replaces, &icon, &summary,
				&body);
	if (r < 0)
		return r;
	if ((r = sd_bus_message_skip(m, "asa{sv}")) < 0 ||
	    (r = sd_bus_message_read(m, "i", &timeout)) < 0)
		return r;
	id = replaces ? replaces : g_next++;
	log_line("notify %u %u %d %s\n", id, replaces, timeout, summary);
	return sd_bus_reply_method_return(m, "u", id);
}

static int m_close(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	uint32_t id;
	int r;

	(void)ud;
	(void)err;
	if ((r = sd_bus_message_read(m, "u", &id)) < 0)
		return r;
	log_line("close %u\n", id);
	sd_bus_emit_signal(sd_bus_message_get_bus(m),
			   "/org/freedesktop/Notifications",
			   "org.freedesktop.Notifications",
			   "NotificationClosed", "uu", id, 3u);
	return sd_bus_reply_method_return(m, "");
}

static int m_caps(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return sd_bus_reply_method_return(m, "as", 2, "actions", "body");
}

static int m_info(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	return sd_bus_reply_method_return(m, "ssss", "fakebus", "KDOS", "1",
					  "1.2");
}

static void emit_action(sd_bus *bus, uint32_t id, const char *action)
{
	sd_bus_emit_signal(bus, "/org/freedesktop/Notifications",
			   "org.freedesktop.Notifications", "ActivationToken",
			   "us", id, "token-from-server");
	sd_bus_emit_signal(bus, "/org/freedesktop/Notifications",
			   "org.freedesktop.Notifications", "ActionInvoked",
			   "us", id, action);
}

static int m_invoke(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	uint32_t id;
	const char *action;
	int r;

	(void)ud;
	(void)err;
	if ((r = sd_bus_message_read(m, "us", &id, &action)) < 0)
		return r;
	emit_action(sd_bus_message_get_bus(m), id, action);
	return sd_bus_reply_method_return(m, "");
}

static int m_bless(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	log_line("bless %lld\n", (long long)time(NULL));
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable notify_vt[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", m_notify, 0),
	SD_BUS_METHOD("CloseNotification", "u", "", m_close, 0),
	SD_BUS_METHOD("GetCapabilities", "", "as", m_caps, 0),
	SD_BUS_METHOD("GetServerInformation", "", "ssss", m_info, 0),
	SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
	SD_BUS_SIGNAL("ActionInvoked", "us", 0),
	SD_BUS_SIGNAL("ActivationToken", "us", 0),
	SD_BUS_VTABLE_END
};

static const sd_bus_vtable test_vt[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Invoke", "us", "", m_invoke, 0),
	SD_BUS_VTABLE_END
};

static const sd_bus_vtable slots_vt[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Bless", "", "", m_bless, 0),
	SD_BUS_VTABLE_END
};

int main(int argc, char **argv)
{
	sd_bus *bus = NULL;
	const char *name = NULL;
	int r;

	if (argc < 2 || sd_bus_open_user(&bus) < 0) {
		fprintf(stderr, "usage: fakebus notify|forge|shell|slots ...\n");
		return 2;
	}
	if (!strcmp(argv[1], "notify") && argc > 2) {
		g_log = argv[2];
		name = "org.freedesktop.Notifications";
		sd_bus_add_object_vtable(bus, NULL, "/org/freedesktop/Notifications",
					 "org.freedesktop.Notifications",
					 notify_vt, NULL);
		sd_bus_add_object_vtable(bus, NULL, "/org/freedesktop/Notifications",
					 "io.kdos.Test", test_vt, NULL);
	} else if (!strcmp(argv[1], "forge") && argc > 3) {
		emit_action(bus, (uint32_t)atoi(argv[2]), argv[3]);
		sd_bus_flush_close_unref(bus);
		return 0;
	} else if (!strcmp(argv[1], "shell")) {
		name = "org.kde.plasmashell";
	} else if (!strcmp(argv[1], "slots") && argc > 2) {
		g_log = argv[2];
		name = "io.kdos.Slots1";
		sd_bus_add_object_vtable(bus, NULL, "/io/kdos/Slots1",
					 "io.kdos.Slots1", slots_vt, NULL);
	} else {
		fprintf(stderr, "usage: fakebus notify|forge|shell|slots ...\n");
		return 2;
	}
	if ((r = sd_bus_request_name(bus, name, 0)) < 0) {
		fprintf(stderr, "fakebus: %s: %d\n", name, r);
		return 1;
	}
	for (;;) {
		r = sd_bus_process(bus, NULL);
		if (r < 0)
			return 1;
		if (r == 0 && sd_bus_wait(bus, UINT64_MAX) < 0)
			return 1;
	}
}
