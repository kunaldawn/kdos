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
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D glycin=disabled \
	-D introspection=disabled \
	-D documentation=false \
	-D man=false \
	-D tests=false \
	-D installed_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
