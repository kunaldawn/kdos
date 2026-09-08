/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-net — joining a network, on the grid
 *
 *   ╔═ network ═══════════════════════════════════════════════════════╗
 *   ║ wlan0                                              wifi on      ║
 *   ║ ▶ MYSSID                     ▂▄▆█  WPA2   connected             ║
 *   ║   neighbour-5G               ▂▄▆_  WPA2   saved                 ║
 *   ║   coffeeshop                 ▂▄__  open                         ║
 *   ║ ─────────────────────────────────────────────────────────────── ║
 *   ║ eth0                                       cable unplugged      ║
 *   ╟─────────────────────────────────────────────────────────────────╢
 *   ║ Enter join  f forget  c copy  a wifi off  Esc Close             ║
 *   ╚═════════════════════════════════════════════════════════════════╝
 *
 * A SURFACE, NOT A TERMINAL WITH `nmtui` IN IT. NetworkManager runs on this
 * machine and answers D-Bus; what it lacked was somewhere to be seen from.
 *
 * EVERY WRITE HERE IS AUTHORISED BY polkit, AND ONLY BY THE SHIPPED RULES.
 * There is no authentication agent on this system and there cannot be one —
 * polkit has no way to see a session here, so a refusal is flat and raises no
 * challenge for an agent to answer. `/etc/polkit-1/rules.d/50-kdos.rules`
 * names the actions this file calls and grants them to `wheel`; without
 * it, joining, forgetting, scanning and the wifi toggle are all refused and
 * the status line is the only thing that says so. The reasoning is in
 * `docs/kdos/03-architecture/security-model.md`.
 *
 * D-BUS, NEVER `nmcli`. Shelling out to a CLI to parse its output is how an
 * SSID with a space in it becomes two networks, and this program has no shell
 * anywhere in it by the same rule every other launcher here keeps.
 *
 * NOTHING BLOCKS THE LOOP. One GetManagedObjects on org.freedesktop.NetworkManager
 * gives every device, every access point and every saved connection in a
 * single reply, asked with sd_bus_call_async and parsed where it arrives —
 * the shape audio.c's bluez pane already proved. A synchronous call is a
 * bounded block and a bounded block is still a block.
 *
 * THE LIST DOES NOT REORDER UNDER THE POINTER. Scanning is asynchronous and
 * signal strength moves constantly; a list sorted live by strength swaps the
 * row under the cursor between the press and the release. It is sorted once
 * per REFRESH, by (connected, saved, strength), and the selection is followed
 * by SSID rather than by index across a refresh.
 *
 * THE PASSPHRASE TYPED HERE IS WRITTEN INTO THE PROFILE, AND EVERY LATER ONE
 * IS ASKED FOR BY kdos-netagent. This surface joins a network it can see and
 * has no way to be asked anything: NetworkManager raises a secret request
 * against the registered agents, not against whichever program started the
 * activation. So a key that has changed since, 802.1X enterprise wifi and a
 * VPN one-time code are the agent's questions and never this window's, and a
 * session running without kdos-netagent fails those activations in silence.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. The same selection tray.c, notifyd.c and audio.c make. */
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

#define NM_SVC "org.freedesktop.NetworkManager"
#define NM_OBJ "/org/freedesktop/NetworkManager"
#define NM_IF_DEV NM_SVC ".Device"
#define NM_IF_WL NM_SVC ".Device.Wireless"
#define NM_IF_AP NM_SVC ".AccessPoint"
#define NM_IF_CONN NM_SVC ".Settings.Connection"
#define NM_IF_ACT NM_SVC ".Connection.Active"
#define NM_IF_VPN NM_SVC ".VPN.Connection"

#define NET_COLS 74
#define NET_ROWS 24
#define NET_MAX_DEV 8
#define NET_MAX_AP 64
#define NET_MAX_CONN 64
/*
 * FIVE SECONDS, not the 400 ms this started with, and the reason is the shape
 * of the loop rather than the speed of NetworkManager.
 *
 * libkwl's poll waits on the WAYLAND fd; the bus is pumped once per turn, so a
 * reply cannot be processed sooner than the next wake — up to `poll_event`'s
 * timeout away. A bus timeout shorter than that expires while the answer is
 * sitting in the socket, and sd-bus then synthesises an error reply: the
 * surface drew a correct device list with "NetworkManager is not answering"
 * printed under it. Photographed on a booted ISO.
 *
 * Nothing here blocks on it either way — the call is async and the timeout is
 * only how long an unanswered question is remembered.
 */
#define NET_TIMEOUT_US 5000000
#define NET_REFRESH_S 5

/* NM_DEVICE_TYPE */
enum { NMDT_ETHERNET = 1, NMDT_WIFI = 2 };
/* NM_WIFI_DEVICE_CAP_AP. Without the bit NetworkManager refuses an AP-mode
 * activation outright, so the control is drawn as unavailable rather than
 * offered and then refused with a message nobody can act on. */
#define NM_WIFI_CAP_AP 0x40u
/* _NM_802_11_MODE_AP, on Device.Wireless.Mode — the honest "the hotspot is up"
 * signal. The fake access point NetworkManager publishes for a hotspot reports
 * itself as INFRASTRUCTURE, so the AP list cannot answer it. */
#define NM_WIFI_MODE_AP 3u
/* NM_DEVICE_STATE, the few that matter to a person reading a list */
enum {
	NMDS_UNAVAILABLE = 20,
	NMDS_DISCONNECTED = 30,
	NMDS_ACTIVATED = 100,
};

struct net_dev {
	char path[160];
	char iface[32];
	unsigned type, state, wcaps, wmode;
	char active_ap[160];
	/*
	 * THE ACCESS POINTS THIS RADIO SEES, BY OBJECT PATH, and there is no
	 * other way to know. An AccessPoint is exported at
	 * /org/freedesktop/NetworkManager/AccessPoint/<n> and a device at
	 * /org/freedesktop/NetworkManager/Devices/<n>: the two share a prefix
	 * and nothing more, so a path test cannot associate them. This list is
	 * the association, and it arrives in the same reply.
	 */
	char aps[NET_MAX_AP][160];
	int nap;
};

struct net_ap {
	char path[160];
	char ssid[64];
	unsigned strength, flags, wpa, rsn, freq;
	int dev;			/* index into devs */
	int active;
	int saved;			/* a saved connection matches its ssid */
	char conn[160];			/* that connection's object path */
};

struct net_conn {
	char path[160];
	char id[64];
	char type[40];
	char ssid[64];
	/* The ActiveConnection carrying this profile, or "" — the object that
	 * DeactivateConnection takes and the only place its live state is. */
	char active[160];
	unsigned state;			/* NM_ACTIVE_CONNECTION_STATE */
	unsigned vpn_state;		/* NM_VPN_CONNECTION_STATE, VPNs only */
};

/*
 * WHAT KIND OF PROFILE IT IS, CACHED AGAINST ITS OBJECT PATH.
 *
 * `Settings.Connection` publishes only Unsaved, Flags and Filename — the type,
 * the id and the uuid are not properties and are not in the ObjectManager
 * reply at all. Telling a `vpn` from a `wireguard` from an `802-11-wireless`
 * needs one GetSettings per profile, which needs no polkit but is a round trip.
 *
 * KEYED BY PATH BECAUSE `conns[]` IS REFILLED FROM SCRATCH EVERY REFRESH. An
 * async reply lands after at least one such reset, so an index into conns[]
 * would by then describe a different profile. A profile's type does not change
 * while it exists, so a path seen before is never asked about again.
 */
struct conn_kind {
	char path[160];
	char type[40];
	char id[64];
	int asked;
};

static sd_bus *bus;
static struct net_dev devs[NET_MAX_DEV];
static int ndev;
static struct net_ap aps[NET_MAX_AP];
static int nap;
static struct net_conn conns[NET_MAX_CONN];
static int nconn;
static struct conn_kind kinds[NET_MAX_CONN];
static int nkind;
/* GetSettings questions still outstanding. A dump has to wait for these as
 * well as for the object list: they are ASKED at the moment the object list
 * arrives, so a settle that watched only the list would stop exactly then. */
static int kind_pending;

/*
 * The active connections, which arrive in the SAME ObjectManager reply — they
 * are ordinary exported objects and this surface used to skip them. One is
 * what DeactivateConnection takes, and its `Connection` property is the only
 * link back to the profile that started it.
 */
struct net_act {
	char path[160];
	char conn[160];
	/* The device it is on. A wifi activation has exactly one, and it is
	 * the only way to find the activation that is running the hotspot —
	 * the profile behind it is an ordinary wifi profile, not a tunnel. */
	char dev[160];
	unsigned state;
	unsigned vpn_state;
	int is_vpn;
};

static struct net_act acts[NET_MAX_CONN];
static int nact;
static int wifi_enabled = 1;
static int pending;
static char why[128];
static char status[128];

/* ── rows: devices and their networks, in one list ─────────────────────── */

enum { ROW_DEV = 0, ROW_AP, ROW_CONN };

