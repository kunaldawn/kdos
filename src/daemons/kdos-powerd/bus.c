/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   bus.c — io.kdos.Power1, kdos-powerd on the system bus
 *
 *     ListFirewallServices() -> a(sbs)     name, open, what it is for
 *     ListFirewallKnocks()   -> a(st)      a closed name, packets turned away
 *     SetFirewallService(s name, b open)   polkit io.kdos.firewall.set
 *     SetAccent(s scheme)                  polkit io.kdos.accent.set
 *     SetSplash(s motion)                  polkit io.kdos.accent.set
 *     StartFileSharing()                   polkit io.kdos.filesharing.start
 *     Accent   s   (read, emits change)    the scheme /etc/kdos/accent names
 *     Accents  as  (read, constant)        every scheme this build knows
 *     FirewallServicesChanged()            after any change, from either face
 *
 * The face System Settings speaks, so a KCM needs no socket client and no
 * setuid helper. The verbs are main.c's; this file adds the bus, the
 * vocabulary of a D-Bus API and polkit in front of each change.
 *
 * EVERY CHANGE IS polkit's DECISION, ASKED ABOUT THE CALLER'S BUS NAME. The
 * subject is `system-bus-name`, which polkitd resolves to the caller's process
 * and session itself, so nothing the caller says about who it is takes part.
 * The shipped defaults (io.kdos.power1.policy) are auth_admin_keep for the
 * firewall and yes for the accent, the splash and starting Samba in an
 * active session;
 * anything a rules file decides instead applies here unchanged. The reads ask
 * nobody: the list is a file every user can read, the knocks are counts of
 * packets that were dropped, and the accent is on the splash every boot.
 *
 * THE QUESTION IS ASKED ASYNCHRONOUSLY. A password typed into polkit's agent
 * takes as long as a person takes, and a daemon blocked in that call would
 * answer neither the socket nor the next KCM until they finished. The method
 * call is kept, polkit's answer resumes it, and the reply goes out from the
 * callback.
 *
 * A CALLER THAT DID NOT ALLOW A PROMPT IS NOT PROMPTED. A challenge it could
 * have answered comes back as InteractiveAuthorizationRequired, the standard
 * error that tells a client to retry with the header flag set, rather than an
 * AccessDenied that reads as a final no.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* basu on the target; libsystemd's sd-bus is the same API and is what a
 * developer host is likely to have. */
#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#else
#  include <systemd/sd-bus.h>
#endif

#include "kbase.h"
#include "kcolor.h"
#include "powerd.h"

#define PK_NAME  "org.freedesktop.PolicyKit1"
#define PK_PATH  "/org/freedesktop/PolicyKit1/Authority"
#define PK_IFACE "org.freedesktop.PolicyKit1.Authority"
#define PK_ALLOW_USER_INTERACTION 1u

#define KP_ERR_NO_SERVICE "io.kdos.Power1.Error.NoSuchService"
#define KP_ERR_NO_ACCENT  "io.kdos.Power1.Error.NoSuchAccent"
#define KP_ERR_NO_MOTION  "io.kdos.Power1.Error.NoSuchMotion"
#define KP_ERR_FAILED     "io.kdos.Power1.Error.Failed"

/* How long a reconnect waits after the bus went away or refused us. The same
 * five seconds ksvc waits before a respawn. */
#define KP_BUS_RETRY_S 5.0
/* An interactive answer is a person typing a password; a non-interactive one
 * is polkitd reading two files. */
#define KP_PK_INTERACTIVE_US (120ULL * 1000000ULL)
#define KP_PK_QUICK_US       (5ULL * 1000000ULL)

static sd_bus *g_bus;
static sd_bus_slot *g_vtable;
static double g_retry_at;
/* Set while the bus is down and the reason has been logged, so a machine with
 * no system bus logs one line, not one every five seconds. */
static int g_logged_down;

static double mono_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ── a method call waiting for polkit ──────────────────────────────────── */

enum { OP_FIREWALL, OP_ACCENT, OP_SPLASH, OP_SHARING };

static const char *op_action(int op)
{
	return op == OP_FIREWALL ? KP_ACT_FIREWALL
	     : op == OP_SHARING  ? KP_ACT_SHARING : KP_ACT_ACCENT;
}

typedef struct KpPending {
	struct KpPending *next;
	sd_bus_message *call;		/* the caller's message, replied later */
	sd_bus_slot *slot;		/* polkit's reply                     */
	int op;
	char arg[64];
	int open;
	int interactive;
} KpPending;

