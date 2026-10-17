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

opts="-DHAVE_DBUS -DHAVE_DNSSEC -DHAVE_NFTSET -DHAVE_LIBIDN2"
make PREFIX=/usr CONFFILE=/etc/dnsmasq.conf COPTS="$opts" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
make PREFIX=/usr CONFFILE=/etc/dnsmasq.conf COPTS="$opts" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" DESTDIR=$PKG install
install -Dm644 dbus/dnsmasq.conf "$PKG/usr/share/dbus-1/system.d/dnsmasq.conf"
