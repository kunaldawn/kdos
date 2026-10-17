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
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Ddoc=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dgpl=enabled \
	-Dx264=enabled \
	-Dorc=enabled \
	-Dorc-compiler=disabled \
	-Da52dec=enabled \
	-Dcdio=enabled \
	-Ddvdread=enabled \
	-Dmpeg2dec=enabled \
	-Dsidplay=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