static KpPending *g_pending;

static void pending_drop(KpPending *p)
{
	for (KpPending **pp = &g_pending; *pp; pp = &(*pp)->next) {
		if (*pp == p) {
			*pp = p->next;
			break;
		}
	}
	sd_bus_slot_unref(p->slot);
	sd_bus_message_unref(p->call);
	free(p);
}

/* `err …\n` from a writer, as a D-Bus error message: the reason without the
 * protocol word and the newline. */
static void err_text(const char *line, char *out, size_t cap)
{
	if (!strncmp(line, "err ", 4))
		line += 4;
	kb_strlcpy(out, line, cap);
	out[strcspn(out, "\r\n")] = '\0';
}

static void perform(KpPending *p)
{
	char msg[256], why[256];
	int rc;

	if (p->op == OP_FIREWALL)
		rc = kp_fw_set(p->arg, p->open, msg, sizeof(msg));
	else if (p->op == OP_ACCENT)
		rc = kp_accent_set(p->arg, msg, sizeof(msg));
	else if (p->op == OP_SHARING)
		rc = kp_share_start(msg, sizeof(msg));
	else
		rc = kp_splash_set(p->arg, msg, sizeof(msg));
	if (rc == 0) {
		sd_bus_reply_method_return(p->call, "");
		return;
	}
	err_text(msg, why, sizeof(why));
	sd_bus_reply_method_errorf(p->call, KP_ERR_FAILED, "%s", why);
}

static int pk_reply(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	KpPending *p = userdata;
	int authorized = 0, challenge = 0;
	const char *action = op_action(p->op);

	(void)ret;
	if (sd_bus_message_is_method_error(m, NULL)) {
		/* No polkitd is a refusal, never a grant: a daemon that acted
		 * when it could not ask would be a daemon that acts for anyone
		 * whenever polkit is down. */
		const sd_bus_error *e = sd_bus_message_get_error(m);

		fprintf(stderr, "kdos-powerd: polkit did not answer for %s: "
			"%s\n", action, e && e->message ? e->message : "?");
		sd_bus_reply_method_errorf(p->call, SD_BUS_ERROR_ACCESS_DENIED,
					   "polkit did not answer");
		pending_drop(p);
		return 0;
	}
	if (sd_bus_message_enter_container(m, 'r', "bba{ss}") < 0 ||
	    sd_bus_message_read(m, "bb", &authorized, &challenge) < 0) {
		sd_bus_reply_method_errorf(p->call, SD_BUS_ERROR_ACCESS_DENIED,
					   "polkit's answer did not parse");
		pending_drop(p);
		return 0;
	}
	if (authorized) {
		perform(p);
	} else if (challenge && !p->interactive) {
		sd_bus_reply_method_errorf(p->call,
			SD_BUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED,
			"%s needs authentication; call again allowing it",
			action);
	} else {
		fprintf(stderr, "kdos-powerd: polkit refused %s to %s\n",
			action, sd_bus_message_get_sender(p->call));
		sd_bus_reply_method_errorf(p->call, SD_BUS_ERROR_ACCESS_DENIED,
					   "not authorised for %s", action);
	}
	pending_drop(p);
	return 0;
}

/*
 * Ask polkit about the sender of `call`, then perform `op` from the reply.
 * The details carry what is being changed, so a rules file can decide on it
 * (`action.lookup("service")`) and the agent's dialog can name it.
 */
