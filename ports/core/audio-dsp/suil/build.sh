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

# Each wrapper module embeds one plugin toolkit in one host toolkit. gtk2 and
# qt5 stay off: no GTK 2 is built, and a Qt 5 host is served by its own
# plugin-side UI. x11 gives the X11-in-GTK 3 and X11-in-Qt 6 wrappers, which
# are how most plugin UIs (plain X11 windows) are embedded.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release -Ddocs=disabled -Dtests=disabled \
	-Dgtk3=enabled -Dqt6=enabled -Dx11=enabled \
	-Dgtk2=disabled -Dqt5=disabled -Dcocoa=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