struct row {
	int kind;
	int dev;			/* index into devs */
	int ap;				/* index into aps, for ROW_AP */
	int conn;			/* index into conns, for ROW_CONN */
	/*
	 * WHAT THE SELECTION FOLLOWS ACROSS A REFRESH, and it is qualified by
	 * kind. The list is re-sorted by a signal strength that moves on its
	 * own, so an index points at a different network every few seconds —
	 * but a bare name is not enough either once a second kind is in the
	 * list: a VPN may be called the same thing as an access point, and the
	 * selection would jump between them. An access point is followed by
	 * SSID and a profile by its object path, which is the only identity
	 * NetworkManager guarantees unique.
	 */
	char key[176];
};

/*
 * Every kind at once, and the device append is bounds-checked like the others.
 * It was safe only because ndev is capped at eight; a third kind reading a
 * third field makes an unchecked append a stack overwrite rather than a
 * truncated list.
 */
static struct row rows[NET_MAX_DEV + NET_MAX_AP + NET_MAX_CONN];
static int nrows;
static int sel, top;
/* Where the last frame put the list. The header band is two rows plus a rule,
 * so the first list row is no longer 1 — and a click test that still assumed
 * it would act on the row above the one under the pointer. */
static int list_y0 = 4, list_rows;
/* comp.conf's `icons = no`, through --no-icons. Off is not a degraded mode:
 * it is what a tty draws. */
static int icons_on = 1;
static char sel_key[176];	/* the selected row's `key`, followed across a refresh */

/* ── the passphrase prompt ─────────────────────────────────────────────── */

static int asking;		/* the prompt is up */
static int asking_hotspot;	/* …and the answer starts a hotspot, not a join */
static char pass[128];
static char ask_ssid[64];

/* ONE RUNG: the passphrase prompt. Esc in it abandons the join and Esc on the
 * list closes the window. */
static KtuiKeys keys;

static int ask_up(void *user)
{
	(void)user;
	return asking;
}

static void ask_cancel(void *user)
{
	(void)user;
	asking = 0;
	asking_hotspot = 0;
	pass[0] = '\0';
}

/* ── sd-bus helpers ────────────────────────────────────────────────────── */

/*
 * One a{sv} of properties, read into whatever the caller is filling.
 *
 * The dict ENTRY is left before any early exit: the exit after the loop closes
 * the a{sv}, and breaking from inside an entry makes it close the entry
 * instead — every exit above it is then one level off and the rest of the
 * reply is parsed against the wrong nesting. That trap is audio.c's, paid for
 * once already.
 */
typedef void (*prop_fn)(void *ctx, const char *key, sd_bus_message *m,
			const char *contents);

static void read_props(sd_bus_message *m, prop_fn fn, void *ctx)
{
	if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0)
		return;
	while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
		const char *key = NULL, *contents = NULL;
		char t = 0;

		if (sd_bus_message_read_basic(m, 's', &key) < 0 ||
		    sd_bus_message_peek_type(m, &t, &contents) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		fn(ctx, key, m, contents);
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
}

static int take_str(sd_bus_message *m, const char *contents, char *out,
		    size_t n)
{
	const char *s = NULL;

	if (!contents || strcmp(contents, "s"))
		return 0;
	if (sd_bus_message_enter_container(m, 'v', "s") <= 0)
		return 0;
	if (sd_bus_message_read_basic(m, 's', &s) >= 0 && s)
		snprintf(out, n, "%s", s);
	sd_bus_message_exit_container(m);
	return 1;
}

static int take_obj(sd_bus_message *m, const char *contents, char *out,
		    size_t n)
{
	const char *s = NULL;

	if (!contents || strcmp(contents, "o"))
		return 0;
	if (sd_bus_message_enter_container(m, 'v', "o") <= 0)
		return 0;
	if (sd_bus_message_read_basic(m, 'o', &s) >= 0 && s)
		snprintf(out, n, "%s", s);
	sd_bus_message_exit_container(m);
	return 1;
}

/*
 * An array of object paths into a fixed table, and the count with it. Anything
 * past the table is dropped rather than wrapped: a truncated list shows fewer
 * networks, and a wrapped one shows the wrong ones.
 */
