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
 *   ║ Enter join   f forget   r rescan   a wifi on/off   Esc          ║
 *   ╚═════════════════════════════════════════════════════════════════╝
 *
 * WHAT WAS HERE BEFORE: `foot -e nmtui`. NetworkManager has been running on
 * this distro since it was a distro, with polkit configured and `wheel` given
 * admin rights, and the only way to reach it from the desktop was a terminal
 * with a curses program in it. That is the single largest daily-use gap on
 * this machine and it is not a missing dependency — it is a missing surface.
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
 * KNOWN LIMIT, STATED: there is no org.freedesktop.NetworkManager.SecretAgent
 * here, so the passphrase is written into the connection when it is created
 * and NetworkManager cannot come back and ASK for another one. A wrong
 * password fails the activation and is retried by joining again; 802.1X
 * enterprise wifi and a VPN with a one-time code still need `nmtui`. The agent
 * is the correct answer and it is a piece of work, not an oversight.
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
/* NM_DEVICE_STATE, the few that matter to a person reading a list */
enum {
	NMDS_UNAVAILABLE = 20,
	NMDS_DISCONNECTED = 30,
	NMDS_ACTIVATED = 100,
};

struct net_dev {
	char path[160];
	char iface[32];
	unsigned type, state;
	char active_ap[160];
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
};

static sd_bus *bus;
static struct net_dev devs[NET_MAX_DEV];
static int ndev;
static struct net_ap aps[NET_MAX_AP];
static int nap;
static struct net_conn conns[NET_MAX_CONN];
static int nconn;
static int wifi_enabled = 1;
static int pending;
static char why[128];
static char status[128];

/* ── rows: devices and their networks, in one list ─────────────────────── */

enum { ROW_DEV = 0, ROW_AP };

struct row {
	int kind;
	int dev;			/* index into devs */
	int ap;				/* index into aps, for ROW_AP */
};

static struct row rows[NET_MAX_DEV + NET_MAX_AP];
static int nrows;
static int sel, top;
/* Where the last frame put the list. The header band is two rows plus a rule,
 * so the first list row is no longer 1 — and a click test that still assumed
 * it would act on the row above the one under the pointer. */
static int list_y0 = 4, list_rows;
/* comp.conf's `icons = no`, through --no-icons. Off is not a degraded mode:
 * it is what a tty draws. */
static int icons_on = 1;
static char sel_ssid[64];	/* what the selection FOLLOWS across a refresh */

/* ── the passphrase prompt ─────────────────────────────────────────────── */

static int asking;		/* the prompt is up */
static char pass[128];
static char ask_ssid[64];

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
	ndev = nap = nconn = 0;

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

	/* An access point's object path is the wireless device's path plus
	 * /AccessPoint/<n>, which is the only association the ObjectManager
	 * reply carries — the alternative is a Get per AP. */
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
	for (int d = 0; d < ndev; d++) {
		size_t n = strlen(devs[d].path);
		if (n && !strncmp(ap_path, devs[d].path, n) &&
		    ap_path[n] == '/')
			return d;
	}
	return -1;
}

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

