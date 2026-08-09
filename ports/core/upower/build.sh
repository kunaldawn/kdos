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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Dintrospection=disabled \
	-Dgtk-doc=false \
	-Dman=false \
	-Dinstalled_tests=false \
	-Didevice=disabled \
	-Dpolkit=enabled \
	-Dsystemdsystemunitdir=no \
	-Dudevrulesdir=/usr/lib/udev/rules.d
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