static int pk_ask(sd_bus_message *call, int op, const char *arg, int open)
{
	const char *sender = sd_bus_message_get_sender(call);
	const char *action = op_action(op);
	sd_bus_message *q = NULL;
	KpPending *p;
	int interactive =
		sd_bus_message_get_allow_interactive_authorization(call) > 0;
	int r;

	if (!sender)
		return sd_bus_reply_method_errorf(call,
			SD_BUS_ERROR_ACCESS_DENIED, "no sender to ask about");
	r = sd_bus_message_new_method_call(g_bus, &q, PK_NAME, PK_PATH,
					   PK_IFACE, "CheckAuthorization");
	if (r >= 0)
		r = sd_bus_message_append(q, "(sa{sv})s", "system-bus-name", 1,
					  "name", "s", sender, action);
	if (r >= 0 && op == OP_FIREWALL)
		r = sd_bus_message_append(q, "a{ss}", 2, "service", arg,
					  "state", open ? "on" : "off");
	else if (r >= 0 && op == OP_SHARING)
		r = sd_bus_message_append(q, "a{ss}", 0);
	else if (r >= 0)
		r = sd_bus_message_append(q, "a{ss}", 1,
					  op == OP_ACCENT ? "accent" : "splash",
					  arg);
	if (r >= 0)
		r = sd_bus_message_append(q, "us",
					  interactive ? PK_ALLOW_USER_INTERACTION
						      : 0u, "");
	if (r < 0) {
		sd_bus_message_unref(q);
		return r;
	}

	p = kb_calloc(1, sizeof(*p));
	p->call = sd_bus_message_ref(call);
	p->op = op;
	p->open = open;
	p->interactive = interactive;
	kb_strlcpy(p->arg, arg, sizeof(p->arg));
	r = sd_bus_call_async(g_bus, &p->slot, q, pk_reply, p,
			      interactive ? KP_PK_INTERACTIVE_US
					  : KP_PK_QUICK_US);
	sd_bus_message_unref(q);
	if (r < 0) {
		sd_bus_message_unref(p->call);
		free(p);
		return r;
	}
	p->next = g_pending;
	g_pending = p;
	/* Positive with no reply sent: the reply goes out from pk_reply. */
	return 1;
}

/* ── the methods and properties ────────────────────────────────────────── */

static int m_list(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	sd_bus_message *reply = NULL;
	int n = kp_fw_count();
	int *on = kb_calloc((size_t)n, sizeof(*on));
	int r;

	(void)userdata;
	(void)ret;
	kp_fw_state(on);
	r = sd_bus_message_new_method_return(m, &reply);
	if (r >= 0)
		r = sd_bus_message_open_container(reply, 'a', "(sbs)");
	for (int i = 0; r >= 0 && i < n; i++)
		r = sd_bus_message_append(reply, "(sbs)", kp_fw_name(i), on[i],
					  kp_fw_what(i));
	if (r >= 0)
		r = sd_bus_message_close_container(reply);
	if (r >= 0)
		r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	free(on);
	return r;
}

static int m_set_fw(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	const char *name;
	int open;
	int r = sd_bus_message_read(m, "sb", &name, &open);

	(void)userdata;
	if (r < 0)
		return r;
	/* Refused before polkit is asked: a prompt for a name that cannot be
	 * opened would be a password typed for nothing. */
	if (kp_fw_find(name) < 0)
		return sd_bus_error_setf(ret, KP_ERR_NO_SERVICE,
					 "no service called %s", name);
	return pk_ask(m, OP_FIREWALL, name, open);
}

static int m_set_accent(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	const char *name;
	int r = sd_bus_message_read(m, "s", &name);

	(void)userdata;
	if (r < 0)
		return r;
	if (!kcol_find(name))
		return sd_bus_error_setf(ret, KP_ERR_NO_ACCENT,
					 "%s is not an accent", name);
	return pk_ask(m, OP_ACCENT, name, 0);
}

static int m_set_splash(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	const char *motion;
	int r = sd_bus_message_read(m, "s", &motion);

	(void)userdata;
	if (r < 0)
		return r;
	if (!kp_splash_ok(motion))
		return sd_bus_error_setf(ret, KP_ERR_NO_MOTION,
					 "%s is not a splash motion (phosphor "
					 "or modern)", motion);
	return pk_ask(m, OP_SPLASH, motion, 0);
}

/*
 * No argument: what starts is fixed (kp_share_start), so there is nothing to
 * check before polkit is asked and nothing for a caller to aim.
 */
static int m_start_sharing(sd_bus_message *m, void *userdata,
			   sd_bus_error *ret)
{
	(void)userdata;
	(void)ret;
	return pk_ask(m, OP_SHARING, "", 0);
}

/*
 * The knock counters, for the Firewall page and kdos-fwask: a counter the
 * kernel could not be asked about is an error rather than an empty list, so
 * a caller can tell "nothing knocked" from "nothing counted".
 */
