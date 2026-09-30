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
	-Dgrapheme-shaping=enabled \
	-Drun-shaping=enabled \
	-Dsvg-backend=nanosvg \
	-Dsystem-nanosvg=disabled \
	-Dexamples=false \
	-Ddocs=enabled \
	-Dtest-text-shaping=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
