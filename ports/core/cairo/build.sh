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
	-D fontconfig=enabled \
	-D freetype=enabled \
	-D png=enabled \
	-D zlib=enabled \
	-D lzo=enabled \
	-D glib=enabled \
	-D tee=enabled \
	-D xlib=disabled \
	-D xcb=disabled \
	-D xlib-xcb=disabled \
	-D dwrite=disabled \
	-D quartz=disabled \
	-D spectre=disabled \
	-D symbol-lookup=disabled \
	-D gtk2-utils=disabled \
	-D gtk_doc=false \
	-D tests=disabled
meson compile -C build
meson install --no-rebuild -C build --destdir $PKG
