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
	-Dwrap_mode=nodownload \
	-Dwindows=wayland \
	-Dunicode=icu \
	-Dman=disabled \
	-Dtest=disabled \
	-Dlibpng=enabled \
	-Dlibjpeg=enabled \
	-Dlibtiff=enabled \
	-Dlibrsvg=enabled \
	-Dlibnsgif=enabled \
	-Dlibnsbmp=disabled \
	-Dlibheif=enabled \
	-Dlibjxl=disabled \
	-Dlibwebp=enabled \
	-Dfarbfeld=enabled \
	-Dqoi=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
