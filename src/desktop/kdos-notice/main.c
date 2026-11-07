/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-notice — the system's notices in the Plasma session
 *
 *     kdos-notice           the notices, for the life of the session
 *     kdos-notice --scan    look once, post what is news, and exit
 *     kdos-notice --bless   confirm this boot once Plasma has stayed up
 *
 * Started by the session's autostart (kdos-notice.desktop, OnlyShowIn=KDE),
 * it is an org.freedesktop.Notifications client and nothing else: Plasma
 * draws, groups, keeps the history and honours Do Not Disturb. What it
 * posts and when is decide.c's; where the facts come from is facts.c's.
 *
 * WHEN IT LOOKS. Nothing here polls a command:
 *
 *   /var/lib/kpkg/db changes       a package was installed or removed: ask
 *   /var/lib/kdos/update.json      `kdos restarts` and `kdos cve`, once the
 *                                  database has been quiet for ten seconds,
 *                                  so an update of three hundred packages is
 *                                  one question and not three hundred
 *   /run/kdos/bootstate.json       the slots changed: ask `kdos-bootctl`
 *   every ten seconds              kdos-oomd's kill count, one line over its
 *                                  socket, which is what a kill is visible by
 *   twenty seconds after login     everything, once the session has settled
 *   a new notification server      everything, two seconds after it took
 *                                  org.freedesktop.Notifications: a
 *                                  restarted plasmashell lost the notices
 *                                  that were waiting for a person
 *   every six hours                everything, for a secdb or a trial that
 *                                  changed under a watch that missed it, and
 *                                  for the installer's apps-pending file,
 *                                  which Discover empties
 *
 * WHAT IT RUNS. The facts' commands, and the action a person clicked —
 * Konsole on `kdos cve` or `kdos restarts`, System Settings at System
 * Versions, Discover at "Selected during install" — each an argument vector; logging out is a D-Bus call to
 * Plasma's logout prompt. No shell, and no input from a notification ever
 * becomes an argument: an action is one of the fixed ids decide.c offered.
 *
 * WHOSE SIGNAL. ActionInvoked is a signal, and any connection on the bus can
 * emit one with the server's path and interface. Only one from the current
 * owner of org.freedesktop.Notifications is acted on.
 *
 * ONE PER SESSION: it holds io.kdos.Notice on the session bus, and a second
 * start exits. io.kdos.Notice1.Rescan() looks at everything now, for a
 * script that just changed something and for the tests.
 *
 * The firewall's "Allow on this network?" prompt is kdos-fwask's, beside the
 * Firewall settings page, which shares its client of kdos-powerd.
 * ---------------------------------
 */

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#include "notice.h"

#define KN_BUS        "io.kdos.Notice"
#define KN_PATH       "/io/kdos/Notice"
#define KN_IFACE      "io.kdos.Notice1"
#define NOTIFY_DEST   "org.freedesktop.Notifications"
#define NOTIFY_PATH   "/org/freedesktop/Notifications"
#define CALL_US       (5ULL * 1000 * 1000)

#define OOM_MS        (10 * 1000)
#define FULL_MS       (6LL * 3600 * 1000)
#define REWATCH_MS    (60 * 1000)

int kn_bless(void);

static sd_bus *g_bus;
static KnState g_state;
static char g_boot[64];
static int g_oom_seen = -1;	/* kdos-oomd's count at the last look */
static char g_token[KN_COUNT][256];
static int g_relook;		/* a new notification server: look again */

static long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static long long env_ms(const char *var, long long def)
{
	const char *v = getenv(var);

	return v && *v ? atoll(v) : def;
}

static const char *env_path(const char *var, const char *def)
{
	const char *v = getenv(var);

	return v && *v == '/' ? v : def;
}

/* ── posting ───────────────────────────────────────────────────────────── */

