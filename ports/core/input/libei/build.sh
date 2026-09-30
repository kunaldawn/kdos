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

meson setup build --prefix=/usr --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dlibei=enabled \
	-Dlibeis=enabled \
	-Dliboeffis=enabled \
	-Dsd-bus-provider=basu \
	-Dtests=disabled \
	-Ddocumentation=[]
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
