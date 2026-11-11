/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos toggle — the switches a desktop needs at hand
 *
 *   kdos toggle                 list them and their state
 *   kdos toggle <name>          flip it
 *   kdos toggle <name> on|off   set it
 *
 * EACH SWITCH IS THE PLASMA SURFACE'S OWN, so the applet that shows it shows
 * what this set and a click there undoes it here:
 *
 *   stay-awake   an inhibition of sleep and of the screen locker, held by a
 *                process of ours for as long as the switch is on — the
 *                battery applet lists it as blocking. PowerDevil drops an
 *                inhibition the moment its holder leaves the bus, so the
 *                holder IS the state, and its pid file says where it is.
 *   night-light  kwinrc [NightColor] Active, the key the Night Light page
 *                writes and KWin watches, with Mode=Constant on the way on, so
 *                the screen warms at once and all day rather than on the
 *                evening schedule. KWin's "Toggle Night Color" only suspends
 *                and resumes; it is pressed only to lift a suspension that
 *                would hide the switch being on.
 *   dnd          plasmanotifyrc [DoNotDisturb] Until, the key the
 *                notification applet's own switch writes, set far ahead;
 *                deleted to switch it off.
 *   reduced-motion
 *                kdeglobals [KDE] AnimationDurationFactor=0 in the person's
 *                own file, which stops every animation KWin, Plasma and the
 *                Phosphor motion draw; off deletes the key, so the shipped
 *                factor applies again rather than a copy of it.
 *
 * A CHORD HAS NOWHERE TO PRINT, so off a terminal the new state is a
 * notification instead.
 * ---------------------------------
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "kbase.h"
#include "kdos-tools.h"

static const struct {
	const char *name;
	const char *what;
} TOGGLES[] = {
	{ "stay-awake", "never sleep, lock or blank on idle" },
	{ "night-light", "warm the screen (KWin's Night Light)" },
	{ "dnd", "hold notifications back (Do Not Disturb)" },
	{ "reduced-motion", "no animations (AnimationDurationFactor 0)" },
};

#define NTOGGLES ((int)(sizeof(TOGGLES) / sizeof(TOGGLES[0])))

/* Far enough ahead that it reads as "until switched off". KConfig writes a
 * date-time as six integers, and this is that spelling. */
#define DND_FOREVER "2100,1,1,0,0,0"

/* ── stay-awake ────────────────────────────────────────────────────────── */

static char *awake_pidfile(void)
{
	return kb_path_join(kb_runtime_dir(), "kdos/stay-awake.pid");
}

/* The holder's pid when one is alive, is ours and is this program, else 0. A
 * pid file outlives a holder killed with the session, and the number may by
 * then be another of the person's processes, which `off` would signal. */
static pid_t awake_pid(void)
{
	char *pf = awake_pidfile(), buf[32] = "", path[64];
	char comm[32], self[32];
	struct stat st;
	pid_t pid = 0;

	if (kb_read_line_file(pf, buf, sizeof(buf)) > 0)
		pid = (pid_t)atol(buf);
	free(pf);
	if (pid <= 1)
		return 0;
	snprintf(path, sizeof(path), "/proc/%ld", (long)pid);
	if (stat(path, &st) != 0 || st.st_uid != getuid())
		return 0;
	/* By name, not by executable: an update replaces the binary under a
	 * holder that is still running, and it must stay one `off` can stop. */
	snprintf(path, sizeof(path), "/proc/%ld/comm", (long)pid);
	if (kb_read_line_file(path, comm, sizeof(comm)) <= 0 ||
	    kb_read_line_file("/proc/self/comm", self, sizeof(self)) <= 0 ||
	    strcmp(comm, self))
		return 0;
	return pid;
}

static volatile sig_atomic_t awake_stop;

static void awake_term(int sig)
{
	(void)sig;
	awake_stop = 1;
}

/*
 * The holder: its own bus connection, both inhibitions, and then nothing
 * until it is told to stop. Readiness — or the reason it could not take an
 * inhibition — is written down `fd` so the command that started it can say
 * which.
 */
