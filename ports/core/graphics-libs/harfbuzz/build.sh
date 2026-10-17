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

export CFLAGS="$CFLAGS -frandom-seed=harfbuzz" CXXFLAGS="$CXXFLAGS -frandom-seed=harfbuzz"
meson setup build --buildtype=release -D b_lto=true \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D glib=enabled \
	-D gobject=enabled \
	-D freetype=enabled \
	-D cairo=enabled \
	-D png=enabled \
	-D zlib=enabled \
	-D chafa=disabled \
	-D graphite2=enabled \
	-D icu=enabled \
	-D gpu_demo=disabled \
	-D docs=disabled \
	-D tests=disabled \
	-D benchmark=disabled \
	-D introspection=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
