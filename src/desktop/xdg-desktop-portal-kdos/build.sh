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

# source= is empty: this is ours. It links basu and nothing else — the chooser
# is a separate program driven over a pipe, so none of libktui, libkwl or
# Wayland is reachable from here.
gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	$(pkg-config --cflags basu) \
	-o xdg-desktop-portal-kdos "$PORT_SRC/main.c" \
	$(pkg-config --libs basu) $LDFLAGS

# /usr/lib, where kdos-desktop-start already looks for the other portal
# daemons — xdg-desktop-portal-wlr is built --libexecdir=/usr/lib for the
# same reason.
install -Dm755 xdg-desktop-portal-kdos \
	"$PKG/usr/lib/xdg-desktop-portal-kdos"

# The backend declaration. `UseIn` is what xdg-desktop-portal matches against
# XDG_CURRENT_DESKTOP, and without it this backend is never considered no
# matter what kdos-portals.conf says.
install -Dm644 "$PORT_SRC/kdos.portal" \
	"$PKG/usr/share/xdg-desktop-portal/portals/kdos.portal"

# D-Bus activation. The portal daemon starts backends on demand rather than
# expecting them to be running, so without this file the first Open dialog
# fails with ServiceUnknown and the daemon remembers it.
install -Dm644 "$PORT_SRC/kdos.service" \
	"$PKG/usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.kdos.service"