static void build_rows(void)
{
	nrows = 0;
	qsort(aps, (size_t)nap, sizeof(aps[0]), cmp_ap);

	for (int d = 0; d < ndev; d++) {
		if (devs[d].type != NMDT_WIFI && devs[d].type != NMDT_ETHERNET)
			continue;
		rows[nrows].kind = ROW_DEV;
		rows[nrows].dev = d;
		nrows++;
		if (devs[d].type != NMDT_WIFI)
			continue;
		/* An SSID can be broadcast by several radios; one row per name
		 * is what a person is choosing between. */
		for (int i = 0; i < nap && nrows < (int)(sizeof(rows) /
							 sizeof(rows[0]));
		     i++) {
			if (aps[i].dev != d || !aps[i].ssid[0])
				continue;
			int dup = 0;
			for (int j = 0; j < i; j++)
				if (aps[j].dev == d &&
				    !strcmp(aps[j].ssid, aps[i].ssid))
					dup = 1;
			if (dup)
				continue;
			rows[nrows].kind = ROW_AP;
			rows[nrows].dev = d;
			rows[nrows].ap = i;
			nrows++;
		}
	}

	/* Follow the selection by NAME across a refresh: the list is re-sorted
	 * by a signal strength that moves on its own, and an index would point
	 * at a different network every few seconds. */
	if (sel_ssid[0]) {
		for (int i = 0; i < nrows; i++)
			if (rows[i].kind == ROW_AP &&
			    !strcmp(aps[rows[i].ap].ssid, sel_ssid)) {
				sel = i;
				return;
			}
	}
	if (sel >= nrows)
		sel = nrows ? nrows - 1 : 0;
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

static void forget(const struct net_ap *a)
{
	if (!bus || !a->saved || !a->conn[0])
		return;
	sd_bus_call_method_async(bus, NULL, NM_SVC, a->conn, NM_IF_CONN,
				 "Delete", action_reply, NULL, NULL);
	set_status("forgot %s", a->ssid);
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
	struct kch_button b[NB_N];

	b[NB_CONNECT].label = a && a->active ? "Disconnect" : "Connect";
	b[NB_CONNECT].enabled = a != NULL;
	b[NB_FORGET].label = "Forget";
	b[NB_FORGET].enabled = a && a->saved;
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
		ktui_draw_text(42, y, 6,
			       ap_secure(a) ? (a->rsn ? "WPA2" : "WPA") : "open",
			       on ? KT_SURFACE : ap_secure(a) ? KT_MID : KT_WARN,
			       bg, KT_A_NONE);
		if (a->active)
			ktui_draw_text(50, y, 12, "connected",
				       on ? KT_SURFACE : KT_ACCENT, bg,
				       KT_A_NONE);
		else if (a->saved)
			ktui_draw_text(50, y, 12, "saved",
				       on ? KT_SURFACE : KT_MID, bg, KT_A_NONE);
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
		static const char HINT[] = "Enter join   f forget   c copy   "
					   "r rescan   Esc";
		int room = bx - 3;
		if (status[0] ? room >= 8
			      : room >= (int)ktui_utf8_width(HINT))
			ktui_draw_text(2, h - 2, room,
				       status[0] ? status : HINT,
				       status[0] ? KT_MID : KT_DIM, KT_BG,
				       KT_A_NONE);
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
	sel += d;
	if (sel < 0)
		sel = nrows - 1;
	if (sel >= nrows)
		sel = 0;
	sel_ssid[0] = '\0';
	if (rows[sel].kind == ROW_AP)
		snprintf(sel_ssid, sizeof(sel_ssid), "%s",
			 aps[rows[sel].ap].ssid);
}

static void settle(int ms)
{
	/* Connect, ask, and wait out the reply — a dump that drew before the
	 * answer arrived would report a machine with no networks because it
	 * asked three milliseconds ago. The same settle audio.c needs. */
	for (int i = 0; i < ms / 10 && pending; i++) {
		sd_bus_process(bus, NULL);
		usleep(10000);
	}
	sd_bus_process(bus, NULL);
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
				if (in_list) {
					sel = idx;
					sel_ssid[0] = '\0';
					if (rows[sel].kind == ROW_AP)
						snprintf(sel_ssid,
							 sizeof(sel_ssid), "%s",
							 aps[rows[sel].ap].ssid);
				}
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
					if (sel < nrows &&
					    rows[sel].kind == ROW_AP)
						forget(&aps[rows[sel].ap]);
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

		if (asking) {
			if (ev.key == KT_K_ESC) {
				asking = 0;
				pass[0] = '\0';
			} else if (ev.key == KT_K_ENTER) {
				for (int i = 0; i < nap; i++)
					if (!strcmp(aps[i].ssid, ask_ssid)) {
						join_new(&aps[i], pass);
						break;
					}
				asking = 0;
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
		case KT_K_ESC:
			goto done;
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
			if (sel < nrows && rows[sel].kind == ROW_AP)
				forget(&aps[rows[sel].ap]);
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
