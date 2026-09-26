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

# notify-send and the client library. Nothing here draws a notification: the
# library speaks org.freedesktop.Notifications on the session bus, which
# kdos-notifyd owns. GTK appears only in upstream's tests, which are off.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dtests=false \
	-Dintrospection=disabled \
	-Dman=true \
	-Dgtk_doc=false \
	-Ddocbook_docs=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
