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
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dwith_x11=no \
	-Dwith_glx=no \
	-Dwith_win32=no \
	-Dwith_wayland=yes \
	-Ddisable_drm=false \
	-Denable_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
