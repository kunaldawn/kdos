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
	-D man-pages=disabled \
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
	-D introspection=enabled
meson compile -C build
DESTDIR=$PWD/stage meson install --no-rebuild -C build

install -d $PKG/usr/share/gir-1.0 $PKG/usr/lib/girepository-1.0
cp -a stage/usr/share/gir-1.0/*.gir $PKG/usr/share/gir-1.0/
cp -a stage/usr/lib/girepository-1.0/*.typelib $PKG/usr/lib/girepository-1.0/
