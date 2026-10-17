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

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	$(pkg-config --cflags basu) \
	-o xdg-desktop-portal-kdos "$PORT_SRC/main.c" \
	$(pkg-config --libs basu) $LDFLAGS

install -Dm755 xdg-desktop-portal-kdos \
	"$PKG/usr/lib/xdg-desktop-portal-kdos"

install -Dm644 "$PORT_SRC/kdos.portal" \
	"$PKG/usr/share/xdg-desktop-portal/portals/kdos.portal"
install -Dm644 "$PORT_SRC/kdos.service" \
	"$PKG/usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.kdos.service"
