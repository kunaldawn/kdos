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
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	-Dprogrammer=auto,internal,ch341a_spi,dediprog,ft2232_spi,jlink_spi \
	-Drpmc=enabled \
	-Dbash_completion=enabled \
	-Duse_git_version=disabled \
	-Dtests=disabled \
	-Dman-pages=enabled \
	-Ddocumentation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
