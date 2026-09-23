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

patch -p1 -i $PORT_SRC/wayland-generated-header.patch

meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dflavors=wayland-gl,wayland-glesv2,drm-gl,drm-glesv2,gbm-gl,gbm-glesv2
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
