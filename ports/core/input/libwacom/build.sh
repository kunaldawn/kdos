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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib --buildtype=release \
	-Dudev-dir=/usr/lib/udev \
	-Dtests=disabled \
	-Ddocumentation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
rm "$PKG/usr/bin/libwacom-show-stylus" "$PKG/usr/share/man/man1/libwacom-show-stylus.1"
