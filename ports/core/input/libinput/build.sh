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
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Db_ndebug=false \
	-Dudev-dir=/lib/udev \
	-Ddebug-gui=false \
	-Dtests=false \
	-Ddocumentation=false \
	-Dmtdev=true \
	-Dlua-plugins=enabled \
	-Dautoload-plugins=true \
	-Dlibwacom=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
