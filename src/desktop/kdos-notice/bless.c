/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   bless.c — `kdos-notice --bless`: this boot reached a working desktop
 *
 * The second of the two marks that confirm a trial slot (kdos-sysupdated's
 * bless.c holds the rule; the first mark is the end of rcS). Started from
 * kdos-session-good.desktop in Plasma's last autostart phase, it waits until
 * org.kde.plasmashell has been owned on the session bus and answering a
 * Ping for a whole minute, without a gap, and then calls
 * io.kdos.Slots1.Bless() on the system bus once and exits.
 *
 * A MINUTE OF ANSWERS, NOT A MINUTE OF TIME. A shell that crashes and is
 * restarted — seen as a new owner of the name even when no look fell in the
 * gap — or one that hangs, breaks the run and the minute starts again:
 * a trial slot whose desktop cannot stay up must not be confirmed by a
 * session that merely lasted. A shell that never manages it is given up on
 * after fifteen minutes, and the slot is then not confirmed by this session;
 * kdos-bootctl's attempt counter rolls it back.
 *
 * POLKIT DECIDES WHO MAY BLESS (io.kdos.slots.bless, the active local
 * session only, never with a prompt), so the call carries no interactive
 * flag. A refusal is reported and is final for this session.
 *
 * WHAT'S NEW, ONCE PER VERSION. After the boot is confirmed, the version the
 * running slot holds is compared with the one this account last saw
 * ($XDG_STATE_HOME/kdos/whatsnew); a different one opens
 * `plasma-welcome --pages 90-WhatsNew.qml`, the slot's own release notes,
 * and is recorded. An account's first login records its version and opens
 * nothing: no update happened to it. A boot that is not confirmed shows
 * nothing, because the next boot may roll it back.
 *
 *   KDOS_NOTICE_BLESS_SECONDS    the run of answers wanted (60)
 *   KDOS_NOTICE_BLESS_GIVEUP     when to stop waiting, in seconds (900)
 *   KDOS_SLOTS1_BUS=session      call Bless on the session bus, where a test
 *                                fixture serves io.kdos.Slots1
 * ---------------------------------
 */

#include <errno.h>
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

#include "notice.h"

#define SHELL_NAME  "org.kde.plasmashell"
#define SLOTS_DEST  "io.kdos.Slots1"
#define SLOTS_PATH  "/io/kdos/Slots1"

int kn_bless(void);

static double mono_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static double env_s(const char *var, double def)
{
	const char *v = getenv(var);
	double d = v && *v ? atof(v) : 0;

	return d > 0 ? d : def;
}

/*
 * Does the shell answer now, and which connection is it? The name's owner,
 * then a Ping to that owner; no owner, a timeout or an error are all "no".
 * The owner is what tells a restarted shell from the one that was asked a
 * moment ago: a crash and a restart between two looks leave the name owned
 * at both, by two different connections.
 */
static int shell_answers(sd_bus *bus, uint64_t timeout_us, char *owner,
			 size_t n)
{
	sd_bus_message *m = NULL, *reply = NULL;
	const char *who = NULL;
	int r;

	owner[0] = '\0';
	r = sd_bus_call_method(bus, "org.freedesktop.DBus",
			       "/org/freedesktop/DBus", "org.freedesktop.DBus",
			       "GetNameOwner", NULL, &reply, "s", SHELL_NAME);
	if (r >= 0)
		r = sd_bus_message_read(reply, "s", &who);
	if (r >= 0 && who)
		kb_strlcpy(owner, who, n);
	sd_bus_message_unref(reply);
	if (r < 0 || !owner[0])
		return 0;
	r = sd_bus_message_new_method_call(bus, &m, owner, "/",
					   "org.freedesktop.DBus.Peer", "Ping");
	if (r >= 0)
		r = sd_bus_call(bus, m, timeout_us, NULL, NULL);
	sd_bus_message_unref(m);
	return r >= 0;
}

