/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-bt — pairing a device, on the grid
 *
 *   ╔═ bluetooth ═════════════════════════════════════════════════════╗
 *   ║ hci0  KDOS                        powered   discovering         ║
 *   ║ ─────────────────────────────────────────────────────────────── ║
 *   ║ ▶ ● WH-1000XM4        38:18:4C:…   connected  trusted    72%    ║
 *   ║   · Keyboard K380     C4:2C:03:…   paired                       ║
 *   ║   · JBL Flip 5        A0:11:22:…                                ║
 *   ╟─────────────────────────────────────────────────────────────────╢
 *   ║ Enter connect  p pair  t trust  x remove  s scan  o power  Esc  ║
 *   ╚═════════════════════════════════════════════════════════════════╝
 *
 * kdos-audio has a read-only bluetooth pane, which answers "is my headset
 * connected" and cannot answer "how do I connect it in the first place".
 * PAIRING IS THE WHOLE POINT, and pairing needs an agent.
 *
 * WITHOUT AN org.bluez.Agent1 A KEYBOARD CANNOT BE PAIRED AT ALL. bluez asks
 * the agent to confirm a passkey and refuses the pairing when nobody answers;
 * a program that only calls Pair() works for a speaker with no passkey and
 * silently fails for everything else. So this registers a DisplayYesNo agent
 * and implements the four methods that carry a number a person has to look at.
 *
 * THE CONFIRMATION IS A DEFERRED REPLY, not a blocking dialog. The bus method
 * handler REFS the message, returns "handled" without replying, and the reply
 * is sent when the user presses y or n. A handler that sat in its own event
 * loop waiting would stop answering bluez — the same rule
 * xdg-desktop-portal-kdos had to learn about its file chooser, on a different
 * interface.
 *
 * Everything else is one GetManagedObjects, re-read on a timer, exactly as
 * audio.c does it: a property-change subscription would be fewer bytes and one
 * more thing to keep in agreement with what is on screen, and the refresh IS
 * how the result of a fire-and-forget Pair becomes visible.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kicon.h"
#include "kwl.h"
#include "shell.h"

#define BT_COLS 72
#define BT_ROWS 22
#define BT_MAX 48
#define BT_NAME 64
/* Five seconds for the same reason kdos-net uses five: the bus is pumped once
 * per loop turn, so a timeout shorter than the poll interval expires while the
 * reply is already in the socket and sd-bus synthesises an error. */
#define BT_TIMEOUT_US 5000000
#define BT_REFRESH_S 2
#define BT_AGENT_PATH "/org/kdos/btagent"

struct btdev {
	char path[160];
	char alias[BT_NAME];
	char addr[24];
	int paired, connected, trusted, blocked;
	/* bluez's own `Icon` — "audio-headset", "input-keyboard", "phone".
	 * The freedesktop names, chosen by the device's class of service, so
	 * the picture is the DEVICE's answer rather than a guess made from
	 * its name. */
	char icon[64];
	unsigned battery;	/* org.bluez.Battery1, 0 when absent */
	unsigned rssi_set;
	int rssi;
};

static sd_bus *bus;
static struct btdev devs[BT_MAX];
static int ndev;
static char adapter[160];
static char adapter_name[BT_NAME];
static int powered, discovering, discoverable;
static int pending;
static char why[128];
static char status[128];
static int sel, top;
/* Where the last frame put the list. The header band is two rows plus a rule,
 * so the first device row is no longer 3 — recorded rather than recomputed,
 * because two places deriving one origin is how a click lands a row off. */
static int list_y0 = 4, list_rows;
/* comp.conf's `icons = no`, through --no-icons. */
static int icons_on = 1;

/* The agent's pending question, if any. */
static sd_bus_message *ask_msg;
static char ask_text[128];

/* ── the object tree ───────────────────────────────────────────────────── */

