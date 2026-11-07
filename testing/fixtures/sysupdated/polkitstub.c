/*
 * The half of polkitd a mechanism can see: org.freedesktop.PolicyKit1 and one
 * CheckAuthorization, on the bus the test gives it.
 *
 * It exists because what is under test is kdos-sysupdated's half of the
 * conversation — which action it names, whether it lets the agent ask the
 * person, and what it does with each answer — and those are the arguments
 * polkitd receives and the reply it sends, read off polkit's own D-Bus API.
 *
 *   polkitstub RULES LOG
 *
 * RULES holds lines `<action-id> yes|no|challenge`; an action it does not
 * name is answered yes. Every request is appended to LOG as
 *
 *   CHECK <action-id> flags=<n> subject=<kind>:<bus name>
 *
 * The rules are read again on every request, so a test changes the answer by
 * rewriting the file.
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

static const char *rules_path, *log_path;

static const char *answer(const char *action)
{
	static char word[16];
	FILE *f = fopen(rules_path, "r");
	char line[512];

	snprintf(word, sizeof(word), "yes");
	while (f && fgets(line, sizeof(line), f)) {
		char a[400], w[16];
		if (sscanf(line, "%399s %15s", a, w) == 2 && !strcmp(a, action)) {
			snprintf(word, sizeof(word), "%s", w);
			break;
		}
	}
	if (f)
		fclose(f);
	return word;
}

static int check(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *kind = NULL, *action = NULL, *key, *name = "?";
	uint32_t flags = 0;
	int r;

	(void)ud;
	(void)err;
	if ((r = sd_bus_message_enter_container(m, 'r', "sa{sv}")) < 0 ||
	    (r = sd_bus_message_read(m, "s", &kind)) < 0 ||
	    (r = sd_bus_message_enter_container(m, 'a', "{sv}")) < 0)
		return r;
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		if (sd_bus_message_read(m, "s", &key) < 0)
			return -EINVAL;
		if (!strcmp(key, "name")) {
			if (sd_bus_message_read(m, "v", "s", &name) < 0)
				return -EINVAL;
		} else if (sd_bus_message_skip(m, "v") < 0) {
			return -EINVAL;
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
	sd_bus_message_exit_container(m);
	if ((r = sd_bus_message_read(m, "s", &action)) < 0 ||
	    (r = sd_bus_message_skip(m, "a{ss}")) < 0 ||
	    (r = sd_bus_message_read(m, "u", &flags)) < 0)
		return r;

	FILE *l = fopen(log_path, "a");
	if (l) {
		fprintf(l, "CHECK %s flags=%u subject=%s:%s\n", action, flags,
			kind, name);
		fclose(l);
	}
	const char *w = answer(action);
	int yes = !strcmp(w, "yes");
	int challenge = !strcmp(w, "challenge");
	/* A challenge the caller allowed interaction for is what the agent
	 * would turn into a password prompt; here the prompt is answered at
	 * once, as a person typing the right password would. */
	if (challenge && (flags & 1))
		yes = 1, challenge = 0;
	return sd_bus_reply_method_return(m, "(bba{ss})", yes, challenge, 0);
}

static const sd_bus_vtable vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("CheckAuthorization", "(sa{sv})sa{ss}us", "(bba{ss})",
		      check, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

int main(int argc, char **argv)
{
	sd_bus *bus = NULL;

	if (argc != 3) {
		fprintf(stderr, "usage: polkitstub RULES LOG\n");
		return 2;
	}
	rules_path = argv[1];
	log_path = argv[2];
	if (sd_bus_open_user(&bus) < 0 ||
	    sd_bus_add_object_vtable(bus, NULL,
				     "/org/freedesktop/PolicyKit1/Authority",
				     "org.freedesktop.PolicyKit1.Authority",
				     vtable, NULL) < 0 ||
	    sd_bus_request_name(bus, "org.freedesktop.PolicyKit1", 0) < 0) {
		fprintf(stderr, "polkitstub: cannot serve the bus\n");
		return 1;
	}
	for (;;) {
		int r = sd_bus_process(bus, NULL);
		if (r < 0)
			return 0;
		if (r == 0 && sd_bus_wait(bus, (uint64_t)-1) < 0)
			return 0;
	}
}
