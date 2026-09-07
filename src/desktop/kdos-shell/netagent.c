/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-netagent — NetworkManager asks, a person answers
 *
 *   ╔═ NetworkManager ═══════════════════════════════════════════╗
 *   ║ Home Wifi                                                  ║
 *   ║ wants its passphrase.                                      ║
 *   ║                                                            ║
 *   ║ ***********_                                               ║
 *   ║                                                            ║
 *   ║ Enter connect  Esc cancel          [ Cancel ]  [ Connect ] ║
 *   ╚════════════════════════════════════════════════════════════╝
 *
 * NetworkManager NEVER PROMPTS. When a profile it is activating is missing a
 * secret it asks the registered agents and fails the activation if none
 * answers, so without this process the only passphrase this desktop can use is
 * one written into the profile at the moment it was created. That is enough
 * for WPA-PSK typed once and nothing else: a passphrase that has been changed,
 * 802.1X enterprise wifi, a VPN with a one-time code and a re-keyed WEP
 * network all end as a silent activation failure.
 *
 * THE SYSTEM BUS, because NetworkManager is a system service and there is no
 * session-bus NetworkManager to register with. `org.freedesktop.NetworkManager
 * .conf` lets any context send to the AgentManager and lets only root send to
 * the SecretAgent interface — root being NetworkManager calling back.
 *
 * THE REPLY IS DEFERRED, not a blocking dialog. The bus handler REFS the
 * message and returns "handled" without replying; the reply is sent when the
 * user presses Enter or Escape. A handler that ran its own event loop would
 * stop answering CancelGetSecrets, which is the one message that arrives while
 * the prompt is up. Same rule bt.c's pairing agent keeps.
 *
 * THE ERROR NAME HAS NO `.Error.` IN IT. libnm builds every D-Bus error name
 * as the interface, a dot, and the enum nick — so a cancelled prompt is
 * `org.freedesktop.NetworkManager.SecretAgent.UserCanceled`. An agent that
 * spells it with `.Error.` is reported to NetworkManager as an unclassified
 * failure and the whole activation is abandoned instead of retried.
 *
 * NOTHING IS ASKED WITHOUT ALLOW_INTERACTION. NetworkManager polls its agents
 * for stored secrets on paths a person is not watching; a prompt raised from
 * one of those is a dialog with no question behind it. Without the flag this
 * answers NoSecrets at once, which is what "I keep no store" means.
 *
 * ONE HUNDRED AND TWENTY SECONDS is NetworkManager's own reply timeout, and it
 * does not send CancelGetSecrets when it expires. So the prompt has a deadline
 * of its own and closes just inside it: a passphrase field left on screen
 * after that is collecting an answer nothing is waiting for.
 *
 * THE WINDOW EXISTS ONLY WHILE ASKING. This process is session-lifetime and
 * idle almost always, so it holds no Wayland connection between prompts; the
 * display is opened per question and shut down after. It is a TOPLEVEL and not
 * an overlay because the compositor focuses a toplevel when it maps and
 * focuses an on-demand layer surface only when it is PRESSED — a passphrase
 * box that appears unbidden and swallows the first keystrokes is worse than
 * none. The cost is a font cache rebuilt per question.
 *
 * THE SECRET GOES INTO THE REPLY AND NOWHERE ELSE — not into argv, not into a
 * file, not into a log line, not into a core file, and not into memory past
 * the reply.
 *
 * THE BUS HAS TO BE THE REAL ONE. `sd_bus_open_system` honours
 * $DBUS_SYSTEM_BUS_ADDRESS, and a program that types a passphrase into
 * whatever answers must not take that address on trust: a socket owned by an
 * ordinary user would be handed the secret by a forged GetSecrets carrying the
 * interaction flag. SO_PEERCRED says who owns it, and only root or the bus
 * daemon's own account is accepted.
 *
 * NOTHING FROM THE REQUEST IS DRAWN AS IT ARRIVED. An SSID is thirty-two
 * arbitrary bytes that NetworkManager copies into `connection.id` unfiltered,
 * so naming an access point is a way to write escape sequences into the
 * console session this box is drawn on. Same rule net.c's take_ssid() keeps.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. The same selection net.c, bt.c and notifyd.c make. */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kwl.h"
