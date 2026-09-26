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

# --prefix and --libdir are both required.
#
# meson defaults to /usr/local, and its libdir on a 64-bit host to lib64, so
# omitting either installs to /usr/local/lib64 — a path the runtime linker
# does not search. The symptom is `Error loading shared library` from a
# consumer, which points at the consumer rather than here.
#
# A stale /usr/local/**/pkgconfig/*.pc shadows the correct one, so if such a
# tree exists it must be deleted before consumers are rebuilt or they link
# against the wrong path.
#
# Lua plugin support resolves lua5.4.pc from lua54; left on `auto` it would be
# on or off by whether that port happened to be installed first. wlroots
# creates its context without ever asking libinput to load plugins, so
# without autoload-plugins the Lua support is compiled in and a plugin
# dropped in /etc/libinput/plugins is never read.
#
# libwacom is on and in depends: it is the tablet database that pairs a
# stylus with its pad, groups pad buttons and rings into modes, and says
# whether a tablet is built into a screen, which touch arbitration and output
# mapping read. Without it a tablet is a generic absolute pointer.
meson \
--prefix=/usr \
--libdir=lib \
-Dudev-dir=/lib/udev \
-Ddebug-gui=false \
-Db_ndebug=false \
-Dtests=false \
-Ddocumentation=false \
-Dmtdev=true \
-Dlua-plugins=enabled \
-Dautoload-plugins=true \
-Dlibwacom=true \
build
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