static int take_ao(sd_bus_message *m, const char *contents,
		   char out[][160], int cap, int *n)
{
	if (!contents || strcmp(contents, "ao"))
		return 0;
	if (sd_bus_message_enter_container(m, 'v', "ao") <= 0)
		return 0;
	*n = 0;
	if (sd_bus_message_enter_container(m, 'a', "o") > 0) {
		for (;;) {
			const char *s = NULL;

			if (sd_bus_message_read_basic(m, 'o', &s) <= 0)
				break;
			if (*n < cap && s)
				snprintf(out[(*n)++], 160, "%s", s);
		}
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
	return 1;
}

static int take_u32(sd_bus_message *m, const char *contents, unsigned *out)
{
	uint32_t v = 0;

	if (!contents || strcmp(contents, "u"))
		return 0;
	if (sd_bus_message_enter_container(m, 'v', "u") <= 0)
		return 0;
	if (sd_bus_message_read_basic(m, 'u', &v) >= 0)
		*out = v;
	sd_bus_message_exit_container(m);
	return 1;
}

static int take_u8(sd_bus_message *m, const char *contents, unsigned *out)
{
	uint8_t v = 0;

	if (!contents || strcmp(contents, "y"))
		return 0;
	if (sd_bus_message_enter_container(m, 'v', "y") <= 0)
		return 0;
	if (sd_bus_message_read_basic(m, 'y', &v) >= 0)
		*out = v;
	sd_bus_message_exit_container(m);
	return 1;
}

/*
 * An SSID is a byte array, not a string — 802.11 allows any 32 bytes and a
 * network really can be named with a NUL in it. Rendered as printable ASCII
 * with everything else as a dot, because a taskbar-width row is not the place
 * to discover that somebody named their access point with a control code.
 */
static int take_ssid(sd_bus_message *m, const char *contents, char *out,
		     size_t n)
{
	const void *data = NULL;
	size_t len = 0;

	if (!contents || strcmp(contents, "ay"))
		return 0;
	if (sd_bus_message_enter_container(m, 'v', "ay") <= 0)
		return 0;
	if (sd_bus_message_read_array(m, 'y', &data, &len) >= 0 && data) {
		const unsigned char *p = data;
		size_t k = 0;
		for (; k < len && k + 1 < n; k++)
			out[k] = (p[k] >= 0x20 && p[k] < 0x7f) ? (char)p[k]
							       : '.';
		out[k] = '\0';
	}
	sd_bus_message_exit_container(m);
	return 1;
}

/* ── parsing the one big reply ─────────────────────────────────────────── */

static void dev_prop(void *ctx, const char *key, sd_bus_message *m,
		     const char *c)
{
	struct net_dev *d = ctx;

	if (!strcmp(key, "Interface") && take_str(m, c, d->iface, sizeof(d->iface)))
		return;
	if (!strcmp(key, "DeviceType") && take_u32(m, c, &d->type))
		return;
	if (!strcmp(key, "State") && take_u32(m, c, &d->state))
		return;
	sd_bus_message_skip(m, "v");
}

static void wl_prop(void *ctx, const char *key, sd_bus_message *m,
		    const char *c)
{
	struct net_dev *d = ctx;

	if (!strcmp(key, "ActiveAccessPoint") &&
	    take_obj(m, c, d->active_ap, sizeof(d->active_ap)))
		return;
	if (!strcmp(key, "AccessPoints") &&
	    take_ao(m, c, d->aps, NET_MAX_AP, &d->nap))
		return;
	/* Whether this radio can be an access point at all. Without the bit
	 * NetworkManager refuses the activation outright, so the hotspot is
	 * drawn as unavailable rather than offered and then refused. */
	if (!strcmp(key, "WirelessCapabilities") && take_u32(m, c, &d->wcaps))
		return;
	/* 3 is AP mode, which is the honest "the hotspot is up" signal: the
	 * profile that started it is an ordinary active connection and the
	 * fake AP it publishes reports itself as infrastructure. */
	if (!strcmp(key, "Mode") && take_u32(m, c, &d->wmode))
		return;
	sd_bus_message_skip(m, "v");
}

static void act_prop(void *ctx, const char *key, sd_bus_message *m,
		     const char *c)
{
	struct net_act *a = ctx;

	if (!strcmp(key, "Connection") &&
	    take_obj(m, c, a->conn, sizeof(a->conn)))
		return;
	if (!strcmp(key, "State") && take_u32(m, c, &a->state))
		return;
	if (!strcmp(key, "Devices")) {
		char d[1][160];
		int n = 0;

		if (take_ao(m, c, d, 1, &n)) {
			if (n > 0)
				snprintf(a->dev, sizeof(a->dev), "%s", d[0]);
			return;
		}
	}
	sd_bus_message_skip(m, "v");
}

static void vpn_prop(void *ctx, const char *key, sd_bus_message *m,
		     const char *c)
{
	struct net_act *a = ctx;

	a->is_vpn = 1;
	if (!strcmp(key, "VpnState") && take_u32(m, c, &a->vpn_state))
		return;
	sd_bus_message_skip(m, "v");
}

static void ap_prop(void *ctx, const char *key, sd_bus_message *m,
		    const char *c)
{
	struct net_ap *a = ctx;

	if (!strcmp(key, "Ssid") && take_ssid(m, c, a->ssid, sizeof(a->ssid)))
		return;
	if (!strcmp(key, "Strength") && take_u8(m, c, &a->strength))
		return;
	if (!strcmp(key, "Flags") && take_u32(m, c, &a->flags))
		return;
	if (!strcmp(key, "WpaFlags") && take_u32(m, c, &a->wpa))
		return;
	if (!strcmp(key, "RsnFlags") && take_u32(m, c, &a->rsn))
		return;
	if (!strcmp(key, "Frequency") && take_u32(m, c, &a->freq))
		return;
	sd_bus_message_skip(m, "v");
}

static void nm_prop(void *ctx, const char *key, sd_bus_message *m,
		    const char *c)
{
	(void)ctx;
	if (!strcmp(key, "WirelessEnabled") && c && !strcmp(c, "b") &&
	    sd_bus_message_enter_container(m, 'v', "b") > 0) {
		int b = 0;
		sd_bus_message_read_basic(m, 'b', &b);
		wifi_enabled = b;
		sd_bus_message_exit_container(m);
		return;
	}
	sd_bus_message_skip(m, "v");
}

static void conn_prop(void *ctx, const char *key, sd_bus_message *m,
		      const char *c)
{
	struct net_conn *cn = ctx;

	/* NM exposes only Filename/Flags/Unsaved as properties on a saved
	 * connection; the id and the ssid come from GetSettings, asked per
	 * connection below. The filename is a decent label meanwhile. */
	if (!strcmp(key, "Filename")) {
		char path[256] = "";
		if (take_str(m, c, path, sizeof(path))) {
			const char *b = strrchr(path, '/');
			if (!cn->id[0])
				snprintf(cn->id, sizeof(cn->id), "%s",
					 b ? b + 1 : path);
			return;
		}
	}
	sd_bus_message_skip(m, "v");
}

static int which_dev(const char *ap_path);
static struct conn_kind *kind_find(const char *path);
static void ask_kinds(void);

static int nm_reply(sd_bus_message *reply, void *userdata, sd_bus_error *e)
{
	(void)userdata;
	(void)e;

	pending = 0;
	if (sd_bus_message_is_method_error(reply, NULL)) {
		snprintf(why, sizeof(why),
			 "NetworkManager is not answering "
			 "(service start 42_networkmanager)");
		return 0;
	}
	why[0] = '\0';
	ndev = nap = nconn = nact = 0;

	if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") <= 0)
		return 0;
	while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
		const char *obj = NULL;

		if (sd_bus_message_read_basic(reply, 'o', &obj) < 0)
			break;
		if (sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") > 0) {
			/*
			 * An object carries SEVERAL interfaces — a wifi device
			 * is Device and Device.Wireless in the same entry — so
			 * the device slot is found or created here and filled
			 * by whichever interface comes round.
			 */
			struct net_dev *d = NULL;
			struct net_ap *a = NULL;
			struct net_conn *cn = NULL;
			struct net_act *ac = NULL;

			while (sd_bus_message_enter_container(reply, 'e',
							      "sa{sv}") > 0) {
				const char *iface = NULL;

				if (sd_bus_message_read_basic(reply, 's',
							      &iface) < 0)
					break;
				if (!strcmp(iface, NM_IF_DEV) &&
				    ndev < NET_MAX_DEV) {
					if (!d) {
						d = &devs[ndev++];
						memset(d, 0, sizeof(*d));
						snprintf(d->path,
							 sizeof(d->path), "%s",
							 obj);
					}
					read_props(reply, dev_prop, d);
				} else if (!strcmp(iface, NM_IF_WL) && d) {
					read_props(reply, wl_prop, d);
				} else if (!strcmp(iface, NM_IF_AP) &&
					   nap < NET_MAX_AP) {
					if (!a) {
						a = &aps[nap++];
						memset(a, 0, sizeof(*a));
						snprintf(a->path,
							 sizeof(a->path), "%s",
							 obj);
						a->dev = -1;
					}
					read_props(reply, ap_prop, a);
				} else if (!strcmp(iface, NM_IF_CONN) &&
					   nconn < NET_MAX_CONN) {
					if (!cn) {
						cn = &conns[nconn++];
						memset(cn, 0, sizeof(*cn));
						snprintf(cn->path,
							 sizeof(cn->path), "%s",
							 obj);
					}
					read_props(reply, conn_prop, cn);
				} else if ((!strcmp(iface, NM_IF_ACT) ||
					    !strcmp(iface, NM_IF_VPN)) &&
					   nact < NET_MAX_CONN) {
					/* Both interfaces are on the SAME
					 * object for a VPN, exactly as Device
					 * and Device.Wireless are for a radio. */
					if (!ac) {
						ac = &acts[nact++];
						memset(ac, 0, sizeof(*ac));
						snprintf(ac->path,
							 sizeof(ac->path), "%s",
							 obj);
					}
					read_props(reply,
						   !strcmp(iface, NM_IF_VPN)
							   ? vpn_prop
							   : act_prop,
						   ac);
				} else if (!strcmp(iface, NM_SVC)) {
					read_props(reply, nm_prop, NULL);
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

	/*
	 * WHICH RADIO SAW WHICH NETWORK, out of the device's own AccessPoints
	 * list. The paths cannot answer it: an AccessPoint is exported under
	 * /org/freedesktop/NetworkManager/AccessPoint/<n> and a device under
	 * .../Devices/<n>, so a prefix test associates nothing and every
	 * network is dropped from a list that still draws its radios.
	 */
	for (int i = 0; i < nap; i++)
		aps[i].dev = which_dev(aps[i].path);

	for (int i = 0; i < nap; i++) {
		aps[i].active = 0;
		for (int d = 0; d < ndev; d++)
			if (devs[d].active_ap[0] &&
			    !strcmp(devs[d].active_ap, aps[i].path))
				aps[i].active = 1;
	}

	/* A saved connection is matched by NAME: NM's file names are the
	 * connection id, and the id of a wifi connection is its SSID unless
	 * somebody renamed it. Approximate on purpose — the alternative is a
	 * GetSettings round trip per connection on every refresh, and getting
	 * it wrong costs a "saved" mark, not a wrong action: joining goes
	 * through AddAndActivateConnection either way and NM reuses a matching
	 * profile. */
	ask_kinds();

	for (int i = 0; i < nap; i++) {
		aps[i].saved = 0;
		for (int c = 0; c < nconn; c++) {
			char base[64];
			snprintf(base, sizeof(base), "%s", conns[c].id);
			char *dot = strstr(base, ".nmconnection");
			if (dot)
				*dot = '\0';
			if (aps[i].ssid[0] && !strcmp(base, aps[i].ssid)) {
				aps[i].saved = 1;
				snprintf(aps[i].conn, sizeof(aps[i].conn), "%s",
					 conns[c].path);
			}
		}
	}
	return 0;
}

static int which_dev(const char *ap_path)
{
	for (int d = 0; d < ndev; d++)
		for (int i = 0; i < devs[d].nap; i++)
			if (!strcmp(ap_path, devs[d].aps[i]))
				return d;
	return -1;
}

/* ── what kind of profile, asked once per path ─────────────────────────── */

static struct conn_kind *kind_find(const char *path)
{
	for (int i = 0; i < nkind; i++)
		if (!strcmp(kinds[i].path, path))
			return &kinds[i];
	return NULL;
}

/*
 * The `connection` setting's `id` and `type` out of a GetSettings reply, and
 * nothing else.
 *
 * THE REPLY IS ONE LEVEL DEEPER THAN ANYTHING ELSE HERE — a{sa{sv}} whose
 * values are themselves a{sv} — and the dict-entry rule bites harder for it:
 * every early exit must leave the entry it is inside, because an exit taken
 * from within one closes the entry instead and every exit above it is then a
 * level off, silently.
 */
static int settings_reply(sd_bus_message *m, void *userdata, sd_bus_error *e)
{
	struct conn_kind *k = userdata;

	(void)e;
	if (kind_pending > 0)
		kind_pending--;
	if (!k || sd_bus_message_is_method_error(m, NULL))
		return 0;
	if (sd_bus_message_enter_container(m, 'a', "{sa{sv}}") <= 0)
		return 0;
	while (sd_bus_message_enter_container(m, 'e', "sa{sv}") > 0) {
		const char *setting = NULL;

		if (sd_bus_message_read(m, "s", &setting) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		if (strcmp(setting, "connection")) {
			sd_bus_message_skip(m, "a{sv}");
			sd_bus_message_exit_container(m);
			continue;
		}
		if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
			const char *key = NULL, *contents = NULL;
			char t = 0;

			if (sd_bus_message_read(m, "s", &key) < 0) {
				sd_bus_message_exit_container(m);
				break;
			}
			sd_bus_message_peek_type(m, &t, &contents);
			if (!strcmp(key, "type"))
				take_str(m, contents, k->type, sizeof(k->type));
			else if (!strcmp(key, "id"))
				take_str(m, contents, k->id, sizeof(k->id));
			else
				sd_bus_message_skip(m, "v");
			sd_bus_message_exit_container(m);
		}
		sd_bus_message_exit_container(m);
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
	return 0;
}

/* One question per profile this surface has never seen, and never a second. */
static void ask_kinds(void)
{
	for (int c = 0; c < nconn && nkind < NET_MAX_CONN; c++) {
		struct conn_kind *k = kind_find(conns[c].path);

		if (k)
			continue;
		k = &kinds[nkind++];
		memset(k, 0, sizeof(*k));
		snprintf(k->path, sizeof(k->path), "%s", conns[c].path);
		k->asked = 1;
		if (!bus)
			continue;
		if (sd_bus_call_method_async(bus, NULL, NM_SVC, k->path,
					     NM_IF_CONN, "GetSettings",
					     settings_reply, k, NULL) >= 0)
			kind_pending++;
	}
}

/*
 * A tunnel is a profile with no place in the radio list: NetworkManager calls
 * one `vpn` and the other `wireguard`, and they are not the same thing —
 * a VPN runs a plugin service and a WireGuard profile realises a DEVICE — but
 * to a person choosing one they are the same row.
 */
static int conn_is_tunnel(const struct net_conn *c)
{
	return !strcmp(c->type, "vpn") || !strcmp(c->type, "wireguard");
}

/* The tunnel is up, and not on its way down — see conn_state_word for why the
 * two have to be told apart. */
static int conn_is_up(const struct net_conn *c);

static void refresh(void)
{
	sd_bus_message *m = NULL;

	if (!bus) {
		snprintf(why, sizeof(why),
			 "no system bus — NetworkManager is not reachable");
		return;
	}
	if (pending)
		return;
	if (sd_bus_message_new_method_call(bus, &m, NM_SVC, "/org/freedesktop",
					   "org.freedesktop.DBus.ObjectManager",
					   "GetManagedObjects") < 0)
		return;
	if (sd_bus_call_async(bus, NULL, m, nm_reply, NULL, NET_TIMEOUT_US) >= 0)
		pending = 1;
	sd_bus_message_unref(m);
}

/* ── the row list ──────────────────────────────────────────────────────── */

static int cmp_ap(const void *pa, const void *pb)
{
	const struct net_ap *a = pa, *b = pb;

	if (a->dev != b->dev)
		return a->dev - b->dev;
	if (a->active != b->active)
		return b->active - a->active;
	if (a->saved != b->saved)
		return b->saved - a->saved;
	if (a->strength != b->strength)
		return (int)b->strength - (int)a->strength;
	return strcasecmp(a->ssid, b->ssid);
}

#define NROWS_MAX ((int)(sizeof(rows) / sizeof(rows[0])))

/*
 * A row is written WHOLE. They used to be filled field by field, so a device
 * row carried whatever `.ap` the previous build left in that slot — harmless
 * only while nothing read it, and a wrong-target action the moment a third
 * kind reads a third field.
 */
static struct row *row_push(int kind, int dev)
{
	struct row *r;

	if (nrows >= NROWS_MAX)
		return NULL;
	r = &rows[nrows++];
	memset(r, 0, sizeof(*r));
	r->kind = kind;
	r->dev = dev;
	r->ap = -1;
	r->conn = -1;
	return r;
}

/*
 * THE TYPE AND THE LIVE STATE, JOINED ONTO EACH PROFILE — here rather than
 * where the reply is parsed, because the type arrives on a LATER reply than the
 * one that listed the profile. GetSettings is asked when a path is first seen;
 * joining at parse time would leave the answer unused until the next refresh
 * five seconds later, and a `--dump` would never see it at all.
 */
static void conn_sync(void)
{
	for (int c = 0; c < nconn; c++) {
		const struct conn_kind *k = kind_find(conns[c].path);

		conns[c].type[0] = '\0';
		conns[c].active[0] = '\0';
		conns[c].state = 0;
		conns[c].vpn_state = 0;
		if (k) {
			snprintf(conns[c].type, sizeof(conns[c].type), "%s",
				 k->type);
			if (k->id[0])
				snprintf(conns[c].id, sizeof(conns[c].id), "%s",
					 k->id);
		}
		for (int i = 0; i < nact; i++)
			if (!strcmp(acts[i].conn, conns[c].path)) {
				snprintf(conns[c].active,
					 sizeof(conns[c].active), "%s",
					 acts[i].path);
				conns[c].state = acts[i].state;
				conns[c].vpn_state = acts[i].vpn_state;
				break;
			}
	}
}

static void build_rows(void)
{
	nrows = 0;
	conn_sync();
	qsort(aps, (size_t)nap, sizeof(aps[0]), cmp_ap);

	for (int d = 0; d < ndev; d++) {
		struct row *r;

		if (devs[d].type != NMDT_WIFI && devs[d].type != NMDT_ETHERNET)
			continue;
		if (!(r = row_push(ROW_DEV, d)))
			break;
		snprintf(r->key, sizeof(r->key), "d:%s", devs[d].path);
		if (devs[d].type != NMDT_WIFI)
			continue;
		/* An SSID can be broadcast by several radios; one row per name
		 * is what a person is choosing between. */
		for (int i = 0; i < nap; i++) {
			struct row *ar;

			if (aps[i].dev != d || !aps[i].ssid[0])
				continue;
			int dup = 0;
			for (int j = 0; j < i; j++)
				if (aps[j].dev == d &&
				    !strcmp(aps[j].ssid, aps[i].ssid))
					dup = 1;
			if (dup)
				continue;
			if (!(ar = row_push(ROW_AP, d)))
				break;
			ar->ap = i;
			snprintf(ar->key, sizeof(ar->key), "a:%s",
				 aps[i].ssid);
		}
	}

	/*
	 * THE TUNNELS, AFTER THE RADIOS. A VPN and a WireGuard profile are
	 * saved connections rather than anything the machine can see, so they
	 * have no device to sit under and no signal to sort by: they are one
	 * section at the end, in the order NetworkManager listed them, which
	 * does not move.
	 */
	for (int c = 0; c < nconn; c++) {
		struct row *r;

		if (!conn_is_tunnel(&conns[c]))
			continue;
		if (!(r = row_push(ROW_CONN, -1)))
			break;
		r->conn = c;
		snprintf(r->key, sizeof(r->key), "c:%s", conns[c].path);
	}

	if (sel_key[0]) {
		for (int i = 0; i < nrows; i++)
			if (!strcmp(rows[i].key, sel_key)) {
				sel = i;
				return;
			}
	}
	if (sel >= nrows)
		sel = nrows ? nrows - 1 : 0;
}

/* Both the keyboard and the pointer move the selection, and they used to carry
 * a verbatim copy of this each. One of the two was always going to be missed. */
static void select_row(int i)
{
	sel = i;
	sel_key[0] = '\0';
	if (i >= 0 && i < nrows)
		snprintf(sel_key, sizeof(sel_key), "%s", rows[i].key);
}

/* ── actions ───────────────────────────────────────────────────────────── */

static int ap_secure(const struct net_ap *a)
{
	/* NM_802_11_AP_FLAGS_PRIVACY is bit 0; either RSN or WPA flags being
	 * non-zero is the modern answer and privacy alone is WEP. */
	return (a->flags & 1) || a->wpa || a->rsn;
}

static void set_status(const char *fmt, const char *arg)
{
	snprintf(status, sizeof(status), fmt, arg ? arg : "");
}

/*
 * A method whose answer we do not wait for, but whose ERROR we do want to see.
 * Every action here is fire-and-forget with a callback that only writes the
 * status line — a surface that blocked on ActivateConnection would freeze for
 * as long as a DHCP lease takes.
 */
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

static void activate_saved(const struct net_ap *a)
{
	sd_bus_message *m = NULL;

	if (!bus || a->dev < 0)
		return;
	if (sd_bus_message_new_method_call(bus, &m, NM_SVC, NM_OBJ, NM_SVC,
					   "ActivateConnection") < 0)
		return;
	if (sd_bus_message_append(m, "ooo", a->conn, devs[a->dev].path,
				  a->path) >= 0)
		sd_bus_call_async(bus, NULL, m, action_reply, NULL,
				  5 * 1000000);
	sd_bus_message_unref(m);
	set_status("joining %s…", a->ssid);
}

static int append_sv_str(sd_bus_message *m, const char *k, const char *v)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");
	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', k)) < 0)
		return r;
	if ((r = sd_bus_message_open_container(m, 'v', "s")) < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', v)) < 0)
		return r;
	if ((r = sd_bus_message_close_container(m)) < 0)
		return r;
	return sd_bus_message_close_container(m);
}

static int append_sv_bool(sd_bus_message *m, const char *k, int v)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");

	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', k)) < 0)
		return r;
	if ((r = sd_bus_message_open_container(m, 'v', "b")) < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 'b', &v)) < 0)
		return r;
	if ((r = sd_bus_message_close_container(m)) < 0)
		return r;
	return sd_bus_message_close_container(m);
}

static int append_sv_ay(sd_bus_message *m, const char *k, const void *d,
			size_t n)
{
	int r = sd_bus_message_open_container(m, 'e', "sv");
	if (r < 0)
		return r;
	if ((r = sd_bus_message_append_basic(m, 's', k)) < 0)
		return r;
	if ((r = sd_bus_message_open_container(m, 'v', "ay")) < 0)
		return r;
	if ((r = sd_bus_message_append_array(m, 'y', d, n)) < 0)
		return r;
	if ((r = sd_bus_message_close_container(m)) < 0)
		return r;
	return sd_bus_message_close_container(m);
}

/*
 * Join a network we have no profile for.
 *
 * The connection is PARTIAL on purpose: NetworkManager completes a missing
 * uuid, autoconnect and ipv4/ipv6 method for AddAndActivateConnection, and
 * spelling them out here would be four more things to keep in agreement with
 * whatever NM's defaults become.
 *
 * The passphrase goes into the message and NOWHERE ELSE — not into argv, not
 * into a log, not into the status line. /proc/<pid>/cmdline is world-readable
 * and that is the rule kdos-checkpass and the installer's LUKS step already
 * keep.
 */
static void join_new(const struct net_ap *a, const char *psk)
{
	sd_bus_message *m = NULL;

	if (!bus || a->dev < 0)
		return;
	if (sd_bus_message_new_method_call(bus, &m, NM_SVC, NM_OBJ, NM_SVC,
					   "AddAndActivateConnection") < 0)
		return;
	if (sd_bus_message_open_container(m, 'a', "{sa{sv}}") < 0)
		goto out;

	/* [connection] */
	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "connection");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_str(m, "id", a->ssid);
	append_sv_str(m, "type", "802-11-wireless");
	/*
	 * NO `permissions` KEY, SO THIS IS A SYSTEM CONNECTION — and on this
	 * build that is not a preference, it is the only kind that works.
	 *
	 * NetworkManager decides a profile is VISIBLE by asking its session
	 * monitor whether each user named in `permissions` has a session. This
	 * build has none: it is compiled `-Dsession_tracking=no` because there
	 * is no logind and no ConsoleKit here, so that call is a literal
	 * `return FALSE`. A profile carrying `user:NAME:` is therefore
	 * permanently invisible, and an invisible profile has autoconnect
	 * blocked — the wifi joined here would never come back after a reboot.
	 *
	 * The cost is that `forget` and reading the passphrase back are gated
	 * on `settings.modify.system` rather than `settings.modify.own`, which
	 * is why 50-kdos.rules grants it. That grant hands `wheel` nothing it
	 * did not have: `%wheel ALL=(ALL) ALL` is in the shipped sudoers, and
	 * the passphrases are files under /etc/NetworkManager.
	 */
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	/* [802-11-wireless] */
	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "802-11-wireless");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_ay(m, "ssid", a->ssid, strlen(a->ssid));
	append_sv_str(m, "mode", "infrastructure");
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	if (psk && *psk) {
		if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
			goto out;
		sd_bus_message_append_basic(m, 's', "802-11-wireless-security");
		sd_bus_message_open_container(m, 'a', "{sv}");
		append_sv_str(m, "key-mgmt", "wpa-psk");
		append_sv_str(m, "psk", psk);
		sd_bus_message_close_container(m);
		sd_bus_message_close_container(m);
	}

	if (sd_bus_message_close_container(m) < 0)
		goto out;
	if (sd_bus_message_append(m, "oo", devs[a->dev].path, a->path) < 0)
		goto out;
	sd_bus_call_async(bus, NULL, m, action_reply, NULL, 10 * 1000000);
	set_status("joining %s…", a->ssid);
out:
	sd_bus_message_unref(m);
}

