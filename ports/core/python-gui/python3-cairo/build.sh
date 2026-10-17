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
	-Dpython=python3 \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

test -n "$(find "$PKG"/usr/lib/python3*/site-packages/cairo -maxdepth 1 -name '_cairo.*.so')"
test -f "$PKG"/usr/lib/pkgconfig/py3cairo.pc
