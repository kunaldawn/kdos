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

export CFLAGS="${CFLAGS:--O3 -pipe} -fno-strict-aliasing"
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dgrapheme-clustering=enabled \
	-Dterminfo=enabled \
	-Dime=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
