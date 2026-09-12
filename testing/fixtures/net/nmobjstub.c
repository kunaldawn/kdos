/*
 * The one reply kdos-net asks NetworkManager for, recorded.
 *
 * kdos-net has never had a golden, and testing/selftest.sh said why: it needs a
 * system bus, and what is on that bus — an access-point list, a saved profile —
 * is the machine's rather than a fixture's. That reason stops being true the
 * moment the bus is one the test starts. This serves a single
 * `org.freedesktop.DBus.ObjectManager.GetManagedObjects` on `/org/freedesktop`,
 * which is the only call kdos-net makes to draw a frame.
 *
 * THE SHAPE IS NetworkManager'S, NOT AN INVENTION. Object paths, interface
 * names and property types are read off the interface XML this image ships and
 * off the NM sources — in particular an AccessPoint lives under
 * /org/freedesktop/NetworkManager/AccessPoint/<n> and a device under
 * .../Devices/<n>, which share a prefix and nothing else. That is the whole
 * reason this fixture exists: a surface that associated the two by path drew
 * its radios over an empty list, and no test could see it.
 *
 * WHAT IS IN THE RECORDING, and each row is here to make one thing visible:
 * a wired device that is up; a radio that is up and on one of the networks; a
 * network with a saved profile; one without; an open one; and a network only
 * the second radio can see, so that "which radio saw it" is answered wrongly by
 * anything that guesses.
 *
 * AND TWO TUNNELS, because they are the other half of the list and neither is
 * discoverable from this reply alone: `Settings.Connection` publishes only
 * Unsaved, Flags and Filename, so a profile's TYPE comes from a GetSettings per
 * path — which this answers too. One is a `vpn` and is UP, with its
 * ActiveConnection carrying both Connection.Active and VPN.Connection the way
 * NetworkManager exports one; the other is a `wireguard` that is saved and
 * down, and is not a VPN to NetworkManager at all.
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#define NM  "org.freedesktop.NetworkManager"
#define DEV NM ".Device"
#define WL  NM ".Device.Wireless"
#define AP  NM ".AccessPoint"
#define SC  NM ".Settings.Connection"

#define P_MGR  "/org/freedesktop/NetworkManager"
#define P_DEV0 P_MGR "/Devices/0"
#define P_DEV1 P_MGR "/Devices/1"
#define P_AP1  P_MGR "/AccessPoint/1"
#define P_AP2  P_MGR "/AccessPoint/2"
#define P_AP3  P_MGR "/AccessPoint/3"
#define P_CONN P_MGR "/Settings/1"
#define P_VPN  P_MGR "/Settings/2"
#define P_WG   P_MGR "/Settings/3"
#define P_ACT  P_MGR "/ActiveConnection/1"

/* NM_802_11_AP_SEC_* — a network is "secure" to kdos-net when either the WPA
 * or the RSN flag word is non-zero, or the privacy bit is set in Flags. */
#define SEC_PAIR_CCMP 0x00000100
#define SEC_KEY_MGMT_PSK 0x00000400
#define AP_FLAG_PRIVACY 0x1

static sd_bus *bus;

/* ── the little appenders ──────────────────────────────────────────────── */

static void sv_s(sd_bus_message *m, const char *k, const char *v)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "s");
	sd_bus_message_append_basic(m, 's', v);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void sv_o(sd_bus_message *m, const char *k, const char *v)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "o");
	sd_bus_message_append_basic(m, 'o', v);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void sv_u(sd_bus_message *m, const char *k, uint32_t v)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "u");
	sd_bus_message_append_basic(m, 'u', &v);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void sv_y(sd_bus_message *m, const char *k, uint8_t v)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "y");
	sd_bus_message_append_basic(m, 'y', &v);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void sv_b(sd_bus_message *m, const char *k, int v)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "b");
	sd_bus_message_append_basic(m, 'b', &v);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

