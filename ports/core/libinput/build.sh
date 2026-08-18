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
meson \
--prefix=/usr \
--libdir=lib \
-Dudev-dir=/lib/udev \
-Ddebug-gui=false \
-Db_ndebug=false \
-Dtests=false \
-Ddocumentation=false \
-Dlibwacom=false \
build
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