static void forget_conn(const struct net_conn *c)
{
	if (!bus || !c->path[0])
		return;
	sd_bus_call_method_async(bus, NULL, NM_SVC, c->path, NM_IF_CONN,
				 "Delete", action_reply, NULL, NULL);
	set_status("forgot %s", c->id);
}

static void forget(const struct net_ap *a)
{
	if (!bus || !a->saved || !a->conn[0])
		return;
	sd_bus_call_method_async(bus, NULL, NM_SVC, a->conn, NM_IF_CONN,
				 "Delete", action_reply, NULL, NULL);
	set_status("forgot %s", a->ssid);
}

/*
 * A TUNNEL IS A TOGGLE, NOT AN "Enter joins". NetworkManager refuses to
 * re-activate a connection that is already active — CONNECTION_ALREADY_ACTIVE,
 * and its own source calls that a bug — so the row has to dispatch on whether
 * an ActiveConnection carrying this profile exists.
 *
 * THE DEVICE ARGUMENT IS "/", AND THE INTROSPECTION XML IS WRONG ABOUT IT. The
 * documentation says the parameter is ignored for a VPN; NetworkManager errors
 * with "The device doesn't match the active connection" whenever a device is
 * passed that is not the one carrying the primary connection — so copying the
 * access point's shape here works until ethernet and wifi are up at once.
 * With "/" NetworkManager picks the primary connection itself, and says
 * "Could not find source connection" when there is none.
 */