static int m_knocks(sd_bus_message *m, void *userdata, sd_bus_error *ret)
{
	sd_bus_message *reply = NULL;
	int n = kp_fw_count();
	long long *k = kb_calloc((size_t)n, sizeof(*k));
	int r;

	(void)userdata;
	if (kp_fw_knocks(k) != 0) {
		free(k);
		return sd_bus_error_setf(ret, KP_ERR_FAILED,
					 "the firewall's counters could not be "
					 "read");
	}
	r = sd_bus_message_new_method_return(m, &reply);
	if (r >= 0)
		r = sd_bus_message_open_container(reply, 'a', "(st)");
	for (int i = 0; r >= 0 && i < n; i++)
		if (k[i] >= 0)
			r = sd_bus_message_append(reply, "(st)", kp_fw_name(i),
						  (uint64_t)k[i]);
	if (r >= 0)
		r = sd_bus_message_close_container(reply);
	if (r >= 0)
		r = sd_bus_send(NULL, reply, NULL);
	sd_bus_message_unref(reply);
	free(k);
	return r;
}

static int p_accent(sd_bus *bus, const char *path, const char *iface,
		    const char *prop, sd_bus_message *reply, void *userdata,
		    sd_bus_error *ret)
{
	(void)bus; (void)path; (void)iface; (void)prop;
	(void)userdata; (void)ret;
	return sd_bus_message_append(reply, "s", kp_accent_get());
}

static int p_accents(sd_bus *bus, const char *path, const char *iface,
		     const char *prop, sd_bus_message *reply, void *userdata,
		     sd_bus_error *ret)
{
	int r;

	(void)bus; (void)path; (void)iface; (void)prop;
	(void)userdata; (void)ret;
	r = sd_bus_message_open_container(reply, 'a', "s");
	for (int i = 0; r >= 0 && i < kcol_nscheme; i++)
		r = sd_bus_message_append(reply, "s", kcol_schemes[i].name);
	if (r >= 0)
		r = sd_bus_message_close_container(reply);
	return r;
}

/*
 * UNPRIVILEGED ON EVERY METHOD. Without the flag sd-bus refuses a caller that
 * lacks CAP_SYS_ADMIN before the handler runs, which is every desktop user;
 * polkit is the gate here, and it is asked inside the handler.
 */
