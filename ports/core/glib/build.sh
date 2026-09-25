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
	--libdir=lib \
	--buildtype=release \
	-D documentation=false \
	-D man-pages=enabled \
	-D nls=disabled \
	-D selinux=disabled \
	-D tests=false \
	-D wrap_mode=nodownload \
	-D libmount=enabled \
	-D libelf=enabled \
	-D glib_debug=disabled \
	-D dtrace=disabled \
	-D systemtap=disabled \
	-D sysprof=disabled \
	-D introspection=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
