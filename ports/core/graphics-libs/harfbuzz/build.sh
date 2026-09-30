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

# chafa stays off: it reaches librsvg, which depends on this port, so declaring
# it would be a cycle. graphite2 stays off until it is a port. icu builds
# libharfbuzz-icu, which WebKitGTK requires. introspection installs
# HarfBuzz-0.0.gir, which pango's GIR includes.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D glib=enabled \
	-D gobject=enabled \
	-D freetype=enabled \
	-D cairo=enabled \
	-D png=enabled \
	-D zlib=enabled \
	-D chafa=disabled \
	-D graphite2=disabled \
	-D icu=enabled \
	-D gpu_demo=disabled \
	-D docs=disabled \
	-D tests=disabled \
	-D benchmark=disabled \
	-D introspection=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