static void post(const KnPost *p)
{
	sd_bus_message *call = NULL, *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	uint32_t id = 0;
	int r;

	if (p->close) {
		if (g_state.id[p->kind])
			sd_bus_call_method(g_bus, NOTIFY_DEST, NOTIFY_PATH,
					   NOTIFY_DEST, "CloseNotification",
					   NULL, NULL, "u", g_state.id[p->kind]);
		g_state.id[p->kind] = 0;
		g_state.key[p->kind][0] = '\0';
		kn_state_save(&g_state);
		return;
	}

	r = sd_bus_message_new_method_call(g_bus, &call, NOTIFY_DEST,
					   NOTIFY_PATH, NOTIFY_DEST, "Notify");
	if (r >= 0)
		r = sd_bus_message_append(call, "susss", "KDOS",
					  (uint32_t)g_state.id[p->kind],
					  p->icon ? p->icon : "", p->summary,
					  p->body);
	if (r >= 0)
		r = sd_bus_message_open_container(call, 'a', "s");
	for (int i = 0; r >= 0 && p->actions[i]; i++)
		r = sd_bus_message_append(call, "s", p->actions[i]);
	/* A click on the notification itself is the first action. */
	if (r >= 0 && p->actions[0])
		r = sd_bus_message_append(call, "ss", "default", p->actions[1]);
	if (r >= 0)
		r = sd_bus_message_close_container(call);
	if (r >= 0)
		r = sd_bus_message_open_container(call, 'a', "{sv}");
	if (r >= 0)
		r = sd_bus_message_append(call, "{sv}", "urgency", "y",
					  (uint8_t)p->urgency);
	if (r >= 0)
		r = sd_bus_message_append(call, "{sv}", "x-kdos-notice", "s",
					  kn_kind_name(p->kind));
	if (r >= 0)
		r = sd_bus_message_close_container(call);
	if (r >= 0)
		r = sd_bus_message_append(call, "i", p->persistent ? 0 : -1);
	if (r >= 0)
		r = sd_bus_call(g_bus, call, CALL_US, &err, &reply);
	if (r >= 0)
		r = sd_bus_message_read(reply, "u", &id);
	if (r < 0) {
		/* Not recorded, so the next look tries again: a session whose
		 * notification server was not up yet still hears it. */
		fprintf(stderr, "kdos-notice: %s: not posted: %s\n",
			kn_kind_name(p->kind),
			err.message ? err.message : strerror(-r));
	} else {
		g_state.id[p->kind] = id;
		kb_strlcpy(g_state.key[p->kind], p->key, sizeof(g_state.key[0]));
		kn_state_save(&g_state);
	}
	sd_bus_error_free(&err);
	sd_bus_message_unref(call);
	sd_bus_message_unref(reply);
}

/* ── looking ───────────────────────────────────────────────────────────── */

static void look_packages(void)
{
	KnRestarts *r = kb_calloc(1, sizeof(*r));
	KnCve *c = kb_calloc(1, sizeof(*c));
	KnPost p;

	kn_read_restarts(r);
	if (kn_decide_restarts(r, g_boot, &g_state, &p))
		post(&p);
	kn_read_cve(c);
	if (kn_decide_cve(c, &g_state, &p))
		post(&p);
	free(r);
	free(c);
}

static void look_boot(void)
{
	KnBoot b;
	KnPost p;

	kn_read_boot(&b);
	if (kn_decide_rollback(&b, &g_state, &p))
		post(&p);
	if (kn_decide_trial(&b, g_boot, &g_state, &p))
		post(&p);
}

static void look_oom(void)
{
	KnOom o;
	KnPost p;

	kn_read_oom(&o);
	if (!o.ok)
		return;
	/* The count kdos-oomd had when this session first asked is the
	 * baseline: a kill before the login is not this session's news. A
	 * count that went down is a restarted daemon. */
	if (g_oom_seen >= 0 && o.kills >= g_oom_seen &&
	    kn_decide_oom(&o, g_oom_seen, &p))
		post(&p);
	g_oom_seen = o.kills;
}

static void look_pending(void)
{
	KnPending pd;
	KnPost p;

	kn_read_pending(&pd);
	if (kn_decide_pending(&pd, &g_state, &p))
		post(&p);
}

static void look_all(void)
{
	look_packages();
	look_boot();
	look_oom();
	look_pending();
}

