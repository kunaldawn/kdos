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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nofallback \
	-Dpython=python3 \
	-Dpycairo=enabled \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

test -n "$(find "$PKG"/usr/lib/python3*/site-packages/gi -maxdepth 1 -name '_gi.*.so')"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/gi -maxdepth 1 -name '_gi_cairo.*.so')"
