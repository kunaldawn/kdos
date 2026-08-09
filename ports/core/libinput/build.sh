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

meson \
-Dudev-dir=/lib/udev \
-Ddebug-gui=false \
-Db_ndebug=false \
-Dtests=false \
-Ddocumentation=false \
-Dlibwacom=false \
build
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
