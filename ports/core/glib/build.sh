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
	--buildtype=release \
	-D documentation=false \
	-D nls=disabled \
	-D selinux=disabled \
	-D tests=false \
	-D wrap_mode=nodownload \
	-D libmount=disabled \
	-D introspection=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
