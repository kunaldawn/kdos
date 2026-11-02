/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   pkstub — polkitd's CheckAuthorization, scripted, on a private bus
 *
 *     PKSTUB_ANSWERS="io.kdos.firewall.set=challenge io.kdos.accent.set=yes" \
 *         pkstub <log> <seconds>
 *
 * The other end of a daemon that asks polkit, for testing/selftest.sh: it owns
 * org.freedesktop.PolicyKit1 on whatever DBUS_SYSTEM_BUS_ADDRESS names and
 * answers each CheckAuthorization from PKSTUB_ANSWERS. `yes` and `no` are
 * polkit's two verdicts; `challenge` is an action that needs a password, which
 * is granted when the caller passed AllowUserInteraction — an agent that was
 * asked and said yes — and comes back as a challenge when it did not. A word
 * keyed `<subject kind>:<action>` (`unix-process:io.kdos.accent.set=yes`)
 * answers for that kind of subject alone and wins over the bare action. An
 * action the list does not name is `no`.
 *
 * Every question is written to <log> as one line, so a test can assert what
 * was asked as well as what was answered:
 *
 *     CHECK system-bus-name io.kdos.firewall.set flags=1 service=mosh state=on
 *     CHECK unix-process uid=1000 io.kdos.accent.set flags=0
 *
 * It exits after <seconds>, or on SIGTERM.
 */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

static FILE *g_log;
static volatile sig_atomic_t g_stop;

static void on_term(int sig)
{
	(void)sig;
	g_stop = 1;
}

/* The word PKSTUB_ANSWERS gives `action`, or "no". */
static const char *lookup(const char *action)
{
	const char *all = getenv("PKSTUB_ANSWERS");
	size_t al = strlen(action);

	for (const char *p = all ? all : ""; *p;) {
		const char *end = p + strcspn(p, " ");

		if ((size_t)(end - p) > al + 1 && !strncmp(p, action, al) &&
		    p[al] == '=') {
			static char word[32];
			size_t wl = (size_t)(end - p) - al - 1;

			if (wl >= sizeof(word))
				wl = sizeof(word) - 1;
			memcpy(word, p + al + 1, wl);
			word[wl] = '\0';
			return word;
		}
		p = *end ? end + 1 : end;
	}
	return NULL;
}

static const char *answer_for(const char *kind, const char *action)
{
	char keyed[256];
	const char *a;

	snprintf(keyed, sizeof(keyed), "%s:%s", kind, action);
	a = lookup(keyed);
	if (!a)
		a = lookup(action);
	return a ? a : "no";
}

static int check(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	const char *kind, *action, *cancel;
	char subj[128] = "";
	char details[256] = "";
	uint32_t flags;
	int r;

	(void)userdata;
	(void)ret;
	r = sd_bus_message_enter_container(m, 'r', "sa{sv}");
	if (r < 0)
		return r;
	if ((r = sd_bus_message_read(m, "s", &kind)) < 0)
		return r;
	if ((r = sd_bus_message_enter_container(m, 'a', "{sv}")) < 0)
		return r;
	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		const char *key;
		char t;
		const char *contents;

		if ((r = sd_bus_message_read(m, "s", &key)) < 0)
			return r;
		if ((r = sd_bus_message_peek_type(m, &t, &contents)) < 0)
			return r;
		if (!strcmp(key, "uid") && !strcmp(contents, "i")) {
			int32_t uid;

			if ((r = sd_bus_message_read(m, "v", "i", &uid)) < 0)
				return r;
			snprintf(subj, sizeof(subj), " uid=%d", (int)uid);
		} else if ((r = sd_bus_message_skip(m, "v")) < 0) {
			return r;
		}
		if ((r = sd_bus_message_exit_container(m)) < 0)
			return r;
	}
	if (r < 0)
		return r;
	if ((r = sd_bus_message_exit_container(m)) < 0 ||
	    (r = sd_bus_message_exit_container(m)) < 0)
		return r;
	if ((r = sd_bus_message_read(m, "s", &action)) < 0)
		return r;
	if ((r = sd_bus_message_enter_container(m, 'a', "{ss}")) < 0)
		return r;
	for (;;) {
		const char *k, *v;
		size_t used = strlen(details);

		r = sd_bus_message_read(m, "{ss}", &k, &v);
		if (r <= 0)
			break;
		snprintf(details + used, sizeof(details) - used, " %s=%s", k, v);
	}
	if (r < 0)
		return r;
	if ((r = sd_bus_message_exit_container(m)) < 0)
		return r;
	if ((r = sd_bus_message_read(m, "us", &flags, &cancel)) < 0)
		return r;

	fprintf(g_log, "CHECK %s%s %s flags=%u%s\n", kind, subj, action,
		(unsigned)flags, details);
	fflush(g_log);

	const char *a = answer_for(kind, action);
	int yes = !strcmp(a, "yes") ||
		  (!strcmp(a, "challenge") && (flags & 1u));
	int challenge = !strcmp(a, "challenge") && !(flags & 1u);

	return sd_bus_reply_method_return(m, "(bba{ss})", yes, challenge, 0);
}

static const sd_bus_vtable vt[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("CheckAuthorization", "(sa{sv})sa{ss}us", "(bba{ss})",
		      check, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

int main(int argc, char **argv)
{
	sd_bus *bus = NULL;
	time_t until;
	int r;

	if (argc != 3) {
		fprintf(stderr, "usage: pkstub <log> <seconds>\n");
		return 2;
	}
	g_log = fopen(argv[1], "a");
	if (!g_log)
		return 1;
	until = time(NULL) + atoi(argv[2]);
	signal(SIGTERM, on_term);
	if ((r = sd_bus_open_system(&bus)) < 0 ||
	    (r = sd_bus_add_object_vtable(bus, NULL,
				"/org/freedesktop/PolicyKit1/Authority",
				"org.freedesktop.PolicyKit1.Authority",
				vt, NULL)) < 0 ||
	    (r = sd_bus_request_name(bus, "org.freedesktop.PolicyKit1", 0)) < 0) {
		fprintf(stderr, "pkstub: %s\n", strerror(-r));
		return 1;
	}
	while (!g_stop && time(NULL) < until) {
		r = sd_bus_process(bus, NULL);
		if (r < 0)
			break;
		if (r == 0)
			(void)sd_bus_wait(bus, 200000);
	}
	sd_bus_flush_close_unref(bus);
	fclose(g_log);
	return 0;
}
