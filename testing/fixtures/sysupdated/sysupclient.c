/*
 * Discover's half of org.freedesktop.sysupdate1, in the order Discover 6.8.2
 * makes the calls (libdiscover/backends/SystemdSysupdateBackend):
 *
 *   Ping, ListTargets, then per target GetAppStream, GetVersion, CheckNew;
 *   Acquire("", 0) with interactive authorisation allowed, the job's
 *   PropertiesChanged(Progress) followed until Manager.JobRemoved; then
 *   Install("", 0) and its JobRemoved.
 *
 * It exists because the contract being tested is Discover's, not ours: a
 * client written from the daemon's own idea of the API would agree with the
 * daemon and with nothing else. JobRemoved is subscribed to before Acquire is
 * sent, as Discover subscribes in its backend's constructor, and the job's
 * properties only after the reply names the job.
 *
 *   sysupclient discover     the whole flow
 *   sysupclient cancel       Acquire, then Cancel at the first progress
 *
 * Prints one line per event: PING, TARGET, APPSTREAM, VERSION, CHECKNEW,
 * ACQUIRE, INSTALL, PROGRESS, CANCEL, REMOVED, ERROR, TIMEOUT.
 */

#define _POSIX_C_SOURCE 200809L
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

#define BUS     "org.freedesktop.sysupdate1"
#define PATH    "/org/freedesktop/sysupdate1"
#define MANAGER "org.freedesktop.sysupdate1.Manager"
#define TARGET  "org.freedesktop.sysupdate1.Target"
#define JOB     "org.freedesktop.sysupdate1.Job"

static sd_bus *bus;
static char job_path[256];
static int removed, removed_status, cancel_mode, cancelled, progress_seen;

static int on_removed(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	uint64_t id;
	const char *path;
	int32_t status;
	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "toi", &id, &path, &status) < 0)
		return 0;
	printf("REMOVED %llu %s %d\n", (unsigned long long)id, path, status);
	fflush(stdout);
	removed = 1;
	removed_status = status;
	return 0;
}

static int on_props(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *iface, *name;
	(void)ud;
	(void)err;
	if (strcmp(sd_bus_message_get_path(m), job_path))
		return 0;
	if (sd_bus_message_read(m, "s", &iface) < 0 || strcmp(iface, JOB))
		return 0;
	if (sd_bus_message_enter_container(m, 'a', "{sv}") < 0)
		return 0;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		if (sd_bus_message_read(m, "s", &name) < 0)
			return 0;
		if (!strcmp(name, "Progress")) {
			uint32_t p;
			if (sd_bus_message_read(m, "v", "u", &p) < 0)
				return 0;
			printf("PROGRESS %u\n", p);
			fflush(stdout);
			progress_seen = 1;
		} else {
			sd_bus_message_skip(m, "v");
		}
		sd_bus_message_exit_container(m);
	}
	return 0;
}

static void print_error(const sd_bus_error *e)
{
	printf("ERROR %s %s\n", e->name ? e->name : "?",
	       e->message ? e->message : "");
	fflush(stdout);
}

/* Run the bus until JobRemoved, or 60 seconds. */
static int wait_removed(void)
{
	time_t end = time(NULL) + 60;
	removed = 0;
	while (!removed && time(NULL) < end) {
		int r = sd_bus_process(bus, NULL);
		if (r < 0)
			return -1;
		if (cancel_mode && progress_seen && !cancelled) {
			sd_bus_error e = SD_BUS_ERROR_NULL;
			cancelled = 1;
			if (sd_bus_call_method(bus, BUS, job_path, JOB, "Cancel",
					       &e, NULL, "") < 0)
				print_error(&e);
			else
				printf("CANCEL ok\n");
			fflush(stdout);
			sd_bus_error_free(&e);
			continue;
		}
		if (r == 0)
			sd_bus_wait(bus, 200000);
	}
	if (!removed) {
		printf("TIMEOUT\n");
		return -1;
	}
	return 0;
}