#include "shell.h"

#define NA_BUS_NAME	"org.freedesktop.NetworkManager"
#define NA_AGENT_PATH	"/org/freedesktop/NetworkManager/SecretAgent"
#define NA_AGENT_IFACE	"org.freedesktop.NetworkManager.SecretAgent"
#define NA_AM_PATH	"/org/freedesktop/NetworkManager/AgentManager"
#define NA_AM_IFACE	"org.freedesktop.NetworkManager.AgentManager"

/* Three to 255 characters of alphanumerics, `_`, `-` and `.`, neither leading
 * nor trailing dot and no doubled one: NetworkManager rejects the Register
 * outright otherwise. */
#define NA_ID		"org.kdos.netagent"

#define NA_ERR_CANCELED	NA_AGENT_IFACE ".UserCanceled"
#define NA_ERR_AGENT	NA_AGENT_IFACE ".AgentCanceled"
#define NA_ERR_NONE	NA_AGENT_IFACE ".NoSecrets"
/* The AgentManager's own domain, not the agent's: a refused Register. */
#define NA_ERR_AM_DENIED	NA_AM_IFACE ".PermissionDenied"
#define NA_ERR_AM_IDENT		NA_AM_IFACE ".InvalidIdentifier"

/* NM_SECRET_AGENT_GET_SECRETS_FLAG_* */
#define NA_F_INTERACT	0x1u
#define NA_F_NEW	0x2u

#define NA_ASK_MS	115000	/* inside NetworkManager's 120-second wait */
#define NA_RETRY_MS	5000	/* between Register attempts */
#define NA_COLS		62
#define NA_ROWS		8
#define NA_GAP		2

static sd_bus *bus;
static int registered;
static int registering;
static int fatal;
static int64_t next_try;
static const char *font;

/* The question in flight. `ask_msg` being non-NULL IS "the prompt is up": the
 * window follows it and the reply is owed for exactly as long as it is set. */
static sd_bus_message *ask_msg;
static char ask_id[80];		/* the profile's name, for the title      */
static char ask_setting[64];	/* the setting the reply must be under    */
static char ask_key[64];	/* the key inside it                      */
static char ask_path[192];	/* the profile, so a Cancel can be matched */
static int ask_new;		/* the stored secret was refused          */
static int64_t ask_deadline;
static char pass[256];
static int win;			/* the display is open                    */
static int sel = 1;		/* 0 = Cancel, 1 = Connect                */

static int64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ── what arrives from the bus, made safe to look at ───────────────────── */

/* Printable ASCII, everything else a dot. A name is drawn into a terminal's
 * own cells, so a control code in one is a command to that terminal. */
static void plain(char *s)
{
	for (; *s; s++)
		if ((unsigned char)*s < 0x20 || (unsigned char)*s >= 0x7f)
			*s = '.';
}

/*
 * A libnm property name, which is what the hint becomes in the reply's
 * dictionary. NetworkManager's own are lower-case letters, digits and hyphens;
 * anything else did not come from need_secrets() and is not answered.
 */
static int key_ok(const char *k)
{
	if (!*k || strlen(k) >= 48)
		return 0;
	for (const char *p = k; *p; p++)
		if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
		      *p == '-'))
			return 0;
	return 1;
}

/*
 * WHAT TO ASK FOR WHEN THE REQUEST NAMED NOTHING. libnm's need_secrets() hands
 * back a hint for every wireless and 802.1X secret it wants; the VPN setting
 * hands back none at all, because only the plugin knows what it is asking for.
 * A setting that named nothing and is not one of these is NOT guessed at — a
 * passphrase written into the wrong property is a join that fails as "wrong
 * password" for as long as the profile exists.
 */
