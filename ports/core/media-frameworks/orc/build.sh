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
	-Dorc-target=all \
	-Dorc-test=enabled \
	-Dtools=enabled \
	-Dbenchmarks=disabled \
	-Dexamples=disabled \
	-Dhotdoc=disabled \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
