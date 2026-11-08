/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settingsd — hostname1, timedate1 and locale1 on the system bus
 *
 * KDE changes the machine's name, clock, time zone, locale and keyboard by
 * calling three interfaces systemd defined: org.freedesktop.hostname1,
 * org.freedesktop.timedate1 and org.freedesktop.locale1. The Date & Time
 * settings page saves through timedate1 and nothing else; plasma-setup uses
 * all three; the login screen's compositor runs `kwin_wayland --locale1` and
 * takes its keyboard layout from locale1; startplasma copies that layout into
 * the session. There is no systemd here, so this daemon serves the three over
 * the files KDOS already keeps (store.c says which).
 *
 * FOUR RULES, each a way a daemon like this is got wrong:
 *
 *   - CHECK, THEN ASK, THEN WRITE. A method validates its arguments before
 *     polkit is asked, so a bad value is refused without a password prompt,
 *     and polkit answers before a single byte is written.
 *   - POLKIT IS ASKED WITHOUT WAITING. An interactive check lasts as long as
 *     somebody takes to type a password, and a daemon blocked in it answers
 *     nobody else — not the login screen's compositor reading the layout, not
 *     the panel reading the zone. The question goes out asynchronously and
 *     the call is answered when polkit is.
 *   - THE ACTION IDS ARE systemd's, EXACTLY. plasma-setup's polkit rule grants
 *     its user `org.freedesktop.timedate1.set-timezone` and the rest by name,
 *     and the Date & Time page authorises `…timedate1.set-time` once for the
 *     three calls it makes; a renamed action would be one nobody grants.
 *   - NO SHELL. A program this runs — `service`, `loadkeys`, `iw` — is an
 *     argument vector, because every argument it is given came over a bus.
 *
 * STARTED BY 43_settingsd.sh, NEVER BY ACTIVATION: a daemon the supervisor
 * runs ships no D-Bus activation file, or a caller that came early would make
 * the bus start a second, unsupervised copy (service_helper).
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "bus.h"

#define KS_VERSION "0.1.0"

/* The exit status restarting cannot change: another process owns one of the
 * names. 43_settingsd.sh gives it to `supervise --final-exit`, so the
 * supervisor stops instead of retrying every five seconds for ever. */
#define KS_EXIT_TAKEN 3

/* How long an interactive polkit question may stay open. The agent's dialog
 * has no timeout of its own, and a question nobody answers would otherwise
 * hold its KsAuth for the life of the daemon. */
#define KS_AUTH_WAIT_LONG (300ULL * 1000000ULL)

#define POLKIT_FLAG_ALLOW_INTERACTION 1u

static sd_bus *g_bus;

sd_bus *ks_bus(void)
{
	return g_bus;
}

uint64_t ks_now_mono(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000;
}

int ks_run(const char *prog, const char *a1, const char *a2, const char *a3)
{
	KbArgv a = { 0 };

	if (!kb_have_prog(prog))
		return -1;
	kb_argv_add(&a, prog);
	if (a1)
		kb_argv_add(&a, a1);
	if (a2)
		kb_argv_add(&a, a2);
	if (a3)
		kb_argv_add(&a, a3);
	kb_argv_end(&a);
	return kb_run(&a);
}

/* ── polkit ──────────────────────────────────────────────────────────────── */

KsAuth *ks_auth_new(sd_bus_message *call, ks_apply_fn apply, int interactive)
{
	KsAuth *a = kb_calloc(1, sizeof(*a));

	a->call = sd_bus_message_ref(call);
	a->apply = apply;
	/* The argument, or the message's own flag: a caller may ask for a
	 * prompt either way, and systemd honours both. */
	a->interactive = interactive ||
		sd_bus_message_get_allow_interactive_authorization(call) > 0;
	a->since = ks_now_mono();
	return a;
}

static void auth_free(KsAuth *a)
{
	sd_bus_message_unref(a->call);
	free(a->text);
	free(a);
}