static int start(const char *target, const char *method)
{
	sd_bus_error e = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	const char *ver, *path;
	uint64_t id;

	if (sd_bus_call_method(bus, BUS, target, TARGET, method, &e, &reply,
			       "st", "", (uint64_t)0) < 0) {
		print_error(&e);
		sd_bus_error_free(&e);
		return -1;
	}
	if (sd_bus_message_read(reply, "sto", &ver, &id, &path) < 0) {
		sd_bus_message_unref(reply);
		return -1;
	}
	snprintf(job_path, sizeof(job_path), "%s", path);
	printf("%s %s %llu %s\n", !strcmp(method, "Acquire") ? "ACQUIRE"
							       : "INSTALL",
	       ver, (unsigned long long)id, path);
	fflush(stdout);
	sd_bus_message_unref(reply);
	return wait_removed();
}

static int str_call(const char *path, const char *iface, const char *member,
		    const char *tag)
{
	sd_bus_error e = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	const char *s;
	if (sd_bus_call_method(bus, BUS, path, iface, member, &e, &reply, "") <
	    0) {
		print_error(&e);
		sd_bus_error_free(&e);
		return -1;
	}
	if (sd_bus_message_read(reply, "s", &s) >= 0)
		printf("%s %s\n", tag, s);
	sd_bus_message_unref(reply);
	return 0;
}

int main(int argc, char **argv)
{
	sd_bus_error e = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	char target[256] = "";
	char offer[128] = "";

	if (argc != 2 || (strcmp(argv[1], "discover") &&
			  strcmp(argv[1], "cancel"))) {
		fprintf(stderr, "usage: sysupclient discover|cancel\n");
		return 2;
	}
	cancel_mode = !strcmp(argv[1], "cancel");
	if (sd_bus_open_user(&bus) < 0)
		return 1;
	/* Discover sets this on every Target and Job proxy. */
	sd_bus_set_allow_interactive_authorization(bus, 1);
	sd_bus_match_signal(bus, NULL, BUS, PATH, MANAGER, "JobRemoved",
			    on_removed, NULL);
	sd_bus_add_match(bus, NULL,
			 "type='signal',sender='" BUS "',"
			 "interface='org.freedesktop.DBus.Properties',"
			 "member='PropertiesChanged',"
			 "path_namespace='/org/freedesktop/sysupdate1/job'",
			 on_props, NULL);

	if (sd_bus_call_method(bus, BUS, PATH, "org.freedesktop.DBus.Peer",
			       "Ping", &e, NULL, "") < 0) {
		print_error(&e);
		return 1;
	}
	printf("PING ok\n");

	if (sd_bus_call_method(bus, BUS, PATH, MANAGER, "ListTargets", &e,
			       &reply, "") < 0) {
		print_error(&e);
		return 1;
	}
	if (sd_bus_message_enter_container(reply, 'a', "(sso)") >= 0) {
		const char *cls, *name, *path;
		while (sd_bus_message_read(reply, "(sso)", &cls, &name, &path) >
		       0) {
			printf("TARGET %s %s %s\n", cls, name, path);
			if (!target[0])
				snprintf(target, sizeof(target), "%s", path);
		}
	}
	sd_bus_message_unref(reply);
	reply = NULL;
	if (!target[0])
		return 1;

	if (sd_bus_call_method(bus, BUS, target, TARGET, "GetAppStream", &e,
			       &reply, "") < 0) {
		print_error(&e);
		return 1;
	}
	if (sd_bus_message_enter_container(reply, 'a', "s") >= 0) {
		const char *u;
		while (sd_bus_message_read(reply, "s", &u) > 0)
			printf("APPSTREAM %s\n", u);
	}
	sd_bus_message_unref(reply);
	reply = NULL;

	if (str_call(target, TARGET, "GetVersion", "VERSION") < 0)
		return 1;
	if (sd_bus_call_method(bus, BUS, target, TARGET, "CheckNew", &e, &reply,
			       "") < 0) {
		print_error(&e);
		return 1;
	}
	const char *s;
	if (sd_bus_message_read(reply, "s", &s) >= 0) {
		printf("CHECKNEW %s\n", s);
		snprintf(offer, sizeof(offer), "%s", s);
	}
	sd_bus_message_unref(reply);
	fflush(stdout);
	if (!offer[0])
		return 0;

	if (start(target, "Acquire") < 0)
		return 1;
	if (removed_status != 0 || cancel_mode)
		return 0;
	if (start(target, "Install") < 0)
		return 1;
	return 0;
}