/* ── actions ───────────────────────────────────────────────────────────── */

static int kind_of_id(uint32_t id)
{
	for (int k = 0; k < KN_COUNT; k++)
		if (id && g_state.id[k] == id)
			return k;
	return -1;
}

/* A signal is the notification server's only when its sender owns the
 * name now. */
static int from_server(sd_bus_message *m)
{
	const char *sender = sd_bus_message_get_sender(m), *owner = NULL;
	sd_bus_message *reply = NULL;
	int ok = 0;

	if (!sender)
		return 0;
	if (sd_bus_call_method(g_bus, "org.freedesktop.DBus",
			       "/org/freedesktop/DBus", "org.freedesktop.DBus",
			       "GetNameOwner", NULL, &reply, "s",
			       NOTIFY_DEST) >= 0 &&
	    sd_bus_message_read(reply, "s", &owner) >= 0)
		ok = owner && !strcmp(owner, sender);
	sd_bus_message_unref(reply);
	return ok;
}

static int on_token(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	uint32_t id;
	const char *token;
	int k;

	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "us", &id, &token) < 0 ||
	    (k = kind_of_id(id)) < 0 || !from_server(m))
		return 0;
	for (const char *c = token; *c; c++)
		if ((unsigned char)*c < 0x21 || (unsigned char)*c > 0x7e)
			return 0;
	kb_strlcpy(g_token[k], token, sizeof(g_token[k]));
	return 0;
}

static int on_action(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	uint32_t id;
	const char *action;
	KbArgv a;
	int k;

	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "us", &id, &action) < 0 ||
	    (k = kind_of_id(id)) < 0 || !from_server(m))
		return 0;
	if (!strcmp(action, KN_ACT_LOGOUT) && k == KN_RESTARTS) {
		sd_bus_call_method_async(g_bus, NULL, "org.kde.LogoutPrompt",
					 "/LogoutPrompt", "org.kde.LogoutPrompt",
					 "promptLogout", NULL, NULL, "");
	} else if (kn_action_argv(k, action, &a)) {
		if (g_token[k][0])
			setenv("XDG_ACTIVATION_TOKEN", g_token[k], 1);
		kb_run_detach(&a);
		unsetenv("XDG_ACTIVATION_TOKEN");
	}
	g_token[k][0] = '\0';
	return 0;
}

static int on_closed(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	uint32_t id, reason;
	int k;

	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "uu", &id, &reason) < 0 ||
	    (k = kind_of_id(id)) < 0 || !from_server(m))
		return 0;
	/* Dismissed or expired: the key stays, so the same state is not
	 * announced again; only the id is gone. */
	g_state.id[k] = 0;
	g_token[k][0] = '\0';
	kn_state_save(&g_state);
	return 0;
}

/* The notification server went away or was replaced. Its ids mean nothing
 * now, and a notice that waits for a person went with it, so that one is
 * forgotten as well and the next look posts it again to the new server. */
static int on_server_owner(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	const char *name, *old, *new;

	(void)ud;
	(void)err;
	if (sd_bus_message_read(m, "sss", &name, &old, &new) < 0 ||
	    strcmp(name, NOTIFY_DEST))
		return 0;
	for (int k = 0; k < KN_COUNT; k++) {
		if (g_state.id[k] && (k == KN_RESTARTS || k == KN_CVE))
			g_state.key[k][0] = '\0';
		g_state.id[k] = 0;
		g_token[k][0] = '\0';
	}
	kn_state_save(&g_state);
	if (new && *new)
		g_relook = 1;
	return 0;
}