static const sd_bus_vtable kp_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("ListFirewallServices", "", "a(sbs)", m_list,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("ListFirewallKnocks", "", "a(st)", m_knocks,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetFirewallService", "sb", "", m_set_fw,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetAccent", "s", "", m_set_accent,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetSplash", "s", "", m_set_splash,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("StartFileSharing", "", "", m_start_sharing,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_PROPERTY("Accent", "s", p_accent, 0,
			SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
	SD_BUS_PROPERTY("Accents", "as", p_accents, 0,
			SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_SIGNAL("FirewallServicesChanged", "", 0),
	SD_BUS_VTABLE_END
};

/* ── the connection ────────────────────────────────────────────────────── */

static void bus_down(const char *what, int r)
{
	if (!g_logged_down)
		fprintf(stderr, "kdos-powerd: %s: %s — serving the socket "
			"alone, retrying every %.0f s\n", what, strerror(-r),
			KP_BUS_RETRY_S);
	g_logged_down = 1;
	g_retry_at = mono_s() + KP_BUS_RETRY_S;
}

int kp_bus_open(void)
{
	sd_bus *b = NULL;
	int r;

	if (g_bus)
		return 0;
	r = sd_bus_open_system(&b);
	if (r < 0) {
		bus_down("the system bus", r);
		return -1;
	}
	r = sd_bus_add_object_vtable(b, &g_vtable, KP_BUS_PATH, KP_BUS_IFACE,
				     kp_vtable, NULL);
	if (r < 0) {
		bus_down("exporting " KP_BUS_PATH, r);
		sd_bus_flush_close_unref(b);
		return -1;
	}
	/* The name last, so nothing can call before the object answers. A
	 * second owner is a second daemon, and this one stays off the bus
	 * rather than queueing behind it. */
	r = sd_bus_request_name(b, KP_BUS_NAME, 0);
	if (r < 0) {
		bus_down("owning " KP_BUS_NAME, r);
		g_vtable = sd_bus_slot_unref(g_vtable);
		sd_bus_flush_close_unref(b);
		return -1;
	}
	g_bus = b;
	if (g_logged_down)
		fprintf(stderr, "kdos-powerd: " KP_BUS_NAME
				" is on the system bus\n");
	g_logged_down = 0;
	return 0;
}

void kp_bus_close(void)
{
	while (g_pending)
		pending_drop(g_pending);
	g_vtable = sd_bus_slot_unref(g_vtable);
	if (g_bus)
		g_bus = sd_bus_flush_close_unref(g_bus);
}

int kp_bus_fd(void)
{
	return g_bus ? sd_bus_get_fd(g_bus) : -1;
}

short kp_bus_events(void)
{
	int e = g_bus ? sd_bus_get_events(g_bus) : 0;

	return e > 0 ? (short)e : 0;
}

int kp_bus_timeout_ms(void)
{
	double now = mono_s();

	if (!g_bus) {
		double left = g_retry_at - now;

		return left <= 0 ? 0 : (int)(left * 1000.0) + 1;
	}

	uint64_t until;

	if (sd_bus_get_timeout(g_bus, &until) < 0 || until == UINT64_MAX)
		return -1;
	/* sd-bus answers on CLOCK_MONOTONIC in microseconds, absolute. */
	double left = (double)until / 1e6 - now;

	return left <= 0 ? 0 : (int)(left * 1000.0) + 1;
}

void kp_bus_dispatch(void)
{
	int r;

	if (!g_bus)
		return;
	while ((r = sd_bus_process(g_bus, NULL)) > 0)
		;
	if (r < 0) {
		/* The bus restarted or went away. Every method call waiting on
		 * polkit died with it: its caller's connection is gone too. */
		kp_bus_close();
		bus_down("lost the system bus", r);
	}
}

void kp_bus_tick(void)
{
	if (!g_bus && mono_s() >= g_retry_at)
		(void)kp_bus_open();
}

/* No arguments: a client re-reads the list, which is the one answer to what
 * is open, rather than keeping a copy that a missed signal would leave
 * wrong. */
void kp_bus_fw_changed(void)
{
	if (g_bus)
		(void)sd_bus_emit_signal(g_bus, KP_BUS_PATH, KP_BUS_IFACE,
					 "FirewallServicesChanged", "");
}

void kp_bus_accent_changed(void)
{
	if (g_bus)
		(void)sd_bus_emit_properties_changed(g_bus, KP_BUS_PATH,
						     KP_BUS_IFACE, "Accent",
						     NULL);
}

/* ── the socket's question ─────────────────────────────────────────────── */

/*
 * /proc/<pid>/stat's 22nd field, the process's start time in clock ticks
 * since boot. polkit takes it with the pid so that a pid recycled between the
 * question and the answer is a different subject; 0 when the process is gone.
 */
static unsigned long long start_time(pid_t pid)
{
	char path[64], buf[1024];
	unsigned long long st = 0;
	const char *close;

	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	if (kb_read_file(path, buf, sizeof(buf)) <= 0)
		return 0;
	/* From the LAST ')', because comm may hold either parenthesis. */
	close = strrchr(buf, ')');
	if (!close || sscanf(close + 2,
			     "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
			     "%*u %*u %*d %*d %*d %*d %*d %*d %llu", &st) != 1)
		return 0;
	return st;
}

int kp_bus_peer_may(pid_t pid, uid_t uid, const char *action)
{
	sd_bus_message *q = NULL, *reply = NULL;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	unsigned long long st;
	int authorized = 0, challenge = 0;
	int r;

	if (!g_bus || pid <= 0)
		return 0;
	st = start_time(pid);
	if (!st)
		return 0;
	r = sd_bus_message_new_method_call(g_bus, &q, PK_NAME, PK_PATH,
					   PK_IFACE, "CheckAuthorization");
	if (r >= 0)
		r = sd_bus_message_append(q, "(sa{sv})s", "unix-process", 3,
					  "pid", "u", (uint32_t)pid,
					  "start-time", "t", (uint64_t)st,
					  "uid", "i", (int32_t)uid, action);
	if (r >= 0)
		r = sd_bus_message_append(q, "a{ss}us", 0, 0u, "");
	/* Synchronous and short: this is polkitd reading its rules, with no
	 * agent to wait for, and the socket's client is waiting on it. */
	if (r >= 0)
		r = sd_bus_call(g_bus, q, KP_PK_QUICK_US, &err, &reply);
	if (r >= 0 && sd_bus_message_enter_container(reply, 'r', "bba{ss}") >= 0)
		r = sd_bus_message_read(reply, "bb", &authorized, &challenge);
	else if (r < 0 && sd_bus_error_is_set(&err))
		fprintf(stderr, "kdos-powerd: polkit did not answer for %s: "
			"%s\n", action, err.message ? err.message : err.name);
	sd_bus_error_free(&err);
	sd_bus_message_unref(reply);
	sd_bus_message_unref(q);
	return r >= 0 && authorized ? 1 : 0;
}
