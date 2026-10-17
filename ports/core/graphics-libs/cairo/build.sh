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

export CFLAGS="$CFLAGS -frandom-seed=cairo"
meson setup build --buildtype=release -D b_lto=true \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D fontconfig=enabled \
	-D freetype=enabled \
	-D png=enabled \
	-D zlib=enabled \
	-D lzo=enabled \
	-D glib=enabled \
	-D tee=enabled \
	-D xlib=enabled \
	-D xcb=enabled \
	-D xlib-xcb=enabled \
	-D dwrite=disabled \
	-D quartz=disabled \
	-D spectre=disabled \
	-D symbol-lookup=disabled \
	-D gtk2-utils=disabled \
	-D gtk_doc=false \
	-D tests=disabled
meson compile -C build
meson install --no-rebuild -C build --destdir $PKG
