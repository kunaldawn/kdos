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
	-D man-pages=enabled \
	-D valgrind=disabled \
	-D cairo-tests=disabled \
	-D intel=enabled \
	-D amdgpu=enabled \
	-D radeon=enabled \
	-D nouveau=enabled \
	-D vmwgfx=enabled \
	-D install-test-programs=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