static void read_props(sd_bus_message *m, struct btdev *d, int is_adapter,
		       int is_battery)
{
	if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0)
		return;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL, *contents = NULL, *s = NULL;
		char t = 0;

		/* Leave the dict ENTRY before any early exit, or every
		 * container close above is one level off — audio.c's trap. */
		if (sd_bus_message_read_basic(m, 's', &key) < 0 ||
		    sd_bus_message_peek_type(m, &t, &contents) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}

		if (contents && !strcmp(contents, "s") &&
		    sd_bus_message_enter_container(m, 'v', "s") > 0) {
			sd_bus_message_read_basic(m, 's', &s);
			if (s) {
				if (d && !strcmp(key, "Alias"))
					snprintf(d->alias, BT_NAME, "%s", s);
				else if (d && !strcmp(key, "Address"))
					snprintf(d->addr, sizeof(d->addr), "%s",
						 s);
				else if (d && !strcmp(key, "Icon"))
					snprintf(d->icon, sizeof(d->icon), "%s",
						 s);
				else if (is_adapter && !strcmp(key, "Alias"))
					snprintf(adapter_name, BT_NAME, "%s", s);
			}
			sd_bus_message_exit_container(m);
		} else if (contents && !strcmp(contents, "b") &&
			   sd_bus_message_enter_container(m, 'v', "b") > 0) {
			int b = 0;
			sd_bus_message_read_basic(m, 'b', &b);
			if (d) {
				if (!strcmp(key, "Paired"))
					d->paired = b;
				else if (!strcmp(key, "Connected"))
					d->connected = b;
				else if (!strcmp(key, "Trusted"))
					d->trusted = b;
				else if (!strcmp(key, "Blocked"))
					d->blocked = b;
			} else if (is_adapter) {
				if (!strcmp(key, "Powered"))
					powered = b;
				else if (!strcmp(key, "Discovering"))
					discovering = b;
				else if (!strcmp(key, "Discoverable"))
					discoverable = b;
			}
			sd_bus_message_exit_container(m);
		} else if (contents && !strcmp(contents, "y") &&
			   sd_bus_message_enter_container(m, 'v', "y") > 0) {
			uint8_t v = 0;
			sd_bus_message_read_basic(m, 'y', &v);
			if (d && is_battery && !strcmp(key, "Percentage"))
				d->battery = v;
			sd_bus_message_exit_container(m);
		} else if (contents && !strcmp(contents, "n") &&
			   sd_bus_message_enter_container(m, 'v', "n") > 0) {
			int16_t v = 0;
			sd_bus_message_read_basic(m, 'n', &v);
			if (d && !strcmp(key, "RSSI")) {
				d->rssi = v;
				d->rssi_set = 1;
			}
			sd_bus_message_exit_container(m);
		} else {
			sd_bus_message_skip(m, "v");
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
}

static int bt_reply(sd_bus_message *reply, void *userdata, sd_bus_error *e)
{
	(void)userdata;
	(void)e;

	pending = 0;
	if (sd_bus_message_is_method_error(reply, NULL)) {
		snprintf(why, sizeof(why),
			 "not running — service start 60_bluetooth");
		return 0;
	}
	why[0] = '\0';
	/* Cleared HERE rather than when the call was sent: the list on screen
	 * survives until there is a new one to show. */
	ndev = 0;
	adapter[0] = '\0';
	powered = discovering = discoverable = 0;

	if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") <= 0)
		return 0;
	while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
		const char *obj = NULL;

		if (sd_bus_message_read_basic(reply, 'o', &obj) < 0)
			break;
		if (sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") > 0) {
			struct btdev *d = NULL;

			while (sd_bus_message_enter_container(reply, 'e',
							      "sa{sv}") > 0) {
				const char *iface = NULL;

				if (sd_bus_message_read_basic(reply, 's',
							      &iface) < 0)
					break;
				if (!strcmp(iface, "org.bluez.Adapter1")) {
					/* The FIRST adapter. Two is rare
					 * enough that a chooser would be more
					 * chrome than answer. */
					if (!adapter[0])
						snprintf(adapter,
							 sizeof(adapter), "%s",
							 obj);
					read_props(reply, NULL, 1, 0);
				} else if (!strcmp(iface, "org.bluez.Device1") &&
					   ndev < BT_MAX) {
					if (!d) {
						d = &devs[ndev++];
						memset(d, 0, sizeof(*d));
						snprintf(d->path,
							 sizeof(d->path), "%s",
							 obj);
					}
					read_props(reply, d, 0, 0);
				} else if (!strcmp(iface, "org.bluez.Battery1") &&
					   d) {
					read_props(reply, d, 0, 1);
				} else {
					sd_bus_message_skip(reply, "a{sv}");
				}
				sd_bus_message_exit_container(reply);
			}
			sd_bus_message_exit_container(reply);
		}
		sd_bus_message_exit_container(reply);
	}
	sd_bus_message_exit_container(reply);

	if (!adapter[0])
		snprintf(why, sizeof(why),
			 "no bluetooth adapter on this machine");
	return 0;
}

