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

export XML_CATALOG_FILES=/etc/xml/catalog

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dwrap_mode=nodownload \
	-Dwindows=wayland \
	-Dunicode=icu \
	-Dman=enabled \
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
