/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-fwask — "KDE Connect can't be reached by your phone. Allow it?"
 *
 * Started with the Plasma session (autostart, OnlyShowIn=KDE). Every ten
 * seconds it asks kdos-powerd how many packets each CLOSED named service has
 * turned away (io.kdos.Power1.ListFirewallKnocks), and when a count rose for
 * a service whose program runs as this user, and this user has not been asked
 * about it on this network, it posts one notification with two answers.
 *
 *   Allow                  SetFirewallService(name, true), polkit asking
 *                          for the password as the daemon's check requires
 *   Not on This Network    remembered, and never asked again on this network
 *
 * NAMES, NEVER PORTS. The service names and what each opens are kdos-powerd's
 * table; this program carries only which session program serves a name and
 * the sentence a person reads (fwcore.c). Allow can open nothing the
 * Firewall settings page could not.
 *
 * ALLOW IS FOR EVERY NETWORK. The firewall is one table for the machine, and
 * opening a name on one network only would mean closing it again on every
 * network change, each a fresh password prompt. So the prompt says what
 * Allow does, and what is remembered per network is the question: one prompt
 * per service per NetworkManager connection, whatever the answer was —
 * declined, dismissed, allowed, or the service opened elsewhere while the
 * prompt was up, which withdraws it (FirewallServicesChanged). A network
 * NetworkManager does not name is the one network "-".
 *
 * A KNOCK IS NOT ENOUGH ON ITS OWN. A rising counter with no program behind
 * it is a scan or a neighbour's device, and asking about it teaches people to
 * click Allow. The program must be running as this user (fwcore.c's table).
 *
 * WHAT IT NEVER DOES: open a service without a click, ask twice on one
 * network, read nftables (kdos-powerd does, as root), or run a shell. The
 * one program it starts, System Settings at the Firewall page, is an argument
 * vector.
 *
 * THE MEMORY is $XDG_STATE_HOME/kdos/fwask, one `<network> <service>
 * <answer>` line per answer (fwcore.h). Deleting it asks every question
 * again.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <basu/sd-bus.h>

#include "fwcore.h"
#include "kbase.h"

#define FA_POLL_MS (10 * 1000)
/* A daemon without the counters, or none at all, is looked for again this
 * much later rather than every ten seconds for the whole session. */
#define FA_IDLE_MS (5 * 60 * 1000)
#define FA_CALL_US (2ULL * 1000 * 1000)
/* SetFirewallService waits on a person typing a password. */
#define FA_SET_US (5ULL * 60 * 1000 * 1000)
#define FA_MAX_PROMPTS 8

#define NOTIFY_DEST "org.freedesktop.Notifications"
#define NOTIFY_PATH "/org/freedesktop/Notifications"
#define POWER_DEST "io.kdos.Power1"
#define POWER_PATH "/io/kdos/Power1"
#define NM_DEST "org.freedesktop.NetworkManager"
#define NM_PATH "/org/freedesktop/NetworkManager"

typedef struct {
	int used;
	/* 0 while the Allow it answered is waiting on polkit and the daemon. */
	uint32_t id;
	/* The notification server's unique name: an ActionInvoked from any
	 * other connection is not an answer to this prompt. */
	char server[64];
	char name[FA_NAME];
	char net[FA_NET];
	const FaAsk *ask;
} FaPrompt;

static sd_bus *sysbus, *sesbus;
static FaMemory memory;
static char memory_path[512];
static FaKnocks prev;
static FaPrompt prompts[FA_MAX_PROMPTS];
static char net[FA_NET] = "-";
static char netname[128];
static const char *proc_root = "/proc";
static int poll_ms = FA_POLL_MS;
static int interval_ms = FA_POLL_MS;
static int warned;
static volatile sig_atomic_t quit;

static uint64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void on_signal(int sig)
{
	(void)sig;
	quit = 1;
}

/* ── calls ─────────────────────────────────────────────────────────────── */

static int call(sd_bus *bus, const char *dest, const char *path,
		const char *iface, const char *member, sd_bus_message **reply,
		const char *types, ...)
{
	sd_bus_message *m = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	int r;

	r = sd_bus_message_new_method_call(bus, &m, dest, path, iface, member);
	if (r < 0)
		return r;
	if (types) {
		va_list ap;

		va_start(ap, types);
		r = sd_bus_message_appendv(m, types, ap);
		va_end(ap);
		if (r < 0) {
			sd_bus_message_unref(m);
			return r;
		}
	}
	r = sd_bus_call(bus, m, FA_CALL_US, &err, reply);
	sd_bus_message_unref(m);
	sd_bus_error_free(&err);
	return r;
}

/* One property, read through org.freedesktop.DBus.Properties.Get. */
static int get_prop(const char *dest, const char *path, const char *iface,
		    const char *prop, char type, char *out, size_t n)
{
	sd_bus_message *reply = NULL;
	const char *v = NULL;
	char sig[2] = { type, '\0' };
	int r;

	out[0] = '\0';
	r = call(sysbus, dest, path, "org.freedesktop.DBus.Properties", "Get",
		 &reply, "ss", iface, prop);
	if (r < 0)
		return r;
	r = sd_bus_message_enter_container(reply, 'v', sig);
	if (r >= 0)
		r = sd_bus_message_read_basic(reply, type, &v);
	if (r >= 0 && v)
		kb_strlcpy(out, v, n);
	sd_bus_message_unref(reply);
	return r < 0 ? r : 0;
}

/* ── the network ───────────────────────────────────────────────────────── */

/*
 * Which network this is, by NetworkManager's primary connection: its UUID is
 * the key the answers are remembered under, its name is what the prompt
 * says. No NetworkManager, or no connection, is the one network "-".
 */
static void read_network(void)
{
	char path[256], uuid[64], id[128];

	if (get_prop(NM_DEST, NM_PATH, "org.freedesktop.NetworkManager",
		     "PrimaryConnection", 'o', path, sizeof(path)) < 0 ||
	    !strcmp(path, "/") || !path[0] ||
	    get_prop(NM_DEST, path, "org.freedesktop.NetworkManager.Connection.Active",
		     "Uuid", 's', uuid, sizeof(uuid)) < 0 ||
	    !fa_token_ok(uuid, FA_NET)) {
		kb_strlcpy(net, "-", sizeof(net));
		netname[0] = '\0';
		return;
	}
	kb_strlcpy(net, uuid, sizeof(net));
	if (get_prop(NM_DEST, path, "org.freedesktop.NetworkManager.Connection.Active",
		     "Id", 's', id, sizeof(id)) < 0)
		id[0] = '\0';
	kb_strlcpy(netname, id, sizeof(netname));
}

/* ── the counters ──────────────────────────────────────────────────────── */

static int read_knocks(FaKnocks *out)
{
	sd_bus_message *reply = NULL;
	int r;

	out->n = 0;
	r = call(sysbus, POWER_DEST, POWER_PATH, POWER_DEST,
		 "ListFirewallKnocks", &reply, NULL);
	if (r < 0)
		return r;
	r = sd_bus_message_enter_container(reply, 'a', "(st)");
	while (r >= 0 &&
	       (r = sd_bus_message_enter_container(reply, 'r', "st")) > 0) {
		const char *name = NULL;
		uint64_t packets = 0;

		r = sd_bus_message_read(reply, "st", &name, &packets);
		if (r >= 0)
			r = sd_bus_message_exit_container(reply);
		if (r >= 0 && fa_token_ok(name, FA_NAME) &&
		    out->n < FA_MAX_KNOCKS) {
			kb_strlcpy(out->k[out->n].name, name, FA_NAME);
			out->k[out->n].packets = packets;
			out->n++;
		}
	}
	sd_bus_message_unref(reply);
	return r < 0 ? r : 0;
}

/* ── polkit ────────────────────────────────────────────────────────────── */

/*
 * Would polkit let this user change the firewall — yes, or yes after a
 * password — asked without interaction so no dialog appears for a question
 * the person has not been put yet. polkit that cannot be asked counts as yes:
 * the daemon's own check then answers, and its refusal is reported.
 */
static int polkit_may(void)
{
	sd_bus_message *m = NULL, *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	const char *unique = NULL;
	int authorised = 0, challenge = 0, r;

	if (sd_bus_get_unique_name(sysbus, &unique) < 0)
		return 1;
	r = sd_bus_message_new_method_call(sysbus, &m, "org.freedesktop.PolicyKit1",
					   "/org/freedesktop/PolicyKit1/Authority",
					   "org.freedesktop.PolicyKit1.Authority",
					   "CheckAuthorization");
	if (r >= 0)
		r = sd_bus_message_open_container(m, 'r', "sa{sv}");
	if (r >= 0)
		r = sd_bus_message_append(m, "s", "system-bus-name");
	if (r >= 0)
		r = sd_bus_message_append(m, "a{sv}", 1, "name", "s", unique);
	if (r >= 0)
		r = sd_bus_message_close_container(m);
	if (r >= 0)
		r = sd_bus_message_append(m, "sa{ss}us", "io.kdos.firewall.set",
					  0, 0, "");
	if (r >= 0)
		r = sd_bus_call(sysbus, m, FA_CALL_US, &err, &reply);
	sd_bus_message_unref(m);
	sd_bus_error_free(&err);
	if (r < 0)
		return 1;
	r = sd_bus_message_enter_container(reply, 'r', "bba{ss}");
	if (r >= 0)
		r = sd_bus_message_read(reply, "bb", &authorised, &challenge);
	sd_bus_message_unref(reply);
	if (r < 0)
		return 1;
	return authorised || challenge;
}

/* ── notifications ─────────────────────────────────────────────────────── */

/*
 * One notification. `persistent` is a decision and stays until answered;
 * anything else is the server's default lifetime. The desktop entry names the
 * Firewall settings page, so the notification wears its name and icon and is
 * configured with it. Returns the id, 0 when there is no server, and the
 * server's unique name in `server`.
 */
static uint32_t notify(const char *summary, const char *body,
		       const char *const *actions, int nactions, int persistent,
		       char *server, size_t nserver)
{
	sd_bus_message *m = NULL, *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	uint32_t id = 0;
	int r;

	r = sd_bus_message_new_method_call(sesbus, &m, NOTIFY_DEST, NOTIFY_PATH,
					   NOTIFY_DEST, "Notify");
	if (r >= 0)
		r = sd_bus_message_append(m, "susss", "Firewall", 0,
					  "security-high", summary, body);
	if (r >= 0)
		r = sd_bus_message_open_container(m, 'a', "s");
	for (int i = 0; r >= 0 && i < nactions; i++)
		r = sd_bus_message_append(m, "s", actions[i]);
	if (r >= 0)
		r = sd_bus_message_close_container(m);
	if (r >= 0)
		r = sd_bus_message_append(m, "a{sv}", 2, "desktop-entry", "s",
					  "kcm_kdos_firewall", "urgency", "y",
					  (uint8_t)(persistent ? 1 : 0));
	if (r >= 0)
		r = sd_bus_message_append(m, "i", persistent ? 0 : -1);
	if (r >= 0)
		r = sd_bus_call(sesbus, m, FA_CALL_US, &err, &reply);
	sd_bus_message_unref(m);
	sd_bus_error_free(&err);
	if (r < 0)
		return 0;
	if (sd_bus_message_read(reply, "u", &id) < 0)
		id = 0;
	if (server) {
		const char *s = sd_bus_message_get_sender(reply);

		kb_strlcpy(server, s ? s : "", nserver);
	}
	sd_bus_message_unref(reply);
	return id;
}

static void close_notification(uint32_t id)
{
	sd_bus_message *reply = NULL;

	if (call(sesbus, NOTIFY_DEST, NOTIFY_PATH, NOTIFY_DEST,
		 "CloseNotification", &reply, "u", id) >= 0)
		sd_bus_message_unref(reply);
}

/* ── prompts ───────────────────────────────────────────────────────────── */

static FaPrompt *prompt_for(const char *name)
{
	for (int i = 0; i < FA_MAX_PROMPTS; i++)
		if (prompts[i].used && !strcmp(prompts[i].name, name))
			return &prompts[i];
	return NULL;
}

static FaPrompt *prompt_by_id(uint32_t id)
{
	for (int i = 0; i < FA_MAX_PROMPTS; i++)
		if (prompts[i].used && prompts[i].id && prompts[i].id == id)
			return &prompts[i];
	return NULL;
}

static FaPrompt *prompt_slot(void)
{
	for (int i = 0; i < FA_MAX_PROMPTS; i++)
		if (!prompts[i].used)
			return &prompts[i];
	return NULL;
}

static void remember(const char *net_, const char *name, const char *answer)
{
	if (!memory_path[0])
		return;
	if (fa_memory_record(&memory, memory_path, net_, name, answer) != 0)
		kb_warn("cannot write %s: %s", memory_path, strerror(errno));
}

/* 0 when the prompt is up, -1 when there was no slot or no server. */
static int ask(const FaAsk *a)
{
	static const char *const allow_actions[] = {
		"default", "Firewall Settings",
		"allow", "Allow",
		"deny", "Not on This Network",
	};
	static const char *const info_actions[] = {
		"default", "Firewall Settings",
	};
	FaPrompt *p = prompt_slot();
	char body[768];
	int may;

	if (!p)
		return -1;
	may = polkit_may();
	fa_body(body, sizeof(body), a, netname, may);
	memset(p, 0, sizeof(*p));
	p->id = notify(a->summary, body, may ? allow_actions : info_actions,
		       may ? 6 : 2, 1, p->server, sizeof(p->server));
	if (!p->id)
		return -1;
	p->used = 1;
	p->ask = a;
	kb_strlcpy(p->name, a->service, sizeof(p->name));
	kb_strlcpy(p->net, net, sizeof(p->net));
	/* A person who cannot open it has been told, and that is the answer. */
	if (!may)
		remember(net, a->service, "informed");
	return 0;
}

static int on_set_reply(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	FaPrompt *p = userdata;
	char summary[160], body[512];
	(void)ret;

	if (sd_bus_message_is_method_error(m, NULL)) {
		const sd_bus_error *e = sd_bus_message_get_error(m);
		const char *why = e && e->message && *e->message ? e->message
								 : "kdos-powerd refused it.";

		if (e && e->name && (sd_bus_error_has_name(e, SD_BUS_ERROR_ACCESS_DENIED) ||
				     !strncmp(e->name, "org.freedesktop.PolicyKit1.", 27)))
			why = "You may not change the firewall, or the "
			      "authentication was cancelled.";
		remember(p->net, p->name, "failed");
		snprintf(summary, sizeof(summary), "The firewall was not changed");
		fa_escape(body, sizeof(body), why, 0);
	} else {
		remember(p->net, p->name, "allowed");
		snprintf(summary, sizeof(summary), "%s can now be reached",
			 p->ask->app);
		snprintf(body, sizeof(body),
			 "The firewall lets it in on every network. Close it "
			 "again in Firewall settings.");
	}
	notify(summary, body, NULL, 0, 0, NULL, 0);
	memset(p, 0, sizeof(*p));
	return 0;
}

static void allow(FaPrompt *p)
{
	sd_bus_message *m = NULL;
	int r;

	r = sd_bus_message_new_method_call(sysbus, &m, POWER_DEST, POWER_PATH,
					   POWER_DEST, "SetFirewallService");
	if (r >= 0)
		r = sd_bus_message_append(m, "sb", p->name, 1);
	/* Without it polkit answers auth_admin_keep with a refusal rather than
	 * a password prompt. */
	if (r >= 0)
		r = sd_bus_message_set_allow_interactive_authorization(m, 1);
	if (r >= 0)
		r = sd_bus_call_async(sysbus, NULL, m, on_set_reply, p, FA_SET_US);
	sd_bus_message_unref(m);
	if (r < 0) {
		kb_warn("cannot ask kdos-powerd: %s", strerror(-r));
		memset(p, 0, sizeof(*p));
		return;
	}
	/* Held, with no id, until the reply: the counter keeps rising while
	 * polkit's dialog is open, and that must not post a second prompt. */
	p->id = 0;
}

static void open_settings(void)
{
	KbArgv a = { 0 };

	kb_argv_add(&a, "systemsettings");
	kb_argv_add(&a, "kcm_kdos_firewall");
	kb_argv_end(&a);
	kb_run_detach(&a);
}

static int from_server(sd_bus_message *m, const FaPrompt *p)
{
	const char *s = sd_bus_message_get_sender(m);

	return s && p->server[0] && !strcmp(s, p->server);
}

static int on_action(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	uint32_t id = 0;
	const char *key = NULL;
	FaPrompt *p;
	(void)userdata;
	(void)ret;

	if (sd_bus_message_read(m, "us", &id, &key) < 0 || !key)
		return 0;
	p = prompt_by_id(id);
	if (!p || !from_server(m, p))
		return 0;
	if (!strcmp(key, "allow")) {
		allow(p);
	} else if (!strcmp(key, "deny")) {
		remember(p->net, p->name, "declined");
		memset(p, 0, sizeof(*p));
	} else if (!strcmp(key, "default")) {
		open_settings();
		remember(p->net, p->name, "opened");
		memset(p, 0, sizeof(*p));
	}
	return 0;
}

static int on_closed(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	uint32_t id = 0, reason = 0;
	FaPrompt *p;
	(void)userdata;
	(void)ret;

	if (sd_bus_message_read(m, "uu", &id, &reason) < 0)
		return 0;
	p = prompt_by_id(id);
	if (!p || !from_server(m, p))
		return 0;
	/* 2 is "dismissed by the user": an answer. Expiry or a server that
	 * closed it for its own reasons is not, and the question stands. */
	if (reason == 2)
		remember(p->net, p->name, "dismissed");
	memset(p, 0, sizeof(*p));
	return 0;
}

/* Whether `m` came from the connection that owns io.kdos.Power1 now. A
 * signal carries its sender's unique name only, and any connection on the
 * system bus may emit one with the daemon's path and interface. */
static int from_daemon(sd_bus_message *m)
{
	sd_bus_message *reply = NULL;
	const char *sender = sd_bus_message_get_sender(m), *owner = NULL;
	int same = 0;

	if (!sender)
		return 0;
	if (call(sysbus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		 "org.freedesktop.DBus", "GetNameOwner", &reply, "s",
		 POWER_DEST) < 0)
		return 0;
	if (sd_bus_message_read(reply, "s", &owner) >= 0 && owner)
		same = !strcmp(owner, sender);
	sd_bus_message_unref(reply);
	return same;
}

/*
 * A SERVICE OPENED SOMEWHERE ELSE IS NO LONGER A QUESTION. The Firewall
 * page, `kdos-power firewall` or another login's prompt opened it while this
 * prompt was up, so the prompt is withdrawn and the answer is the one given
 * there. FirewallServicesChanged names no service, so the table is asked
 * again and every prompt whose service it now lists open goes. A prompt
 * whose own Allow is waiting on polkit (no id) is left to on_set_reply,
 * which reports how that call ended.
 */
static int on_fw_changed(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	sd_bus_message *reply = NULL;
	int up = 0, r;
	(void)userdata;
	(void)ret;

	for (int i = 0; i < FA_MAX_PROMPTS; i++)
		up |= prompts[i].used && prompts[i].id;
	if (!up || !from_daemon(m))
		return 0;
	if (call(sysbus, POWER_DEST, POWER_PATH, POWER_DEST,
		 "ListFirewallServices", &reply, NULL) < 0)
		return 0;
	r = sd_bus_message_enter_container(reply, 'a', "(sbs)");
	while (r >= 0 &&
	       (r = sd_bus_message_enter_container(reply, 'r', "sbs")) > 0) {
		const char *name = NULL, *what = NULL;
		int open = 0;
		FaPrompt *p;

		r = sd_bus_message_read(reply, "sbs", &name, &open, &what);
		if (r >= 0)
			r = sd_bus_message_exit_container(reply);
		if (r < 0 || !open || !name || !(p = prompt_for(name)) || !p->id)
			continue;
		close_notification(p->id);
		remember(p->net, p->name, "elsewhere");
		memset(p, 0, sizeof(*p));
	}
	sd_bus_message_unref(reply);
	return 0;
}

/* ── one look ──────────────────────────────────────────────────────────── */

static void step(void)
{
	FaKnocks now;
	char rose[FA_MAX_KNOCKS][FA_NAME];
	int nrose, r;

	r = read_knocks(&now);
	if (r < 0) {
		if (!warned)
			kb_warn("kdos-powerd lists no firewall knocks (%s); "
				"looking again in %d s",
				strerror(-r), FA_IDLE_MS / 1000);
		warned = 1;
		interval_ms = FA_IDLE_MS;
		return;
	}
	warned = 0;
	interval_ms = poll_ms;

	read_network();
	/* A prompt names the network it was asked on; on another network it
	 * is a question about somewhere else, and is withdrawn unanswered. */
	for (int i = 0; i < FA_MAX_PROMPTS; i++)
		if (prompts[i].used && prompts[i].id &&
		    strcmp(prompts[i].net, net)) {
			close_notification(prompts[i].id);
			memset(&prompts[i], 0, sizeof(prompts[i]));
		}

	nrose = fa_knocks_rose(&prev, &now, rose, FA_MAX_KNOCKS);
	if (nrose == 0)
		return;
	/* Another login of the same user may have answered since. */
	if (memory_path[0] && fa_memory_load(&memory, memory_path) < 0)
		kb_warn("cannot read %s", memory_path);
	for (int i = 0; i < nrose; i++) {
		const FaAsk *a = fa_find(rose[i]);

		if (!a || prompt_for(a->service) ||
		    fa_memory_asked(&memory, net, a->service) ||
		    !fa_running(proc_root, a->program, getuid()))
			continue;
		/* No notification server yet — a session still starting —
		 * leaves the knocks counted as unseen, so the next look asks. */
		if (ask(a) != 0)
			for (int j = 0; j < prev.n; j++)
				if (!strcmp(prev.k[j].name, a->service))
					prev.k[j].packets = 0;
	}
}

/* ── the loop ──────────────────────────────────────────────────────────── */

static int process(sd_bus *bus)
{
	int r;

	while ((r = sd_bus_process(bus, NULL)) > 0)
		;
	return r;
}

static int bus_timeout_ms(sd_bus *bus, uint64_t now_us_)
{
	uint64_t t = 0;

	if (sd_bus_get_timeout(bus, &t) < 0 || t == UINT64_MAX)
		return -1;
	if (t <= now_us_)
		return 0;
	return (int)((t - now_us_ + 999) / 1000);
}

int main(int argc, char **argv)
{
	struct sigaction sa = { 0 };
	const char *env;
	uint64_t next;
	int r;

	kb_set_progname("kdos-fwask");
	if (argc > 1) {
		fprintf(stderr, "usage: %s\n", argv[0]);
		return 2;
	}
	/* The tests run it against a fake daemon, a fake /proc and a short
	 * interval; a session never sets either. */
	env = getenv("KDOS_FWASK_PROC");
	if (env && *env)
		proc_root = env;
	env = getenv("KDOS_FWASK_INTERVAL_MS");
	if (env && atoi(env) > 0)
		poll_ms = atoi(env);
	interval_ms = poll_ms;

	sa.sa_handler = on_signal;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);

	r = sd_bus_open_system(&sysbus);
	if (r < 0) {
		kb_warn("no system bus: %s", strerror(-r));
		return 1;
	}
	r = sd_bus_open_user(&sesbus);
	if (r < 0) {
		kb_warn("no session bus: %s", strerror(-r));
		return 1;
	}
	if (!kb_state_path("kdos/fwask", memory_path, sizeof(memory_path)))
		memory_path[0] = '\0';
	if (memory_path[0] && fa_memory_load(&memory, memory_path) < 0)
		kb_warn("cannot read %s", memory_path);

	r = sd_bus_match_signal(sesbus, NULL, NULL, NOTIFY_PATH, NOTIFY_DEST,
				"ActionInvoked", on_action, NULL);
	if (r >= 0)
		r = sd_bus_match_signal(sesbus, NULL, NULL, NOTIFY_PATH,
					NOTIFY_DEST, "NotificationClosed",
					on_closed, NULL);
	if (r < 0) {
		kb_warn("cannot watch notifications: %s", strerror(-r));
		return 1;
	}
	/* No sender in the match: a well-known sender is resolved by the bus
	 * but not by sd-bus's own matching, which sees the unique name the
	 * signal carries. from_daemon() checks the sender instead. */
	r = sd_bus_match_signal(sysbus, NULL, NULL, POWER_PATH, POWER_DEST,
				"FirewallServicesChanged", on_fw_changed, NULL);
	if (r < 0)
		kb_warn("cannot watch the firewall's changes: %s", strerror(-r));

	next = now_ms();
	while (!quit) {
		uint64_t t = now_ms();
		struct pollfd pfd[2];
		int timeout, bt;

		if (t >= next) {
			step();
			t = now_ms();
			next = t + (uint64_t)interval_ms;
		}
		if (process(sesbus) < 0) {
			/* The session bus is the session: when it goes, so does
			 * every person this program could ask. */
			break;
		}
		if (process(sysbus) < 0) {
			kb_warn("lost the system bus");
			break;
		}
		timeout = (int)(next - t);
		bt = bus_timeout_ms(sesbus, t * 1000);
		if (bt >= 0 && bt < timeout)
			timeout = bt;
		bt = bus_timeout_ms(sysbus, t * 1000);
		if (bt >= 0 && bt < timeout)
			timeout = bt;
		pfd[0].fd = sd_bus_get_fd(sesbus);
		pfd[0].events = (short)sd_bus_get_events(sesbus);
		pfd[1].fd = sd_bus_get_fd(sysbus);
		pfd[1].events = (short)sd_bus_get_events(sysbus);
		if (poll(pfd, 2, timeout) < 0 && errno != EINTR) {
			kb_warn("poll: %s", strerror(errno));
			break;
		}
	}

	fa_memory_free(&memory);
	sd_bus_flush_close_unref(sesbus);
	sd_bus_flush_close_unref(sysbus);
	return quit ? 0 : 1;
}