static void refresh(void)
{
	sd_bus_message *m = NULL;

	if (!bus) {
		snprintf(why, sizeof(why),
			 "no system bus — bluetooth is not reachable");
		return;
	}
	if (pending)
		return;
	if (sd_bus_message_new_method_call(bus, &m, "org.bluez", "/",
					   "org.freedesktop.DBus.ObjectManager",
					   "GetManagedObjects") < 0)
		return;
	if (sd_bus_call_async(bus, NULL, m, bt_reply, NULL, BT_TIMEOUT_US) >= 0)
		pending = 1;
	sd_bus_message_unref(m);
}

/* ── the agent ─────────────────────────────────────────────────────────── */

/*
 * A DisplayYesNo agent. Only the methods that can arrive for a keyboard, a
 * headset or a phone are implemented as questions; the rest answer at once,
 * because a pairing that hangs waiting for a dialog nobody drew is worse than
 * one that is simply refused.
 */
static int agent_release(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	(void)e;
	return sd_bus_reply_method_return(m, "");
}

/* Ask, and DEFER: the message is reffed and the handler returns without
 * replying. bluez waits, the loop keeps running, and the answer goes out from
 * the key handler. */
static int agent_ask(sd_bus_message *m, const char *fmt, const char *who,
		     unsigned num)
{
	if (ask_msg) {
		/* One question at a time. A second pairing while the first is
		 * unanswered is refused rather than queued: the user is
		 * looking at a passkey and a queue would show them the wrong
		 * one. */
		return sd_bus_reply_method_errorf(m, "org.bluez.Error.Rejected",
						  "busy");
	}
	ask_msg = sd_bus_message_ref(m);
	snprintf(ask_text, sizeof(ask_text), fmt, who ? who : "device", num);
	return 1;
}

static const char *dev_name_for(const char *path)
{
	for (int i = 0; i < ndev; i++)
		if (!strcmp(devs[i].path, path))
			return devs[i].alias[0] ? devs[i].alias : devs[i].addr;
	return "device";
}

static int agent_confirm(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *obj = NULL;
	uint32_t passkey = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "ou", &obj, &passkey) < 0)
		return sd_bus_reply_method_errorf(m, "org.bluez.Error.Rejected",
						  "bad request");
	return agent_ask(m, "%s wants to pair — passkey %06u. y/n?",
			 dev_name_for(obj), passkey);
}

static int agent_authorize(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *obj = NULL;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "o", &obj) < 0)
		return sd_bus_reply_method_errorf(m, "org.bluez.Error.Rejected",
						  "bad request");
	return agent_ask(m, "%s wants to pair. y/n?%.0u", dev_name_for(obj), 0);
}

static int agent_authorize_service(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *obj = NULL, *uuid = NULL;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "os", &obj, &uuid) < 0)
		return sd_bus_reply_method_errorf(m, "org.bluez.Error.Rejected",
						  "bad request");
	/* A service on a device that is already TRUSTED is authorised without
	 * asking — that is what trusting it meant. Anything else asks. */
	for (int i = 0; i < ndev; i++)
		if (!strcmp(devs[i].path, obj) && devs[i].trusted)
			return sd_bus_reply_method_return(m, "");
	return agent_ask(m, "%s wants to use a service. y/n?%.0u",
			 dev_name_for(obj), 0);
}

static int agent_display_passkey(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *obj = NULL;
	uint32_t passkey = 0;
	uint16_t entered = 0;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "ouq", &obj, &passkey, &entered) >= 0)
		snprintf(status, sizeof(status), "type %06u on %s", passkey,
			 dev_name_for(obj));
	return sd_bus_reply_method_return(m, "");
}

static int agent_display_pin(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *obj = NULL, *pin = NULL;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "os", &obj, &pin) >= 0)
		snprintf(status, sizeof(status), "type %s on %s", pin,
			 dev_name_for(obj));
	return sd_bus_reply_method_return(m, "");
}