static void awake_hold(int fd)
{
	KdtBus *b = kdt_bus_open(0);
	KdtMsg *r = NULL;
	char err[256] = "";
	unsigned cookie = 0;
	int held = 0;

	if (b && kdt_bus_call(b, "org.freedesktop.PowerManagement",
			      "/org/freedesktop/PowerManagement/Inhibit",
			      "org.freedesktop.PowerManagement.Inhibit",
			      "Inhibit", &r, err, sizeof(err), "ss", "kdos",
			      "kdos toggle stay-awake") == 0 &&
	    kdt_bus_read(b, r, "u", &cookie) > 0)
		held++;
	kdt_bus_free(b, r);
	r = NULL;
	if (b && kdt_bus_call(b, "org.freedesktop.ScreenSaver",
			      "/org/freedesktop/ScreenSaver",
			      "org.freedesktop.ScreenSaver", "Inhibit", &r,
			      held ? NULL : err, held ? 0 : sizeof(err), "ss",
			      "kdos", "kdos toggle stay-awake") == 0)
		held++;
	kdt_bus_free(b, r);
	if (!held) {
		dprintf(fd, "%s\n", *err ? err : "no power manager answered");
		close(fd);
		kdt_bus_close(b);
		_exit(1);
	}
	dprintf(fd, "ok\n");
	close(fd);
	signal(SIGTERM, awake_term);
	signal(SIGINT, awake_term);
	signal(SIGHUP, SIG_IGN);
	while (!awake_stop)
		pause();
	kdt_bus_close(b);
	_exit(0);
}

static int awake_set(int on, char *why, size_t n)
{
	pid_t pid = awake_pid();
	char *pf;
	int p[2];
	char buf[256] = "";
	ssize_t got;

	if (!on) {
		if (pid)
			kill(pid, SIGTERM);
		pf = awake_pidfile();
		unlink(pf);
		free(pf);
		return 0;
	}
	if (pid)
		return 0;
	if (pipe(p) != 0) {
		snprintf(why, n, "%s", strerror(errno));
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		snprintf(why, n, "%s", strerror(errno));
		return -1;
	}
	if (pid == 0) {
		/* Its own session, so the terminal that started it closing does
		 * not take the inhibition with it. */
		close(p[0]);
		setsid();
		if (fork() > 0)
			_exit(0);
		{
			char *pfile = awake_pidfile(), line[32];

			kdt_mkparent(pfile);
			snprintf(line, sizeof(line), "%ld\n", (long)getpid());
			kb_write_file_atomic(pfile, line);
			free(pfile);
		}
		if (!freopen("/dev/null", "r", stdin) ||
		    !freopen("/dev/null", "w", stdout) ||
		    !freopen("/dev/null", "w", stderr))
			_exit(1);
		awake_hold(p[1]);
	}
	close(p[1]);
	waitpid(pid, NULL, 0);
	got = read(p[0], buf, sizeof(buf) - 1);
	close(p[0]);
	if (got > 0)
		buf[got] = '\0';
	buf[strcspn(buf, "\n")] = '\0';
	if (strcmp(buf, "ok")) {
		pf = awake_pidfile();
		unlink(pf);
		free(pf);
		snprintf(why, n, "%s", *buf ? buf : "the holder did not start");
		return -1;
	}
	return 0;
}

/* ── night light ───────────────────────────────────────────────────────── */

/* KWin's Night Light on the bus; 0 when it answers, -1 when it does not. */
static int night_prop(const char *prop, int *val)
{
	KdtBus *b = kdt_bus_open(0);
	KdtMsg *r = NULL;
	int rc = -1;

	*val = 0;
	if (b && kdt_bus_get_prop(b, "org.kde.KWin", "/org/kde/KWin/NightLight",
				  "org.kde.KWin.NightLight", prop, "b", &r, NULL,
				  0) == 0 &&
	    kdt_bus_read(b, r, "b", val) > 0)
		rc = 0;
	kdt_bus_free(b, r);
	kdt_bus_close(b);
	return rc;
}

/* On is enabled and not suspended: `running` alone is false by day under a
 * schedule, and `enabled` alone is true while the person's suspension holds
 * the screen cold. With no KWin to ask, the configuration says. */
static int night_on(void)
{
	int en, inh;
	char *v;

	if (night_prop("enabled", &en) == 0 &&
	    night_prop("inhibited", &inh) == 0)
		return en && !inh;
	v = kdt_kcfg_cascade("kwinrc", "NightColor", "Active");
	en = kdt_kcfg_bool(v, 0);
	free(v);
	return en;
}

static int night_press(char *why, size_t n)
{
	KdtBus *b = kdt_bus_open(0);
	int rc = kdt_bus_call(b, "org.kde.kglobalaccel", "/component/kwin",
			      "org.kde.kglobalaccel.Component", "invokeShortcut",
			      NULL, why, n, "s", "Toggle Night Color");

	kdt_bus_close(b);
	if (rc != 0 && !*why)
		snprintf(why, n, "kglobalaccel did not answer");
	return rc;
}

/* The suspension KWin's key takes is a flag of its own on top of a count any
 * program may add to, so a press that leaves Night Light still suspended has
 * turned the flag on: it is pressed again to put it back, and the switch
 * reports what holds it. */