static int m_rescan(sd_bus_message *m, void *ud, sd_bus_error *err)
{
	(void)ud;
	(void)err;
	look_all();
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Rescan", "", "", m_rescan, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

/* ── watches ───────────────────────────────────────────────────────────── */

typedef struct {
	const char *var, *def;	/* the path, overridable for tests */
	int dir_itself;		/* watch the path, not its parent  */
	int boot;		/* a change asks kdos-bootctl      */
	char path[512], dir[512], name[128];
	int wd;
} Watch;

static Watch watches[] = {
	{ "KDOS_NOTICE_PKGDB", "/var/lib/kpkg/db", 1, 0, "", "", "", -1 },
	{ "KDOS_NOTICE_UPDATE_JSON", "/var/lib/kdos/update.json", 0, 0, "", "", "", -1 },
	{ "KDOS_NOTICE_BOOTSTATE", "/run/kdos/bootstate.json", 0, 1, "", "", "", -1 },
};

#define NWATCH ((int)(sizeof(watches) / sizeof(watches[0])))

static void watch_add(int ifd)
{
	for (int i = 0; i < NWATCH; i++) {
		Watch *w = &watches[i];
		char *slash;

		if (w->wd >= 0)
			continue;
		kb_strlcpy(w->path, env_path(w->var, w->def), sizeof(w->path));
		kb_strlcpy(w->dir, w->path, sizeof(w->dir));
		if (!w->dir_itself && (slash = strrchr(w->dir, '/'))) {
			kb_strlcpy(w->name, slash + 1, sizeof(w->name));
			*slash = '\0';
		}
		w->wd = inotify_add_watch(ifd, w->dir,
					  IN_CLOSE_WRITE | IN_MOVED_TO |
					  IN_CREATE | IN_DELETE);
	}
}

/* ── the loop ──────────────────────────────────────────────────────────── */

static int serve(int once)
{
	long long settle = env_ms("KDOS_NOTICE_SETTLE_MS", 20000);
	long long quiet = env_ms("KDOS_NOTICE_QUIET_MS", 10000);
	long long oom_every = env_ms("KDOS_NOTICE_OOM_MS", OOM_MS);
	long long t_pkgs = -1, t_boot = -1, t_full, t_oom, t_rewatch;
	int r, ifd;

	signal(SIGPIPE, SIG_IGN);
	kn_boot_id(g_boot, sizeof(g_boot));
	kn_state_load(&g_state);
	for (int k = 0; k < KN_COUNT; k++)
		g_state.id[k] = 0;
	if ((r = sd_bus_open_user(&g_bus)) < 0) {
		fprintf(stderr, "kdos-notice: no session bus: %s\n", strerror(-r));
		return 1;
	}
	if (once) {
		look_all();
		sd_bus_flush_close_unref(g_bus);
		return 0;
	}
	r = sd_bus_request_name(g_bus, KN_BUS, 0);
	if (r < 0) {
		fprintf(stderr, "kdos-notice: %s is taken: %s\n", KN_BUS,
			strerror(-r));
		return r == -EEXIST ? 0 : 1;
	}
	if (sd_bus_add_object_vtable(g_bus, NULL, KN_PATH, KN_IFACE, vtable,
				     NULL) < 0 ||
	    sd_bus_match_signal(g_bus, NULL, NULL, NOTIFY_PATH, NOTIFY_DEST,
				"ActionInvoked", on_action, NULL) < 0 ||
	    sd_bus_match_signal(g_bus, NULL, NULL, NOTIFY_PATH, NOTIFY_DEST,
				"ActivationToken", on_token, NULL) < 0 ||
	    sd_bus_match_signal(g_bus, NULL, NULL, NOTIFY_PATH, NOTIFY_DEST,
				"NotificationClosed", on_closed, NULL) < 0 ||
	    sd_bus_add_match(g_bus, NULL,
			     "type='signal',sender='org.freedesktop.DBus',"
			     "path='/org/freedesktop/DBus',"
			     "interface='org.freedesktop.DBus',"
			     "member='NameOwnerChanged',"
			     "arg0='" NOTIFY_DEST "'", on_server_owner, NULL) < 0) {
		fprintf(stderr, "kdos-notice: cannot listen on the bus\n");
		return 1;
	}
	ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (ifd >= 0)
		watch_add(ifd);

	t_full = now_ms() + settle;
	t_oom = now_ms() + (settle < oom_every ? settle : oom_every);
	t_rewatch = now_ms() + REWATCH_MS;
	for (;;) {
		struct pollfd pfd[2];
		long long now, next;
		uint64_t usec;
		int timeout;

		while ((r = sd_bus_process(g_bus, NULL)) > 0)
			;
		if (r < 0) {
			/* The session ending takes its bus with it, and
			 * that is the end of this program's job, not a
			 * failure. */
			if (r == -ECONNRESET || r == -ENOTCONN)
				break;
			fprintf(stderr, "kdos-notice: the bus failed: %s\n",
				strerror(-r));
			return 1;
		}

		now = now_ms();
		/* Two seconds for the new server to settle before it is
		 * handed the notices again. */
		if (g_relook) {
			g_relook = 0;
			if (t_full > now + 2000)
				t_full = now + 2000;
		}
		if (now >= t_full) {
			look_all();
			t_full = now + FULL_MS;
			t_pkgs = t_boot = -1;
			t_oom = now + oom_every;
		}
		if (t_boot >= 0 && now >= t_boot) {
			look_boot();
			t_boot = -1;
		}
		if (t_pkgs >= 0 && now >= t_pkgs) {
			look_packages();
			t_pkgs = -1;
		}
		if (now >= t_oom) {
			look_oom();
			t_oom = now + oom_every;
		}
		if (ifd >= 0 && now >= t_rewatch) {
			watch_add(ifd);
			t_rewatch = now + REWATCH_MS;
		}

		next = t_full;
		if (t_boot >= 0 && t_boot < next)
			next = t_boot;
		if (t_pkgs >= 0 && t_pkgs < next)
			next = t_pkgs;
		if (t_oom < next)
			next = t_oom;
		if (ifd >= 0 && t_rewatch < next)
			next = t_rewatch;
		if (sd_bus_get_timeout(g_bus, &usec) >= 0 && usec != UINT64_MAX) {
			struct timespec ts;
			uint64_t mono;

			clock_gettime(CLOCK_MONOTONIC, &ts);
			mono = (uint64_t)ts.tv_sec * 1000000 +
			       (uint64_t)ts.tv_nsec / 1000;
			if (usec <= mono)
				next = now;
			else if ((long long)((usec - mono) / 1000) < next - now)
				next = now + (long long)((usec - mono) / 1000);
		}
		timeout = next <= now ? 0 : next - now > INT_MAX ? INT_MAX
					  : (int)(next - now);
		pfd[0].fd = sd_bus_get_fd(g_bus);
		pfd[0].events = (short)sd_bus_get_events(g_bus);
		pfd[1].fd = ifd;
		pfd[1].events = POLLIN;
		if (poll(pfd, ifd >= 0 ? 2 : 1, timeout) < 0 && errno != EINTR) {
			fprintf(stderr, "kdos-notice: poll: %s\n", strerror(errno));
			return 1;
		}
		if (ifd >= 0 && (pfd[1].revents & POLLIN)) {
			char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
			ssize_t n;

			while ((n = read(ifd, buf, sizeof(buf))) > 0) {
				for (char *p = buf; p < buf + n;) {
					struct inotify_event *ev = (void *)p;

					for (int i = 0; i < NWATCH; i++) {
						Watch *w = &watches[i];

						if (ev->wd != w->wd)
							continue;
						if (ev->mask & IN_IGNORED)
							w->wd = -1;
						if (!w->dir_itself &&
						    (!ev->len || strcmp(ev->name, w->name)))
							continue;
						/* The quiet time restarts with
						 * each change: one look after
						 * the last of them. */
						if (w->boot)
							t_boot = now_ms() + 1000;
						else
							t_pkgs = now_ms() + quiet;
					}
					p += sizeof(*ev) + ev->len;
				}
			}
		}
	}
	if (ifd >= 0)
		close(ifd);
	sd_bus_flush_close_unref(g_bus);
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-notice");
	if (argc == 2 && !strcmp(argv[1], "--bless"))
		return kn_bless();
	if (argc == 2 && !strcmp(argv[1], "--scan"))
		return serve(1);
	if (argc > 1) {
		fprintf(stderr, "usage: kdos-notice [--scan | --bless]\n");
		return 2;
	}
	return serve(0);
}
