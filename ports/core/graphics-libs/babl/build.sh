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
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dwith-docs=false \
	-Dgi-docgen=disabled \
	-Denable-gir=true \
	-Denable-vapi=true \
	-Dwith-lcms=enabled \
	-Drelocatable-bundle=no
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
