/*
 * polkit — a stand-in for org.freedesktop.PolicyKit1, for kdos-settingsd's
 * bus fixture.
 *
 *     polkit <answers> <log>
 *
 * It owns the authority's name on the bus DBUS_SYSTEM_BUS_ADDRESS names and
 * answers CheckAuthorization from <answers>, one `<action-id> yes|no|challenge`
 * per line, read again on every call so the driver can change an answer
 * between two calls. `challenge` is what polkit says when a password would
 * settle it: authorised when the call allowed a prompt (as if one had been
 * typed), and "not authorised, but a challenge" when it did not. An action
 * with no line is `no`.
 *
 * Each question is appended to <log> as `<action-id> <flags> <subject-kind>
 * <subject has a unique name>`, which is how the driver asserts that the
 * daemon asked for the right action, with the caller's bus name as the
 * subject, and asked nothing at all for a call it refused on its arguments.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

static const char *answers, *log_path;

static const char *answer_for(const char *action)
{
	static char want[16];
	char line[512], id[256], how[16];
	FILE *f = fopen(answers, "r");

	strcpy(want, "no");
	while (f && fgets(line, sizeof(line), f))
		if (sscanf(line, "%255s %15s", id, how) == 2 && !strcmp(id, action))
			strcpy(want, how);
	if (f)
		fclose(f);
	return want;
}

static int check(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *kind = "", *action = "", *cancel = "", *key, *name = "";
	uint32_t flags = 0;
	const char *how;
	int yes, challenge, r;
	FILE *f;

	(void)u, (void)e;
	r = sd_bus_message_enter_container(m, 'r', "sa{sv}");
	if (r >= 0)
		r = sd_bus_message_read(m, "s", &kind);
	if (r >= 0)
		r = sd_bus_message_enter_container(m, 'a', "{sv}");
	while (r >= 0 && sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		r = sd_bus_message_read(m, "s", &key);
		if (r >= 0 && !strcmp(key, "name"))
			r = sd_bus_message_read(m, "v", "s", &name);
		else if (r >= 0)
			r = sd_bus_message_skip(m, "v");
		if (r >= 0)
			r = sd_bus_message_exit_container(m);
	}
	if (r >= 0)
		r = sd_bus_message_exit_container(m);
	if (r >= 0)
		r = sd_bus_message_exit_container(m);
	if (r >= 0)
		r = sd_bus_message_read(m, "s", &action);
	if (r >= 0)
		r = sd_bus_message_skip(m, "a{ss}");
	if (r >= 0)
		r = sd_bus_message_read(m, "us", &flags, &cancel);
	if (r < 0)
		return r;

	how = answer_for(action);
	f = fopen(log_path, "a");
	if (f) {
		fprintf(f, "%s %u %s %s\n", action, flags, kind,
			name[0] == ':' ? "unique" : "none");
		fclose(f);
	}
	yes = !strcmp(how, "yes") || (!strcmp(how, "challenge") && (flags & 1));
	challenge = !strcmp(how, "challenge") && !(flags & 1);
	return sd_bus_reply_method_return(m, "(bba{ss})", yes, challenge, 0);
}

static const sd_bus_vtable authority[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("CheckAuthorization", "(sa{sv})sa{ss}us", "(bba{ss})",
		      check, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

int main(int argc, char **argv)
{
	sd_bus *bus = NULL;
	int r;

	if (argc != 3) {
		fprintf(stderr, "usage: polkit <answers> <log>\n");
		return 2;
	}
	answers = argv[1];
	log_path = argv[2];
	signal(SIGPIPE, SIG_IGN);
	r = sd_bus_open_system(&bus);
	if (r >= 0)
		r = sd_bus_add_object_vtable(bus, NULL,
			"/org/freedesktop/PolicyKit1/Authority",
			"org.freedesktop.PolicyKit1.Authority", authority, NULL);
	if (r >= 0)
		r = sd_bus_request_name(bus, "org.freedesktop.PolicyKit1", 0);
	while (r >= 0) {
		r = sd_bus_process(bus, NULL);
		if (r > 0)
			continue;
		if (r >= 0)
			r = sd_bus_wait(bus, (uint64_t)-1);
	}
	fprintf(stderr, "polkit stub: %s\n", strerror(-r));
	sd_bus_unref(bus);
	return 1;
}
