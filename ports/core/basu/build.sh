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

sed -i "s|if not meson.is_cross_build()|if false|" meson.build
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Daudit=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
