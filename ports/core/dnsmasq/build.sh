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

# config.h leaves D-Bus, DNSSEC, nftables sets and IDN off; COPTS is the only
# switch, and the Makefile asks pkg-config for each library only when its
# HAVE_ is named here. Conntrack marking stays off: libnetfilter_conntrack is
# not a port.
opts="-DHAVE_DBUS -DHAVE_DNSSEC -DHAVE_NFTSET -DHAVE_LIBIDN2"
make PREFIX=/usr CONFFILE=/etc/dnsmasq.conf COPTS="$opts"
make PREFIX=/usr CONFFILE=/etc/dnsmasq.conf COPTS="$opts" DESTDIR=$PKG install
install -Dm644 dbus/dnsmasq.conf "$PKG/usr/share/dbus-1/system.d/dnsmasq.conf"