static void auth_finish(KsAuth *a, bool yes, bool challenge, const char *why)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r;

	if (yes) {
		r = a->apply(a, &err);
		if (r < 0 && !sd_bus_error_is_set(&err))
			sd_bus_error_set_errno(&err, -r);
		if (sd_bus_error_is_set(&err))
			sd_bus_reply_method_error(a->call, &err);
		else if (r == 0)
			sd_bus_reply_method_return(a->call, NULL);
	} else if (challenge && !a->interactive) {
		/* Not a refusal: the caller may ask again with interactive set,
		 * and this name is how it knows to. */
		sd_bus_reply_method_errorf(a->call,
			SD_BUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED,
			"%s needs authentication, and the call did not allow "
			"a prompt", a->action);
	} else {
		sd_bus_reply_method_errorf(a->call, SD_BUS_ERROR_ACCESS_DENIED,
					   "%s: %s", a->action, why);
	}
	sd_bus_error_free(&err);
	auth_free(a);
}

static int auth_reply(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	KsAuth *a = userdata;
	int yes = 0, challenge = 0;

	(void)ret;
	if (sd_bus_message_is_method_error(m, NULL)) {
		const sd_bus_error *e = sd_bus_message_get_error(m);

		/* No polkit is no permission: a missing authority must never
		 * read as an open door. */
		fprintf(stderr, "kdos-settingsd: polkit: %s\n",
			e && e->message ? e->message : "no answer");
		auth_finish(a, false, false, "polkit did not answer");
		return 0;
	}
	if (sd_bus_message_enter_container(m, 'r', "bba{ss}") < 0 ||
	    sd_bus_message_read(m, "bb", &yes, &challenge) < 0) {
		auth_finish(a, false, false, "polkit's answer did not parse");
		return 0;
	}
	auth_finish(a, yes, challenge, "not authorised");
	return 0;
}

int ks_auth_run(KsAuth *a, const char *action)
{
	const char *sender = sd_bus_message_get_sender(a->call);
	sd_bus_message *m = NULL;
	sd_bus_creds *c = NULL;
	uid_t uid = (uid_t)-1;
	int r;

	a->action = action;
	if (!sender) {
		auth_finish(a, false, false, "the call has no sender");
		return 1;
	}
	/* The uid the bus recorded when the caller connected, which the caller
	 * cannot choose. dbus-daemon's UnixUserID is the EFFECTIVE uid, and
	 * sd-bus files it as that: asked for the real uid instead, it has
	 * none, and root would be sent to polkit like anyone else. A failure
	 * to read it is not root. */
	r = sd_bus_query_sender_creds(a->call, SD_BUS_CREDS_EUID, &c);
	if (r >= 0)
		r = sd_bus_creds_get_euid(c, &uid);
	sd_bus_creds_unref(c);
	if (r >= 0 && uid == 0) {
		auth_finish(a, true, false, NULL);
		return 1;
	}

	/* The subject is the caller's unique bus name, which polkit resolves
	 * to a process and a session itself; a pid sent from here could be
	 * recycled between the call and the check. */
	r = sd_bus_message_new_method_call(g_bus, &m,
		"org.freedesktop.PolicyKit1",
		"/org/freedesktop/PolicyKit1/Authority",
		"org.freedesktop.PolicyKit1.Authority", "CheckAuthorization");
	if (r >= 0)
		r = sd_bus_message_append(m, "(sa{sv})sa{ss}us",
			"system-bus-name", 1, "name", "s", sender, action, 0,
			a->interactive ? POLKIT_FLAG_ALLOW_INTERACTION : 0u, "");
	if (r >= 0)
		r = sd_bus_message_set_allow_interactive_authorization(
			m, a->interactive);
	if (r >= 0)
		r = sd_bus_call_async(g_bus, NULL, m, auth_reply, a,
				      a->interactive ? KS_AUTH_WAIT_LONG : 0);
	sd_bus_message_unref(m);
	if (r < 0) {
		fprintf(stderr, "kdos-settingsd: cannot ask polkit: %s\n",
			strerror(-r));
		auth_finish(a, false, false, "polkit could not be asked");
	}
	return 1;
}

/* ── the loop ─────────────────────────────────────────────────────────── */

static void usage(FILE *f)
{
	fprintf(f,
		"usage: kdos-settingsd [--version]\n"
		"\n"
		"Serves org.freedesktop.hostname1, org.freedesktop.timedate1 and\n"
		"org.freedesktop.locale1 on the system bus. Started at boot by\n"
		"/etc/init.d/43_settingsd.sh; it takes no options.\n");
}

