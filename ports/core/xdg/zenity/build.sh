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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dwebkitgtk=false \
	-Dmanpage=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

test -x "$PKG/usr/bin/zenity"
test -f "$PKG/usr/share/man/man1/zenity.1"