static void tunnel_toggle(const struct net_conn *c)
{
	if (!bus)
		return;
	if (conn_is_up(c)) {
		sd_bus_call_method_async(bus, NULL, NM_SVC, NM_OBJ, NM_SVC,
					 "DeactivateConnection", action_reply,
					 NULL, "o", c->active);
		set_status("disconnecting %s…", c->id);
		return;
	}
	sd_bus_call_method_async(bus, NULL, NM_SVC, NM_OBJ, NM_SVC,
				 "ActivateConnection", action_reply, NULL,
				 "ooo", c->path, "/", "/");
	set_status("connecting %s…", c->id);
}

/*
 * Forget whatever the selection is, which is a Delete on the profile either
 * way: an access point's saved connection, or the tunnel's own. One function
 * because two controls meaning the same verb must not mean two different
 * things — the same rule the rescan button already keeps.
 */
static void forget_selected(void)
{
	if (sel < 0 || sel >= nrows)
		return;
	if (rows[sel].kind == ROW_AP)
		forget(&aps[rows[sel].ap]);
	else if (rows[sel].kind == ROW_CONN)
		forget_conn(&conns[rows[sel].conn]);
}

/* ── the hotspot ───────────────────────────────────────────────────────── */

/* The radio that can be an access point, or -1. */
static int hotspot_dev(void)
{
	for (int d = 0; d < ndev; d++)
		if (devs[d].type == NMDT_WIFI && (devs[d].wcaps & NM_WIFI_CAP_AP))
			return d;
	return -1;
}

static int hotspot_up(void)
{
	int d = hotspot_dev();

	return d >= 0 && devs[d].wmode == NM_WIFI_MODE_AP;
}

/*
 * Share this machine's network over its own radio.
 *
 * `ipv4.method = "shared"` is what makes it a hotspot rather than an unrouted
 * AP: NetworkManager takes an address out of its own pool, starts dnsmasq for
 * DHCP and DNS, and installs an nftables table of its own for the NAT. That
 * table is `nm-shared-<iface>`, and /etc/nftables.conf has to leave room for it
 * — see the comment there.
 *
 * NO band AND NO channel. Both are optional for AP mode and NetworkManager
 * chooses a frequency the radio actually supports, seeded from the SSID so it
 * is stable; sending a channel WITHOUT a band is a hard rejection, and sending
 * a band alone narrows the radio for no gain.
 *
 * `key-mgmt = "wpa-psk"` AND NOT `"sae"`. The supplicant adds SAE to an AP's
 * key management by itself where it can, which is WPA2 and WPA3 from one
 * value; asking for `sae` forces protected management frames and locks out
 * every WPA2-only client.
 *
 * autoconnect = false, because NetworkManager always allows autoconnect for an
 * AP profile: a saved hotspot left on would take the radio at the next boot
 * instead of joining the network the machine is normally on.
 */
static void hotspot_on(const char *psk)
{
	sd_bus_message *m = NULL;
	int d = hotspot_dev();
	char id[80];
	const char *ssid = "KDOS";

	if (!bus || d < 0)
		return;
	snprintf(id, sizeof(id), "%s hotspot", ssid);
	if (sd_bus_message_new_method_call(bus, &m, NM_SVC, NM_OBJ, NM_SVC,
					   "AddAndActivateConnection") < 0)
		return;
	if (sd_bus_message_open_container(m, 'a', "{sa{sv}}") < 0)
		goto out;

	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "connection");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_str(m, "id", id);
	append_sv_str(m, "type", "802-11-wireless");
	append_sv_bool(m, "autoconnect", 0);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "802-11-wireless");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_ay(m, "ssid", ssid, strlen(ssid));
	append_sv_str(m, "mode", "ap");
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "802-11-wireless-security");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_str(m, "key-mgmt", "wpa-psk");
	append_sv_str(m, "psk", psk);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	/*
	 * SPELLED OUT, unlike join_new's deliberately partial dict. With no
	 * [ipv4] setting NetworkManager normalises one in with method `auto` —
	 * a hotspot running a DHCP CLIENT on its own access point, which never
	 * comes up.
	 */
	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "ipv4");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_str(m, "method", "shared");
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	if (sd_bus_message_open_container(m, 'e', "sa{sv}") < 0)
		goto out;
	sd_bus_message_append_basic(m, 's', "ipv6");
	sd_bus_message_open_container(m, 'a', "{sv}");
	append_sv_str(m, "method", "ignore");
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);

	if (sd_bus_message_close_container(m) < 0)
		goto out;
	/* "/" for the access point: AP mode has none to point at. */
	if (sd_bus_message_append(m, "oo", devs[d].path, "/") < 0)
		goto out;
	sd_bus_call_async(bus, NULL, m, action_reply, NULL, NET_TIMEOUT_US);
	set_status("starting the hotspot on %s…", devs[d].iface);
