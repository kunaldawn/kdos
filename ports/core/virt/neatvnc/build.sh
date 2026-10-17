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

patch -p1 -i $PORT_SRC/nettle4.patch

meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dtests=false \
	-Dexamples=false \
	-Dbenchmarks=false \
	-Dexperimental=false \
	-Dsystemtap=false \
	-Dtls=enabled \
	-Dnettle=enabled \
	-Djpeg=enabled \
	-Dgbm=enabled \
	-Dh264=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