static int night_resume(char *why, size_t n)
{
	int inh = 0;
	struct timespec ts = { 0, 100 * 1000 * 1000 };

	if (night_prop("inhibited", &inh) != 0 || !inh)
		return 0;
	if (night_press(why, n) != 0)
		return -1;
	for (int i = 0; i < 20; i++) {
		if (night_prop("inhibited", &inh) != 0 || !inh)
			return 0;
		nanosleep(&ts, NULL);
	}
	night_press(why, n);
	snprintf(why, n, "another program is holding Night Light off");
	return -1;
}

static int night_set(int on, char *why, size_t n)
{
	if (on && kdt_kcfg_set("kwinrc", "NightColor", "Mode", "Constant", 1)) {
		snprintf(why, n, "kwriteconfig6 could not write kwinrc");
		return -1;
	}
	if (kdt_kcfg_set("kwinrc", "NightColor", "Active",
			 on ? "true" : "false", 1) != 0) {
		snprintf(why, n, "kwriteconfig6 could not write kwinrc");
		return -1;
	}
	return on ? night_resume(why, n) : 0;
}

/* ── do not disturb ────────────────────────────────────────────────────── */

static int dnd_on(void)
{
	char *v = kdt_kcfg_user("plasmanotifyrc", "DoNotDisturb", "Until");
	int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0, on = 0;

	if (v && sscanf(v, "%d,%d,%d,%d,%d,%d", &y, &mo, &d, &h, &mi, &s) >= 3) {
		struct tm tm = { 0 };
		time_t until;

		tm.tm_year = y - 1900;
		tm.tm_mon = mo - 1;
		tm.tm_mday = d;
		tm.tm_hour = h;
		tm.tm_min = mi;
		tm.tm_sec = s;
		tm.tm_isdst = -1;
		until = mktime(&tm);
		on = until > time(NULL);
	}
	free(v);
	return on;
}

static int dnd_set(int on, char *why, size_t n)
{
	if (kdt_kcfg_set("plasmanotifyrc", "DoNotDisturb", "Until",
			 on ? DND_FOREVER : NULL, 1) != 0) {
		snprintf(why, n, "kwriteconfig6 could not write plasmanotifyrc");
		return -1;
	}
	return 0;
}

/* ── reduced motion ────────────────────────────────────────────────────── */

static int motion_reduced(void)
{
	char *v = kdt_kcfg_user("kdeglobals", "KDE", "AnimationDurationFactor");
	int on = v && atof(v) == 0.0 && *v;

	free(v);
	return on;
}

static int motion_set(int on, char *why, size_t n)
{
	if (kdt_kcfg_set("kdeglobals", "KDE", "AnimationDurationFactor",
			 on ? "0" : NULL, 1) != 0) {
		snprintf(why, n, "kwriteconfig6 could not write kdeglobals");
		return -1;
	}
	return 0;
}

/* ── the verb ──────────────────────────────────────────────────────────── */

static int state(int i)
{
	switch (i) {
	case 0: return awake_pid() != 0;
	case 1: return night_on();
	case 2: return dnd_on();
	default: return motion_reduced();
	}
}

int cmd_toggle(int argc, char **argv)
{
	char why[256] = "";
	int which = -1, on, rc;

	if (argc < 1) {
		for (int i = 0; i < NTOGGLES; i++)
			printf("%-14s %-3s  %s\n", TOGGLES[i].name,
			       state(i) ? "on" : "off", TOGGLES[i].what);
		return 0;
	}
	for (int i = 0; i < NTOGGLES; i++)
		if (!strcmp(TOGGLES[i].name, argv[0]))
			which = i;
	if (which < 0 || argc > 2) {
		fprintf(stderr, "kdos toggle: no such toggle '%s' — try: "
				"kdos toggle\n", argv[0]);
		return 2;
	}
	if (argc < 2)
		on = !state(which);
	else if (!strcmp(argv[1], "on"))
		on = 1;
	else if (!strcmp(argv[1], "off"))
		on = 0;
	else {
		fprintf(stderr, "kdos toggle: %s takes on or off\n", argv[0]);
		return 2;
	}

	rc = which == 0 ? awake_set(on, why, sizeof(why))
	   : which == 1 ? night_set(on, why, sizeof(why))
	   : which == 2 ? dnd_set(on, why, sizeof(why))
			: motion_set(on, why, sizeof(why));
	if (rc != 0) {
		fprintf(stderr, "kdos toggle: %s: %s\n", argv[0], why);
		return 1;
	}
	if (isatty(STDOUT_FILENO)) {
		printf("%s %s\n", argv[0], on ? "on" : "off");
	} else {
		char sum[64];

		snprintf(sum, sizeof(sum), "%s %s", argv[0], on ? "on" : "off");
		kb_notify("kdos", sum, TOGGLES[which].what);
	}
	return 0;
}
