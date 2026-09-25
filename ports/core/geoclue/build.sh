#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The service runs as its own account, `geoclue`, which postinstall.sh
# creates. D-Bus activation starts it on the system bus through
# dbus-daemon-launch-helper when the first client asks, and it exits after a
# minute with none. Its bus policy lets that account own the name and ask
# wpa_supplicant for a scan, which is what the Wi-Fi source reads; its polkit
# rule lets the same account switch on a modem's GPS through ModemManager.
#
# Every source is named on. Wi-Fi and 3G ask beaconDB, upstream's default
# lookup service, over libsoup; the modem sources read ModemManager; the NMEA
# source finds a phone's GPS feed through Avahi; the compass is iio-sensor-proxy
# on the bus. The convenience library is what xdg-desktop-portal links for the
# Location portal. The demo agent is off: it is a notification-based consent
# dialog, and consent on this desktop is the portal's access dialog.
#
# Upstream holds every GetClient until a consent agent registers, with no
# timeout, and none can register under the empty whitelist below: the portal's
# call would hang until D-Bus gave up on it. no-agent.patch completes GetClient
# and Start at once when the whitelist is empty.
patch -p1 -i "$PORT_SRC/no-agent.patch"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=/usr/lib \
	--buildtype=release \
	-Dlibgeoclue=true \
	-Dintrospection=false \
	-Dvapi=false \
	-Dgtk-doc=false \
	-Dwifi-source=true \
	-D3g-source=true \
	-Dcdma-source=true \
	-Dmodem-gps-source=true \
	-Dnmea-source=true \
	-Dstatic-source=true \
	-Dip-source=true \
	-Dcompass=true \
	-Denable-backend=true \
	-Ddemo-agent=false \
	-Ddbus-srv-user=geoclue
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# sysusers.d is systemd's account mechanism; postinstall.sh makes the account.
rm -rf "$PKG/usr/lib/sysusers.d" "$PKG/usr/lib/systemd"

# where-am-i stays, as a command under /usr/lib/geoclue-2.0/demos; its entry is
# NoDisplay and names that path in Exec, which is not a menu row.
rm -f "$PKG/usr/share/applications/geoclue-where-am-i.desktop"
rmdir "$PKG/usr/share/applications" 2>/dev/null || true

# WHO MAY ASK. No consent agent runs here. The empty whitelist is what
# no-agent.patch reads to let a client through without one, so geoclue answers
# every client on the system bus that is not disallowed by name. The section
# for xdg-desktop-portal marks the Location portal a system component, which is
# the client this port is here for.
install -Dm644 /dev/stdin "$PKG/etc/geoclue/conf.d/90-kdos.conf" <<'CONF'
[agent]
whitelist=

[xdg-desktop-portal]
allowed=true
system=true
users=
CONF
