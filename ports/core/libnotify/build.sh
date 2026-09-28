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
# introspection installs Notify-0.7, which Python programs load through
# gi.require_version('Notify', '0.7'); its GIR includes GdkPixbuf-2.0.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dtests=false \
	-Dintrospection=enabled \
	-Dman=true \
	-Dgtk_doc=false \
	-Ddocbook_docs=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
