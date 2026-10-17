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
	--wrap-mode=nodownload \
	-D png=enabled \
	-D jpeg=enabled \
	-D tiff=enabled \
	-D gif=enabled \
	-D others=enabled \
	-D glycin=disabled \
	-D introspection=enabled \
	-D documentation=false \
	-D man=true \
	-D tests=false \
	-D installed_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