int main(int argc, char **argv)
{
	static const char *const NAMES[] = {
		KS_HOSTNAME_IF, KS_TIMEDATE_IF, KS_LOCALE_IF, NULL
	};
	sd_bus *bus = NULL;
	sigset_t mask;
	int sfd, r;

	kb_set_progname("kdos-settingsd");
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--version")) {
			printf("kdos-settingsd " KS_VERSION "\n");
			return 0;
		}
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage(stdout);
			return 0;
		}
		usage(stderr);
		return 2;
	}
	/* Every file this writes is root's. A test tree is the one place an
	 * unprivileged copy has anything to write. */
	if (geteuid() != 0 && !ks_test_tree()) {
		fprintf(stderr, "kdos-settingsd: must run as root\n");
		return 1;
	}

	/* A peer gone mid-reply is an error code, not a signal. */
	signal(SIGPIPE, SIG_IGN);
	umask(022);

	/* `service` and `iw` live in /usr/sbin. A copy started with no PATH
	 * would find them through kb_have_prog's own list and then fail to
	 * exec them through the C library's, which has no sbin in it; one
	 * PATH for both keeps the check and the exec agreeing. */
	if (!getenv("PATH") || !*getenv("PATH"))
		setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:"
		       "/usr/bin:/sbin:/bin", 1);

	/* SIGTERM through a descriptor the loop polls: a flag set by a handler
	 * between the check and the poll would wait for the next message, and
	 * the supervisor's stop would end in SIGKILL. The children run with an
	 * empty mask (kb_child_reset_signals). */
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (sfd < 0) {
		fprintf(stderr, "kdos-settingsd: signalfd: %s\n", strerror(errno));
		return 1;
	}

	r = sd_bus_open_system(&bus);
	if (r < 0) {
		fprintf(stderr, "kdos-settingsd: no system bus: %s\n",
			strerror(-r));
		return 1;
	}
	g_bus = bus;

	if ((r = ks_hostname_add(bus)) < 0 || (r = ks_timedate_add(bus)) < 0 ||
	    (r = ks_locale_add(bus)) < 0)
		goto fail;
	ks_hostname_init();

	/* The names LAST, after every interface exists: the login screen's
	 * compositor asks for the layout the moment locale1 appears, and a call
	 * that landed before the object was there would come back as
	 * UnknownObject — and US QWERTY at the password prompt. */
	for (int i = 0; NAMES[i]; i++) {
		r = sd_bus_request_name(bus, NAMES[i], 0);
		if (r == -EEXIST) {
			fprintf(stderr, "kdos-settingsd: %s is owned by another "
				"process\n", NAMES[i]);
			sd_bus_flush_close_unref(bus);
			return KS_EXIT_TAKEN;
		}
		if (r < 0)
			goto fail;
	}

	for (;;) {
		struct pollfd fds[2];
		struct signalfd_siginfo si;
		uint64_t usec = 0;
		int timeout = -1;

		r = sd_bus_process(bus, NULL);
		if (r < 0)
			goto fail;
		if (r > 0)
			continue;

		fds[0].fd = sd_bus_get_fd(bus);
		fds[0].events = (short)sd_bus_get_events(bus);
		fds[0].revents = 0;
		fds[1].fd = sfd;
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		if (sd_bus_get_timeout(bus, &usec) >= 0 && usec != UINT64_MAX) {
			uint64_t now = ks_now_mono();

			timeout = usec > now ? (int)((usec - now + 999) / 1000) : 0;
		}
		if (poll(fds, 2, timeout) < 0) {
			if (errno == EINTR)
				continue;
			r = -errno;
			goto fail;
		}
		if (fds[1].revents && read(sfd, &si, sizeof(si)) == sizeof(si))
			break;
	}
	sd_bus_flush_close_unref(bus);
	close(sfd);
	return 0;

fail:
	fprintf(stderr, "kdos-settingsd: %s\n", strerror(-r));
	sd_bus_flush_close_unref(bus);
	close(sfd);
	return 1;
}