static const char *default_key(const char *setting)
{
	if (!strcmp(setting, "vpn"))
		return "password";
	if (!strcmp(setting, "802-11-wireless-security"))
		return "psk";
	if (!strcmp(setting, "802-1x"))
		return "password";
	return NULL;
}

/* ── what to call the thing being asked for ────────────────────────────── */

/*
 * The hint names a key, not a label. NetworkManager's `need_secrets` returns
 * bare property names — `psk`, `wep-key0`, `leap-password` — so this is the
 * only place that turns one into English; a prompt reading "needs its psk"
 * asks a question only somebody who has read libnm can answer.
 */
static const char *key_label(const char *setting, const char *key)
{
	if (!strcmp(key, "psk"))
		return "passphrase";
	if (!strncmp(key, "wep-key", 7))
		return "WEP key";
	if (!strcmp(key, "leap-password") || !strcmp(key, "password"))
		return "password";
	if (!strcmp(key, "pin"))
		return "PIN";
	if (!strcmp(key, "private-key-password") ||
	    !strcmp(key, "phase2-private-key-password"))
		return "private key password";
	if (!strcmp(setting, "vpn"))
		return "VPN password";
	return key;
}

/* ── the reply ─────────────────────────────────────────────────────────── */

/*
 * `a{sa{sv}}` holding the one setting that was asked about. Every other
 * setting is left out on purpose: NetworkManager merges what comes back into
 * the connection it already has, and a reply that echoed the whole profile
 * would overwrite properties this agent never looked at.
 *
 * A VPN secret is the exception in shape, not in kind — the `vpn` setting
 * keeps its secrets in one `a{ss}` under the key `secrets` rather than as
 * properties of its own.
 */
static int send_secrets(void)
{
	sd_bus_message *rep = NULL;
	int r = sd_bus_message_new_method_return(ask_msg, &rep);

	if (r < 0)
		return r;
	if ((r = sd_bus_message_open_container(rep, 'a', "{sa{sv}}")) < 0)
		goto out;
	if ((r = sd_bus_message_open_container(rep, 'e', "sa{sv}")) < 0)
		goto out;
	if ((r = sd_bus_message_append_basic(rep, 's', ask_setting)) < 0)
		goto out;
	if ((r = sd_bus_message_open_container(rep, 'a', "{sv}")) < 0)
		goto out;

	if (!strcmp(ask_setting, "vpn")) {
		if ((r = sd_bus_message_open_container(rep, 'e', "sv")) < 0)
			goto out;
		if ((r = sd_bus_message_append_basic(rep, 's', "secrets")) < 0)
			goto out;
		if ((r = sd_bus_message_open_container(rep, 'v', "a{ss}")) < 0)
			goto out;
		if ((r = sd_bus_message_open_container(rep, 'a', "{ss}")) < 0)
			goto out;
		if ((r = sd_bus_message_append(rep, "{ss}", ask_key, pass)) < 0)
			goto out;
		if ((r = sd_bus_message_close_container(rep)) < 0)
			goto out;
		if ((r = sd_bus_message_close_container(rep)) < 0)
			goto out;
		if ((r = sd_bus_message_close_container(rep)) < 0)
			goto out;
	} else {
		if ((r = sd_bus_message_open_container(rep, 'e', "sv")) < 0)
			goto out;
		if ((r = sd_bus_message_append_basic(rep, 's', ask_key)) < 0)
			goto out;
		if ((r = sd_bus_message_open_container(rep, 'v', "s")) < 0)
			goto out;
		if ((r = sd_bus_message_append_basic(rep, 's', pass)) < 0)
			goto out;
		if ((r = sd_bus_message_close_container(rep)) < 0)
			goto out;
		if ((r = sd_bus_message_close_container(rep)) < 0)
			goto out;
	}

	if ((r = sd_bus_message_close_container(rep)) < 0)
		goto out;
	if ((r = sd_bus_message_close_container(rep)) < 0)
		goto out;
	if ((r = sd_bus_message_close_container(rep)) < 0)
		goto out;
	r = sd_bus_send(NULL, rep, NULL);
out:
	sd_bus_message_unref(rep);
	return r;
}

