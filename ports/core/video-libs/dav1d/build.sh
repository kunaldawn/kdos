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

export CFLAGS="$CFLAGS -frandom-seed=dav1d"
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Db_lto=true \
	-Denable_asm=true \
	-Denable_tests=false -Denable_tools=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