static int call_bless(void)
{
	const char *which = getenv("KDOS_SLOTS1_BUS");
	sd_bus *sys = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r = which && !strcmp(which, "session") ? sd_bus_open_user(&sys)
						   : sd_bus_open_system(&sys);

	if (r < 0) {
		fprintf(stderr, "kdos-notice: no bus to reach %s: %s\n",
			SLOTS_DEST, strerror(-r));
		return 1;
	}
	r = sd_bus_call_method(sys, SLOTS_DEST, SLOTS_PATH, SLOTS_DEST, "Bless",
			       &err, NULL, "");
	if (r < 0) {
		/* No kdos-sysupdated is a machine without slots to confirm,
		 * not a failure of this session. */
		int absent = sd_bus_error_has_name(&err,
				"org.freedesktop.DBus.Error.ServiceUnknown") ||
			     sd_bus_error_has_name(&err,
				"org.freedesktop.DBus.Error.NameHasNoOwner");

		fprintf(stderr, "kdos-notice: %s.Bless: %s\n", SLOTS_DEST,
			err.message ? err.message : strerror(-r));
		sd_bus_error_free(&err);
		sd_bus_flush_close_unref(sys);
		return absent ? 0 : 1;
	}
	sd_bus_flush_close_unref(sys);
	printf("kdos-notice: the desktop answered for its minute; this boot "
	       "is confirmed\n");
	return 0;
}

static void whats_new(void)
{
	KnBoot b;
	char p[1024], dir[1024], last[64] = "";
	const char *ver;
	KbArgv a = { 0 };

	kn_read_boot(&b);
	if (!b.ok || !b.running[0])
		return;
	ver = b.ver[b.running[0] == 'b'];
	if (!*ver || !kb_state_path("kdos/whatsnew", p, sizeof(p)))
		return;
	kb_read_line_file(p, last, sizeof(last));
	if (!strcmp(last, ver))
		return;
	kb_strlcpy(dir, p, sizeof(dir));
	*strrchr(dir, '/') = '\0';
	if (kb_mkdir_p(dir) != 0)
		return;
	{
		char line[80];
		snprintf(line, sizeof(line), "%s\n", ver);
		if (kb_write_file_atomic(p, line) != 0)
			return;
	}
	if (!last[0])
		return;
	kb_argv_add(&a, "plasma-welcome");
	kb_argv_add(&a, "--pages");
	kb_argv_add(&a, "90-WhatsNew.qml");
	kb_argv_end(&a);
	kb_run_detach(&a);
}

int kn_bless(void)
{
	double want = env_s("KDOS_NOTICE_BLESS_SECONDS", 60);
	double giveup = env_s("KDOS_NOTICE_BLESS_GIVEUP", 900);
	/* Asked often enough that a gap is seen, never so often that the
	 * shell spends anything on it: twelve looks per run, at most one
	 * every five seconds. */
	double step = want / 12 < 5 ? want / 12 : 5;
	double start = mono_s(), since = -1;
	char owner[128], was[128] = "";
	sd_bus *bus = NULL;
	int r;

	if ((r = sd_bus_open_user(&bus)) < 0) {
		fprintf(stderr, "kdos-notice: no session bus: %s\n", strerror(-r));
		return 1;
	}
	for (;;) {
		double now = mono_s();

		if (shell_answers(bus, (uint64_t)(step * 1e6) + 1, owner,
				  sizeof(owner))) {
			if (since < 0 || strcmp(owner, was))
				since = now;
		} else {
			since = -1;
		}
		kb_strlcpy(was, owner, sizeof(was));
		if (since >= 0 && mono_s() - since >= want)
			break;
		if (mono_s() - start >= giveup) {
			fprintf(stderr, "kdos-notice: %s did not answer for %.0f "
				"seconds running within %.0f; this boot is not "
				"confirmed by this session\n", SHELL_NAME, want,
				giveup);
			sd_bus_flush_close_unref(bus);
			return 1;
		}
		{
			double left = step - (mono_s() - now);

			if (left > 0)
				usleep((useconds_t)(left * 1e6));
		}
	}
	sd_bus_flush_close_unref(bus);
	r = call_bless();
	if (r == 0)
		whats_new();
	return r;
}