/* An SSID is `ay` and is not NUL-terminated. */
static void sv_ay(sd_bus_message *m, const char *k, const char *v)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "ay");
	sd_bus_message_append_array(m, 'y', v, strlen(v));
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void sv_ao(sd_bus_message *m, const char *k, const char *const *v, int n)
{
	sd_bus_message_open_container(m, 'e', "sv");
	sd_bus_message_append_basic(m, 's', k);
	sd_bus_message_open_container(m, 'v', "ao");
	sd_bus_message_open_container(m, 'a', "o");
	for (int i = 0; i < n; i++)
		sd_bus_message_append_basic(m, 'o', v[i]);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void iface_open(sd_bus_message *m, const char *name)
{
	sd_bus_message_open_container(m, 'e', "sa{sv}");
	sd_bus_message_append_basic(m, 's', name);
	sd_bus_message_open_container(m, 'a', "{sv}");
}

static void iface_close(sd_bus_message *m)
{
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

static void obj_open(sd_bus_message *m, const char *path)
{
	sd_bus_message_open_container(m, 'e', "oa{sa{sv}}");
	sd_bus_message_append_basic(m, 'o', path);
	sd_bus_message_open_container(m, 'a', "{sa{sv}}");
}

static void obj_close(sd_bus_message *m)
{
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
}

/* ── the recording ─────────────────────────────────────────────────────── */

/*
 * A FILTER RATHER THAN A VTABLE. sd-bus owns
 * `org.freedesktop.DBus.ObjectManager` itself — sd_bus_add_object_vtable()
 * refuses it with EINVAL — and its own implementation composes the reply from
 * objects registered with property vtables. A recording is a fixed reply, so
 * this intercepts the call and answers it.
 */
static int managed(sd_bus_message *msg, void *u, sd_bus_error *e)
{
	sd_bus_message *r = NULL;
	static const char *const dev1_aps[] = { P_AP1, P_AP2 };
	static const char *const dev0_aps[] = { P_AP3 };

	(void)u;
	(void)e;
	/*
	 * GetSettings on a profile, which is the ONLY place its type and its id
	 * are: Settings.Connection publishes neither as a property, so a
	 * surface telling a vpn from a wireguard from a wifi profile has to ask
	 * once per path.
	 */
	if (sd_bus_message_is_method_call(msg, SC, "GetSettings")) {
		const char *p = sd_bus_message_get_path(msg);
		const char *id = "", *ty = "";

		if (p && !strcmp(p, P_CONN)) {
			id = "Home Wifi";
			ty = "802-11-wireless";
		} else if (p && !strcmp(p, P_VPN)) {
			id = "Work VPN";
			ty = "vpn";
		} else if (p && !strcmp(p, P_WG)) {
			id = "wg0";
			ty = "wireguard";
		}
		if (sd_bus_message_new_method_return(msg, &r) < 0)
			return -ENOMEM;
		sd_bus_message_open_container(r, 'a', "{sa{sv}}");
		iface_open(r, "connection");
		sv_s(r, "id", id);
		sv_s(r, "type", ty);
		iface_close(r);
		sd_bus_message_close_container(r);
		sd_bus_send(NULL, r, NULL);
		sd_bus_message_unref(r);
		return 1;
	}
	if (!sd_bus_message_is_method_call(msg,
					   "org.freedesktop.DBus.ObjectManager",
					   "GetManagedObjects"))
		return 0;
	if (sd_bus_message_new_method_return(msg, &r) < 0)
		return -ENOMEM;
	sd_bus_message_open_container(r, 'a', "{oa{sa{sv}}}");

	/* The manager itself, for WirelessEnabled. */
	obj_open(r, P_MGR);
	iface_open(r, NM);
	sv_b(r, "WirelessEnabled", 1);
	iface_close(r);
	obj_close(r);

	/* A wired device, connected. */
	obj_open(r, P_DEV0);
	iface_open(r, DEV);
	sv_s(r, "Interface", "eth0");
	sv_u(r, "DeviceType", 1);
	sv_u(r, "State", 100);
	iface_close(r);
	obj_close(r);

	/*
	 * The radio, connected to AP1, with AP1 and AP2 in its own list. AP3 is
	 * deliberately NOT in it: a surface that guessed the association from
	 * the object paths would show all three under this radio.
	 */
	obj_open(r, P_DEV1);
	iface_open(r, DEV);
	sv_s(r, "Interface", "wlan0");
	sv_u(r, "DeviceType", 2);
	sv_u(r, "State", 100);
	iface_close(r);
	iface_open(r, WL);
	sv_o(r, "ActiveAccessPoint", P_AP1);
	sv_ao(r, "AccessPoints", dev1_aps, 2);
	/* NM_WIFI_DEVICE_CAP_AP: this radio can run the hotspot, and the
	 * second one below deliberately cannot, so the control's two states
	 * are both reachable from one recording. */
	sv_u(r, "WirelessCapabilities", 0x40);
	sv_u(r, "Mode", 2);			/* infrastructure */
	iface_close(r);
	obj_close(r);

	/* A second radio that can see one network and cannot be an AP. */
	obj_open(r, P_MGR "/Devices/2");
	iface_open(r, DEV);
	sv_s(r, "Interface", "wlan1");
	sv_u(r, "DeviceType", 2);
	sv_u(r, "State", 30);
	iface_close(r);
	iface_open(r, WL);
	sv_o(r, "ActiveAccessPoint", "/");
	sv_ao(r, "AccessPoints", dev0_aps, 1);
	sv_u(r, "WirelessCapabilities", 0x1);
	sv_u(r, "Mode", 2);
	iface_close(r);
	obj_close(r);

	/* The network the radio is on, and which has a saved profile. */
	obj_open(r, P_AP1);
	iface_open(r, AP);
	sv_ay(r, "Ssid", "Home Wifi");
	sv_y(r, "Strength", 82);
	sv_u(r, "Flags", AP_FLAG_PRIVACY);
	sv_u(r, "WpaFlags", 0);
	sv_u(r, "RsnFlags", SEC_PAIR_CCMP | SEC_KEY_MGMT_PSK);
	sv_u(r, "Frequency", 5220);
	iface_close(r);
	obj_close(r);

	/* Secured, not saved. */
	obj_open(r, P_AP2);
	iface_open(r, AP);
	sv_ay(r, "Ssid", "Cafe");
	sv_y(r, "Strength", 44);
	sv_u(r, "Flags", AP_FLAG_PRIVACY);
	sv_u(r, "WpaFlags", 0);
	sv_u(r, "RsnFlags", SEC_PAIR_CCMP | SEC_KEY_MGMT_PSK);
	sv_u(r, "Frequency", 2437);
	iface_close(r);
	obj_close(r);

	/* Open, and only the SECOND radio can see it. */
	obj_open(r, P_AP3);
	iface_open(r, AP);
	sv_ay(r, "Ssid", "Airport Free");
	sv_y(r, "Strength", 17);
	sv_u(r, "Flags", 0);
	sv_u(r, "WpaFlags", 0);
	sv_u(r, "RsnFlags", 0);
	sv_u(r, "Frequency", 2412);
	iface_close(r);
	obj_close(r);

	/* One saved profile, named the way NM names its keyfile. */
	obj_open(r, P_CONN);
	iface_open(r, SC);
	sv_s(r, "Filename",
	     "/etc/NetworkManager/system-connections/Home Wifi.nmconnection");
	sv_b(r, "Unsaved", 0);
	sv_u(r, "Flags", 0);
	iface_close(r);
	obj_close(r);

	/* A VPN profile, and the active connection carrying it. */
	obj_open(r, P_VPN);
	iface_open(r, SC);
	sv_s(r, "Filename",
	     "/etc/NetworkManager/system-connections/Work VPN.nmconnection");
	sv_b(r, "Unsaved", 0);
	sv_u(r, "Flags", 0);
	iface_close(r);
	obj_close(r);

	/*
	 * ONE OBJECT, TWO INTERFACES — exactly as a wifi device is Device and
	 * Device.Wireless. A surface that looked for the VPN state on an object
	 * of its own would find neither.
	 */
	obj_open(r, P_ACT);
	iface_open(r, NM ".Connection.Active");
	sv_o(r, "Connection", P_VPN);
	sv_s(r, "Type", "vpn");
	sv_u(r, "State", 2);			/* ACTIVATED */
	sv_b(r, "Vpn", 1);
	iface_close(r);
	iface_open(r, NM ".VPN.Connection");
	sv_u(r, "VpnState", 5);			/* ACTIVATED */
	sv_s(r, "Banner", "");
	iface_close(r);
	obj_close(r);

	/* A WireGuard profile, saved and down. */
	obj_open(r, P_WG);
	iface_open(r, SC);
	sv_s(r, "Filename",
	     "/etc/NetworkManager/system-connections/wg0.nmconnection");
	sv_b(r, "Unsaved", 0);
	sv_u(r, "Flags", 0);
	iface_close(r);
	obj_close(r);

	sd_bus_message_close_container(r);
	sd_bus_send(NULL, r, NULL);
	sd_bus_message_unref(r);
	return 1;
}

int main(void)
{
	int r = sd_bus_open_system(&bus);
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000 * 1000 };

	if (r < 0) {
		fprintf(stderr, "nmobjstub: no bus: %s\n", strerror(-r));
		return 1;
	}
	r = sd_bus_add_filter(bus, NULL, managed, NULL);
	if (r < 0) {
		fprintf(stderr, "nmobjstub: cannot filter: %s\n", strerror(-r));
		return 1;
	}
	if (sd_bus_request_name(bus, NM, 0) < 0) {
		fprintf(stderr, "nmobjstub: cannot take the name\n");
		return 1;
	}
	/* Long enough for the surface's own settle() and no longer: the
	 * harness kills this anyway, and a stub that outlived its test would
	 * answer the next one. */
	for (int i = 0; i < 150; i++) {
		while (sd_bus_process(bus, NULL) > 0)
			;
		sd_bus_wait(bus, 200 * 1000);
		(void)ts;
	}
	sd_bus_unref(bus);
	return 0;
}