out:
	sd_bus_message_unref(m);
}

/*
 * DeactivateConnection, never Device.Disconnect. Disconnect marks the DEVICE
 * autoconnect-blocked, so after turning the hotspot off the radio would not
 * come back to the saved network at all and nothing would say why.
 */
static void hotspot_off(void)
{
	int d = hotspot_dev();

	if (!bus || d < 0)
		return;
	for (int i = 0; i < nact; i++)
		if (!strcmp(acts[i].dev, devs[d].path)) {
			sd_bus_call_method_async(bus, NULL, NM_SVC, NM_OBJ,
						 NM_SVC, "DeactivateConnection",
						 action_reply, NULL, "o",
						 acts[i].path);
			set_status("stopping the hotspot on %s…",
				   devs[d].iface);
			return;
		}
	set_status("nothing is running on %s", devs[d].iface);
}

static void rescan(int d)
{
	sd_bus_message *m = NULL;

	if (!bus || d < 0 || devs[d].type != NMDT_WIFI)
		return;
	if (sd_bus_message_new_method_call(bus, &m, NM_SVC, devs[d].path,
					   NM_IF_WL, "RequestScan") < 0)
		return;
	/* An empty options dict: the argument is required and there is nothing
	 * to put in it. */
	if (sd_bus_message_open_container(m, 'a', "{sv}") >= 0) {
		sd_bus_message_close_container(m);
		sd_bus_call_async(bus, NULL, m, action_reply, NULL,
				  NET_TIMEOUT_US);
	}
	sd_bus_message_unref(m);
	set_status("scanning…", NULL);
}

