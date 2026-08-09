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
	-D xlib=disabled \
	-D xcb=disabled \
	-D xlib-xcb=disabled \
	-D dwrite=disabled \
	-D spectre=disabled \
	-D symbol-lookup=disabled \
	-D tests=disabled
meson compile -C build
meson install --no-rebuild -C build --destdir $PKG
