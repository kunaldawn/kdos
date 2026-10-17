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

unset CFLAGS CXXFLAGS LDFLAGS
meson setup build \
	--cross-file scripts/cross-arm-none-eabi.txt \
	--prefix=/usr/arm-none-eabi \
	-Dsysroot-install=true \
	-Dtests=false \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