static void wifi_toggle(void)
{
	sd_bus_message *m = NULL;

	if (!bus)
		return;
	if (sd_bus_message_new_method_call(bus, &m, NM_SVC, NM_OBJ,
					   "org.freedesktop.DBus.Properties",
					   "Set") < 0)
		return;
	sd_bus_message_append(m, "ss", NM_SVC, "WirelessEnabled");
	if (sd_bus_message_open_container(m, 'v', "b") >= 0) {
		int want = !wifi_enabled;
		sd_bus_message_append_basic(m, 'b', &want);
		sd_bus_message_close_container(m);
		sd_bus_call_async(bus, NULL, m, action_reply, NULL,
				  NET_TIMEOUT_US);
	}
	sd_bus_message_unref(m);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * Signal as four block characters. The vt tier has ░ ▒ █ and no eighth
 * blocks, so this is four CELLS of two states rather than one cell of eight —
 * which is also easier to read across a room, and reads identically on tty1.
 */
static void draw_signal(int x, int y, unsigned strength)
{
	int bars = (int)(strength + 12) / 25;	/* 0..4 */

	for (int i = 0; i < 4; i++)
		ktui_draw_text(x + i, y, 1,
			       i < bars ? ktui_glyph[KT_G_FULL]
					: ktui_glyph[KT_G_SHADE],
			       i < bars ? KT_ACCENT : KT_DIM, KT_BG,
			       KT_A_NONE);
}

static const char *dev_state_word(const struct net_dev *d)
{
	if (d->state >= NMDS_ACTIVATED)
		return "connected";
	if (d->state <= NMDS_UNAVAILABLE)
		return d->type == NMDT_ETHERNET ? "cable unplugged"
						: "unavailable";
	if (d->state <= NMDS_DISCONNECTED)
		return "disconnected";
	return "connecting…";
}

/* NM_ACTIVE_CONNECTION_STATE and NM_VPN_CONNECTION_STATE, the values a person
 * can be told apart. */
enum { NMAC_ACTIVATING = 1, NMAC_ACTIVATED = 2, NMAC_DEACTIVATING = 3 };
enum { NMVPN_NEED_AUTH = 2, NMVPN_CONNECT = 3, NMVPN_IP_CONFIG = 4,
       NMVPN_ACTIVATED = 5, NMVPN_FAILED = 6 };

/*
 * WHAT A TUNNEL IS DOING, AND WHY THE VPN STATE IS NOT ASKED FIRST.
 * NetworkManager reports a VPN that is TEARING DOWN as still ACTIVATED, to
 * preserve an API it cannot change — so a row drawn from VpnState alone says
 * "connected" for the whole of a disconnect. Connection.Active's own state is
 * honest there, so it is consulted first and the VPN's finer states only after.
 */
static const char *conn_state_word(const struct net_conn *c)
{
	if (!c->active[0])
		return "saved";
	if (c->state == NMAC_DEACTIVATING)
		return "disconnecting…";
	switch (c->vpn_state) {
	case NMVPN_NEED_AUTH:
		return "asking";
	case NMVPN_CONNECT:
		return "connecting…";
	case NMVPN_IP_CONFIG:
		return "addressing…";
	case NMVPN_FAILED:
		return "failed";
	case NMVPN_ACTIVATED:
		return "connected";
	default:
		break;
	}
	return c->state == NMAC_ACTIVATED	? "connected"
	       : c->state == NMAC_ACTIVATING	? "connecting…"
						: "saved";
}

static int conn_is_up(const struct net_conn *c)
{
	return c->active[0] && c->state != NMAC_DEACTIVATING;
}

/*
 * The header's subject line: what this machine's networking is DOING, in one
 * sentence, at the top of the window rather than somewhere in the list. The
 * question anybody opens this program to answer is "am I connected", and a
 * list of eight access points does not answer it at a glance.
 */
static void net_subtitle(char *out, size_t n)
{
	for (int i = 0; i < nap; i++)
		if (aps[i].active) {
			snprintf(out, n, "connected to %.40s", aps[i].ssid);
			return;
		}
	for (int i = 0; i < ndev; i++)
		if (devs[i].type == NMDT_ETHERNET &&
		    devs[i].state >= NMDS_ACTIVATED) {
			snprintf(out, n, "connected over %.16s", devs[i].iface);
			return;
		}
	if (pending) {
		snprintf(out, n, "asking NetworkManager…");
		return;
	}
	snprintf(out, n, "%s", ndev ? "not connected" : "no network devices");
}

/* The verbs, in the order somebody reaches for them. Their enabled state is
 * the SELECTION's, so a button that cannot do anything says so instead of
 * failing when it is pressed. */
enum { NB_CONNECT = 0, NB_FORGET, NB_RESCAN, NB_WIFI, NB_CLOSE, NB_N };

static int net_buttons(int w, int row)
{
	const struct row *r = sel >= 0 && sel < nrows ? &rows[sel] : NULL;
	const struct net_ap *a = r && r->kind == ROW_AP ? &aps[r->ap] : NULL;
	const struct net_conn *c = r && r->kind == ROW_CONN ? &conns[r->conn]
							    : NULL;
	struct kch_button b[NB_N];

	/*
	 * NO SIXTH BUTTON, and that is a measurement rather than a preference.
	 * kch_buttons drops from the right until the row fits and returns the
	 * column it took, which is what the status line and the hint row are
	 * clipped to. At seventy-four columns the five already leave eleven,
	 * and a sixth takes the left footer entirely — F1, every key hint, and
	 * every polkit refusal, which on a system with no authentication agent
	 * is the ONLY way a denial is ever reported.
	 */
	b[NB_CONNECT].label = (a && a->active) || (c && conn_is_up(c))
				      ? "Disconnect"
				      : "Connect";
	b[NB_CONNECT].enabled = a != NULL || c != NULL;
	b[NB_FORGET].label = "Forget";
	b[NB_FORGET].enabled = (a && a->saved) || c != NULL;
	b[NB_RESCAN].label = "Rescan";
	b[NB_RESCAN].enabled = 1;
	b[NB_WIFI].label = wifi_enabled ? "Wi-Fi Off" : "Wi-Fi On";
	b[NB_WIFI].enabled = 1;
	b[NB_CLOSE].label = "Close";
	b[NB_CLOSE].enabled = 1;
	return kch_buttons(w, row, b, NB_N, -1);
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	char sub[96];

	if (w < 30 || h < 10)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Network", KT_ACCENT, KT_BG, 1);

	net_subtitle(sub, sizeof(sub));
	/* `network-wireless` when there is a radio to talk about, the wired
	 * mark otherwise — the picture says which kind of machine this is
	 * before the list does. */
	int have_wifi = 0;
	for (int i = 0; i < ndev; i++)
		if (devs[i].type == NMDT_WIFI)
			have_wifi = 1;
	int body_y = kch_header(w, have_wifi ? "network-wireless"
						   : "network-wired",
				      "Network", sub, icons_on);
	int body = h - body_y - 3;
	if (body < 1)
		body = 1;
	list_y0 = body_y;
	list_rows = body;

	if (why[0]) {
		ktui_draw_text(2, body_y, w - 4, why, KT_ERR, KT_BG,
			       KT_A_NONE);
	} else if (!nrows) {
		ktui_draw_text(2, body_y, w - 4,
			       pending ? "asking NetworkManager…"
				       : "no network devices",
			       KT_MID, KT_BG, KT_A_NONE);
	}

	if (sel < top)
		top = sel;
	if (sel >= top + body)
		top = sel - body + 1;
	if (top < 0)
		top = 0;

	for (int i = 0; i < body; i++) {
		int idx = top + i;
		int y = body_y + i;

		if (idx >= nrows)
			break;
		const struct row *r = &rows[idx];
		int on = idx == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		if (r->kind == ROW_DEV) {
			const struct net_dev *d = &devs[r->dev];
			/* A device heading is not selectable furniture: it is
			 * the row Enter rescans from, so it highlights too. */
			ktui_draw_fill(krect(1, y, w - 2, 1), bg);
			int icon = icons_on
					   ? kicon_slot(d->type == NMDT_WIFI
								? "network-wireless"
								: "network-wired",
							2, 1)
					   : -1;
			int tx = 2;
			if (icon >= 0) {
				ktui_draw_sprite(krect(2, y, 2, 1), icon,
						 on ? KT_SURFACE : KT_ACCENT,
						 bg);
				tx = 5;
			}
			ktui_draw_text(tx, y, 20, d->iface,
				       on ? KT_SURFACE : KT_ACCENT, bg,
				       KT_A_NONE);
			const char *st = dev_state_word(d);
			ktui_draw_text_right(0, y, w - 2, st,
					     on ? KT_SURFACE : KT_DIM, bg,
					     KT_A_NONE);
			if (d->type == NMDT_WIFI)
				ktui_draw_text(tx + 22, y, 12,
					       wifi_enabled ? "wifi on"
							    : "wifi OFF",
					       on	     ? KT_SURFACE
					       : wifi_enabled ? KT_MID
							      : KT_WARN,
					       bg, KT_A_NONE);
			continue;
		}

		if (r->kind == ROW_CONN) {
			const struct net_conn *c = &conns[r->conn];
			const char *st = conn_state_word(c);
			int sw = ktui_utf8_width(st) + 1;
			int kindmax = w - 2 - sw - 42;

			ktui_draw_fill(krect(1, y, w - 2, 1), bg);
			/*
			 * AT COLUMN TWO, WHERE THE DEVICES ARE, and not
			 * indented like an access point. A tunnel hangs off no
			 * radio; indenting one puts it visually under
			 * whichever device happened to be listed last.
			 */
			ktui_draw_text(2, y, 34, c->id[0] ? c->id : c->path,
				       fg, bg, KT_A_NONE);
			/* Whole or not at all, for the reason the security
			 * word on an access-point row is: `wiregu` is not an
			 * abbreviation of anything. */
			if (kindmax > 9)
				kindmax = 9;
			if (kindmax >= (int)strlen(c->type))
				ktui_draw_text(42, y, kindmax, c->type,
					       on ? KT_SURFACE : KT_MID, bg,
					       KT_A_NONE);
			ktui_draw_text_right(0, y, w - 2, st,
					     on		    ? KT_SURFACE
					     : conn_is_up(c) ? KT_ACCENT
							     : KT_MID,
					     bg, KT_A_NONE);
			continue;
		}

		const struct net_ap *a = &aps[r->ap];
		ktui_draw_fill(krect(1, y, w - 2, 1), bg);
		/* The mark is the SIGNAL, not a generic wifi icon: it is the
		 * one thing about an access point a picture can say faster
		 * than the number beside it. */
		ktui_draw_text(4, y, 30, a->ssid, fg, bg, KT_A_NONE);
		if (!on)
			draw_signal(36, y, a->strength);
		else
			ktui_draw_textf(36, y, 4, fg, bg, KT_A_NONE, "%3u%%",
					a->strength);
		/*
		 * THE STATE WORD IS RIGHT-ALIGNED, like the device row's, and
		 * not placed at a fixed column. ktui_draw_text clips to the
		 * CELL BUFFER and not to the frame, so a fixed x of 50 with a
		 * width of 12 wrote over the box's right border on the
		 * fifty-two-column popup — the word truncated and the border
		 * gone with it.
		 */
		const char *state = a->active ? "connected"
				  : a->saved  ? "saved"
					      : NULL;
		int sw = state ? ktui_utf8_width(state) + 1 : 0;
		/*
		 * AND THE SECURITY WORD IS DRAWN WHOLE OR NOT AT ALL, because
		 * it is the least important thing on the row and a truncated
		 * one reads as a different standard: `WPA` is what this prints
		 * for a network with no RSN, so a clipped `WPA2` is a lie
		 * rather than an abbreviation. Four is the longest of the
		 * three words it can be.
		 */
		int secmax = w - 2 - sw - 42;

		if (secmax > 6)
			secmax = 6;
		if (secmax >= 4)
			ktui_draw_text(42, y, secmax,
				       ap_secure(a) ? (a->rsn ? "WPA2" : "WPA")
						    : "open",
				       on	     ? KT_SURFACE
				       : ap_secure(a) ? KT_MID
						      : KT_WARN,
				       bg, KT_A_NONE);
		if (state)
			ktui_draw_text_right(0, y, w - 2, state,
					     on	       ? KT_SURFACE
					     : a->active ? KT_ACCENT
							 : KT_MID,
					     bg, KT_A_NONE);
	}

	/* ── the footer ── */
	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_BG);
	if (asking) {
		char masked[64];
		size_t n = strlen(pass);
		if (n > sizeof(masked) - 2)
			n = sizeof(masked) - 2;
		for (size_t i = 0; i < n; i++)
			masked[i] = '*';
		masked[n] = '_';
		masked[n + 1] = '\0';
		/* The field owns the row whole; the pool is still drained,
		 * because a frame that skipped the call would carry its hints
		 * into the next one. */
		ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_BG);
		ktui_draw_textf(2, h - 2, w - 4, KT_TEXT, KT_BG, KT_A_NONE,
				"passphrase for %s: %s", ask_ssid, masked);
	} else {
		/*
		 * THE BUTTONS FIRST, AND THE TEXT CLIPPED TO WHERE THEY START.
		 * They share this row: on a wide window the bar sits to the
		 * right of the hints, and on the fifty-two column popup it
		 * starts most of the way across — drawing the text first and
		 * letting the bar clear its own span cut it off mid-word,
		 * `Enter [ Connect ]`, photographed.
		 */
		int bx = net_buttons(w, h - 2);
		/*
		 * The status when there is one and the key hints otherwise —
		 * the buttons carry the verbs now, so the text row is free to
		 * say what just happened.
		 *
		 * A HINT ROW IS DRAWN WHOLE OR NOT AT ALL. Clipped to the four
		 * columns a popup's button bar leaves, it reads `Enter [
		 * Connect ]` — a fragment of one sentence against the start of
		 * another. A message is different: it is what the user just
		 * did, so it takes whatever room there is.
		 */
		int room = bx - 3;
		const struct row *sr = sel >= 0 && sel < nrows ? &rows[sel]
							      : NULL;
		const struct net_ap *sa = sr && sr->kind == ROW_AP
						  ? &aps[sr->ap]
						  : NULL;
		const struct net_conn *sc = sr && sr->kind == ROW_CONN
						    ? &conns[sr->conn]
						    : NULL;

		if (status[0]) {
			ktui_hint_row(&keys, krect(0, h - 2, 0, 0), KT_BG);
			if (room >= 8)
				ktui_draw_text(2, h - 2, room, status, KT_MID,
					       KT_BG, KT_A_NONE);
		} else {
			/* An ACTIVE network gets no Enter hint: joining it
			 * answers "already on", so naming the key would
			 * advertise one that does nothing. */
			ktui_hint_if(sa && !sa->active, "Enter", "join");
			ktui_hint_if(sr && sr->kind == ROW_DEV, "Enter",
				     "rescan");
			ktui_hint_if(sc != NULL, "Enter",
				     sc && conn_is_up(sc) ? "disconnect"
							  : "connect");
			ktui_hint_if((sa && sa->saved) || sc != NULL, "f",
				     "forget");
			ktui_hint_if(sa != NULL, "c", "copy");
			ktui_hint_if(hotspot_dev() >= 0, "h",
				     hotspot_up() ? "hotspot off"
						  : "hotspot");
			ktui_hint_if(have_wifi, "a",
				     wifi_enabled ? "wifi off" : "wifi on");
			ktui_hint("Esc", ktui_esc_verb(&keys));
			/* UNCONDITIONALLY, even where `room` is negative: the
			 * popup's button bar can start at column two, and the
			 * row is what clears the pool. */
			ktui_hint_row(&keys, krect(2, h - 2, room, 1), KT_BG);
		}
	}
	ktui_draw_flush();
}

