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
	-Ddoc=disabled \
	-Dtests=disabled \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

test -n "$(find "$PKG"/usr/lib/python3*/site-packages -maxdepth 1 -name '_dbus_bindings.*.so')"
test -f "$PKG"/usr/include/dbus-1.0/dbus/dbus-python.h
