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

meson setup build --buildtype=release \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=enabled \
	-Dnls=enabled \
	-Ddocumentation=disabled \
	-Dgtk_doc=disabled \
	-Dman=true \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