/* ── the loop ──────────────────────────────────────────────────────────── */

static void activate_row(void)
{
	if (sel < 0 || sel >= nrows)
		return;
	if (rows[sel].kind == ROW_DEV) {
		rescan(rows[sel].dev);
		return;
	}
	if (rows[sel].kind == ROW_CONN) {
		tunnel_toggle(&conns[rows[sel].conn]);
		return;
	}
	struct net_ap *a = &aps[rows[sel].ap];
	if (a->active) {
		set_status("already on %s", a->ssid);
		return;
	}
	if (a->saved) {
		activate_saved(a);
		return;
	}
	if (!ap_secure(a)) {
		join_new(a, NULL);
		return;
	}
	asking = 1;
	pass[0] = '\0';
	snprintf(ask_ssid, sizeof(ask_ssid), "%s", a->ssid);
}

static void step(int d)
{
	if (!nrows)
		return;
	int i = sel + d;

	if (i < 0)
		i = nrows - 1;
	if (i >= nrows)
		i = 0;
	select_row(i);
}

static void settle(int ms)
{
	/* Connect, ask, and wait out the reply — a dump that drew before the
	 * answer arrived would report a machine with no networks because it
	 * asked three milliseconds ago. The same settle audio.c needs. */
	for (int i = 0; i < ms / 10 && (pending || kind_pending); i++) {
		while (sd_bus_process(bus, NULL) > 0)
			;
		usleep(10000);
	}
	while (sd_bus_process(bus, NULL) > 0)
		;
}

int net_main(int argc, char **argv)
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
		/* comp.conf's `icons = no`. The glyph tier is the fallback,
		 * so off is what a tty draws rather than a degraded mode. */
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else {
			fprintf(stderr, "usage: kdos-net [--font NAME] "
					"[--no-icons] [--dump]\n");
			return 2;
		}
	}

	if (sd_bus_open_system(&bus) < 0) {
		bus = NULL;
		snprintf(why, sizeof(why),
			 "no system bus — NetworkManager is not reachable");
	}
	refresh();

	if (dump) {
		sh_theme_from_cache();
		/* A golden frame is the CHARACTER grid: a layout that only
		 * lines up once the pictures load is a layout that is broken.
		 */
		icons_on = 0;
		if (bus)
			settle(1000);
		build_rows();
		ktui_offscreen_init(NET_COLS, NET_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	/*
	 * ANCHORED MEANS POPUP; CENTRED MEANS WINDOW, and the two want
	 * different sizes and different dismissal.
	 *
	 * Opened from the panel — `--at-bottom`, the applet's own column — this
	 * is the bar's own popup and has to behave like one: small, sitting on
	 * the bar it came from, and gone when the pointer goes elsewhere.
	 * Seventy-four columns of it filled the screen from the top edge down,
	 * with the taskbar visible underneath, which is a full-screen
	 * application that happens to list networks. Typed by name or picked
	 * from the Start menu it IS that application, and then it is centred,
	 * full size, and stays up until it is closed — because somebody who
	 * went looking for it is going to go looking at something else in the
	 * middle of using it.
	 */
	int popup = at_x >= 0;
	KDispConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = popup ? KDISP_ROLE_OVERLAY : KDISP_ROLE_TOPLEVEL,
		.cols = popup ? 52 : NET_COLS,
		.rows = popup ? 16 : NET_ROWS,
		.corner = popup ? KDISP_CORNER_BOTTOM_LEFT : KDISP_CORNER_CENTER,
		.margin_x = popup ? at_x : 0,
		.margin_y = popup ? at_y : 0,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Network",
		.app_id = "kdos-net",
		.font = font,
		.keyboard = 1,
		/* A popup dismisses on a click elsewhere; the window does not,
		 * because a passphrase is typed with the network's own page
		 * open beside it and people click away mid-choice as a matter
		 * of course. */
		.dismiss_on_unfocus = popup,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-net: no compositor or no layer-shell\n");
		return 1;
	}
	/* AFTER kdisp_init: the icon layer needs the cell size and the output
	 * scale, neither of which exists until the surface does. */
	if (icons_on)
		kicon_init(kdisp_cell_w(), kdisp_cell_h(), kdisp_scale());
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_BG);
	/* The page in /usr/share/kdos/doc that F1 opens. A name with no
	 * file there is refused by testing/preflight.sh. */
	keys.doc = "net";
	keys.help = sh_help;
	ktui_keys_layer(&keys, "Cancel", ask_up, ask_cancel, NULL);

	time_t last = 0;
	while (!kdisp_should_close()) {
		sh_theme_poll();
		time_t now = time(NULL);
		if (now - last >= NET_REFRESH_S) {
			last = now;
			refresh();
		}
		if (bus)
			sd_bus_process(bus, NULL);
		build_rows();
		draw_frame();

		KtuiEvent ev;
		/* Short, because the bus is only pumped once per turn: this is
		 * the latency of every answer NetworkManager sends. */
		if (!ktui_backend()->poll_event(&ev, 200)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			/* The list starts under the header band, not at row 1.
			 * Recorded by the draw rather than recomputed here:
			 * two places deriving the same origin is how a click
			 * ends up one row off. */
			int idx = top + ev.my - list_y0;
			int in_list = ev.my >= list_y0 &&
				      ev.my < list_y0 + list_rows &&
				      idx >= 0 && idx < nrows;
			if (ev.press == KT_MP_DRAG) {
				if (in_list)
					select_row(idx);
				/* The button bar lights under the pointer —
				 * see kch_hover. */
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				step(-1);
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				step(1);
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT)
				continue;
			int b = kch_button_at(ev.mx, ev.my);
			if (b >= 0) {
				switch (b) {
				case NB_CONNECT:
					activate_row();
					break;
				case NB_FORGET:
					forget_selected();
					break;
				case NB_RESCAN:
					/* The device the selection is under —
					 * the same thing `r` does, because two
					 * controls that mean the same verb must
					 * not mean two different scans. */
					rescan(sel < nrows ? rows[sel].dev : -1);
					break;
				case NB_WIFI:
					wifi_toggle();
					break;
				case NB_CLOSE:
					goto done;
				}
				continue;
			}
			if (in_list)
				activate_row();
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				goto done;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		if (asking) {
			if (ev.key == KT_K_ENTER) {
				if (asking_hotspot) {
					/* WPA rejects anything shorter, and
					 * NetworkManager would take it and
					 * fail the activation instead. */
					if (strlen(pass) >= 8)
						hotspot_on(pass);
					else
						set_status("a hotspot needs "
							   "eight characters "
							   "or more%s", "");
				} else {
					for (int i = 0; i < nap; i++)
						if (!strcmp(aps[i].ssid,
							    ask_ssid)) {
							join_new(&aps[i], pass);
							break;
						}
				}
				asking = 0;
				asking_hotspot = 0;
				/* The passphrase does not stay in memory for
				 * the life of the surface. */
				memset(pass, 0, sizeof(pass));
			} else if (ev.key == KT_K_BACKSPACE) {
				size_t n = strlen(pass);
				if (n)
					pass[n - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f) {
				size_t n = strlen(pass);
				if (n + 1 < sizeof(pass)) {
					pass[n] = (char)ev.key;
					pass[n + 1] = '\0';
				}
			}
			continue;
		}

		switch (ev.key) {
		case KT_K_UP:
			step(-1);
			break;
		case KT_K_DOWN:
			step(1);
			break;
		case KT_K_ENTER:
			activate_row();
			break;
		case 'f':
			forget_selected();
			break;
		case 'h':
			/*
			 * ONE KEY, BOTH DIRECTIONS, because the radio can only
			 * be in one of the two states and a separate "off"
			 * would be a control that does nothing most of the
			 * time.
			 */
			if (hotspot_dev() < 0)
				set_status("no radio here can be an access "
					   "point%s", "");
			else if (hotspot_up())
				hotspot_off();
			else {
				asking = 1;
				asking_hotspot = 1;
				pass[0] = '\0';
				snprintf(ask_ssid, sizeof(ask_ssid), "%s",
					 "the hotspot");
			}
			break;
		case 'c':
			/* The SSID, on the clipboard. Small, and the reason
			 * libkwl grew a data SOURCE: a network name is exactly
			 * the sort of thing a person retypes into a phone. */
			if (sel < nrows && rows[sel].kind == ROW_AP) {
				const char *id = aps[rows[sel].ap].ssid;
				if (kdisp_copy(id, strlen(id), 0) == 0)
					set_status("copied %s", id);
				else
					set_status("nothing to copy with%s",
						   "");
			}
			break;
		case 'r':
			rescan(sel < nrows ? rows[sel].dev : -1);
			break;
		case 'a':
			wifi_toggle();
			break;
		default:
			break;
		}
	}
done:
	if (bus)
		sd_bus_unref(bus);
	kdisp_shutdown();
	return 0;
}
