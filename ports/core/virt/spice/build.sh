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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nodownload \
	-Dgstreamer=1.0 \
	-Dlz4=true \
	-Dsasl=true \
	-Dopus=enabled \
	-Dsmartcard=enabled \
	-Dmanual=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