/*
 * A device that wants a PIN or a passkey TYPED HERE is refused, and saying so
 * is better than pretending: this surface has one text field and it is not
 * wired to the agent. DisplayYesNo is what the agent registered as, so bluez
 * only asks these for legacy devices that cannot do secure simple pairing.
 */
static int agent_request_pin(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	(void)e;
	snprintf(status, sizeof(status),
		 "this device needs a typed PIN — pair it with bluetoothctl");
	return sd_bus_reply_method_errorf(m, "org.bluez.Error.Rejected",
					  "no PIN entry here");
}

static int agent_cancel(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	(void)e;
	if (ask_msg) {
		sd_bus_message_unref(ask_msg);
		ask_msg = NULL;
		ask_text[0] = '\0';
	}
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable agent_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Release", "", "", agent_release,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RequestPinCode", "o", "s", agent_request_pin,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DisplayPinCode", "os", "", agent_display_pin,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RequestPasskey", "o", "u", agent_request_pin,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DisplayPasskey", "ouq", "", agent_display_passkey,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RequestConfirmation", "ou", "", agent_confirm,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RequestAuthorization", "o", "", agent_authorize,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("AuthorizeService", "os", "", agent_authorize_service,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Cancel", "", "", agent_cancel,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

static void answer(int yes)
{
	if (!ask_msg)
		return;
	if (yes)
		sd_bus_reply_method_return(ask_msg, "");
	else
		sd_bus_reply_method_errorf(ask_msg, "org.bluez.Error.Rejected",
					   "rejected by the user");
	sd_bus_message_unref(ask_msg);
	ask_msg = NULL;
	ask_text[0] = '\0';
}

static void agent_register(void)
{
	if (!bus)
		return;
	if (sd_bus_add_object_vtable(bus, NULL, BT_AGENT_PATH,
				     "org.bluez.Agent1", agent_vtable,
				     NULL) < 0) {
		snprintf(status, sizeof(status),
			 "no pairing agent — pairing will be refused");
		return;
	}
	sd_bus_call_method_async(bus, NULL, "org.bluez", "/org/bluez",
				 "org.bluez.AgentManager1", "RegisterAgent",
				 NULL, NULL, "os", BT_AGENT_PATH,
				 "DisplayYesNo");
	sd_bus_call_method_async(bus, NULL, "org.bluez", "/org/bluez",
				 "org.bluez.AgentManager1",
				 "RequestDefaultAgent", NULL, NULL, "o",
				 BT_AGENT_PATH);
}

/* ── actions ───────────────────────────────────────────────────────────── */

static int action_reply(sd_bus_message *reply, void *userdata, sd_bus_error *e)
{
	(void)userdata;
	(void)e;
	const sd_bus_error *err = sd_bus_message_get_error(reply);

	if (err && err->message)
		snprintf(status, sizeof(status), "%.100s", err->message);
	refresh();
	return 0;
}

static void dev_method(const char *method)
{
	if (!bus || sel < 0 || sel >= ndev)
		return;
	/* Pairing and connecting take real time — a headset negotiating a
	 * profile is seconds — so the timeout is generous and the call is
	 * fire-and-forget either way. */
	sd_bus_call_method_async(bus, NULL, "org.bluez", devs[sel].path,
				 "org.bluez.Device1", method, action_reply,
				 NULL, NULL);
	snprintf(status, sizeof(status), "%s %s…", method,
		 devs[sel].alias[0] ? devs[sel].alias : devs[sel].addr);
}

static void set_bool_prop(const char *path, const char *iface, const char *prop,
			  int value)
{
	sd_bus_message *m = NULL;

	if (!bus || !path[0])
		return;
	if (sd_bus_message_new_method_call(bus, &m, "org.bluez", path,
					   "org.freedesktop.DBus.Properties",
					   "Set") < 0)
		return;
	sd_bus_message_append(m, "ss", iface, prop);
	if (sd_bus_message_open_container(m, 'v', "b") >= 0) {
		sd_bus_message_append_basic(m, 'b', &value);
		sd_bus_message_close_container(m);
		sd_bus_call_async(bus, NULL, m, action_reply, NULL,
				  BT_TIMEOUT_US);
	}
	sd_bus_message_unref(m);
}

static void remove_device(void)
{
	if (!bus || !adapter[0] || sel < 0 || sel >= ndev)
		return;
	sd_bus_call_method_async(bus, NULL, "org.bluez", adapter,
				 "org.bluez.Adapter1", "RemoveDevice",
				 action_reply, NULL, "o", devs[sel].path);
	snprintf(status, sizeof(status), "removed");
}

static void scan_toggle(void)
{
	if (!bus || !adapter[0])
		return;
	sd_bus_call_method_async(bus, NULL, "org.bluez", adapter,
				 "org.bluez.Adapter1",
				 discovering ? "StopDiscovery" : "StartDiscovery",
				 action_reply, NULL, NULL);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int cmp_dev(const void *pa, const void *pb)
{
	const struct btdev *a = pa, *b = pb;

	if (a->connected != b->connected)
		return b->connected - a->connected;
	if (a->paired != b->paired)
		return b->paired - a->paired;
	return strcasecmp(a->alias[0] ? a->alias : a->addr,
			  b->alias[0] ? b->alias : b->addr);
}

/* The verbs, in the order somebody reaches for them, and each enabled only
 * when the SELECTION can actually do it — a Connect that fails when pressed
 * teaches people to stop trusting the row it was on. */
enum { BB_CONNECT = 0, BB_PAIR, BB_TRUST, BB_REMOVE, BB_SCAN, BB_POWER,
       BB_N };

static int bt_buttons(int w, int row)
{
	const struct btdev *d = sel >= 0 && sel < ndev ? &devs[sel] : NULL;
	struct sh_button b[BB_N];

	b[BB_CONNECT].label = d && d->connected ? "Disconnect" : "Connect";
	b[BB_CONNECT].enabled = d != NULL && powered;
	b[BB_PAIR].label = "Pair";
	b[BB_PAIR].enabled = d != NULL && !d->paired && powered;
	b[BB_TRUST].label = d && d->trusted ? "Untrust" : "Trust";
	b[BB_TRUST].enabled = d != NULL;
	b[BB_REMOVE].label = "Remove";
	b[BB_REMOVE].enabled = d != NULL && d->paired;
	b[BB_SCAN].label = discovering ? "Stop Scan" : "Scan";
	b[BB_SCAN].enabled = powered;
	b[BB_POWER].label = powered ? "Turn Off" : "Turn On";
	b[BB_POWER].enabled = adapter[0] != '\0';
	return sh_chrome_buttons(w, row, b, BB_N, -1);
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	char sub[128];

	if (w < 30 || h < 10)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	ktui_draw_box(krect(0, 0, w, h), "Bluetooth", KT_ACCENT, KT_BG, 1);

	/*
	 * The adapter's state, as the header's subject line. It used to be a
	 * row of five columns at the top of the list, which reads as another
	 * device — and the one thing this window has to say before anything
	 * else is whether the radio is even on.
	 */
	if (adapter[0]) {
		int nconn = 0;
		for (int i = 0; i < ndev; i++)
			if (devs[i].connected)
				nconn++;
		snprintf(sub, sizeof(sub), "%.24s · %s%s · %d device%s",
			 adapter_name[0] ? adapter_name : "adapter",
			 powered ? "on" : "OFF",
			 discovering ? " · scanning" : "", nconn,
			 nconn == 1 ? " connected" : "s connected");
	} else {
		snprintf(sub, sizeof(sub), "%s", why[0] ? why : "looking…");
	}
	int body_y = sh_chrome_header(w, "bluetooth", "Bluetooth", sub,
				      icons_on);
	int body = h - body_y - 3;
	if (body < 1)
		body = 1;
	list_y0 = body_y;
	list_rows = body;

	if (sel < top)
		top = sel;
	if (sel >= top + body)
		top = sel - body + 1;
	if (top < 0)
		top = 0;

	for (int i = 0; i < body && top + i < ndev; i++) {
		const struct btdev *d = &devs[top + i];
		int y = body_y + i;
		int on = top + i == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		/* bluez's own Icon where the theme has a picture for it, the
		 * connected/idle dot where it does not. The dot is not a
		 * placeholder: it is what a tty draws, and it carries the one
		 * bit of state that matters most. */
		int icon = icons_on && d->icon[0] ? kicon_slot(d->icon, 2, 1)
						  : -1;
		int tx = 4;
		if (icon >= 0) {
			ktui_draw_sprite(krect(2, y, 2, 1), icon,
					 on	      ? KT_SURFACE
					 : d->connected ? KT_ACCENT
						      : KT_MID,
					 bg);
			tx = 5;
		} else {
			ktui_draw_text(2, y, 1,
				       d->connected ? ktui_glyph[KT_G_BULLET]
						    : ktui_glyph[KT_G_DOT],
				       on ? KT_SURFACE
					  : d->connected ? KT_ACCENT : KT_DIM,
				       bg, KT_A_NONE);
		}
		ktui_draw_text(tx, y, 26, d->alias[0] ? d->alias : d->addr, fg,
			       bg, KT_A_NONE);
		ktui_draw_text(31, y, 18, d->addr, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		const char *st = d->connected ? "connected"
				 : d->paired	? "paired"
						: "";
		ktui_draw_text(50, y, 10, st, on ? KT_SURFACE : KT_MID, bg,
			       KT_A_NONE);
		if (d->trusted)
			ktui_draw_text(61, y, 8, "trusted",
				       on ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
		if (d->battery)
			ktui_draw_textf(w - 7, y, 5, fg, bg, KT_A_NONE, "%u%%",
					d->battery);
	}
	if (!ndev && !why[0])
		ktui_draw_text(2, body_y, w - 4,
			       powered ? "nothing found yet — press Scan"
				       : "the adapter is off — press Turn On",
			       KT_DIM, KT_BG, KT_A_NONE);

	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_BG);
	if (ask_text[0]) {
		/* A pairing confirmation owns the footer: it is a question
		 * bluez is blocked on, and a row of buttons about something
		 * else beside it would be an invitation to answer the wrong
		 * one. */
		ktui_draw_text(2, h - 2, w - 4, ask_text, KT_WARN, KT_BG,
			       KT_A_NONE);
	} else {
		/* The bar first, the text clipped to where it starts, and the
		 * hints drawn whole or not at all — see the same pair in
		 * net.c, which is where both rules are written down. */
		static const char HINT[] = "Enter connect   p pair   t trust   "
					   "Esc";
		int bx = bt_buttons(w, h - 2);
		int room = bx - 3;
		if (status[0] ? room >= 8 : room >= (int)ktui_utf8_width(HINT))
			ktui_draw_text(2, h - 2, room,
				       status[0] ? status : HINT,
				       status[0] ? KT_MID : KT_DIM, KT_BG,
				       KT_A_NONE);
	}
	ktui_draw_flush();
}

static void settle(int ms)
{
	for (int i = 0; i < ms / 10 && pending; i++) {
		sd_bus_process(bus, NULL);
		usleep(10000);
	}
	sd_bus_process(bus, NULL);
}

int bt_main(int argc, char **argv)
{
	const char *font = NULL;
	int at_x = -1, at_y = 0;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		/* Anchored above the applet that opened it. A panel readout
		 * whose window appears in the middle of the screen reads as a
		 * separate application rather than as part of the bar. */
		if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[i + 1]);
			at_y = atoi(argv[i + 2]);
			i += 2;
		} else
		if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else {
			fprintf(stderr, "usage: kdos-bt [--font NAME] "
					"[--no-icons] [--dump]\n");
			return 2;
		}
	}

	if (sd_bus_open_system(&bus) < 0) {
		bus = NULL;
		snprintf(why, sizeof(why),
			 "no system bus — bluetooth is not reachable");
	}
	refresh();

	if (dump) {
		sh_theme_from_cache();
		/* A golden frame is the character grid. */
		icons_on = 0;
		if (bus)
			settle(1000);
		qsort(devs, (size_t)ndev, sizeof(devs[0]), cmp_dev);
		ktui_offscreen_init(BT_COLS, BT_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	/* Anchored means popup, centred means window — see the same block in
	 * net.c, which is where that split is written down. */
	int popup = at_x >= 0;
	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = popup ? 52 : BT_COLS,
		.rows = popup ? 16 : BT_ROWS,
		.corner = popup ? KWL_CORNER_BOTTOM_LEFT : KWL_CORNER_CENTER,
		.margin_x = popup ? at_x : 0,
		.margin_y = popup ? at_y : 0,
		.app_id = "kdos-bt",
		.font = font,
		.keyboard = 1,
		/* The WINDOW is a dialog: a pairing confirmation must not
		 * vanish because the pointer went to the device on the desk.
		 * The popup is the panel's, and a panel popup that outlives
		 * the click that opened it is the thing this desktop kept
		 * being reported for. */
		.dismiss_on_unfocus = popup,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-bt: no compositor or no layer-shell\n");
		return 1;
	}
	/* AFTER kwl_init: the icon layer needs the cell size and the scale. */
	if (icons_on)
		kicon_init(kwl_cell_w(), kwl_cell_h(), kwl_scale());
	ktui_draw_init();
	agent_register();

	time_t last = 0;
	while (!kwl_should_close()) {
		sh_theme_poll();
		time_t now = time(NULL);
		if (now - last >= BT_REFRESH_S) {
			last = now;
			refresh();
			qsort(devs, (size_t)ndev, sizeof(devs[0]), cmp_dev);
		}
		if (bus)
			sd_bus_process(bus, NULL);
		draw_frame();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 300)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			/* The list starts under the header band, and the draw
			 * is what knows where that is. */
			int idx = top + ev.my - list_y0;
			int in_list = ev.my >= list_y0 &&
				      ev.my < list_y0 + list_rows &&
				      idx >= 0 && idx < ndev;
			if (ev.press == KT_MP_DRAG) {
				if (in_list)
					sel = idx;
				/* The button bar lights under the pointer —
				 * see sh_chrome_hover. */
				sh_chrome_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP && sel > 0) {
				sel--;
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN && sel + 1 < ndev) {
				sel++;
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT)
				continue;
			int b = ask_msg ? -1 : sh_chrome_button_at(ev.mx, ev.my);
			if (b >= 0) {
				switch (b) {
				case BB_CONNECT:
					if (sel < ndev)
						dev_method(devs[sel].connected
								   ? "Disconnect"
								   : "Connect");
					break;
				case BB_PAIR:
					dev_method("Pair");
					break;
				case BB_TRUST:
					if (sel < ndev)
						set_bool_prop(devs[sel].path,
							      "org.bluez.Device1",
							      "Trusted",
							      !devs[sel].trusted);
					break;
				case BB_REMOVE:
					remove_device();
					break;
				case BB_SCAN:
					scan_toggle();
					break;
				case BB_POWER:
					set_bool_prop(adapter,
						      "org.bluez.Adapter1",
						      "Powered", !powered);
					break;
				}
				continue;
			}
			if (in_list) {
				sel = idx;
				dev_method(devs[sel].connected ? "Disconnect"
							       : "Connect");
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		/* The agent's question owns the keyboard while it is up. */
		if (ask_msg) {
			if (ev.key == 'y' || ev.key == 'Y' ||
			    ev.key == KT_K_ENTER)
				answer(1);
			else if (ev.key == 'n' || ev.key == 'N' ||
				 ev.key == KT_K_ESC)
				answer(0);
			continue;
		}

		switch (ev.key) {
		case KT_K_ESC:
			goto done;
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < ndev)
				sel++;
			break;
		case KT_K_ENTER:
			dev_method(sel < ndev && devs[sel].connected
					   ? "Disconnect"
					   : "Connect");
			break;
		case 'p':
			dev_method("Pair");
			break;
		case 't':
			if (sel < ndev)
				set_bool_prop(devs[sel].path,
					      "org.bluez.Device1", "Trusted",
					      !devs[sel].trusted);
			break;
		case 'x':
			remove_device();
			break;
		case 's':
			scan_toggle();
			break;
		case 'o':
			set_bool_prop(adapter, "org.bluez.Adapter1", "Powered",
				      !powered);
			break;
		case 'd':
			set_bool_prop(adapter, "org.bluez.Adapter1",
				      "Discoverable", !discoverable);
			break;
		default:
			break;
		}
	}
done:
	answer(0);
	if (bus)
		sd_bus_unref(bus);
	kwl_shutdown();
	return 0;
}
