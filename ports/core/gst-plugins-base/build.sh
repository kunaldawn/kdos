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
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Dintrospection=disabled \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dx11=disabled \
	-Dxshm=disabled \
	-Dxvideo=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
