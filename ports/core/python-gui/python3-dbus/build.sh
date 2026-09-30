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

# Built with meson directly rather than through pip: only that installs
# dbus-python.pc and dbus/dbus-python.h, which PyQt6's dbus.mainloop.pyqt6
# module compiles against. dbus-gmain is carried inside the tarball.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddoc=disabled \
	-Dtests=disabled \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

test -n "$(find "$PKG"/usr/lib/python3*/site-packages -maxdepth 1 -name '_dbus_bindings.*.so')"
test -f "$PKG"/usr/include/dbus-1.0/dbus/dbus-python.h