/*
 * Answer the deferred call and clear the question. `err` NULL sends the
 * secret; anything else sends that error name.
 *
 * The passphrase is wiped here rather than at the top of the next prompt: this
 * is the one place every path out of the dialog passes through, and a secret
 * that outlives its reply is one a core dump can still be asked for.
 */
static void finish(const char *err, const char *why)
{
	if (!ask_msg)
		return;
	if (err)
		sd_bus_reply_method_errorf(ask_msg, err, "%s", why);
	else
		send_secrets();
	sd_bus_message_unref(ask_msg);
	ask_msg = NULL;
	memset(pass, 0, sizeof(pass));
	ask_setting[0] = ask_key[0] = ask_path[0] = ask_id[0] = '\0';
	sel = 1;
}

/* ── reading the request ───────────────────────────────────────────────── */

/*
 * The profile's own name out of `a{sa{sv}}`, and nothing else.
 *
 * The dict ENTRY is left before every early exit: the exit after the loop
 * closes the a{sv}, and breaking from inside an entry makes it close the entry
 * instead, so every exit above it is then one level off. That trap is audio.c's
 * and net.c's, paid for once already.
 */
static void read_conn_id(sd_bus_message *m, char *out, size_t cap)
{
	out[0] = '\0';
	if (sd_bus_message_enter_container(m, 'a', "{sa{sv}}") <= 0)
		return;
	while (sd_bus_message_enter_container(m, 'e', "sa{sv}") > 0) {
		const char *setting = NULL;

		if (sd_bus_message_read(m, "s", &setting) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		if (strcmp(setting, "connection") ||
		    sd_bus_message_enter_container(m, 'a', "{sv}") <= 0) {
			sd_bus_message_skip(m, "a{sv}");
			sd_bus_message_exit_container(m);
			continue;
		}
		while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
			const char *key = NULL;

			if (sd_bus_message_read(m, "s", &key) < 0) {
				sd_bus_message_exit_container(m);
				break;
			}
			if (!strcmp(key, "id") &&
			    sd_bus_message_enter_container(m, 'v', "s") > 0) {
				const char *v = NULL;

				if (sd_bus_message_read(m, "s", &v) > 0 && v)
					snprintf(out, cap, "%s", v);
				sd_bus_message_exit_container(m);
			} else {
				sd_bus_message_skip(m, "v");
			}
			sd_bus_message_exit_container(m);
		}
		sd_bus_message_exit_container(m);
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
}

static int m_get_secrets(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	const char *path = NULL, *setting = NULL;
	uint32_t flags = 0;
	char hint[64] = "", id[sizeof(ask_id)];
	const char *key;
	int r;

	(void)userdata;
	(void)e;

	/*
	 * EVERYTHING INTO LOCALS UNTIL BOTH GUARDS HAVE PASSED. A second
	 * request that is refused must leave the open question untouched:
	 * relabelling a prompt that still owes its reply to the first one is a
	 * person reading network B and typing B's passphrase into A's answer.
	 */
	read_conn_id(m, id, sizeof(id));
	if ((r = sd_bus_message_read(m, "os", &path, &setting)) < 0)
		return r;
	if ((r = sd_bus_message_enter_container(m, 'a', "s")) < 0)
		return r;
	for (;;) {
		const char *h = NULL;

		if (sd_bus_message_read(m, "s", &h) <= 0)
			break;
		if (!hint[0] && h && *h)
			snprintf(hint, sizeof(hint), "%s", h);
	}
	sd_bus_message_exit_container(m);
	if ((r = sd_bus_message_read(m, "u", &flags)) < 0)
		return r;
	if (!setting || !*setting)
		return sd_bus_reply_method_errorf(m, NA_ERR_NONE,
						  "no setting was named");

	/*
	 * ONE AT A TIME. NetworkManager asks every registered agent in turn
	 * and gives up on one that is busy; a second question stacked behind
	 * the first would be answered with a passphrase typed for the other
	 * network.
	 */
	if (ask_msg)
		return sd_bus_reply_method_errorf(m, NA_ERR_NONE,
						  "another question is open");
	if (!(flags & NA_F_INTERACT))
		return sd_bus_reply_method_errorf(m, NA_ERR_NONE,
						  "this agent stores nothing");

	key = hint[0] ? hint : default_key(setting);
	if (!key || !key_ok(key))
		return sd_bus_reply_method_errorf(m, NA_ERR_NONE,
						  "nothing usable was named");

	snprintf(ask_path, sizeof(ask_path), "%s", path ? path : "");
	snprintf(ask_setting, sizeof(ask_setting), "%s", setting);
	snprintf(ask_key, sizeof(ask_key), "%s", key);
	snprintf(ask_id, sizeof(ask_id), "%s", id[0] ? id : "This network");
	plain(ask_id);
	ask_new = (flags & NA_F_NEW) != 0;
	ask_deadline = now_ms() + NA_ASK_MS;
	pass[0] = '\0';
	sel = 1;

	ask_msg = sd_bus_message_ref(m);
	return 1;
}

static int m_cancel(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	const char *path = NULL, *setting = NULL;

	(void)userdata;
	(void)e;
	if (sd_bus_message_read(m, "os", &path, &setting) >= 0 && ask_msg &&
	    path && setting && !strcmp(path, ask_path) &&
	    !strcmp(setting, ask_setting))
		finish(NA_ERR_AGENT, "NetworkManager withdrew the question");
	return sd_bus_reply_method_return(m, "");
}

/* Nothing is stored, so there is nothing to save and nothing to delete. The
 * methods exist because the interface has them and an agent that answered
 * UnknownMethod would be logged as broken on every profile write. */
static int m_nostore(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	(void)userdata;
	(void)e;
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable agent_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("GetSecrets", "a{sa{sv}}osasu", "a{sa{sv}}",
		      m_get_secrets, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CancelGetSecrets", "os", "", m_cancel,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SaveSecrets", "a{sa{sv}}o", "", m_nostore,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DeleteSecrets", "a{sa{sv}}o", "", m_nostore,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

/* ── registration ──────────────────────────────────────────────────────── */

/*
 * Registration is not a one-off. NetworkManager forgets every agent when it
 * restarts and it may not be on the bus at all when the session starts, so
 * this is retried on a timer and again whenever the name changes owner. An
 * agent that registered once at startup is an agent that stops working the
 * first time NetworkManager is upgraded.
 */
static int on_registered(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	const sd_bus_error *err = sd_bus_message_get_error(m);

	(void)userdata;
	(void)e;
	registering = 0;
	next_try = now_ms() + NA_RETRY_MS;
	if (!err) {
		registered = 1;
		return 0;
	}
	/*
	 * ONE AGENT PER IDENTIFIER PER USER, and both refusals are permanent
	 * for a fixed identifier. Retrying one every five seconds would be a
	 * process that is indistinguishable from "NetworkManager is down" with
	 * nothing on screen and nothing in a log, so it says which and goes.
	 */
	if (err->name && (!strcmp(err->name, NA_ERR_AM_DENIED) ||
			  !strcmp(err->name, NA_ERR_AM_IDENT))) {
		fprintf(stderr, "kdos-netagent: NetworkManager refused the "
				"registration: %s\n",
			err->message ? err->message : err->name);
		fatal = 1;
	}
	return 0;
}

/*
 * ASYNCHRONOUS, because NetworkManager DEFERS this reply behind a polkit
 * authorisation chain of its own. A synchronous call would stop the loop for
 * as long as that took — and it is retried whenever the name changes owner,
 * so the block would land exactly when a prompt is up and a CancelGetSecrets
 * is owed an answer.
 */
static void try_register(void)
{
	next_try = now_ms() + NA_RETRY_MS;
	if (sd_bus_call_method_async(bus, NULL, NA_BUS_NAME, NA_AM_PATH,
				     NA_AM_IFACE, "Register", on_registered,
				     NULL, "s", NA_ID) < 0)
		return;
	registering = 1;
}

static int on_owner_changed(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	const char *name = NULL, *old = NULL, *new = NULL;

	(void)userdata;
	(void)e;
	if (sd_bus_message_read(m, "sss", &name, &old, &new) < 0)
		return 0;
	if (!name || strcmp(name, NA_BUS_NAME))
		return 0;
	registered = 0;
	registering = 0;
	next_try = 0;
	/* A question owed to a NetworkManager that has gone away can never be
	 * answered, and the window would sit over the desktop until somebody
	 * pressed Escape. */
	if (ask_msg && (!new || !*new))
		finish(NA_ERR_AGENT, "NetworkManager went away");
	return 0;
}

/* ── the window ────────────────────────────────────────────────────────── */

static int open_win(void)
{
	KDispConfig cfg = {
		/*
		 * A TOPLEVEL, so the compositor gives it the keyboard when it
		 * maps and dresses it in the same frame every other window
		 * wears. An overlay would have to be clicked first.
		 */
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = NA_COLS,
		.rows = NA_ROWS,
		.title = "NetworkManager",
		.app_id = "kdos-netagent",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0)
		return -1;
	ktui_draw_init();
	kch_px_popup(KT_BG);
	win = 1;
	return 0;
}

static void close_win(void)
{
	if (!win)
		return;
	kdisp_shutdown();
	win = 0;
}

static int btn_cancel_x, btn_cancel_end, btn_ok_x, btn_ok_end;

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	char masked[64];
	/* One star per CHARACTER, not per byte: a password with a non-ASCII
	 * letter in it would otherwise grow by two or three stars a keystroke
	 * and read as though the field were taking more than was typed. */
	size_t n = 0;

	for (const char *q = pass; *q; ) {
		uint32_t cp;

		q = ktui_utf8_next(q, &cp);
		n++;
	}

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	ktui_draw_box(krect(0, 0, w, h), "NetworkManager", KT_ACCENT, KT_BG, 1);

	const char *what = key_label(ask_setting, ask_key);

	ktui_draw_text(2, 1, w - 4, ask_id, KT_TEXT, KT_BG, KT_A_BOLD);
	/* Two sentences rather than one with a word swapped in: REQUEST_NEW
	 * means the stored secret was refused, and "wants its passphrase" over
	 * a network that has one is a question the user answers with the same
	 * passphrase again. */
	if (ask_new)
		ktui_draw_textf(2, 2, w - 4, KT_MID, KT_BG, KT_A_NONE,
				"will not take the saved %s.", what);
	else
		ktui_draw_textf(2, 2, w - 4, KT_MID, KT_BG, KT_A_NONE,
				"wants its %s.", what);

	if (n > sizeof(masked) - 2)
		n = sizeof(masked) - 2;
	for (size_t i = 0; i < n; i++)
		masked[i] = '*';
	masked[n] = '_';
	masked[n + 1] = '\0';
	ktui_draw_text(2, 4, w - 4, masked, KT_TEXT, KT_BG, KT_A_NONE);

	char cb[] = " Cancel ", ob[] = " Connect ";
	int cw = ktui_utf8_width(cb) + 2, ow = ktui_utf8_width(ob) + 2;
	int by = h - 2;

	btn_ok_x = w - 2 - ow;
	btn_ok_end = btn_ok_x + ow;
	btn_cancel_x = btn_ok_x - NA_GAP - cw;
	btn_cancel_end = btn_cancel_x + cw;
	for (int b = 0; b < 2; b++) {
		int x = b ? btn_ok_x : btn_cancel_x;
		const char *label = b ? ob : cb;
		int on = sel == b;
		int fg = on ? KT_BG : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		ktui_draw_text(x, by, 1, "[", KT_MID, KT_BG, KT_A_NONE);
		ktui_draw_fill(krect(x + 1, by, ktui_utf8_width(label), 1), bg);
		ktui_draw_text(x + 1, by, ktui_utf8_width(label), label, fg, bg,
			       KT_A_NONE);
		ktui_draw_text(x + 1 + ktui_utf8_width(label), by, 1, "]",
			       KT_MID, KT_BG, KT_A_NONE);
	}
	ktui_draw_text(2, by, btn_cancel_x - 3, "Enter connect  Esc cancel",
		       KT_DIM, KT_BG, KT_A_NONE);
	ktui_draw_flush();
}

static void on_event(const KtuiEvent *ev)
{
	if (ev->type == KT_EVT_MOUSE) {
		if (ev->my != ktui_h - 2)
			return;
		if (ev->press == KT_MP_DRAG) {
			if (ev->mx >= btn_cancel_x && ev->mx < btn_cancel_end)
				sel = 0;
			else if (ev->mx >= btn_ok_x && ev->mx < btn_ok_end)
				sel = 1;
			return;
		}
		if (ev->press != KT_MP_PRESS)
			return;
		if (ev->btn == KT_MB_RIGHT ||
		    (ev->btn == KT_MB_LEFT && ev->mx >= btn_cancel_x &&
		     ev->mx < btn_cancel_end))
			finish(NA_ERR_CANCELED, "cancelled");
		else if (ev->btn == KT_MB_LEFT && ev->mx >= btn_ok_x &&
			 ev->mx < btn_ok_end)
			finish(NULL, NULL);
		return;
	}
	if (ev->type != KT_EVT_KEY)
		return;

	switch (ev->key) {
	case KT_K_ESC:
		finish(NA_ERR_CANCELED, "cancelled");
		break;
	case KT_K_ENTER:
		if (sel)
			finish(NULL, NULL);
		else
			finish(NA_ERR_CANCELED, "cancelled");
		break;
	case KT_K_TAB:
	case KT_K_BTAB:
		sel = !sel;
		break;
	case KT_K_LEFT:
		sel = 0;
		break;
	case KT_K_RIGHT:
		sel = 1;
		break;
	case KT_K_BACKSPACE: {
		size_t n = strlen(pass);

		if (n)
			pass[n - 1] = '\0';
		break;
	}
	default:
		/*
		 * THE CODEPOINT, ENCODED. A WPA passphrase is ASCII by the
		 * standard, but an 802.1X or a VPN password is whatever the
		 * account was made with, and a character silently dropped is a
		 * password that can never be typed at all.
		 */
		if (ev->key >= 0x20 && ev->key != 0x7f &&
		    ev->key < KT_K_SPECIAL) {
			char u[4];
			int k = ktui_utf8_encode((uint32_t)ev->key, u);
			size_t have = strlen(pass);

			if (k > 0 && have + (size_t)k + 1 < sizeof(pass)) {
				memcpy(pass + have, u, (size_t)k);
				pass[have + k] = '\0';
			}
		}
		break;
	}
}

/* ── main ──────────────────────────────────────────────────────────────── */

/*
 * WHO OWNS THE SOCKET THIS IS TALKING TO. The system bus is run by root or by
 * the bus daemon's own account; anything else is an address the environment
 * chose, and this process would type a passphrase into it.
 */
static int bus_is_the_system_bus(void)
{
	sd_bus_creds *c = NULL;
	uid_t euid = (uid_t)-1;
	int ok = 0;

	if (sd_bus_get_owner_creds(bus, SD_BUS_CREDS_EUID, &c) < 0)
		return 0;
	if (sd_bus_creds_get_euid(c, &euid) >= 0) {
		struct passwd *pw = getpwnam("messagebus");

		ok = euid == 0 || (pw && euid == pw->pw_uid);
	}
	sd_bus_creds_unref(c);
	return ok;
}

int netagent_main(int argc, char **argv)
{
	/* NO CORE FILE. This process holds a typed passphrase and, on a system
	 * profile, the stored one NetworkManager hands back with it. */
	struct rlimit nocore = { 0, 0 };

	setrlimit(RLIMIT_CORE, &nocore);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else {
			fprintf(stderr, "usage: kdos-netagent [--font NAME]\n");
			return 2;
		}
	}

	int r = sd_bus_open_system(&bus);

	if (r < 0) {
		fprintf(stderr, "kdos-netagent: no system bus: %s\n",
			strerror(-r));
		return 1;
	}
	if (!bus_is_the_system_bus()) {
		fprintf(stderr, "kdos-netagent: refusing a system bus owned by "
				"neither root nor messagebus\n");
		sd_bus_unref(bus);
		return 1;
	}
	r = sd_bus_add_object_vtable(bus, NULL, NA_AGENT_PATH, NA_AGENT_IFACE,
				     agent_vtable, NULL);
	if (r < 0) {
		fprintf(stderr, "kdos-netagent: cannot export the agent: %s\n",
			strerror(-r));
		sd_bus_unref(bus);
		return 1;
	}
	/* Fatal, not best-effort: without this the agent never learns that
	 * NetworkManager restarted, `registered` never clears, and it stops
	 * answering permanently — the failure the retry exists to prevent. */
	r = sd_bus_add_match(bus, NULL,
			     "type='signal',sender='org.freedesktop.DBus',"
			     "interface='org.freedesktop.DBus',"
			     "member='NameOwnerChanged',arg0='" NA_BUS_NAME "'",
			     on_owner_changed, NULL);
	if (r < 0) {
		fprintf(stderr, "kdos-netagent: cannot watch for a "
				"NetworkManager restart: %s\n", strerror(-r));
		sd_bus_unref(bus);
		return 1;
	}
	signal(SIGPIPE, SIG_IGN);
	/* SIGHUP is how the accent reaches a running surface; a prompt raised
	 * after a theme change must not wear the old one. */
	sh_theme_watch();
	sh_theme_from_cache();

	int bus_fd = sd_bus_get_fd(bus);

	while (!fatal) {
		while (sd_bus_process(bus, NULL) > 0)
			;
		if (fatal)
			break;
		if (!registered && !registering && now_ms() >= next_try)
			try_register();

		if (ask_msg && !win && open_win() != 0)
			finish(NA_ERR_NONE, "no compositor");
		if (!ask_msg && win)
			close_win();

		int timeout = registered || registering ? -1 : NA_RETRY_MS;

		if (win) {
			if (sh_theme_dirty) {
				sh_theme_dirty = 0;
				sh_theme_from_cache();
				ktui_draw_invalidate();
			}
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			if (kdisp_should_close()) {
				finish(NA_ERR_CANCELED, "the window was closed");
				close_win();
				continue;
			}
			int left = (int)(ask_deadline - now_ms());

			if (left <= 0) {
				finish(NA_ERR_CANCELED,
				       "nobody answered in time");
				close_win();
				continue;
			}
			draw();
			timeout = left;
		}

		struct pollfd fds[2] = {
			{ .fd = bus_fd, .events = POLLIN },
			{ .fd = win ? kdisp_fd() : -1, .events = POLLIN },
		};
		if (poll(fds, 2, timeout) < 0 && errno != EINTR)
			break;
		if (win && fds[1].revents) {
			KtuiEvent ev;

			while (ask_msg && ktui_backend()->poll_event(&ev, 0))
				on_event(&ev);
		}
	}

	finish(NA_ERR_AGENT, "the agent is going away");
	close_win();
	sd_bus_unref(bus);
	return 0;
}
