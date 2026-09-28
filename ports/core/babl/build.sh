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

# The SIMD options build extra copies of the conversion routines, and babl
# picks one at run time from the processor's flags, so they stay on without
# raising the baseline. relocatable-bundle=no keeps the extension directory at
# the configured prefix rather than beside the executable.
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
